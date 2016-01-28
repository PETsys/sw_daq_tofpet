#ifndef __DAQ_CORE_SYSTEMINFORMATION_HPP__DEFINED__
#define __DAQ_CORE_SYSTEMINFORMATION_HPP__DEFINED__

#include <Common/Constants.hpp>

namespace DAQ { namespace Common {

class SystemInformation {
public:
	struct ChannelInformation {
		int region;
		int xi;
		int yi;
		float x;
		float y;
		float z;
	};
	
	SystemInformation();
	~SystemInformation();

	void loadMapFile(const char *fname);

	bool isCoincidenceAllowed(int region1, int region2) {
		return region1 != region2;
	};

	bool isMultihitAllowed(int region1, int region2) {
		return region1 == region2;
	};

	ChannelInformation &getChannelInformation(int channelID) { return channelInformation[channelID]; };

private:
	ChannelInformation channelInformation[DAQ::Common::SYSTEM_NCHANNELS];
};

}}
#endif

