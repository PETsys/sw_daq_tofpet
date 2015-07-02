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
	// Filler Activated (26); Disable Interrupt (24);
	// Use User FIFO Threshold (16); User FIFO Threshold in 64B word addressing (11 -> 0)
	uint32_t word32;
	word32 = 0x09000000;
	Status = PFP_Write(Card, BaseAddrReg + ConfigReg * 4, 4, &word32, WDC_MODE_32, WDC_ADDR_RW_DEFAULT);
	assert(Status == WD_STATUS_SUCCESS);

	// setting up DMA configuration
	// DMA Start toggle (26); DMA Size in 16B word addressing (23 -> 0)
	word32 = 0x08000000 + DMA_TRANS_BYTE_SIZE / 16;
	Status = PFP_Write(Card, BaseAddrReg + DMAConfigReg * 4, 4, &word32, WDC_MODE_32, WDC_ADDR_RW_DEFAULT);
	assert(Status == WD_STATUS_SUCCESS);


//	logFile=fopen("./logfile.txt", "w");
	pthread_mutex_init(&lock, NULL);
	pthread_cond_init(&condCleanBuffer, NULL);
	pthread_cond_init(&condDirtyBuffer, NULL);

	pthread_mutex_init(&hwLock, NULL);
	lastCommandTime = boost::posix_time::microsec_clock::local_time(); 
	
	ReadAndCheck(txWrPointerReg * 4 , &txWrPointer, 1);
	ReadAndCheck(rxRdPointerReg * 4 , &rxRdPointer, 1);
/*	printf("PFP_KX7:: Initial TX WR pointer: %08x\n", txWrPointer);
	printf("PFP_KX7:: Initial RX RD pointer: %08x\n", rxRdPointer);
	
	for(int i = 0; i < 4; i++) 
		for(int j = 0; j < 3; j++)
			printf("%d %d %016llx\n", i, j, getPortCounts(i, j));

	for(int i = 0; i < N_BUFFERS; i++)
	{
		Buffer_t * buffer = new Buffer_t();
		buffer->bufferIndex = i;
		buffer->buffer.ByteCount = DMA_TRANS_BYTE_SIZE;
		AllocateBuffer(i, &buffer->buffer);
		cleanBufferQueue.push(buffer);
	}

	buffers = new Buffer_t[N_BUFFERS];
	for(int i = 0; i < N_BUFFERS; i++) {
		buffers[i].bufferIndex = i;
		buffers[i].buffer.ByteCount = DMA_TRANS_BYTE_SIZE;
		AllocateBuffer(i, &(buffers[i].buffer));
	}
	tail = 0;
	head = 0; 
*/
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

