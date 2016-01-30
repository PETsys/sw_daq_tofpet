#include "FrameServer.hpp"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>  
#include <assert.h>
#include <errno.h>
#include "boost/date_time/posix_time/posix_time.hpp"

using namespace DAQd;

static const char *shmObjectPath = "/daqd_shm";

FrameServer::FrameServer(int nFEB, int *feTypeMap, int debugLevel)
	: debugLevel(debugLevel)
{

	
	dataFrameSharedMemory_fd = shm_open(shmObjectPath, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
	if(dataFrameSharedMemory_fd < 0) {
		perror("Error creating shared memory");
		fprintf(stderr, "Check that no other instance is running and rm /dev/shm%s\n", shmObjectPath);
		exit(1);
	}
	
	unsigned long dataFrameSharedMemorySize = MaxDataFrameQueueSize * sizeof(DataFrame);
	ftruncate(dataFrameSharedMemory_fd, dataFrameSharedMemorySize);
	
	dataFrameSharedMemory = (DataFrame *)mmap(NULL, 
						  dataFrameSharedMemorySize, 
						  PROT_READ | PROT_WRITE, 
						  MAP_SHARED, 
						  dataFrameSharedMemory_fd, 
						  0);
	if(dataFrameSharedMemory == NULL) {
		perror("Error mmaping() shared memory");
		exit(1);
	}

	dataFrameWritePointer = 0;
	dataFrameReadPointer = 0;
	
	pthread_mutex_init(&lock, NULL);
	pthread_cond_init(&condCleanDataFrame, NULL);
	pthread_cond_init(&condDirtyDataFrame, NULL);
	pthread_cond_init(&condReplyQueue, NULL);
	die = true;
	acquisitionMode = 0;
	hasWorker = false;
	
	printf("Size of frame is %u\n", sizeof(DataFrame));
	
	
	this->feTypeMap = new int8_t[N_ASIC/16];
	for(int i = 0; i < N_ASIC/16; i++) {
		this->feTypeMap[i] = 0;
	}
	for(int i = 0; i < nFEB; i++) {
		this->feTypeMap[i] = feTypeMap[i];
	}
	tacLastEventTime = new uint64_t[N_ASIC * 64 * 4];
	for(int i = 0; i < N_ASIC * 64 * 4; i++) {
		tacLastEventTime[i] = 0;
	}
	channelLastEventTime = new uint64_t[N_ASIC * 64];
	for(int i = 0; i < N_ASIC * 64; i++) {
		channelLastEventTime[i] = 0;
	}	
	
	m_lut[ 0x7FFF ] = -1; // invalid state
	uint16_t lfsr = 0x0000;
	for ( int16_t n = 0; n < ( 1 << 15 ) - 1; ++n )
	{
		m_lut[ lfsr ] = n;
		const uint8_t bits13_14 = lfsr >> 13;
		uint8_t new_bit;
		switch ( bits13_14 )
		{ // new_bit = !(bit13 ^ bit14)
			case 0x00:
			case 0x03:
				new_bit = 0x01;
				break;
			case 0x01:
			case 0x02:
				new_bit = 0x00;
				break;
		}// switch
		lfsr = ( lfsr << 1 ) | new_bit; // add the new bit to the right
		lfsr &= 0x7FFF; // throw away the 16th bit from the shift
	}// for
	assert ( lfsr == 0x0000 ); // after 2^15-1 steps we're back at 0
// 	assert ( m_lut[ 0x7FFF ] == -1 ); // check that we didn't touch it	
	
}

FrameServer::~FrameServer()
{
	printf("FrameServer::~FrameServer()\n");
	// WARNING: stopWorker() should be called from derived class destructors!
	delete [] channelLastEventTime;
	delete [] tacLastEventTime;

	pthread_cond_destroy(&condReplyQueue);
	pthread_cond_destroy(&condDirtyDataFrame);
	pthread_cond_destroy(&condCleanDataFrame);
	pthread_mutex_destroy(&lock);	
	
	unsigned long dataFrameSharedMemorySize = MaxDataFrameQueueSize * sizeof(DataFrame);
	munmap(dataFrameSharedMemory, dataFrameSharedMemorySize);
	shm_unlink(shmObjectPath);
	
}

void FrameServer::startAcquisition(int mode)
{
	// NOTE: By the time we got here, the DAQ card has synced the system and we should be in the
	// 100 ms sync'ing pause
	
	// Now we just have to wipe the buffers
	// It should be done twice, because the FrameServer worker thread may be waiting for a frame slot
	// and it may fill at least one slot
	
	pthread_mutex_lock(&lock);
	dataFrameWritePointer = 0;
	dataFrameReadPointer = 0;
	acquisitionMode = 0;
	pthread_cond_signal(&condCleanDataFrame);
	pthread_mutex_unlock(&lock);

	usleep(120000);
	
	pthread_mutex_lock(&lock);
	dataFrameWritePointer = 0;
	dataFrameReadPointer = 0;
	acquisitionMode = mode;
	pthread_cond_signal(&condCleanDataFrame);
	pthread_mutex_unlock(&lock);
}

void FrameServer::stopAcquisition()
{
	pthread_mutex_lock(&lock);
	acquisitionMode = 0;
	dataFrameWritePointer = 0;
	dataFrameReadPointer = 0;
	pthread_cond_signal(&condCleanDataFrame);
	pthread_mutex_unlock(&lock);	
}



const char *FrameServer::getDataFrameSharedMemoryName()
{
	return shmObjectPath;
}

unsigned FrameServer::getDataFrameWritePointer()
{
	pthread_mutex_lock(&lock);
	unsigned r = dataFrameWritePointer;
	pthread_mutex_unlock(&lock);
	return r % (2*MaxDataFrameQueueSize);
}

unsigned FrameServer::getDataFrameReadPointer()
{
	pthread_mutex_lock(&lock);
	unsigned r = dataFrameReadPointer;
	pthread_mutex_unlock(&lock);
	return r % (2*MaxDataFrameQueueSize);
}

void FrameServer::setDataFrameReadPointer(unsigned ptr)
{
	pthread_mutex_lock(&lock);
	dataFrameReadPointer = ptr % (2*MaxDataFrameQueueSize);
	pthread_cond_signal(&condCleanDataFrame);
	pthread_mutex_unlock(&lock);
}

void FrameServer::startWorker()
{
	printf("FrameServer::startWorker called...\n");
	if(hasWorker) return;

	die = false;
	hasWorker = true;
	pthread_create(&worker, NULL, runWorker, (void*)this);
	

	printf("FrameServer::startWorker exiting...\n");

}

void FrameServer::stopWorker()
{
	printf("FrameServer::stopWorker called...\n");
	
	die = true;
	
	pthread_mutex_lock(&lock);
	pthread_cond_signal(&condCleanDataFrame);
	pthread_cond_signal(&condDirtyDataFrame);
	pthread_mutex_unlock(&lock);

	if(hasWorker) {
		hasWorker = false;
		pthread_join(worker, NULL);
	}

	printf("FrameServer::stopWorker exiting...\n");
}

void *FrameServer::runWorker(void *arg)
{
	FrameServer *F = (FrameServer *)arg;
        return F->doWork();
}

bool FrameServer::parseDataFrame(DataFrame *dataFrame)
{
	
	unsigned long long frameID = dataFrame->data[0] & 0xFFFFFFFFFULL;
	unsigned long frameSize = (dataFrame->data[0] >> 36) & 0x7FFF;
	unsigned nEvents = dataFrame->data[1] & 0xFFFF;
	bool frameLost = (dataFrame->data[1] & 0x10000) != 0;
	
	if (frameSize != 2 + nEvents) {
		printf("Inconsistent size: got %4d words, expected %4d words(%d events).\n", 
			frameSize, 2 + nEvents, nEvents);
		return false;
	}
	
	for(unsigned n = 2; n < frameSize; n++) {
		uint64_t eventWord = (dataFrame->data[n]);
		
		// TODO: Decoding of events should be unified into a single place (SHM.hpp)
		uint64_t idWord = eventWord >> 48;
		uint64_t asicID = idWord & 0x3F;
		uint64_t slaveID = (idWord >> 6) & 0x1F;
		uint64_t portID = (idWord >> 11) & 0x1F;
		// WARNING:this is to keep backward compatibility with current numbering scheme
		asicID = (slaveID & 0x1) * 16*16 + (portID & 0xF) * 16 + (asicID & 0xF);
#ifdef __ENDOTOFPET__
		int feType = feTypeMap[asicID / 16];
		dataFrame->feType[n] = feType;
#else
		const int feType = 0;
#endif

#ifndef __NO_CHANNEL_IDLE_TIME__
		unsigned tofpet_ChannelID = (eventWord >> 2) & 0x3F;
		unsigned tofpet_TACID = (eventWord >> 0) & 0x3;
		unsigned long long tofpet_EventTime = (1024ULL*frameID) + ((eventWord >> 38) & 0x3FF);
	
		unsigned stic_ChannelID = ((0x0000fc0000000000 & eventWord) >> (32+10));
		unsigned stic_TACID = 0;
		// Calculating a proper TCoarse for STICv3 requires some CPU, thus we just use the Frame ID
		unsigned long long stic_EventTime = (1024ULL*frameID) + 0; 
		
		
		unsigned channelID = (feType == 0) ? tofpet_ChannelID : stic_ChannelID;
		unsigned tacID = (feType == 0) ? tofpet_TACID : stic_TACID;
		unsigned long long eventTime = (feType == 0) ? tofpet_EventTime : stic_EventTime;
		
		int channelIndex =  64 * asicID + channelID;
		int64_t channelIdleTime = eventTime - channelLastEventTime[channelIndex];
		channelLastEventTime[channelIndex] = eventTime;
		
		int tacIndex = tacID + 4 * channelIndex;
		int64_t tacIdleTime = eventTime - tacLastEventTime[tacIndex];
		tacLastEventTime[tacIndex] = eventTime;

		dataFrame->channelIdleTime[n] = channelIdleTime;
		dataFrame->tacIdleTime[n] = tacIdleTime;
#endif
	}
	
	return true;
}

