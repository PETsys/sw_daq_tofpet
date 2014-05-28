#include "UDPFrameServer.hpp"
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

static const char *shmObjectPath = "/daqd_shm";
static const int MaxDataFrameQueueSize = 16*1024;

static const char *feAddr = "192.168.1.25";
static const unsigned short fePort = 2000;

static const int N_ASIC = 4;


UDPFrameServer::UDPFrameServer(int debugLevel)
: debugLevel(debugLevel)
{
	udpSocket = socket(AF_INET,SOCK_DGRAM,0);
	assert(udpSocket != -1);
	
	// "Connect" our socket to the front end service
	struct sockaddr_in localAddress;	
	memset(&localAddress, 0, sizeof(struct sockaddr_in));
	localAddress.sin_family = AF_INET;
	localAddress.sin_addr.s_addr=inet_addr(feAddr);  
	localAddress.sin_port=htons(fePort);
	int r = connect(udpSocket, (struct sockaddr *)&localAddress, sizeof(struct sockaddr_in));
	assert(r == 0);
	
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 100000;
	r = setsockopt(udpSocket, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv));
	assert(r == 0);
	
	char buffer[32];
	memset(buffer, 0xFF, sizeof(buffer));
	send(udpSocket, buffer, sizeof(buffer), 0);
	
	
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
	
	startWorker();
}


UDPFrameServer::~UDPFrameServer()
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
void UDPFrameServer::startAcquisition(int mode)
{
	pthread_mutex_lock(&lock);
	acquisitionMode = mode;
	pthread_mutex_unlock(&lock);
}

void UDPFrameServer::stopAcquisition()
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


void *UDPFrameServer::runWorker(void *arg)
{	
	printf("UDPFrameServer::runWorker starting...\n");
	UDPFrameServer *m = (UDPFrameServer *)arg;
	
	for(int i = 0; i < N_ASIC * 64 * 4; i++) {
		m->tacLastEventTime[i] = 0;
	}
	for(int i = 0; i < N_ASIC * 64; i++) {
		m->channelLastEventTime[i] = 0;
	}
	
	long nFramesFound = 0;
	long nFramesPushed = 0;
	
	
	unsigned char rxBuffer[2048];
	while(!m->die) {
		int r = recv(m->udpSocket, rxBuffer, sizeof(rxBuffer), 0);

		if (r == -1 && (errno = EAGAIN || errno == EWOULDBLOCK)) {
			pthread_cond_signal(&m->condReplyQueue);
			
			pthread_mutex_lock(&m->lock);
			if(m->dirtyDataFrameQueue.empty()) {
				m->dirtyDataFrameQueue.push(NULL);
			}
			pthread_mutex_unlock(&m->lock);
			pthread_cond_signal(&m->condDirtyDataFrame);
			
			continue;
		}
		
		
		assert(r >= 0);
		
		
		if(rxBuffer[0] == 0x5A) {
			if(m->debugLevel > 2) printf("Worker:: Found a reply frame with %d bytes\n", r);
			for(int i = 0; i < r; i++) {
				printf("%02x ", unsigned(rxBuffer[i]) & 0xFF);
			}
			printf("\n");
			reply_t *reply = new reply_t;
			memcpy(reply->buffer, rxBuffer + 1, r - 1);
			reply->size = r - 1;
			pthread_mutex_lock(&m->lock);
			m->replyQueue.push(reply);
			pthread_mutex_unlock(&m->lock);
		}
		else if(rxBuffer[0] == 0xA5) {
			if(m->debugLevel > 2) printf("Worker:: Found a data frame with %d bytes\n", r);
			if (m->acquisitionMode != 0) {			
				unsigned char *tmp = rxBuffer + 1;
				do {
					
					int  dfLength = (tmp[0] << 8) + tmp[1];
					tmp += 2;
					
					if((tmp + dfLength) > (rxBuffer + r))
						break;
					
					bool decodeOK = decodeDataFrame(m, tmp, dfLength);
					if(!decodeOK) break;
					
					tmp += dfLength;
				}while (tmp < rxBuffer + r);
			}
			pthread_mutex_lock(&m->lock);
			if(m->dirtyDataFrameQueue.empty()) {
				m->dirtyDataFrameQueue.push(NULL);
			}
			pthread_mutex_unlock(&m->lock);
			pthread_cond_signal(&m->condDirtyDataFrame);	
		}
		else {
			printf("Worker: found an unknown frame (0x%02X) with %d bytes\n", unsigned(rxBuffer[0]), r);
			
		}
		pthread_cond_signal(&m->condReplyQueue);
			
		
		
	}
	

// 	printf("UDPFrameServer::runWorker stats\n");
// 	printf("\t%10ld frames found\n", nFramesFound);
// 	printf("\t%10ld frames pushed\n", nFramesPushed);
	printf("UDPFrameServer::runWorker exiting...\n");
	return NULL;
}

