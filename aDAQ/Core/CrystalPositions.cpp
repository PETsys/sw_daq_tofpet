#include "CrystalPositions.hpp"
#include <math.h>
#include <assert.h>
using namespace DAQ::Core;
using namespace DAQ::Common;
using namespace std;

CrystalPositions::CrystalPositions(SystemInformation *systemInformation, EventSink<Hit> *sink) :
	systemInformation(systemInformation),
	OverlappedEventHandler<RawHit, Hit>(sink)
{
	nEventsIn = 0;
	nEventsOut = 0;
}

CrystalPositions::~CrystalPositions()
{
}

EventBuffer<Hit> * CrystalPositions::handleEvents (EventBuffer<RawHit> *inBuffer)
{
	long long tMin = inBuffer->getTMin();
	long long tMax = inBuffer->getTMax();
	unsigned nEvents =  inBuffer->getSize();
	EventBuffer<Hit> * outBuffer = new EventBuffer<Hit>(nEvents, inBuffer);
	outBuffer->setTMin(tMin);
	outBuffer->setTMax(tMax);		
	
	uint32_t lEventsIn = 0;
	uint32_t lEventsOut = 0;
	for(unsigned i = 0; i < nEvents; i++) {
		RawHit &raw = inBuffer->get(i);
		if(raw.time < tMin || raw.time >= tMax) continue;
		lEventsIn += 1;
		
		int id = raw.crystalID;
		SystemInformation::ChannelInformation &channelInformation = systemInformation->getChannelInformation(id);
		
		int region = channelInformation.region;
		if(region == -1) continue;
		
		Hit &hit = outBuffer->getWriteSlot();
		hit.raw = &raw;
		hit.time = raw.time;
		hit.energy = raw.energy;
		hit.missingEnergy = raw.missingEnergy;
		hit.nMissing = raw.nMissing;

		hit.region = region;
		hit.x = channelInformation.x;
		hit.y = channelInformation.y;
		hit.z = channelInformation.z;
		hit.xi	= channelInformation.xi;
		hit.yi	= channelInformation.yi;

		outBuffer->pushWriteSlot();
		lEventsOut += 1;
	}
	
	atomicAdd(nEventsIn, lEventsIn);
	atomicAdd(nEventsOut, lEventsOut);
	
	return outBuffer;
}

void CrystalPositions::report()
{
	fprintf(stderr, ">> CrystalPositions report\n");
	fprintf(stderr, " hits received\n");
	fprintf(stderr, "  %10u\n", nEventsIn);
	fprintf(stderr, " hits passed\n");
	fprintf(stderr, "  %10u\n", nEventsOut);
	OverlappedEventHandler<RawHit, Hit>::report();
}