// static void cacthINT1(int signal)
// {
// 	printf("1: SIG caught, %d\n", signal);
// }
// static void cacthINT2(int signal)
// {
// 	printf("2: SIG caught, %d\n", signal);
// }

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
//	p->dmaBuffer.ByteCount = DMA_TRANS_BYTE_SIZE;

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
//		while((boost::posix_time::microsec_clock::local_time() - p->lastCommandTime).total_milliseconds() < 300) {
//			pthread_mutex_unlock(&p->hwLock);
//			usleep(10000);
//			pthread_mutex_lock(&p->hwLock);
//		}
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
//		assert(p->wordBufferUsed == p->wordBufferStatus * 8);
		if (p->wordBufferUsed != p->wordBufferStatus * 8) {
			printf("0x108=%08X(%08X) 256=%08X(%08X) %s\n", p->wordBufferUsed, word32_108, p->wordBufferStatus * 8, word32_256, PFP_DMA_Status2Str(DMAStatus));
			printf("first word %08X%08X %08X%08X %08X%08X %08X%08X\n",
					((uint32_t*)(p->DMA_Buffer))[1],((uint32_t*)(p->DMA_Buffer))[0],
					((uint32_t*)(p->DMA_Buffer))[3],((uint32_t*)(p->DMA_Buffer))[2],
					((uint32_t*)(p->DMA_Buffer))[5],((uint32_t*)(p->DMA_Buffer))[4],
					((uint32_t*)(p->DMA_Buffer))[7],((uint32_t*)(p->DMA_Buffer))[6]);
			printf("last  word %08X%08X %08X%08X %08X%08X %08X%08X\n",
					((uint32_t*)(p->DMA_Buffer))[DMA_TRANS_BYTE_SIZE/4-15],((uint32_t*)(p->DMA_Buffer))[DMA_TRANS_BYTE_SIZE/4-16],
					((uint32_t*)(p->DMA_Buffer))[DMA_TRANS_BYTE_SIZE/4-13],((uint32_t*)(p->DMA_Buffer))[DMA_TRANS_BYTE_SIZE/4-14],
					((uint32_t*)(p->DMA_Buffer))[DMA_TRANS_BYTE_SIZE/4-11],((uint32_t*)(p->DMA_Buffer))[DMA_TRANS_BYTE_SIZE/4-12],
					((uint32_t*)(p->DMA_Buffer))[DMA_TRANS_BYTE_SIZE/4-9],((uint32_t*)(p->DMA_Buffer))[DMA_TRANS_BYTE_SIZE/4-10]);
			printf("           %08X%08X %08X%08X %08X%08X %08X%08X\n",
					((uint32_t*)(p->DMA_Buffer))[DMA_TRANS_BYTE_SIZE/4-7],((uint32_t*)(p->DMA_Buffer))[DMA_TRANS_BYTE_SIZE/4-8],
					((uint32_t*)(p->DMA_Buffer))[DMA_TRANS_BYTE_SIZE/4-5],((uint32_t*)(p->DMA_Buffer))[DMA_TRANS_BYTE_SIZE/4-6],
					((uint32_t*)(p->DMA_Buffer))[DMA_TRANS_BYTE_SIZE/4-3],((uint32_t*)(p->DMA_Buffer))[DMA_TRANS_BYTE_SIZE/4-4],
					((uint32_t*)(p->DMA_Buffer))[DMA_TRANS_BYTE_SIZE/4-1],((uint32_t*)(p->DMA_Buffer))[DMA_TRANS_BYTE_SIZE/4-2]);
			// writing everything of the last DMA into a file and then exit
/*			FILE *StreamFile = fopen("./LastStreamOutput.txt", "w");
			fprintf(StreamFile, "0x108=%08X 256=%08X\n", p->wordBufferUsed, p->wordBufferStatus * 8);
			unsigned int i;
			for(i=0; i<DMA_TRANS_BYTE_SIZE/8; i++)
				fprintf(StreamFile, "%08X%08X\n",((uint32_t*)(p->DMA_Buffer))[2*i+1],((uint32_t*)(p->DMA_Buffer))[2*i]);
			fprintf(StreamFile, "\n");
			fclose(StreamFile);
*///			assert(word32_108 != 0x10000);
		}
//		else
//			printf("match %08X %08X %s\n", p->wordBufferUsed, p->wordBufferStatus * 8, PFP_DMA_Status2Str(DMAStatus));
//			printf(".");

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
	}
	pthread_cond_signal(&p->condDirtyBuffer);
	// releasing DMA buffer
	PFP_UnsetupDMA(p->DMA_Point);
	
	
	printf("PFP_KX7::runWorker exiting...\n");
	return NULL;
}

