#ifndef __DAQ__COMMON__CONSTANTS_HPP__DEFINED__
#define __DAQ__COMMON__CONSTANTS_HPP__DEFINED__
namespace DAQ { namespace Common {
	
	static const int SYSTEM_NCRYSTALS = 2*16*16*64;
	static const int SYSTEM_NCHANNELS = SYSTEM_NCRYSTALS;
	static const float SYSTEM_PERIOD = 6.25E-9;
	
	static const unsigned EVENT_BLOCK_SIZE = 4096;


}}
#endif
