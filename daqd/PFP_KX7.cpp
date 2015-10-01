#include "PFP_KX7.hpp"
#include <assert.h>

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <assert.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <math.h>
#include <unistd.h>
#include "boost/date_time/posix_time/posix_time.hpp"

using namespace DAQd;

static const unsigned DMA_TRANS_BYTE_SIZE = 262144;  // max for USER_FIFO_THRESHOLD 262128
static const unsigned wordBufferSize = DMA_TRANS_BYTE_SIZE / 8;
static const int N_BUFFERS = 1;

//static int COM_count = 0;

static void *userIntHandler(WDC_DEVICE_HANDLE Device, PFP_INT_RESULT *Result)
{
	return NULL;
}

PFP_KX7::PFP_KX7()
{
	DWORD Status;
	char version[5];
	Status = PFP_OpenDriver(0);  // open driver
	assert(Status == WD_STATUS_SUCCESS);
	PFP_Get_Version(version);  // get driver version
	Status = PFP_OpenCard(&Card, NULL, NULL);  // open card
	assert(Status == WD_STATUS_SUCCESS);

	unsigned int ExtClkFreq;
	PFP_SM_Get_ECF0(Card, &ExtClkFreq);
	if (ExtClkFreq < 199990000 || ExtClkFreq > 200010000) {  // check the ext oscilator frequency
		Status = PFP_SM_Set_Freq0(Card, 200000);  // if not correct, reprogram
		assert(Status == WD_STATUS_SUCCESS);
	}


	// setting up firmware configuration
	// Filler Activated (26); Disable Interrupt (24); Disable AXI Stream (21)
	// Use User FIFO Threshold (16); User FIFO Threshold in 64B word addressing (11 -> 0)
	uint32_t word32;
//	word32 = 0x01000000;  // without filler
	word32 = 0x09000000;  // with filler
//	word32 = 0x09200000;  // stream disable, with filler
	Status = PFP_Write(Card, BaseAddrReg + ConfigReg * 4, 4, &word32, WDC_MODE_32, WDC_ADDR_RW_DEFAULT);
	assert(Status == WD_STATUS_SUCCESS);

	// setting up ToT Threshold
	// ToT Threshold (31 -> 0)
	word32 = 0x00000064;
	Status = PFP_Write(Card, BaseAddrReg + ThresholdReg * 4, 4, &word32, WDC_MODE_32, WDC_ADDR_RW_DEFAULT);
	assert(Status == WD_STATUS_SUCCESS);

	// setting up Coincidence Windows
	// Single Window step (27 -> 24); Coincidence Window (21 -> 20); Coinc. Aft. Window (14 -> 8); Coinc. Pre Window (3 -> 0)
	// Single Window step: 0 = 0 TCoarse; 1 = 10 TCoarse; 2 = 20 TCoarse; 3 = 50 TCoarse; 4->14 = 100 TCoarse; 15 = all
//	word32 = 0x00101001;  // sent coincidence only
	word32 = 0x0F101001;  // sent everthing
	Status = PFP_Write(Card, BaseAddrReg + CoincWindowReg * 4, 4, &word32, WDC_MODE_32, WDC_ADDR_RW_DEFAULT);
	assert(Status == WD_STATUS_SUCCESS);

	// setting up Coincidence Mask
	// (31 -> 16) for one channel; (15 -> 0) for other channel; (2 channels per address)
	// k bit = '1' for i channel be able to search coincidence with k channel
	// i (own) bit = '1' allows coincidence with more than 1 other channel; = '0' only allows coinc with 1 and only 1 other channel
	word32 = 0xFFFFFFFF;
	for (int i = 0; i < (12+1)/2; i++) {  // 12 channels => 6 addresses
		Status = PFP_Write(Card, BaseAddrReg + (CoincMasksReg + i) * 4, 4, &word32, WDC_MODE_32, WDC_ADDR_RW_DEFAULT);
		assert(Status == WD_STATUS_SUCCESS);
	}

	// setting up DMA configuration
	// DMA Start toggle (26); DMA Size in 16B word addressing (23 -> 0)
	word32 = 0x08000000 + DMA_TRANS_BYTE_SIZE / 16;
	Status = PFP_Write(Card, BaseAddrReg + DMAConfigReg * 4, 4, &word32, WDC_MODE_32, WDC_ADDR_RW_DEFAULT);
	assert(Status == WD_STATUS_SUCCESS);

	// read Status
	// Channel Up information
	Status = PFP_Read(Card, BaseAddrReg + statusReg * 4, 4, &word32, WDC_MODE_32, WDC_ADDR_RW_DEFAULT);
	assert(Status == WD_STATUS_SUCCESS);
	printf("Channel Up: %03X\n", word32);


	pthread_mutex_init(&lock, NULL);
	pthread_cond_init(&condCleanBuffer, NULL);
	pthread_cond_init(&condDirtyBuffer, NULL);

	pthread_mutex_init(&hwLock, NULL);
	lastCommandTime = boost::posix_time::microsec_clock::local_time(); 
	
	ReadAndCheck(txWrPointerReg * 4 , &txWrPointer, 1);
	ReadAndCheck(rxRdPointerReg * 4 , &rxRdPointer, 1);

	wordBuffer = new uint64_t[wordBufferSize];
	wordBufferUsed = 0;
	wordBufferStatus = 0;
	
	die = true;
	hasWorker = false;

	setAcquistionOnOff(false);
	startWorker();
}

