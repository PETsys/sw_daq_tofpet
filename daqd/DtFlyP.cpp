#include "DtFlyP.hpp"
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

// typedef unsigned long DWORD;
// typedef unsigned long long UINT64;
// 
// extern "C" {
// #include "dtfly_p_etpu_defs.h"
// #include "dtfly_p_etpu_api.h"
// }

using namespace DTFLY;

extern "C" {
#include "dtfly_p_etpu_api.h"
#include "dtfly_p_etpu_defs.h"
}

static const unsigned DMA_TRANS_BYTE_SIZE = 262144;
static const unsigned wordBufferSize = DMA_TRANS_BYTE_SIZE / sizeof(uint64_t);

static const int N_BUFFERS = 1;

static void userIntHandler(SIntInfoData * IntInfoData)
{
}

DtFlyP::DtFlyP()
{
	int dtFlyPinitStatus = InitDevice(userIntHandler);
	assert(dtFlyPinitStatus == 0);
	
	//	logFile=fopen("./logfile.txt", "w");
	pthread_mutex_init(&lock, NULL);
	pthread_cond_init(&condCleanBuffer, NULL);
	pthread_cond_init(&condDirtyBuffer, NULL);

	pthread_mutex_init(&hwLock, NULL);
	

	// for(int i = 0; i < N_BUFFERS; i++)
	// {
	// 	Buffer_t * buffer = new Buffer_t();
	// 	buffer->bufferIndex = i;
	// 	buffer->buffer.ByteCount = DMA_TRANS_BYTE_SIZE;
	// 	AllocateBuffer(i, &buffer->buffer);
	// 	cleanBufferQueue.push(buffer);
	// }

	// buffers = new Buffer_t[N_BUFFERS];
	// for(int i = 0; i < N_BUFFERS; i++) {
	// 	buffers[i].bufferIndex = i;
	// 	buffers[i].buffer.ByteCount = DMA_TRANS_BYTE_SIZE;
	// 	AllocateBuffer(i, &(buffers[i].buffer));
	// }
	// tail = 0;
	// head = 0; 

	wordBuffer = new uint64_t[wordBufferSize];
	wordBufferUsed = 0;
	wordBufferStatus = 0;
	
	die = true;
	hasWorker = false;

	setAcquistionOnOff(false);
	startWorker();
}

