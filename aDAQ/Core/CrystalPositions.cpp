#include "CrystalPositions.hpp"
#include <math.h>
#include <assert.h>
using namespace DAQ::Core;
using namespace DAQ::Common;
using namespace std;

CrystalPositions::CrystalPositions(int nCrystals, const char *mapFileName, EventSink<Hit> *sink) :
	nCrystals(nCrystals),
	OverlappedEventHandler<RawHit, Hit>(sink)
{
	fprintf(stderr, "Loading map file: '%s' ... ", mapFileName); fflush(stderr);
	map = new Entry[nCrystals];
	for(int crystal = 0; crystal < nCrystals; crystal++) {
		map[crystal].region = -1;
		map[crystal].xi = -1;
		map[crystal].yi = -1;
		map[crystal].x = -INFINITY;
		map[crystal].y = -INFINITY;
	}
	
	FILE *mapFile = fopen(mapFileName, "r");
	assert(mapFile != NULL);
	
	int crystal;
	int region;
	int xi;
	int yi;
	float x;
	float y;
	float z;
	int hv;
	
	int nLoaded = 0;
	while(fscanf(mapFile, "%d\t%d\t%d\t%d\t%f\t%f\t%f\t%d\n", &crystal, &region, &xi, &yi, &x, &y, &z, &hv) == 8) {
		
		assert(crystal < nCrystals);
		
		map[crystal].region = region;
		map[crystal].xi = xi;
		map[crystal].yi = yi;
		map[crystal].x = x;
		map[crystal].y = y;
		map[crystal].z = z;
		
		nLoaded++;
	}
	
	
	fclose(mapFile);
	fprintf(stderr, "%d valid crystal coordinates found\n", nLoaded);
	
	nEventsIn = 0;
	nEventsOut = 0;
}

CrystalPositions::~CrystalPositions()
{
	delete [] map;
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
		if(map[id].region == -1) continue;
		
		Hit &hit = outBuffer->getWriteSlot();
		hit.raw = &raw;
		hit.time = raw.time;
		hit.energy = raw.energy;
		hit.missingEnergy = raw.missingEnergy;
		hit.nMissing = raw.nMissing;

		
		hit.region = map[id].region;		
		hit.x = map[id].x;
		hit.y = map[id].y;
		hit.z = map[id].z;
		hit.xi	= map[id].xi;
		hit.yi	= map[id].yi;

/*		fprintf(stderr, "X1 %20lld %20lld => %8lld\n", 
		       hit.raw.top.time, 
		       hit.raw.top.timeEnd,
		       (hit.raw.top.timeEnd - hit.raw.top.time) / 1000);*/
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
