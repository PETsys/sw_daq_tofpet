#ifndef __FRAMESERVER_HPP__DEFINED__
#define __FRAMESERVER_HPP__DEFINED__

#include <pthread.h>
#include <queue>

#include "Protocol.hpp"


class FrameServer {
public:
	virtual ~FrameServer();	
	struct DataFramePtr {
	public:
		unsigned index;
		bool fake;
		DataFrame *p;
		
		DataFramePtr (unsigned index, bool fake, DataFrame *p) : index(index), fake(fake), p(p) {};
	};
	
	virtual int sendCommand(char *buffer, int bufferSize, int commandLength) = 0;

	virtual const char *getDataFrameSharedMemoryName() = 0;
	virtual DataFramePtr *getDataFrameByPtr() = 0;
	virtual void returnDataFramePtr(DataFramePtr *ptr) = 0;

	virtual void startAcquisition(int mode) = 0;
	virtual void stopAcquisition() = 0;

};

#endif
