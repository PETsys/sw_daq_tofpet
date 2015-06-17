#include "FrameServer.hpp"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <boost/crc.hpp>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>  
#include <assert.h>
#include <errno.h>
#include "boost/date_time/posix_time/posix_time.hpp"

//static const char *shmObjectPath = "/daqd_shm";
//static const int MaxDataFrameQueueSize = 16*1024;
//static const int N_ASIC=4;


FrameServer::FrameServer(int nFEB, int *feTypeMap, int debugLevel)
	: debugLevel(debugLevel), nFEB(nFEB), feTypeMap(feTypeMap)
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

	for(int i = 0; i < MaxDataFrameQueueSize; i++)
	{
		cleanDataFrameQueue.push(new DataFramePtr(i, false, dataFrameSharedMemory+i));
	}
	
	pthread_mutex_init(&lock, NULL);
	pthread_cond_init(&condCleanDataFrame, NULL);
	pthread_cond_init(&condDirtyDataFrame, NULL);
	pthread_cond_init(&condReplyQueue, NULL);
	die = true;
	acquisitionMode = 0;
	hasWorker = false;
	
	printf("Size of event is %u\n", sizeof(Event));
	printf("Size of frame is %u\n", sizeof(DataFrame));
	
	
	tacLastEventTime = new uint64_t[N_ASIC * 64 * 4];
	channelLastEventTime = new uint64_t[N_ASIC * 64];
	
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
	stopWorker();
	delete [] channelLastEventTime;
	delete [] tacLastEventTime;

	while(!cleanDataFrameQueue.empty()) {
		DataFramePtr *dataFrame = cleanDataFrameQueue.front();
		cleanDataFrameQueue.pop();		
		delete dataFrame;
	}

	while(!dirtyDataFrameQueue.empty()) {
		DataFramePtr *dataFrame = dirtyDataFrameQueue.front();
		dirtyDataFrameQueue.pop();		
		delete dataFrame;
	}

	pthread_cond_destroy(&condReplyQueue);
	pthread_cond_destroy(&condDirtyDataFrame);
	pthread_cond_destroy(&condCleanDataFrame);
	pthread_mutex_destroy(&lock);	
	
	unsigned long dataFrameSharedMemorySize = MaxDataFrameQueueSize * sizeof(DataFrame);
	munmap(dataFrameSharedMemory, dataFrameSharedMemorySize);
	shm_unlink(shmObjectPath);
	
}

static unsigned int grayToBinary(unsigned int num)
{
    unsigned int mask;
    for (mask = num >> 1; mask != 0; mask = mask >> 1)
    {
        num = num ^ mask;
    }
    return num;
}
void FrameServer::startAcquisition(int mode)
{
	pthread_mutex_lock(&lock);
	acquisitionMode = mode;
	pthread_mutex_unlock(&lock);
}

void FrameServer::stopAcquisition()
{
	pthread_mutex_lock(&lock);
	acquisitionMode = 0;
	pthread_mutex_unlock(&lock);
	
	pthread_mutex_lock(&lock);
	while(!dirtyDataFrameQueue.empty()) {
		DataFramePtr *dataFramePtr = dirtyDataFrameQueue.front();
		dirtyDataFrameQueue.pop();	
		if(dataFramePtr != NULL)
			cleanDataFrameQueue.push(dataFramePtr);			
		
		pthread_mutex_unlock(&lock);
		pthread_mutex_lock(&lock);
	}
	pthread_mutex_unlock(&lock);	
}