PFP_KX7::~PFP_KX7()
{
	printf("PFP_KX7::~PFP_KX7\n");
	stopWorker();

	delete [] wordBuffer;

	pthread_mutex_destroy(&hwLock);
	pthread_cond_destroy(&condDirtyBuffer);
	pthread_cond_destroy(&condCleanBuffer);
	pthread_mutex_destroy(&lock);	

	PFP_CloseCard(Card);  // closing card
	PFP_CloseDriver();  // closing driver
}

const uint64_t idleWord = 0xFFFFFFFFFFFFFFFFULL;

void *PFP_KX7::runWorker(void *arg)
{
	const uint64_t idleWord = 0xFFFFFFFFFFFFFFFFULL;

	printf("PFP_KX7::runWorker launching...\n");
	PFP_KX7 *p = (PFP_KX7 *)arg;

	DWORD Status, DMAStatus;
	unsigned int ExtraDMAByteCount = 0;
	uint32_t word32_108, word32_256;
	uint32_t word32_dmat = 0x8000000 + DMA_TRANS_BYTE_SIZE / 16;
	int DMA_count = 0;

	// setting up DMA buffer
	Status = PFP_SetupDMA(p->Card, &p->DMA_Buffer, DMA_FROM_DEVICE, DMA_TRANS_BYTE_SIZE, &p->DMA_Point);
	assert(Status == WD_STATUS_SUCCESS);

	// setting up DMA configuration
	// DMA Start toggle (26); DMA Size in 16B word addressing (23 -> 0)
	Status = PFP_Write(p->Card, BaseAddrReg + DMAConfigReg * 4, 4, &word32_dmat, WDC_MODE_32, WDC_ADDR_RW_DEFAULT);
	assert(Status == WD_STATUS_SUCCESS);

	word32_dmat = word32_dmat ^ 0x08000000;
	Status = PFP_Write(p->Card, BaseAddrReg + DMAConfigReg * 4, 4, &word32_dmat, WDC_MODE_32, WDC_ADDR_RW_DEFAULT);
	assert(Status == WD_STATUS_SUCCESS);


	while(!p->die) {
		pthread_mutex_lock(&p->hwLock);
		while((boost::posix_time::microsec_clock::local_time() - p->lastCommandTime).total_milliseconds() < 5) {
			pthread_mutex_unlock(&p->hwLock);
			usleep(1000);
			pthread_mutex_lock(&p->hwLock);
		}
		DMAStatus = PFP_DoDMA(p->Card, p->DMA_Point, 0, DMA_TRANS_BYTE_SIZE, DMA_AXI_STREAM, 10);
		pthread_mutex_unlock(&p->hwLock);
		assert(DMAStatus != TW_DMA_ERROR);
		if (DMAStatus == TW_DMA_TIMEOUT || DMAStatus == TW_DMA_EOT)
			ExtraDMAByteCount = 4;
		else
			ExtraDMAByteCount = 0;

		pthread_mutex_lock(&p->hwLock);
		// read DMA transfered size
		// 0x108 on Timeout and End of Transfer will have an extra 4B added
		Status = PFP_Read(p->Card, 0x108, 4, &word32_108, WDC_MODE_32, WDC_ADDR_RW_DEFAULT);
		assert(Status == WD_STATUS_SUCCESS);
		// DMACptSizeReg on Timeout and End of Transfer will have an extra 16B added
		Status = PFP_Read(p->Card, BaseAddrReg + DMACptSizeReg * 4, 4, &word32_256, WDC_MODE_32, WDC_ADDR_RW_DEFAULT);
		assert(Status == WD_STATUS_SUCCESS);
		pthread_mutex_unlock(&p->hwLock);

		pthread_mutex_lock(&p->lock);	
		while(!p->die && (p->wordBufferUsed < p->wordBufferStatus)) {
			pthread_cond_wait(&p->condCleanBuffer, &p->lock);
		}

		p->wordBufferUsed = word32_108 / 4;  // converting to 16 byte addressing
		p->wordBufferUsed = p->wordBufferUsed * 16;  // converting to byte addressing
		p->wordBufferStatus = word32_256 / 8;
		if (p->wordBufferUsed < DMA_TRANS_BYTE_SIZE) {
			if (word32_256 >= ExtraDMAByteCount * 4)  // prevent negative value to be written
				p->wordBufferStatus = (word32_256 - ExtraDMAByteCount * 4) / 8;
		}

		// Reseting DMA
		word32_dmat = word32_dmat ^ 0x08000000;
		Status = PFP_Write(p->Card, BaseAddrReg + DMAConfigReg * 4, 4, &word32_dmat, WDC_MODE_32, WDC_ADDR_RW_DEFAULT);
		assert(Status == WD_STATUS_SUCCESS);

		if(p->die || p->wordBufferUsed == 0) {
		  	p->wordBufferUsed = ENOWORDS - 1; // Set wordBufferUsed to some number smaller than status
			p->wordBufferStatus = ENOWORDS;
			printf("PFP_KX7::worker wordBufferStatus set to %d\n", p->wordBufferStatus);
		}
		else {
			memcpy(p->wordBuffer, p->DMA_Buffer, p->wordBufferUsed);		
			p->wordBufferUsed = 0;	
		}
		pthread_mutex_unlock(&p->lock);
	       	pthread_cond_signal(&p->condDirtyBuffer);
		
		DMA_count++;
	}
	pthread_cond_signal(&p->condDirtyBuffer);
	// releasing DMA buffer
	PFP_UnsetupDMA(p->DMA_Point);
	
	
	printf("PFP_KX7::runWorker exiting...\n");
	return NULL;
}

