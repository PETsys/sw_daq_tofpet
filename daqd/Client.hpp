#ifndef __CLIENT_HPP__DEFINED__
#define __CLIENT_HPP__DEFINED__

#include "FrameServer.hpp"
#include <map>

class Client {
public:
	Client (int socket, FrameServer *frameServer);
	~Client();
	
	int handleRequest();
	
private:
	int socket;
	unsigned char socketBuffer[16*1024];
	FrameServer *frameServer;
	
	int doAcqOnOff();
	int doGetDataFrameSharedMemoryName();
	int doGetEventFrameBuffers();
	int doReturnEventFrameBuffers();
	int doCommandToFrontEnd(int commandLength);
	int doGetTOFPETDataFrame();
	
	std::map<int, FrameServer::DataFramePtr *> ownedDataFramePtrs;
	
};

#endif