bool FrameServer::decodeSTiCv3Event(uint64_t *data, Event &event)
{


	uint64_t buf_ptr=*data;
	
	event.type = 1;
	
	event.d.sticv3.eFine =  (unsigned short) 	(( 0x0000001f & buf_ptr));
	event.d.sticv3.eCoarseL = m_lut[ (unsigned int)	(( 0x000fffe0 & buf_ptr) >> 5)	];
	event.d.sticv3.eBadHit =  			(( 0x00100000 & buf_ptr) != 0 )? true:false;

	event.d.sticv3.tFine =		(unsigned short)(( 0x03e00000 & buf_ptr) >> 21);

	event.d.sticv3.tCoarseL = m_lut[	(unsigned int) ( 0x7FFF & (buf_ptr  >> 26) ) ];

	event.d.sticv3.tBadHit =  			(( 0x0000020000000000 & buf_ptr) != 0 )? true:false;
	event.channelID = 	(unsigned short) ((0x0000fc0000000000 & buf_ptr) >> (32+10));
	event.asicID =	(unsigned short) ((0x000f000000000000 & buf_ptr) >> (32+16));
	
	
	event.tCoarse = (event.d.sticv3.tCoarseL  >> 2) & 0x3FF;

	if(debugLevel > 1) { 
		printf("ASIC ID: %u\n",event.asicID);
		printf("Channel ID: %u\n",event.channelID);
		printf("eFine: %u\n",event.d.sticv3.eFine);
		printf("eCoarse: %u\n",event.d.sticv3.eCoarseL);
		printf("tFine: %u\n",event.d.sticv3.tFine);
		printf("tCoarse: %u\n",event.d.sticv3.tCoarseL );
		printf("tBadHit: %u\n",event.d.sticv3.tBadHit);
	}
	return true;
  
}


