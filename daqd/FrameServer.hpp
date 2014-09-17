#ifndef __FRAMESERVER_HPP__DEFINED__
#define __FRAMESERVER_HPP__DEFINED__

#include <pthread.h>
#include <queue>

#include "Protocol.hpp"

static const char *shmObjectPath = "/daqd_shm";
static const int MaxDataFrameQueueSize = 16*1024;
static const int N_ASIC=4;

class FrameServer {
public:
	FrameServer(int debugLevel);
        ~FrameServer();	
	struct DataFramePtr {
	public:
		unsigned index;
		bool fake;
		DataFrame *p;
		
		DataFramePtr (unsigned index, bool fake, DataFrame *p) : index(index), fake(fake), p(p) {};
	};
	
	// Sends a command and gets the reply
	// buffer is used to carry the command and the reply
	// bufferSize is the max capacity of the size
	// commandLength is the length of the command in
	// return the reply length or -1 if error
	virtual int sendCommand(char *buffer, int bufferSize, int commandLength) = 0;
	
	virtual const char *getDataFrameSharedMemoryName();
	virtual DataFramePtr *getDataFrameByPtr();
	virtual void returnDataFramePtr(DataFramePtr *ptr);
	
	virtual void startAcquisition(int mode);
	virtual void stopAcquisition();
	
	
protected:
	
	int debugLevel;
	int dataFrameSharedMemory_fd;
	DataFrame *dataFrameSharedMemory;
	
	static void *runWorker(void *);
	virtual void * doWork(void *) = 0;
	static bool decodeDataFrame(FrameServer *m, unsigned char *buffer, int nBytes);
	void startWorker();
	void stopWorker();
	
	volatile bool die;
	pthread_t worker;
	volatile bool hasWorker;
	volatile int acquisitionMode;
	pthread_mutex_t lock;
	pthread_cond_t condCleanDataFrame;
	pthread_cond_t condDirtyDataFrame;
	std::queue<DataFramePtr *> cleanDataFrameQueue;
	std::queue<DataFramePtr *> dirtyDataFrameQueue;
	
	

	struct reply_t {
		int size;
		char buffer[128];
	};
	std::queue<reply_t *> replyQueue;
	pthread_cond_t condReplyQueue;
	
	
	uint64_t *tacLastEventTime;
	uint64_t *channelLastEventTime;
	



};

#endif