int PFP_KX7::getWords(uint64_t *buffer, int count)
{
	int r = 0;
	while(r < count) {		
		int status = getWords_(buffer + r, count - r);
		if(status < 0) 
			return status;
	
		else 
			r += status;
	}

	return r;
}

int PFP_KX7::getWords_(uint64_t *buffer, int count)
{
	int result = -1;
	
	if(wordBufferUsed >= wordBufferStatus)
		pthread_cond_signal(&condCleanBuffer);
	
	pthread_mutex_lock(&lock);		
	while(!die && (wordBufferUsed >= wordBufferStatus)) {
		pthread_cond_wait(&condDirtyBuffer, &lock);
	}
	
	if (die) {
		result = -1;
	}
	else if(wordBufferStatus < 0) {
		printf("Buffer status was set to %d\n", wordBufferStatus);
		wordBufferUsed = wordBufferStatus; // Set wordBufferUsed to some number equal or greater than status
		result = wordBufferStatus;
	}	
	else {	
		int wordsAvailable = wordBufferStatus - wordBufferUsed;
		result = count < wordsAvailable ? count : wordsAvailable;
		if(result < 0) result = 0;
		memcpy(buffer, wordBuffer + wordBufferUsed, result * sizeof(uint64_t));
		wordBufferUsed += result;
	}
	pthread_mutex_unlock(&lock);

	return result;
}

