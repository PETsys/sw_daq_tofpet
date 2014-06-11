#include "CoincidenceFilter.hpp"
#include <vector>

using namespace std;
using namespace DAQ::Common;
using namespace DAQ::Core;

CoincidenceFilter::CoincidenceFilter(float cWindow, float rWindow,
	EventSink<GammaPhoton> *sink)
	: OverlappedEventHandler<GammaPhoton, GammaPhoton>(sink), cWindow((long long)(cWindow*1E12)), rWindow((long long)(rWindow*1E12))
{
	nEventsOut = 0;
}

CoincidenceFilter::~CoincidenceFilter()
{
}

void CoincidenceFilter::report()
{
	fprintf(stderr, ">> CoincidenceFilter report\n");
	fprintf(stderr, "cWindow = %lld\n", cWindow);
	fprintf(stderr, " events passed\n");
	fprintf(stderr, "  %10u \n", nEventsOut);
	OverlappedEventHandler<GammaPhoton, GammaPhoton>::report();
}

EventBuffer<GammaPhoton> * CoincidenceFilter::handleEvents(EventBuffer<GammaPhoton> *inBuffer)
{
	long long tMin = inBuffer->getTMin();
	long long tMax = inBuffer->getTMax();
	unsigned nEvents =  inBuffer->getSize();
	
	u_int32_t lEventsOut = 0;
	vector<bool> matched(nEvents, false);
	vector<bool> rejected(nEvents, false);
	
	for(unsigned i = 0; i < nEvents; i++) {
		GammaPhoton &photon1 = inBuffer->get(i);
		if(cWindow == 0) {
				matched[i] = true;
				continue;
		}
		
		for(unsigned j = i+1; j < nEvents; j++) {
			GammaPhoton &photon2 = inBuffer->get(j);	
			if ((photon2.time - photon1.time) > (cWindow+rWindow+overlap)) break;
						
			long long dt = tAbs(photon1.time - photon2.time);
			if(photon1.region != photon2.region && dt <= cWindow) {
				matched[i] = true;
				matched[j] = true;
			}
			if (dt > cWindow && rWindow > 0 && dt <= rWindow) {
				rejected[i] = true;
			}		
	
		}

	}
	
	for(unsigned i = 0; i < nEvents; i++) {
		GammaPhoton &photon = inBuffer->get(i);
		if(photon.time < tMin ||photon.time >= tMax) continue;

		if(!matched[i]) { 
			photon.time = -1;
			continue;
		}
		
		if(rejected[i]) {
			photon.time = -1;
			continue;
		}				
		
		lEventsOut += 1;
	}
	
	atomicAdd(nEventsOut, lEventsOut);
	
	return inBuffer;
}