int PFP_KX7::getWords(uint64_t *buffer, int count)
{
//	printf("PFP_KX7::getWords(%p, %d)\n", buffer, count);
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
//	printf("PFP_KX7::getWords_(%p, %d) ", buffer, count);
	int result = -1;
	
//	printf("status = %d, used = %d\n", wordBufferStatus, wordBufferUsed);
	
	if(wordBufferUsed >= wordBufferStatus)
		pthread_cond_signal(&condCleanBuffer);
	
	pthread_mutex_lock(&lock);		
	while(!die && (wordBufferUsed >= wordBufferStatus)) {
// 		pthread_mutex_unlock(&lock);
// 		pthread_cond_signal(&condCleanBuffer);
// 		pthread_mutex_lock(&lock);
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

//	if(result <= 0 || emptied) {
//		pthread_cond_signal(&condCleanBuffer);
//	}
	       

	return result;
}

void PFP_KX7::startWorker()
{
	printf("PFP_KX7::startWorker() called...\n");
// 	uint32_t regData = 0x10101010;	
// 	int status = ReadWriteUserRegs(WRITE, 0x002, &regData, 1);
// 	if(status != 0) {
// 		fprintf(stderr, "Error writing to register 0x002\n");
// 	}
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

	pthread_mutex_lock(&lock);
/*	if(currentBuffer != NULL) {
		cleanBufferQueue.push(currentBuffer);
		currentBuffer = NULL;
	}

	while(!dirtyBufferQueue.empty()) {
		Buffer_t *buffer = dirtyBufferQueue.front();
		dirtyBufferQueue.pop();
		cleanBufferQueue.push(buffer);
	}
*/
	pthread_mutex_unlock(&lock);


	printf("PFP_KX7::stopWorker() exiting...\n");
}

bool PFP_KX7::cardOK()
{
	return true;
}


int PFP_KX7::sendCommand(int portID, int slaveID, char *buffer, int bufferSize, int commandLength)
{
// 	printf("8 command to send\n");
// 	for(int i = 0; i < commandLength; i++) {
// 		printf("%02x ", unsigned(buffer[i]) & 0xFF);
// 	}
// 	printf("\n");
	
	int status;

	uint64_t outBuffer[8];
	for(int i = 0; i < 8; i++) outBuffer[i] = 0x0FULL << (4*i);

	uint64_t header = 0;
	header = header + (8ULL << 36);
	header = header + (uint64_t(portID) << 60) + (uint64_t(slaveID) << 54);
	
	outBuffer[0] = header;
	outBuffer[1] = commandLength;
	memcpy((void*)(outBuffer+2), buffer, commandLength);
	
// 	printf("64 command to send: ");
// 	for(int i = 0; i < 8; i++) {
// 		printf("%016llx ", outBuffer[i]);
// 	}
// 	printf("\n");
	
	
	uint32_t txRdPointer;

	boost::posix_time::ptime startTime = boost::posix_time::microsec_clock::local_time();
	do {
		pthread_mutex_lock(&hwLock);
		// Read the RD pointer
//		printf("TX Read WR pointer: status is %2d, result is %08X\n", status, txWrPointer);	
		status = ReadAndCheck(txRdPointerReg * 4 , &txRdPointer, 1);
//		printf("TX Read RD pointer: status is %2d, result is %08X\n", status, txRdPointer);
		pthread_mutex_unlock(&hwLock);

		boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
		if((now - startTime).total_milliseconds() > 10) {
//			printf("REG TX Timeout %04X %04X\n", txWrPointer, txRdPointer);
			return -1;
		}		


		// until there is space to write the command
	} while( ((txWrPointer & 16) != (txRdPointer & 16)) && ((txWrPointer & 15) == (txRdPointer & 15)) );
	
	pthread_mutex_lock(&hwLock);
	uint32_t wrSlot = txWrPointer & 15;
	
//	printf("CMD  TX ");
//	for(int i = 0; i < 8; i++) printf("%016llx ", outBuffer[i]);
//	printf("\n");

	status = WriteAndCheck(wrSlot * 16 * 4 , (uint32_t *)outBuffer, 16);
// 	printf("Wrote data to slot %d (status is %d)\n", wrSlot, status);
		
	txWrPointer += 1;
	txWrPointer = txWrPointer & 31;	// There are only 16 slots, but we use another bit for the empty/full condition
	txWrPointer |= 0xCAFE1500;
	
	status = WriteAndCheck(txWrPointerReg * 4 , &txWrPointer, 1);
// 	printf("Wrote WR pointer: status is %d, result is %d\n", status, txWrPointer);
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
//		printf("RX Read WR pointer: status is %d, result is %08X\n", status, rxWrPointer);	
//		printf("RX Read RD pointer: status is %d, result is %08X\n", status, rxRdPointer);
		pthread_mutex_unlock(&hwLock);

		boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
		if((now - startTime).total_milliseconds() > 10) {
//			printf("REG RX Timeout %04X %04X\n", rxWrPointer, rxRdPointer);
			return -1;
		}		

		// until there a reply to read
	} while((rxWrPointer & 31) == (rxRdPointer & 31));
	pthread_mutex_lock(&hwLock);
	uint32_t rdSlot = rxRdPointer & 15;
	
	status = ReadAndCheck((768 + rdSlot * 16) * 4 , (uint32_t *)outBuffer, 16);
//	printf("REPLY  RX ");
//	for(int i = 0; i < 8; i++) printf("%016llx ", outBuffer[i]);
//	printf("\n");		
	rxRdPointer += 1;
	rxRdPointer = rxRdPointer & 31;	// There are only 16 slots, but we use another bit for the empty/full condition
	rxRdPointer |= 0xFACE9100;
	
	status = WriteAndCheck(rxRdPointerReg * 4 , &rxRdPointer, 1);
// 	printf("Wrote WR pointer: status is %d, result is %d\n", status, rxWrPointer);
	lastCommandTime = boost::posix_time::microsec_clock::local_time();
	pthread_mutex_unlock(&hwLock);
	
	int replyLength = outBuffer[1];
	replyLength = replyLength < bufferSize ? replyLength : bufferSize;
	memcpy(buffer, outBuffer+2, replyLength);
	return replyLength;

}