bool FrameServer::decodeDataFrame(FrameServer *m, unsigned char *buffer, int nBytes, bool useIdleTime)
{
	uint64_t * buffer64 = (uint64_t *)buffer;
	int nWords = nBytes / sizeof(uint64_t);

	unsigned long frameID = buffer64[0] & 0xFFFFFFFFFULL;
	int nEvents = buffer64[1] & 0xFFFF;
//	bool frameLost = (buffer64[1] & 0x10000) != 0;
 	bool frameLost = false;
// 	if (frameLost) nEvents = 0;

	if(nWords != 2 + nEvents) {		
		printf("Inconsistent size: got %4d words, expected %4d words(%d events).\n", 
			nWords, 2 + nEvents, nEvents);
		return false;
	}

	// Print frame ID on occasion or when debugLevel > 1
	if( (m->debugLevel > 1)) {
		// WARNING For some reason, this seems to always print 0 events..
		printf("Frame %12lu has %4d events\n", frameID, nEvents);
	
	}

	if(nEvents > MaxEventsPerFrame) nEvents = MaxEventsPerFrame;
	
	DataFramePtr *dataFrame = NULL;	
	if(m->acquisitionMode > 1 || nEvents > 0) {
		pthread_mutex_lock(&m->lock);
		if(!m->cleanDataFrameQueue.empty()) {
			dataFrame = m->cleanDataFrameQueue.front();
			m->cleanDataFrameQueue.pop();
		}
		pthread_mutex_unlock(&m->lock);
	}
	
	
	
	int nGoodEvents = 0;
	bool hasTOFPET = false;
	bool hasSTiCv3 = false;
	for(int n = 0; n < nEvents; n++) {
		uint64_t eventWord = (buffer64[2 + n]);
		unsigned asicID = eventWord >> 48;
	
		if(asicID >= N_ASIC) {
			fprintf(stderr, "Data from non-existing ASIC: %u\n", asicID);
			continue;
		}
			
		int feType = feTypeMap[asicID / 16];
		//printf("n = %d word = %016llx\n", n, eventWord);
		//printf("ASIC %d type %d\n", asicID, feType);
		
		unsigned short channelID ;
		unsigned short tCoarse_TOFPET;
		unsigned short tCoarse_STiC;
		
		// Decode basic common information: channel ID, tCoarse and absolute coarse event time
		uint64_t eventTime;
		if (feType == 0) {
			channelID = (eventWord >> 2) & 0x3F;
			tCoarse_TOFPET = (eventWord >> 38) & 0x3FF;
			eventTime = 1024ULL*frameID + tCoarse_TOFPET;
			hasTOFPET = true;

			// Adjust channel idle time for the channel
			int channelIndex =  64 * asicID + channelID;
			int64_t channelIdleTime = eventTime - m->channelLastEventTime[channelIndex];
			m->channelLastEventTime[channelIndex] = eventTime;

			// If TOFPET, adjust TAC idle time
			unsigned short tacID_TOFPET = 0;
			int64_t tacIdleTime = 0;
			tacID_TOFPET = (eventWord >> 0) & 0x3;
			//printf("tacID = %d\n", tacID_TOFPET);
			int tacIndex = tacID_TOFPET + 4 * channelIndex;
			tacIdleTime = eventTime - m->tacLastEventTime[tacIndex];
			if(tacIdleTime <= 32) {
				if(m->debugLevel > 1)
					fprintf(stderr, "Event with to very small TAC idle time (%lld)\n", tacIdleTime);
			}
			m->tacLastEventTime[tacIndex] = eventTime;

			if(dataFrame == NULL) continue;	
			Event &event = dataFrame->p->events[nGoodEvents];

			event.type = 0;
			event.asicID = asicID;
			event.channelID = channelID;
			event.d.tofpet.tacID = tacID_TOFPET;
			event.tCoarse = tCoarse_TOFPET;
			event.d.tofpet.eCoarse = (eventWord >> 18) & 0x3FF;
			event.d.tofpet.tFine = (eventWord >> 28) & 0x3FF;
			event.d.tofpet.eFine = (eventWord >> 8) & 0x3FF;
			if(useIdleTime)event.d.tofpet.tacIdleTime = tacIdleTime;
			else event.d.tofpet.tacIdleTime = 0; 
			event.d.tofpet.channelIdleTime = channelIdleTime;	
			
			nGoodEvents += 1;
		}			
		else if (feType == 1) {
			channelID = (unsigned short) ((0x0000fc0000000000 & eventWord) >> (32+10));
			tCoarse_STiC = m_lut[	(unsigned int) ( 0x7FFF & (eventWord  >> 26) ) ];
			eventTime = 1024ULL*frameID + ((tCoarse_STiC >> 2) & 0x3FF);
			hasSTiCv3 = true;
			
			// Adjust channel idle time for the channel
			int channelIndex =  64 * asicID + channelID;
                        int64_t channelIdleTime = eventTime - m->channelLastEventTime[channelIndex];
                        m->channelLastEventTime[channelIndex] = eventTime;

			if(dataFrame == NULL) continue;	
			Event &event = dataFrame->p->events[nGoodEvents];

			event.type = 1;
			event.asicID = asicID;
			event.channelID = channelID;
			event.d.sticv3.eFine =  (unsigned short) 	(( 0x0000001f & eventWord));
			event.d.sticv3.eCoarseL = m_lut[ (unsigned int) (( 0x000fffe0 & eventWord) >> 5)  ];
			event.d.sticv3.eBadHit =  			(( 0x00100000 & eventWord) != 0 ) ? true:false;
			event.d.sticv3.tFine =		(unsigned short)(( 0x03e00000 & eventWord) >> 21);
			event.d.sticv3.tCoarseL = tCoarse_STiC;
			event.d.sticv3.tBadHit =  			(( 0x0000020000000000 & eventWord) != 0 ) ? true:false;
			event.tCoarse = (event.d.sticv3.tCoarseL  >> 2) & 0x3FF;			
			event.d.sticv3.channelIdleTime = channelIdleTime;
			
			nGoodEvents += 1;
		}
		
	}
	
	if(dataFrame == NULL) {
		if(m->debugLevel > 2) {
			printf("no space in buffer to store events...\n");
		}
		return true;
	}


	if(false && hasTOFPET && hasSTiCv3) {
		printf("Frame %lu has %d good events\n", frameID, nGoodEvents);
		for (int n = 0; n < nGoodEvents; n++) { 
			Event &event = dataFrame->p->events[n];
			if(event.type == 0) printf("%4d %2d %4u\n", event.asicID, event.channelID, event.tCoarse);
			if(event.type == 1) printf("%4d %2d %4u %6u %4u\n", event.asicID, event.channelID, event.tCoarse, event.d.sticv3.tCoarseL, (event.d.sticv3.tCoarseL/4) & 0x3FF);
				
		}
	}

	
	dataFrame->p->frameID = frameID;
	dataFrame->p->frameLost = frameLost;
	dataFrame->p->nEvents = nGoodEvents;
	
	pthread_mutex_lock(&m->lock);
	m->dirtyDataFrameQueue.push(dataFrame);
	pthread_mutex_unlock(&m->lock);
	
	return true;

}

