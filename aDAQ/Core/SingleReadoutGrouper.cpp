#include "SingleReadoutGrouper.hpp"
#include <vector>

using namespace DAQ::Common;
using namespace DAQ::Core;
using namespace std;

SingleReadoutGrouper::SingleReadoutGrouper(EventSink<RawHit> *sink) :
	OverlappedEventHandler<Pulse, RawHit>(sink)
{
	nSingleRead = 0;
}
 
EventBuffer<RawHit> * SingleReadoutGrouper::handleEvents (EventBuffer<Pulse> *inBuffer)
{
	long long tMin = inBuffer->getTMin();
	long long tMax = inBuffer->getTMax();
	unsigned nEvents =  inBuffer->getSize();
	EventBuffer<RawHit> * outBuffer = new EventBuffer<RawHit>(nEvents);
	outBuffer->setTMin(tMin);
	outBuffer->setTMax(tMax);	
	
	u_int32_t lSingleRead = 0;

	unsigned N = inBuffer->getSize();
	for(unsigned i = 0; i < N; i++) {
		Pulse &e = inBuffer->get(i);		
		int crystalID = e.channelID;
		
		// WARNING: this needs better handling..
		if(e.badEvent) continue;
		
		RawHit &hit = outBuffer->getWriteSlot();
		hit.top = e;
		hit.time = e.time;
		if(hit.time < tMin || hit.time >=  tMax) continue;
		hit.bottom = Pulse();
		hit.crystalID = crystalID;
		hit.region = e.region;
		hit.energy = e.energy;
		hit.missingEnergy = 0;
		hit.nMissing = 0;
		hit.asymmetry = 0;		
		outBuffer->pushWriteSlot();		
		lSingleRead++;

	}
	atomicAdd(nSingleRead, lSingleRead);
	
	delete inBuffer;
	return outBuffer;
}

void SingleReadoutGrouper::report()
{
	u_int32_t nTotal = nSingleRead;
	fprintf(stderr, ">> SingleReadoutGrouper report\n");
	fprintf(stderr, " hits passed\n");
	fprintf(stderr, "  %10u\n", nSingleRead);
 	OverlappedEventHandler<Pulse, RawHit>::report();
}