void PFP_KX7::startWorker()
{
	printf("PFP_KX7::startWorker() called...\n");
	if(hasWorker)
		return;
       
	die = false;
	hasWorker = true;
	pthread_create(&worker, NULL, runWorker, (void*)this);

	printf("PFP_KX7::startWorker() exiting...\n");	
}
void PFP_KX7::stopWorker()
{
	printf("PFP_KX7::stopWorker() called...\n");

	die = true;
	pthread_cond_signal(&condCleanBuffer);
	pthread_cond_signal(&condDirtyBuffer);
	if(hasWorker) {
		hasWorker = false;

		pthread_join(worker, NULL);
	}
	printf("PFP_KX7::stopWorker() exiting...\n");
}

bool PFP_KX7::cardOK()
{
	return true;
}


int PFP_KX7::sendCommand(int portID, int slaveID, char *buffer, int bufferSize, int commandLength)
{
	int status;

	uint64_t outBuffer[8];
	for(int i = 0; i < 8; i++) outBuffer[i] = 0x0FULL << (4*i);

	uint64_t header = 0;
	header = header + (8ULL << 36);
	header = header + (uint64_t(portID) << 60) + (uint64_t(slaveID) << 54);

	outBuffer[0] = header;
	outBuffer[1] = commandLength;
	memcpy((void*)(outBuffer+2), buffer, commandLength);
	
	uint32_t txRdPointer;

	boost::posix_time::ptime startTime = boost::posix_time::microsec_clock::local_time();
	do {
		pthread_mutex_lock(&hwLock);
		// Read the RD pointer
		status = ReadAndCheck(txRdPointerReg * 4 , &txRdPointer, 1);
		lastCommandTime = boost::posix_time::microsec_clock::local_time(); 
		pthread_mutex_unlock(&hwLock);

		boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
		if((now - startTime).total_milliseconds() > 10) {
			return -1;
		}		


		// until there is space to write the command
	} while( ((txWrPointer & 16) != (txRdPointer & 16)) && ((txWrPointer & 15) == (txRdPointer & 15)) );
	
	pthread_mutex_lock(&hwLock);
	uint32_t wrSlot = txWrPointer & 15;
	

	status = WriteAndCheck(wrSlot * 16 * 4 , (uint32_t *)outBuffer, 16);
		
	txWrPointer += 1;
	txWrPointer = txWrPointer & 31;	// There are only 16 slots, but we use another bit for the empty/full condition
	txWrPointer |= 0xCAFE1500;
	
	status = WriteAndCheck(txWrPointerReg * 4 , &txWrPointer, 1);
	lastCommandTime = boost::posix_time::microsec_clock::local_time();
	pthread_mutex_unlock(&hwLock);
	return 0;

}

int PFP_KX7::recvReply(char *buffer, int bufferSize)
{
	int status;
	uint64_t outBuffer[8];
	
	uint32_t rxWrPointer;

	boost::posix_time::ptime startTime = boost::posix_time::microsec_clock::local_time();
	int nLoops = 0;
	do {
		pthread_mutex_lock(&hwLock);
		// Read the WR pointer
		status = ReadAndCheck(rxWrPointerReg * 4 , &rxWrPointer, 1);
		lastCommandTime = boost::posix_time::microsec_clock::local_time(); 
		pthread_mutex_unlock(&hwLock);

		boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
		if((now - startTime).total_milliseconds() > 10) {
			return -1;
		}		

		// until there a reply to read
	} while((rxWrPointer & 31) == (rxRdPointer & 31));
	pthread_mutex_lock(&hwLock);
	uint32_t rdSlot = rxRdPointer & 15;
	
	status = ReadAndCheck((768 + rdSlot * 16) * 4 , (uint32_t *)outBuffer, 16);
	rxRdPointer += 1;
	rxRdPointer = rxRdPointer & 31;	// There are only 16 slots, but we use another bit for the empty/full condition
	rxRdPointer |= 0xFACE9100;
	
	status = WriteAndCheck(rxRdPointerReg * 4 , &rxRdPointer, 1);
	lastCommandTime = boost::posix_time::microsec_clock::local_time();
	pthread_mutex_unlock(&hwLock);
	
	int replyLength = outBuffer[1];
	replyLength = replyLength < bufferSize ? replyLength : bufferSize;
	memcpy(buffer, outBuffer+2, replyLength);

	return replyLength;
}

