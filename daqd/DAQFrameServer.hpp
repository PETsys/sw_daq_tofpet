#ifndef __DAQFRAMESERVER_HPP__DEFINED__
#define __DAQFRAMESERVER_HPP__DEFINED__

#include "FrameServer.hpp"
#include "DtFlyP.hpp"

#include "boost/tuple/tuple.hpp"
#include <boost/unordered_map.hpp> 
#include "boost/date_time/posix_time/posix_time.hpp"


class DAQFrameServer : public FrameServer
{
public:
	DAQFrameServer(int nFEB, int *feTypeMap, int debugLevel);
	virtual ~DAQFrameServer();	

	//virtual const char *getDataFrameSharedMemoryName()=0;
	//virtual DataFramePtr *getDataFrameByPtr()=0;
	//virtual void returnDataFramePtr(DataFramePtr *ptr)=0;

	int sendCommand(int portID, int slaveID, char *buffer, int bufferSize, int commandLength);
	
	virtual void startAcquisition(int mode);
	virtual void stopAcquisition();
	virtual uint64_t getChannelUp();
	
private:
	DtFlyP *DP;
	//	DtFlyP *P;
	
	int dataFrameSharedMemory_fd;
	DataFrame *dataFrameSharedMemory;
	
protected:
	int nFEB;
	int *feTypeMap;
	
	static const unsigned MAX_PAYLOAD_SIZE = 8192;
	void * doWork();
	struct DAQFrame_t {
	        int size;
		uint8_t payload[MAX_PAYLOAD_SIZE*2];
		uint8_t type;
		uint8_t source;
		uint32_t crc;
	};
	
};

#endif