bool UDPFrameServer::decodeDataFrame(UDPFrameServer *m, unsigned char *buffer, int nBytes)
{
	if(m->debugLevel > 2) {
		for(int i = 0; i < nBytes; i++)
		{
			printf("%02X ", unsigned(buffer[i]));		
		}
		printf("\n");
	}
	
	int nEvents = (buffer[0] << 8) + buffer[1];
	unsigned long frameID = (buffer[2] << 24) + (buffer[3] << 16) + (buffer[4] << 8) + buffer[5];
	
	bool frameLost = false;
	if (nEvents & 0xC000) {
		frameLost = true;
		nEvents = 0;
	}
	
	nEvents /= 2;
	

	if(m->debugLevel > 0 && nBytes != 6 + 16 * nEvents) {
		printf("Inconsistent size: got %4d, expected %4d (%d events).\n", nBytes, 6 + 16 * nEvents, nEvents);
		return false;
	}
	


	
	DataFramePtr *dataFrame = NULL;
	
	if(m->acquisitionMode > 1 || nEvents > 0) {
		pthread_mutex_lock(&m->lock);
		if(!m->cleanDataFrameQueue.empty()) {
			dataFrame = m->cleanDataFrameQueue.front();
			m->cleanDataFrameQueue.pop();
		}
		pthread_mutex_unlock(&m->lock);
	}
	

	
	if(m->debugLevel > 1) {
		printf("Frame %8lu has %4d events\n", frameID, nEvents);
	
	}
	
	int nGoodEvents = 0;
	for(int n = 0; n < nEvents; n++) {
		unsigned char *p1 = buffer + 6 + 16*n;
		unsigned char *p2 = p1 + 3;		
		unsigned asicID = (p1[0] << 8) + p1[1];
		unsigned channelID = p2[4] >> 2;		
		unsigned tacID  = p2[4] & 0x3;
		
		if(asicID >= N_ASIC) continue;
		if(channelID >= 64) continue;
		if(tacID >= 4) continue;

		
		unsigned tCoarse        = ((p2[0] << 2) + (p2[1] >> 6)) & 0x3FF;
		tCoarse = grayToBinary(tCoarse);
		
		// Update last event time
		uint64_t eventTime = 1024ULL*frameID + tCoarse;
		int channelIndex = channelID + 64*asicID;
		int tacIndex = tacID + 4 * channelIndex;
		int64_t tacIdleTime = eventTime - m->tacLastEventTime[tacIndex];
		int64_t channelIdleTime = eventTime - m->channelLastEventTime[channelIndex];
		
		unsigned tSoC   = ((p2[1] << 4) + (p2[2] >> 4)) & 0x3FF;
		unsigned tEoC   = ((p2[2] << 6) + (p2[3] >> 2)) & 0x3FF;
		tSoC = grayToBinary(tSoC);
		tEoC = grayToBinary(tEoC);                
		
		p2 = p1 + 3 + 8;
		unsigned eCoarse        = ((p2[0] << 2) + (p2[1] >> 6)) & 0x3FF;
		unsigned eSoC   = ((p2[1] << 4) + (p2[2] >> 4)) & 0x3FF;
		unsigned eEoC   = ((p2[2] << 6) + (p2[3] >> 2)) & 0x3FF;
		eCoarse = grayToBinary(eCoarse);
		eSoC = grayToBinary(eSoC);
		eEoC = grayToBinary(eEoC);                


		unsigned tFine = (tEoC >= tSoC) ? (tEoC - tSoC) : (1024 + tEoC - tSoC);
		unsigned eFine = (eEoC >= eSoC) ? (eEoC - eSoC) : (1024 + eEoC - eSoC);
		
		if(tSoC != eSoC) {
			if(m->debugLevel > 1) fprintf(stderr, "tSoC != eSoC\n");
			continue;
		}

		if(m->debugLevel > 2) {
			fprintf(stderr, "%u; %u; %u; %u; %u; %u; %u; %u, %lld\n", 
					frameID, asicID, channelID, tacID,
					tCoarse, tFine,
					eCoarse, eFine,
					tacIdleTime
				);
		}
		
		if(tacIdleTime <= 32) {
			if(m->debugLevel > 1)
				fprintf(stderr, "Event with to very small TAC idle time (%lld)\n", tacIdleTime);
		}		
		m->tacLastEventTime[tacIndex] = eventTime;
		m->channelLastEventTime[channelIndex] = eventTime;
		
		if(dataFrame == NULL) continue;				
		Event &event = dataFrame->p->events[nGoodEvents];
		event.type = 0;
		event.d.tofpet.asicID = asicID;
		event.d.tofpet.channelID = channelID;
		event.d.tofpet.tacID = tacID;
		event.d.tofpet.tCoarse = tCoarse;
		event.d.tofpet.eCoarse = eCoarse;
		event.d.tofpet.tFine = tFine;
		event.d.tofpet.eFine = eFine;
		event.d.tofpet.tacIdleTime = tacIdleTime;
		event.d.tofpet.channelIdleTime = channelIdleTime;
		nGoodEvents += 1;
	}
	
	if(dataFrame == NULL) {
		if(m->debugLevel > 2) {
			printf("no space in buffer to store events...\n");
		}
		return true;
	}
	
	dataFrame->p->frameID = frameID;
	dataFrame->p->frameLost = frameLost;
	dataFrame->p->nEvents = nGoodEvents;
	
	pthread_mutex_lock(&m->lock);
	m->dirtyDataFrameQueue.push(dataFrame);
	pthread_mutex_unlock(&m->lock);
	
	return true;

}