DtFlyP::~DtFlyP()
{
	stopWorker();

	delete [] wordBuffer;

	pthread_mutex_destroy(&hwLock);
	pthread_cond_destroy(&condDirtyBuffer);
	pthread_cond_destroy(&condCleanBuffer);
	pthread_mutex_destroy(&lock);	

	ReleaseDevice();
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

void *DtFlyP::runWorker(void *arg)
{
	const uint64_t idleWord = 0xFFFFFFFFFFFFFFFFULL;

	printf("DtFlyP::runWorker launching...\n");
	DtFlyP *p = (DtFlyP *)arg;

	p->dmaBuffer.ByteCount = DMA_TRANS_BYTE_SIZE;
	int r1 = AllocateBuffer(0, &p->dmaBuffer);
	assert(r1 == 0);

	//uint32_t	regData[7];
	//int		regAddress;
	//regAddress = 8;
	//regData[0] =  0x00000000;//0x00000000
	//int status = ReadWriteUserRegs(WRITE, 8, regData, 1);
	
	while(!p->die) {
		
//  		boost::posix_time::ptime startTime = boost::posix_time::microsec_clock::local_time();
		//pthread_mutex_lock(&p->hwLock);
		int status = ReceiveDMAbyBufIndex(DMA1, 0, 2);
		//pthread_mutex_unlock(&p->hwLock);
//  		boost::posix_time::ptime stopTime = boost::posix_time::microsec_clock::local_time();	
//  		printf("receive DMA status is %d, %d\n", status, (stopTime - startTime).total_milliseconds());

		pthread_mutex_lock(&p->lock);	
		while(!p->die && (p->wordBufferUsed < p->wordBufferStatus)) {
// 			pthread_mutex_unlock(&p->lock);	
// 			pthread_cond_signal(&p->condDirtyBuffer);			
// 			pthread_mutex_lock(&p->lock);	
			pthread_cond_wait(&p->condCleanBuffer, &p->lock);
		}
	       
		if(p->die) {
			status = ENOWORDS;
		}

		if (status > 0) {
			memcpy(p->wordBuffer, (void *)p->dmaBuffer.UserAddr, status);		
			p->wordBufferUsed = 0;	
			p->wordBufferStatus = status / sizeof(uint64_t);
		}
		else if(status == 0) {
		  	p->wordBufferUsed = ENOWORDS - 1; // Set wordBufferUsed to some number smaller than status
			p->wordBufferStatus = ENOWORDS;
			printf("DtFlyP::worker wordBufferStatus set to %d\n", p->wordBufferStatus);
		}
		else {
			// Error
			p->wordBufferUsed = status - 1; // Set wordBufferUsed to some number smaller than status
			p->wordBufferStatus = status;
			printf("DtFlyP::worker wordBufferStatus set to %d\n", p->wordBufferStatus);
		}
		pthread_mutex_unlock(&p->lock);
	       	pthread_cond_signal(&p->condDirtyBuffer);
	}
	
	ReleaseBuffer(0);
	
	
	printf("DtFlyP::runWorker exiting...\n");
	return NULL;
}

int DtFlyP::getWords(uint64_t *buffer, int count)
{
//	printf("DtFlyP::getWords(%p, %d)\n", buffer, count);
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

int DtFlyP::getWords_(uint64_t *buffer, int count)
{
	//printf("DtFlyP::getWords_(%p, %d) ", buffer, count);
	int result = -1;
	
/*	printf("status = %d, used = %d\n", wordBufferStatus, wordBufferUsed);*/
	
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

	// if(result <= 0 || emptied) {
	// 	pthread_cond_signal(&condCleanBuffer);
	// }
	       

	return result;
}

void DtFlyP::startWorker()
{
	printf("DtFlyP::startWorker() called...\n");
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

	printf("DtFlyP::startWorker() exiting...\n");	
}
void DtFlyP::stopWorker()
{
	printf("DtFlyP::stopWorker() called...\n");

	die = true;
	pthread_cond_signal(&condCleanBuffer);
	pthread_cond_signal(&condDirtyBuffer);
	if(hasWorker) {
		hasWorker = false;

		pthread_join(worker, NULL);
	}

	pthread_mutex_lock(&lock);
	// if(currentBuffer != NULL) {
	// 	cleanBufferQueue.push(currentBuffer);
	// 	currentBuffer = NULL;
	// }
	
	// while(!dirtyBufferQueue.empty()) {
	// 	Buffer_t *buffer = dirtyBufferQueue.front();
	// 	dirtyBufferQueue.pop();
	// 	cleanBufferQueue.push(buffer);
	// }

	pthread_mutex_unlock(&lock);


	printf("DtFlyP::stopWorker() exiting...\n");
}

bool DtFlyP::cardOK()
{
	return true;
}


int DtFlyP::sendCommand(int febID, char *buffer, int bufferSize, int commandLength)
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
	header = header + (uint64_t(febID) << 61);
	
	outBuffer[0] = header;
	outBuffer[1] = commandLength;
	memcpy((void*)(outBuffer+2), buffer, commandLength);
	
// 	printf("64 command to send: ");
// 	for(int i = 0; i < 8; i++) {
// 		printf("%016llx ", outBuffer[i]);
// 	}
// 	printf("\n");
	
	const int wrPointerReg = 768;
	const int rdPointerReg = 640;
	
	
	uint32_t wrPointer;
	uint32_t rdPointer;

	boost::posix_time::ptime startTime = boost::posix_time::microsec_clock::local_time();
	do {
		pthread_mutex_lock(&hwLock);
		// Read the WR and RD pointer
		status = ReadWriteUserRegs(READ, wrPointerReg * 4 , &wrPointer, 1);
		//printf("Read WR pointer: status is %d, result is %d\n", status, wrPointer);	
		status = ReadWriteUserRegs(READ, rdPointerReg * 4 , &rdPointer, 1);
		//printf("Read RD pointer: status is %d, result is %d\n", status, rdPointer);
		pthread_mutex_unlock(&hwLock);

		boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
		if((now - startTime).total_milliseconds() > 200) {
			printf("Timeout %04X %04X\n", wrPointer, rdPointer);
			return -1;
		}		

		// until there is space to write the command
	} while( ((wrPointer & 32) != (rdPointer & 32)) && ((wrPointer & 31) == (rdPointer & 31)) );
	
	pthread_mutex_lock(&hwLock);
	uint32_t wrSlot = wrPointer & 31;
	
	status = WriteAndCheck(wrSlot * 16 * 4 , (uint32_t *)outBuffer, 16);
// 	printf("Wrote data to slot %d (status is %d)\n", wrSlot, status);
		
	wrPointer += 1;
	wrPointer = wrPointer & 63;	// There are only 32 slots, but we use another bit for the empty/full condition
	
	status = WriteAndCheck(wrPointerReg * 4 , &wrPointer, 1);
// 	printf("Wrote WR pointer: status is %d, result is %d\n", status, wrPointer);
	pthread_mutex_unlock(&hwLock);
	return 0;

}

int DtFlyP::setAcquistionOnOff(bool enable)
{
	const int acqStatusPointerReg = 896;
	uint32_t data[1];
	int status;
	
	data[0] = enable ? 0x000FFFFU : 0xFFFF0000U;
	pthread_mutex_lock(&hwLock);
	status = WriteAndCheck(acqStatusPointerReg * 4, data, 1);	
	pthread_mutex_unlock(&hwLock);
	return status;	
}

int DtFlyP::WriteAndCheck(int reg, uint32_t *data, int count) {
	uint32_t *readBackBuffer = new uint32_t[count];
	bool fail = true;
	int status = -1;
	int iter = 0;
	while(fail) {
		status = ReadWriteUserRegs(WRITE, reg, data, count);
		for(int i = 0; i < count; i++) readBackBuffer[i] = 0;
		ReadWriteUserRegs(READ, reg, readBackBuffer, count);
		fail = false;
		for(int i = 0; i < count; i++) fail |= (data[i] != readBackBuffer[i]);		
		if(fail) 
			printf("DtFlyP::WriteAndCheck(%3d, %8p, %2d) readback failed, retrying (%d)\n", 
				reg, data, count,
				iter);
		iter++;
	}
	delete [] readBackBuffer;
	return status;
};
