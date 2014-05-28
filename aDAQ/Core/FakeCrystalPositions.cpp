#include "FakeCrystalPositions.hpp"

using namespace DAQ::Core;
using namespace DAQ::Common;
using namespace std;


FakeCrystalPositions::FakeCrystalPositions(EventSink<Hit> *sink) :
	OverlappedEventHandler<RawHit, Hit>(sink)
{
}

EventBuffer<Hit> * FakeCrystalPositions::handleEvents (EventBuffer<RawHit> *inBuffer)
{
	long long tMin = inBuffer->getTMin();
	long long tMax = inBuffer->getTMax();
	unsigned nEvents =  inBuffer->getSize();
	EventBuffer<Hit> * outBuffer = new EventBuffer<Hit>(nEvents);
	outBuffer->setTMin(tMin);
	outBuffer->setTMax(tMax);		
	
	for(unsigned i = 0; i < nEvents; i++) {
		RawHit &raw = inBuffer->get(i);
		if(raw.time < tMin || raw.time >= tMax) continue;
		
		int id = raw.crystalID;
		
		Hit &hit = outBuffer->getWriteSlot();
		hit.raw = raw;
		hit.time = raw.time;
		hit.region = raw.region;
		hit.energy = raw.energy;
		hit.missingEnergy = raw.missingEnergy;
		hit.nMissing = raw.nMissing;

		
		hit.x = 3.6 * (id / 4);
		hit.y = 3.6 * (id % 4);
		hit.z = 0;
		
		outBuffer->pushWriteSlot();
	}
	
	delete inBuffer;
	return outBuffer;
}

void FakeCrystalPositions::report()
{
	fprintf(stderr, ">> FakeCrystalPositions report\n");
	OverlappedEventHandler<RawHit, Hit>::report();
}
