#ifndef __DAQFRAMESERVER_HPP__DEFINED__
#define __DAQFRAMESERVER_HPP__DEFINED__

#include "FrameServer.hpp"
#include "DtFlyP.hpp"

#include "boost/tuple/tuple.hpp"
#include <boost/unordered_map.hpp> 
#include "boost/date_time/posix_time/posix_time.hpp"

namespace DAQd {

class DAQFrameServer : public FrameServer
{
public:
	DAQFrameServer(int nFEB, int *feTypeMap, int debugLevel);
	virtual ~DAQFrameServer();	


	int sendCommand(int portID, int slaveID, char *buffer, int bufferSize, int commandLength);
	
	virtual void startAcquisition(int mode);
	virtual void stopAcquisition();
	virtual uint64_t getPortUp();
	virtual uint64_t getPortCounts(int port, int whichCount);
private:
	DtFlyP *DP;
	
protected:

	void * doWork();
	
};

}
#endif
