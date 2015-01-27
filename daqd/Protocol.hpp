#ifndef __PROTOCOL_HPP__DEFINED__
#define __PROTOCOL_HPP__DEFINED__

#include <stdint.h>


static const uint16_t commandAcqOnOff = 0x01;
static const uint16_t commandGetDataFrameSharedMemoryName = 0x02;
static const uint16_t commandGetDataFrameBuffers = 0x03;
static const uint16_t commandReturnDataFrameBuffers = 0x04;
static const uint16_t commandToFrontEnd = 0x05;
static const uint16_t commandGetTOFPETDataFRame = 0x06;


static const int MaxEventsPerFrame = 512;
static const int MaxTimestampsPerCluster = 96;

struct EventTOFPET {
	uint16_t tacID;
	uint16_t eCoarse;
	uint16_t tFine;
	uint16_t eFine;	
	int64_t channelIdleTime;
	int64_t tacIdleTime;
};

struct EventSTiCv3 {
	uint16_t tCoarseL;
	uint16_t eCoarseL;
	uint16_t tFine;
	uint16_t eFine;	
	bool tBadHit;
	bool eBadHit;
	int64_t channelIdleTime;
};

struct EventDISPM {
	uint16_t asicID;
	uint16_t clusterID;
	uint16_t energy;
	uint16_t nTimestamps;
	uint16_t timestamps[MaxTimestampsPerCluster];
};

struct Event {
	uint16_t asicID;
	uint16_t channelID;
	uint16_t tCoarse;
	uint8_t type;
	
	union { 
		EventTOFPET tofpet;
		EventSTiCv3 sticv3;
		//EventDISPM probe;
	} d;
};

struct DataFrame {
	uint64_t frameID;
	bool frameLost;
	uint16_t nEvents;
	Event events[MaxEventsPerFrame];
};

#endif
