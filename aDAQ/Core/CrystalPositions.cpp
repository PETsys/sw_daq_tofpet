#include "CrystalPositions.hpp"
#include <math.h>
#include <assert.h>
using namespace DAQ::Core;
using namespace DAQ::Common;
using namespace std;

CrystalPositions::CrystalPositions(SystemInformation *systemInformation, EventSink<Hit> *sink) :
	systemInformation(systemInformation),
	OverlappedEventHandler<Hit, Hit>(sink)
{
	nEventsIn = 0;
	nEventsOut = 0;
}

CrystalPositions::~CrystalPositions()
{
}

EventBuffer<Hit> * CrystalPositions::handleEvents (EventBuffer<Hit> *inBuffer)
{
	long long tMin = inBuffer->getTMin();
	long long tMax = inBuffer->getTMax();
	unsigned nEvents =  inBuffer->getSize();
	
	uint32_t lEventsIn = 0;
	uint32_t lEventsOut = 0;
	for(unsigned i = 0; i < nEvents; i++) {
		Hit &hit = inBuffer->get(i);
		if(hit.time < tMin || hit.time >= tMax) continue;
		lEventsIn += 1;
		
		int id = hit.raw->channelID;
		SystemInformation::ChannelInformation &channelInformation = systemInformation->getChannelInformation(id);
		
		int region = channelInformation.region;
		if(region == -1) {
			hit.time = -1;
			continue;
		}
		
		hit.region = region;
		hit.x = channelInformation.x;
		hit.y = channelInformation.y;
		hit.z = channelInformation.z;
		hit.xi	= channelInformation.xi;
		hit.yi	= channelInformation.yi;

		lEventsOut += 1;
	}
	
	atomicAdd(nEventsIn, lEventsIn);
	atomicAdd(nEventsOut, lEventsOut);
	
	return inBuffer;
}

void CrystalPositions::report()
{
	fprintf(stderr, ">> CrystalPositions report\n");
	fprintf(stderr, " hits received\n");
	fprintf(stderr, "  %10u\n", nEventsIn);
	fprintf(stderr, " hits passed\n");
	fprintf(stderr, "  %10u\n", nEventsOut);
	OverlappedEventHandler<Hit, Hit>::report();
}
