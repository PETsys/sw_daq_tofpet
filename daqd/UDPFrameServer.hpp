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
	virtual ~UDPFrameServer();	

	int sendCommand(char *buffer, int bufferSize, int commandLength);
	

private:
	
	int udpSocket;

protected:
	void * doWork(void *);
	
};


#endif
