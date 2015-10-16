#include "NaiveGrouper.hpp"
#include <vector>
#include <math.h>

using namespace DAQ::Common;
using namespace DAQ::Core;
using namespace std;

NaiveGrouper::NaiveGrouper(float radius, double timeWindow1, float minEnergy, float maxEnergy, int maxHits,
			EventSink<GammaPhoton> *sink) :
	radius2(radius*radius), timeWindow1((long long)(timeWindow1*1E12)), minEnergy(minEnergy), maxEnergy(maxEnergy), maxHits(maxHits), OverlappedEventHandler<Hit, GammaPhoton>(sink, false)
{
	if(maxHits> GammaPhoton::maxHits)maxHits=GammaPhoton::maxHits;
	for(int i = 0; i < GammaPhoton::maxHits; i++)
		nHits[i] = 0;
	nHitsOverflow = 0;
}

NaiveGrouper::~NaiveGrouper()
{
}

void NaiveGrouper::report()
{
	u_int32_t nPhotons = 0;
	u_int32_t nTotalHits = 0;
	for(int i = 0; i < maxHits; i++) {
		nPhotons += nHits[i];
		nTotalHits += nHits[i] * (i+1);
	}
	nPhotons += nHitsOverflow;
		
	fprintf(stderr, ">> NaiveGrouper report\n");
	fprintf(stderr, " photons passed\n");
	fprintf(stderr, "  %10u total\n", nPhotons);
	for(int i = 0; i < maxHits; i++) {
		fprintf(stderr, "  %10u (%4.1f%%) with %d hits\n", nHits[i], 100.0*nHits[i]/nPhotons, i+1);
	}
	fprintf(stderr, "  %10u (%4.1f%%) with more than %d hits\n", nHitsOverflow, 100.0*nHitsOverflow/nPhotons, maxHits);
	fprintf(stderr, "  %4.1f average hits/photon\n", float(nTotalHits)/float(nPhotons));
			
	OverlappedEventHandler<Hit, GammaPhoton>::report();
}

EventBuffer<GammaPhoton> * NaiveGrouper::handleEvents(EventBuffer<Hit> *inBuffer)
{
	long long tMin = inBuffer->getTMin();
	long long tMax = inBuffer->getTMax();
	unsigned nEvents =  inBuffer->getSize();
	EventBuffer<GammaPhoton> * outBuffer = new EventBuffer<GammaPhoton>(nEvents, inBuffer);
	outBuffer->setTMin(tMin);
	outBuffer->setTMax(tMax);		
	
	u_int32_t lHits[maxHits];	
	for(int i = 0; i < maxHits; i++)
	lHits[i] = 0;
	u_int32_t lHitsOverflow = 0;

	vector<bool> taken(nEvents, false);
	for(unsigned i = 0; i < nEvents; i++) {
		if (taken[i]) continue;
		
		Hit &hit = inBuffer->get(i);
		taken[i] = true;
			
		Hit * hits[maxHits];
		hits[0] = &hit;
		int nHits = 1;
				
		for(int j = i+1; j < nEvents; j++) {
			Hit &hit2 = inBuffer->get(j);
			if(taken[j]) continue;			
			
			if(hit2.region != hit.region) continue;
			if((hit2.time - hit.time) > (overlap + timeWindow1)) break;			
			
			float u = hit.x - hit2.x;
			float v = hit.y - hit2.y;
			float w = hit.z - hit2.z;
			float d2 = u*u + v*v + w*w;


			if(d2 > radius2) continue;
			if(tAbs(hit.time - hit2.time) > timeWindow1) continue;

			taken[j] = true;

			if(nHits >= maxHits) {
				// Increase the hit count but don't actually add a hit
				nHits++;
			}
			else {
				hits[nHits] = &hit2;
				nHits++;
			}

			
			
		}
		
		if(nHits > maxHits) {
			// This event had too many hits
			// Count it and discard it
			lHitsOverflow += 1;
			continue;	
		}
		
		// Buble sorting..
		bool sorted = false;
		while(!sorted) {
			sorted = true;
			for(int k = 1; k < nHits; k++) {
				if(hits[k-1]->energy < hits[k]->energy) {
					sorted = false;
					Hit *tmp = hits[k-1];
					hits[k-1] = hits[k];
					hits[k] = tmp;
				}
			}
		}
		
		
		GammaPhoton &photon = outBuffer->getWriteSlot();
		for(int k = 0; k < nHits; k++)
			photon.hits[k] = hits[k];
		
		photon.nHits = nHits;		
		photon.region = photon.hits[0]->region;
		photon.time = photon.hits[0]->time;
		photon.x = photon.hits[0]->x;
		photon.y = photon.hits[0]->y;		
		photon.z = photon.hits[0]->z;		
		photon.energy = photon.hits[0]->energy;
		photon.missingEnergy = 0;
		photon.nMissing = 0;
		
		if((photon.energy < minEnergy) || (photon.energy > maxEnergy)) continue;
		


		outBuffer->pushWriteSlot();
		lHits[photon.nHits-1]++;
	}

	for(int i = 0; i < maxHits; i++)
		atomicAdd(nHits[i], lHits[i]);
	atomicAdd(nHitsOverflow, lHitsOverflow);
	
	return outBuffer;
}

