#ifndef __CORE__SHAREDMEMORY_HPP__DEFINED__
#define __CORE__SHAREDMEMORY_HPP__DEFINED__

#include <stdint.h>

static const int MaxEventsPerFrame = 512;
static const int MaxTimestampsPerCluster = 96;

struct EventPlate {
	  uint16_t asicID;
	  uint16_t channelID;
	  uint16_t tacID;
	  uint16_t tCoarse;
	  uint16_t tFine;
	  uint16_t eCoarse;
	  uint16_t eFine;
};

struct EventProbe {
	uint16_t asicID;
	uint16_t clusterID;
	uint16_t energy;
	uint16_t nTimestamps;
	uint16_t timestamps[MaxTimestampsPerCluster];
};

struct Event {
	union { 
		EventPlate plate;
		EventProbe probe;
	} d;
};

struct DataFrame {
	uint64_t frameID;
	bool frameLost;
	uint16_t nEvents;
	bool isProbe[MaxEventsPerFrame];
	Event events[MaxEventsPerFrame];
};

#endif