int PFP_KX7::setAcquistionOnOff(bool enable)
{
	uint32_t data[1];
	int status;
	
	data[0] = enable ? 0x000FFFFU : 0xFFFF0000U;
	pthread_mutex_lock(&hwLock);
	status = WriteAndCheck(acqStatusPointerReg * 4, data, 1);	
	lastCommandTime = boost::posix_time::microsec_clock::local_time();
	pthread_mutex_unlock(&hwLock);
	return status;	
}

int PFP_KX7::WriteAndCheck(int reg, uint32_t *data, int count) {
	assert(reg >= 0);
	assert((reg/4 + count) >= 0);
	assert((reg/4 + count) <= 1024);
	
	uint32_t *readBackBuffer = new uint32_t[count];
	bool fail = true;
	DWORD Status;
	int iter = 0;
	while(fail) {
		reg = BaseAddrReg + reg;
		//Status = PFP_Write(Card, reg, count * 4, data, WDC_MODE_32, WDC_ADDR_RW_DEFAULT);
		for(int i = 0; i < count; i++) PFP_Write(Card, reg + 4*i, 4, data+i, WDC_MODE_32, WDC_ADDR_RW_DEFAULT);
		for(int i = 0; i < count; i++) readBackBuffer[i] = 0;
		//PFP_Read(Card, reg, count * 4, readBackBuffer, WDC_MODE_32, WDC_ADDR_RW_DEFAULT);
		for(int i = 0; i < count; i++) PFP_Read(Card, reg + 4*i, 4, readBackBuffer+i, WDC_MODE_32, WDC_ADDR_RW_DEFAULT);
		fail = false;
		for(int i = 0; i < count; i++) fail |= (data[i] != readBackBuffer[i]);		
		iter++;
	}
	delete [] readBackBuffer;
	return (int) Status;
};

int PFP_KX7::ReadAndCheck(int reg, uint32_t *data, int count) {
	assert(reg >= 0);
	assert((reg/4 + count) >= 0);
	assert((reg/4 + count) <= 1024);

	uint32_t *readBackBuffer = new uint32_t[count];
	bool fail = true;
	DWORD Status;
	int iter = 0;
	while(fail) {
		reg = BaseAddrReg + reg;
		//Status = PFP_Read(Card, reg, count * 4, data, WDC_MODE_32, WDC_ADDR_RW_DEFAULT);
		for(int i = 0; i < count; i++) PFP_Read(Card, reg + 4*i, 4, data+i, WDC_MODE_32, WDC_ADDR_RW_DEFAULT);
		for(int i = 0; i < count; i++) readBackBuffer[i] = 0;
		//PFP_Read(Card, reg, count * 4, readBackBuffer, WDC_MODE_32, WDC_ADDR_RW_DEFAULT);
		for(int i = 0; i < count; i++) PFP_Read(Card, reg + 4*i, 4, readBackBuffer+i, WDC_MODE_32, WDC_ADDR_RW_DEFAULT);
		fail = false;
		for(int i = 0; i < count; i++) fail |= (data[i] != readBackBuffer[i]);		
		iter++;
	}
	delete [] readBackBuffer;
	return (int) Status;
};

uint64_t PFP_KX7::getPortUp()
{
	pthread_mutex_lock(&hwLock);
	uint64_t reply = 0;
	uint32_t channelUp = 0;
	ReadAndCheck(statusReg * 4, &channelUp, 1);
	reply = channelUp;
	lastCommandTime = boost::posix_time::microsec_clock::local_time(); 
	pthread_mutex_unlock(&hwLock);
	return reply;
}

uint64_t PFP_KX7::getPortCounts(int channel, int whichCount)
{
	pthread_mutex_lock(&hwLock);
	uint64_t reply;
	ReadAndCheck((statusReg + 1 + 6*channel + 2*whichCount)* 4, (uint32_t *)&reply, 2);
	lastCommandTime = boost::posix_time::microsec_clock::local_time(); 
	pthread_mutex_unlock(&hwLock);
	return reply;
}