int PFP_KX7::setAcquistionOnOff(bool enable)
{
	printf("PFP_KX7::setAcquistionOnOff called: %s\n", enable ? "ENABLE" : "DISABLE");
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
	uint32_t *readBackBuffer = new uint32_t[count];
	bool fail = true;
	DWORD Status;
	int iter = 0;
	while(fail) {
		reg = BaseAddrReg + reg;
		Status = PFP_Write(Card, reg, count * 4, data, WDC_MODE_32, WDC_ADDR_RW_DEFAULT);
		for(int i = 0; i < count; i++) readBackBuffer[i] = 0;
		PFP_Read(Card, reg, count * 4, readBackBuffer, WDC_MODE_32, WDC_ADDR_RW_DEFAULT);
		fail = false;
		for(int i = 0; i < count; i++) fail |= (data[i] != readBackBuffer[i]);		
// 		if(fail) 
// 			printf("PFP_KX7::WriteAndCheck(%3d, %8p, %2d) readback failed, retrying (%d)\n", 
// 				reg, data, count,
// 				iter);
		iter++;
	}
	delete [] readBackBuffer;
	return (int) Status;
};

int PFP_KX7::ReadAndCheck(int reg, uint32_t *data, int count) {
	uint32_t *readBackBuffer = new uint32_t[count];
	bool fail = true;
	DWORD Status;
	int iter = 0;
	while(fail) {
		reg = BaseAddrReg + reg;
		Status = PFP_Read(Card, reg, count * 4, data, WDC_MODE_32, WDC_ADDR_RW_DEFAULT);
		for(int i = 0; i < count; i++) readBackBuffer[i] = 0;
		PFP_Read(Card, reg, count * 4, readBackBuffer, WDC_MODE_32, WDC_ADDR_RW_DEFAULT);
		fail = false;
		for(int i = 0; i < count; i++) fail |= (data[i] != readBackBuffer[i]);		
// 		if(fail) 
// 			printf("PFP_KX7::ReadAndCheck(%3d, %8p, %2d) readback failed, retrying (%d)\n", 
// 				reg, data, count,
// 				iter);
		iter++;
	}
	delete [] readBackBuffer;
	return (int) Status;
};

uint64_t PFP_KX7::getPortUp()
{
	uint64_t reply = 0;
	uint32_t channelUp = 0;
	ReadAndCheck(statusReg * 4, &channelUp, 1);
	reply = channelUp;
	return reply;
}

uint64_t PFP_KX7::getPortCounts(int channel, int whichCount)
{
	uint64_t reply;
	ReadAndCheck((statusReg + 1 + 6*channel + 2*whichCount)* 4, (uint32_t *)&reply, 2);
	return reply;
}