const char *FrameServer::getDataFrameSharedMemoryName()
{
	return shmObjectPath;
}

FrameServer::DataFramePtr * FrameServer::getDataFrameByPtr(bool nonEmpty)
{
	int index = -1;
	
	DataFramePtr * dataFramePtr = NULL;
	pthread_mutex_lock(&lock);
	
	boost::posix_time::ptime t0 = boost::posix_time::microsec_clock::local_time();
	while(	!die && (acquisitionMode != 0)	&& (dataFramePtr == NULL) ) {
		
		boost::posix_time::ptime t1 = boost::posix_time::microsec_clock::local_time();
		if ((t1 - t0).total_milliseconds() > 1000) break;
	
		
		if(dirtyDataFrameQueue.empty()) {
			struct timespec ts;
			clock_gettime(CLOCK_REALTIME, &ts);
			ts.tv_nsec += 100000000L; // 100 ms
			ts.tv_sec += (ts.tv_nsec / 1000000000L);
			ts.tv_nsec = (ts.tv_nsec % 1000000000L);	
			pthread_cond_timedwait(&condDirtyDataFrame, &lock, &ts);
		}
		
		if(!dirtyDataFrameQueue.empty()) {
			dataFramePtr = dirtyDataFrameQueue.front();
			dirtyDataFrameQueue.pop();	
		}
		
		if(dataFramePtr != NULL) {
			if(nonEmpty && (dataFramePtr->p->nEvents == 0)) {
				pthread_mutex_unlock(&lock);
				returnDataFramePtr(dataFramePtr);
				dataFramePtr = NULL;
				pthread_mutex_lock(&lock);
			}
		}
	}
	
/*	boost::posix_time::ptime t2 = boost::posix_time::microsec_clock::local_time();		
	printf("time: %4ld %8p\n", (t2 - t0).total_milliseconds(), dataFramePtr);*/
	pthread_mutex_unlock(&lock);	
	
	return dataFramePtr;
}


void FrameServer::returnDataFramePtr(DataFramePtr *dataFramePtr)
{
	if(dataFramePtr == NULL) return;
	
	pthread_mutex_lock(&lock);
	cleanDataFrameQueue.push(dataFramePtr);
	pthread_mutex_unlock(&lock);	
	pthread_cond_signal(&condCleanDataFrame);
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
	
	pthread_cond_signal(&condCleanDataFrame);
	pthread_cond_signal(&condDirtyDataFrame);

	if(hasWorker) {
		hasWorker = false;
		pthread_join(worker, NULL);
	}

	pthread_mutex_lock(&lock);
	while(!dirtyDataFrameQueue.empty()) {		
		DataFramePtr *dataFrame = dirtyDataFrameQueue.front();
		dirtyDataFrameQueue.pop();	
		if(dataFrame != NULL)
			cleanDataFrameQueue.push(dataFrame);
	}
	printf("Clean buffer queue has %u entries\n", cleanDataFrameQueue.size());
	pthread_mutex_unlock(&lock);


	printf("FrameServer::stopWorker exiting...\n");
}

void *FrameServer::runWorker(void *arg)
{
	FrameServer *F = (FrameServer *)arg;
        return F->doWork();
}



