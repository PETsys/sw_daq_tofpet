#ifndef __FRAMESERVER_HPP__DEFINED__
#define __FRAMESERVER_HPP__DEFINED__

#include <pthread.h>
#include <queue>

#include "Protocol.hpp"

namespace DAQd {

static const char *shmObjectPath = "/daqd_shm";
static const int N_ASIC=16*1024;

class FrameServer {
public:
	FrameServer(int nFEB, int *feTypeMap, int debugLevel);
        virtual ~FrameServer();	

	// Sends a command and gets the reply
	// buffer is used to carry the command and the reply
	// bufferSize is the max capacity of the size
	// commandLength is the length of the command in
	// return the reply length or -1 if error
	virtual int sendCommand(int portID, int slaveID, char *buffer, int bufferSize, int commandLength) = 0;
	
	virtual const char *getDataFrameSharedMemoryName();
	virtual unsigned getDataFrameWritePointer();
	virtual unsigned getDataFrameReadPointer();
	virtual void setDataFrameReadPointer(unsigned ptr);
	
	virtual void startAcquisition(int mode);
	virtual void stopAcquisition();
	
	virtual uint64_t getPortUp() = 0;
	virtual uint64_t getPortCounts(int port, int whichCount) = 0;
	
	
protected:	
	bool parseDataFrame(DataFrame *dataFrame);
	
	int debugLevel;
	
	int8_t *feTypeMap;
	
	int dataFrameSharedMemory_fd;
	DataFrame *dataFrameSharedMemory;
	
	static void *runWorker(void *);
	virtual void * doWork() = 0;
	void startWorker();
	void stopWorker();
	
	volatile bool die;
	pthread_t worker;
	volatile bool hasWorker;
	volatile int acquisitionMode;
	pthread_mutex_t lock;
	pthread_cond_t condCleanDataFrame;
	pthread_cond_t condDirtyDataFrame;
	unsigned dataFrameWritePointer;
	unsigned dataFrameReadPointer;
	
	

	struct reply_t {
		int size;
		char buffer[128];
	};
	std::queue<reply_t *> replyQueue;
	pthread_cond_t condReplyQueue;
	
	
	uint64_t *tacLastEventTime;
	uint64_t *channelLastEventTime;
	
	int16_t m_lut[ 1 << 15 ];
	

};

}
#endif