const char *UDPFrameServer::getDataFrameSharedMemoryName()
{
	return shmObjectPath;
}

UDPFrameServer::DataFramePtr * UDPFrameServer::getDataFrameByPtr()
{
	int index = -1;
	
	DataFramePtr * dataFramePtr = NULL;
	pthread_mutex_lock(&lock);
	while(!die && acquisitionMode != 0 && dirtyDataFrameQueue.empty()) {
		pthread_cond_wait(&condDirtyDataFrame, &lock);
	}
	
	while (dataFramePtr == NULL && !dirtyDataFrameQueue.empty())
	{
		dataFramePtr = dirtyDataFrameQueue.front();
		dirtyDataFrameQueue.pop();	
	}
	pthread_mutex_unlock(&lock);	
	
	return dataFramePtr;
}

void UDPFrameServer::returnDataFramePtr(DataFramePtr *dataFramePtr)
{
	if(dataFramePtr == NULL) return;
	
	pthread_mutex_lock(&lock);
	cleanDataFrameQueue.push(dataFramePtr);
	pthread_mutex_unlock(&lock);	
	pthread_cond_signal(&condCleanDataFrame);
}

void UDPFrameServer::startWorker()
{
	printf("UDPFrameServer::startWorker called...\n");
	if(hasWorker) return;

	die = false;
	hasWorker = true;
	pthread_create(&worker, NULL, runWorker, (void*)this);
	

	printf("UDPFrameServer::startWorker exiting...\n");

}

void UDPFrameServer::stopWorker()
{
	printf("UDPFrameServer::stopWorker called...\n");
	
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


	printf("UDPFrameServer::stopWorker exiting...\n");
}




int UDPFrameServer::sendCommand(char *buffer, int bufferSize, int commandLength)
{
	send(udpSocket, buffer, commandLength, 0);
	
	
	
	boost::posix_time::ptime start = boost::posix_time::microsec_clock::local_time();
	
	int replyLength = 0;
	do {
		pthread_mutex_lock(&lock);
		reply_t *reply = NULL;
		
		if(replyQueue.empty()) {
			pthread_cond_wait(&condReplyQueue, &lock);
		}
		
		if (!replyQueue.empty()) {
			reply = replyQueue.front();
			replyQueue.pop();
		}
		pthread_mutex_unlock(&lock);
		
		if(reply == NULL) {
			boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
			if ((now - start).total_milliseconds() > 200) 
				break;
			else
				continue;
			
		}
		
		// Check that command and this reply have the same SN
		if(reply->size >= 2 && reply->buffer[0] == buffer[0] && reply->buffer[1] == buffer[1]) {
			memcpy(buffer, reply->buffer, reply->size);
			replyLength = reply->size;
		}
		delete reply;
	} while(replyLength == 0);
		
	return replyLength;
}
