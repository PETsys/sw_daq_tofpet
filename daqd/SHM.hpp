#ifndef __DAQD_SHM_CPP__DEFINED__
#define __DAQD_SHM_CPP__DEFINED__

#include "Protocol.hpp"
#include <sys/types.h>
#include <string>

namespace DAQd {
	
static const int MaxDataFrameSize = 2048;
static const unsigned MaxDataFrameQueueSize = 1024;
static const int N_ASIC=16*1024;


struct DataFrame {
		uint64_t data[MaxDataFrameSize];
#ifndef __NO_CHANNEL_IDLE_TIME__
		uint64_t channelIdleTime[MaxDataFrameSize];
		uint64_t tacIdleTime[MaxDataFrameSize];
#endif
#ifdef __ENDOTOFPET__
		int8_t feType[MaxDataFrameSize];
#endif
};	

class SHM {
public:
	SHM(std::string path);
	~SHM();

	unsigned long long getSizeInBytes();
	unsigned long long  getSizeInFrames() { 
		return MaxDataFrameQueueSize;
	};
	
	int getFrameSize(int index) {
		DataFrame *dataFrame = &shm[index];
		uint64_t eventWord = dataFrame->data[0];
		return (eventWord >> 36) & 0x7FFF;
	};
	
	unsigned long long getFrameWord(int index, int n) {
		DataFrame *dataFrame = &shm[index];
		uint64_t eventWord = dataFrame->data[n];
		return eventWord;
	};

	unsigned long long getFrameID(int index) {
		DataFrame *dataFrame = &shm[index];
		uint64_t eventWord = dataFrame->data[0];
		return eventWord & 0xFFFFFFFFFULL;
	};

	bool getFrameLost(int index) {
		DataFrame *dataFrame = &shm[index];
		uint64_t eventWord = dataFrame->data[1];
		return (eventWord & 0x10000) != 0;
	};

	int getNEvents(int index) {
		DataFrame *dataFrame = &shm[index];
		uint64_t eventWord = dataFrame->data[1];
		return eventWord & 0xFFFF;
	}; 

	int getEventType(int index, int event) {
#ifdef __ENDOTOFPET__
		DataFrame *dataFrame = &shm[index];
		return dataFrame->feType[event+2];
#else
		return 0;
#endif
	};

	int getTCoarse(int index, int event) {
		DataFrame *dataFrame = &shm[index];
		uint64_t eventWord = dataFrame->data[event+2];
		if(getEventType(index, event) == 0) {
			return (eventWord >> 38) & 0x3FF;
		}
		else {
			return decodeSticCoarse((unsigned int) ( 0x7FFF & (eventWord  >> 26) ), getFrameID(index));
		}	  
	}

	int getECoarse(int index, int event) {
		DataFrame *dataFrame = &shm[index];
		uint64_t eventWord = dataFrame->data[event+2];
		if(getEventType(index, event) == 0) {
			return (eventWord >> 18) & 0x3FF;
		}
		else {
			return decodeSticCoarse((unsigned int) (( 0x000fffe0 & eventWord) >> 5), getFrameID(index));
		}
	};
		
	int getTFine(int index, int event) {
		DataFrame *dataFrame = &shm[index];
		uint64_t eventWord = dataFrame->data[event+2];
		if(getEventType(index, event) == 0) {
			return (eventWord >> 28) & 0x3FF;
		}
		else {
			return (unsigned short)(( 0x03e00000 & eventWord) >> 21);
		}
	};
	
	int getEFine(int index, int event) {
		DataFrame *dataFrame = &shm[index];
		uint64_t eventWord = dataFrame->data[event+2];
		if(getEventType(index, event) == 0) {
			return (eventWord >> 8) & 0x3FF;
		}
		else {
			return (unsigned short) 	(( 0x0000001f & eventWord));
		}
	};
	
	int getAsicID(int index, int event) {
		DataFrame *dataFrame = &shm[index];
		uint64_t eventWord = dataFrame->data[event+2];
		return eventWord >> 48;
	};
	
	int getChannelID(int index, int event) {
		DataFrame *dataFrame = &shm[index];
		uint64_t eventWord = dataFrame->data[event+2];
		if(getEventType(index, event) == 0) {
			return (eventWord >> 2) & 0x3F;
		}
		else {
			return ((0x0000fc0000000000 & eventWord) >> (32+10));
		}
	};
	
	int getTACID(int index, int event) {
		DataFrame *dataFrame = &shm[index];
		uint64_t eventWord = dataFrame->data[event+2];
		if(getEventType(index, event) == 0) {
			return (eventWord >> 0) & 0x3;
		}
		else {
			return 0;
		}
	};
	
	bool getTBadHit(int index, int event) {
		DataFrame *dataFrame = &shm[index];
		uint64_t eventWord = dataFrame->data[event+2];
		if(getEventType(index, event) == 0) {
			return false;
		}
		else {
			return (( 0x0000020000000000 & eventWord) != 0 ) ? true:false;
		}
	};
	
	bool getEBadHit(int index, int event) {
		DataFrame *dataFrame = &shm[index];
		uint64_t eventWord = dataFrame->data[event+2];
		if(getEventType(index, event) == 0) {
			return false;
		}
		else {
			return (( 0x00100000 & eventWord) != 0 ) ? true:false;
		}
	};

	long long getTACIdleTime(int index, int event) {
#ifndef __NO_CHANNEL_IDLE_TIME__
		DataFrame *dataFrame = &shm[index];
		return dataFrame->tacIdleTime[event+2];
#else
		return 0;
#endif
	};

	long long getChannelIdleTime(int index, int event) {
#ifndef __NO_CHANNEL_IDLE_TIME__
		DataFrame *dataFrame = &shm[index];
		return dataFrame->channelIdleTime[event+2];
#else
		return 0;
#endif
	};
  

private:
	unsigned decodeSticCoarse(unsigned coarse, unsigned long long frameID);
	
	int shmfd;
	DataFrame *shm;
	off_t shmSize;

	int16_t m_lut[ 1 << 15 ];
};

}
#endif