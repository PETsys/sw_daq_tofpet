#include "ComptonGrouper.hpp"
#include <vector>
#include <math.h>

using namespace DAQ::Common;
using namespace DAQ::Core;
using namespace std;

ComptonGrouper::ComptonGrouper(float radius, double timeWindow, int maxHits, float eMin, float eMax,
			EventSink<GammaPhoton> *sink) :
			radius2(radius*radius), timeWindow((long long)(timeWindow*1E12)),
			maxHits(maxHits < GammaPhoton::maxHits ? maxHits : GammaPhoton::maxHits), eMin(eMin), eMax(eMax),
			OverlappedEventHandler<Hit, GammaPhoton>(sink)
{
	for(int i = 0; i < GammaPhoton::maxHits; i++)
		nHits[i] = 0;
}

ComptonGrouper::~ComptonGrouper()
{
}

void ComptonGrouper::report()
{
	u_int32_t nPhotons = 0;
	for(int i = 0; i < GammaPhoton::maxHits; i++)
		nPhotons += nHits[i];
		
	fprintf(stderr, ">> ComptonGrouper report\n");
	fprintf(stderr, " photons passed\n");
	fprintf(stderr, "  %10u total\n", nPhotons);
	for(int i = 0; i < GammaPhoton::maxHits; i++) {
		fprintf(stderr, "  %10u (%4.1f%%) with %d hits\n", nHits[i], 100.0*nHits[i]/nPhotons, i+1);
	}
			
	OverlappedEventHandler<Hit, GammaPhoton>::report();
}

EventBuffer<GammaPhoton> * ComptonGrouper::handleEvents(EventBuffer<Hit> *inBuffer)
{
	long long tMin = inBuffer->getTMin();
	long long tMax = inBuffer->getTMax();
	unsigned nEvents =  inBuffer->getSize();
	EventBuffer<GammaPhoton> * outBuffer = new EventBuffer<GammaPhoton>(nEvents);
	outBuffer->setTMin(tMin);
	outBuffer->setTMax(tMax);		
	
	u_int32_t lHits[GammaPhoton::maxHits];	
	for(int i = 0; i < GammaPhoton::maxHits; i++)
		lHits[i] = 0;

	vector<bool> taken(nEvents, false);
	for(unsigned i = 0; i < nEvents; i++) {
		if (taken[i]) continue;
		
		Hit &hit = inBuffer->get(i);
		taken[i] = true;
			
		GammaPhoton &photon = outBuffer->getWriteSlot();
		photon.time = hit.time;
		photon.region = hit.region;
		photon.hits[0] = hit;
		int nHits = 1;
		
		for(int j = i+1; j < nEvents; j++) {
			Hit &hit2 = inBuffer->get(j);
			if(taken[j]) continue;			
			
			if(hit2.region != hit.region) continue;
			if((hit2.time - hit.time) > (overlap + timeWindow)) break;			
			
			float u = hit.x - hit2.x;
			float v = hit.y - hit2.y;
			float w = hit.y - hit2.y;
			float d2 = u*u + v*v + w*w;
			
			if((d2 < radius2) && (tAbs(hit.time - hit2.time) <= timeWindow)) {
				taken[j] = true;
				if(nHits < GammaPhoton::maxHits) {
					photon.hits[nHits] = hit2;
				}
				nHits++;
				continue;
			}
			
			
		}
		
		if(nHits > maxHits) continue;		
		
		photon.nHits = nHits;
		photon.energy = 0;
		photon.missingEnergy = 0;
		photon.nMissing = 0;
		for(int k = 0; k < photon.nHits; k++) {
			photon.energy += photon.hits[k].energy;
			photon.missingEnergy += photon.hits[k].missingEnergy;// * photon.hits[k].missingEnergy;
			photon.nMissing += photon.hits[k].nMissing;
		}
		//photon.missingEnergy = sqrtf(photon.missingEnergy);

		if((photon.energy + photon.missingEnergy) < eMin || photon.energy > eMax)
			continue;
		
		// WARNING: beware of rounding issues when modifying this code
		long long tA = photon.hits[0].time;
		float tB = 0;
		for(int k = 1; k < photon.nHits; k++) {
			tB += (photon.hits[k].time - tA) * photon.hits[k].energy / photon.energy;
		}			
		photon.time = tA + (long long)roundf(tB);
		
/*		if(photon.nHits > 1) {
			gLock->Lock();
			for (int i = 0; i < photon.nHits; i++)
				fprintf(stderr, "(%f, %f, %lld) ", photon.hits[i].energy, photon.hits[i].missingEnergy, photon.hits[i].time);				
			fprintf(stderr, "=> (%f, %f, %lld)\n", photon.energy, photon.missingEnergy, photon.time);			
			gLock->UnLock();		
		}*/
		
		/*
		 * 1 hit photons get hit position
		 * 2 hit photons get position of the lowest energy hit
		 * others we don't actually care.. 
		 */
		if(photon.nHits == 2 && photon.hits[1].energy < photon.hits[0].energy) {
			photon.x = photon.hits[1].x;
			photon.y = photon.hits[1].y;
			photon.z = photon.hits[1].z;
		}
		else {
			photon.x = photon.hits[0].x;
			photon.y = photon.hits[0].y;
			photon.z = photon.hits[0].z;
		}

		if(photon.time < tMin or photon.time >= tMax) continue;
		if(photon.nHits > maxHits) continue;

		outBuffer->pushWriteSlot();
		lHits[photon.nHits-1]++;
	}

	for(int i = 0; i < GammaPhoton::maxHits; i++)
		atomicAdd(nHits[i], lHits[i]);

	delete inBuffer;
	return outBuffer;
}
