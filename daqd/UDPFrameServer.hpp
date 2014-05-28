#ifndef __UDPFRAMESERVER_HPP__DEFINED__
#define __UDPFRAMESERVER_HPP__DEFINED__

#include "FrameServer.hpp"

#include "boost/tuple/tuple.hpp"
#include <boost/unordered_map.hpp> 
#include "boost/date_time/posix_time/posix_time.hpp"


class UDPFrameServer : public FrameServer
{
public:
	UDPFrameServer(int debugLevel);
	~UDPFrameServer();	

	const char *getDataFrameSharedMemoryName();
	DataFramePtr *getDataFrameByPtr();
	void returnDataFramePtr(DataFramePtr *ptr);

	int sendCommand(char *buffer, int bufferSize, int commandLength);
	

	
	void startAcquisition(int mode);
	void stopAcquisition();

private:
	int debugLevel;
	
	int udpSocket;
	
	
	
	
	int dataFrameSharedMemory_fd;
	DataFrame *dataFrameSharedMemory;
	
	static void *runWorker(void *);
	static bool decodeDataFrame(UDPFrameServer *m, unsigned char *buffer, int nBytes);
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
