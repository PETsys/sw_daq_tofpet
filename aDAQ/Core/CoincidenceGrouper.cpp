#include "CoincidenceGrouper.hpp"

using namespace DAQ::Common;
using namespace DAQ::Core;

CoincidenceGrouper::CoincidenceGrouper(float cWindow, 
	EventSink<Coincidence> *sink)
	: OverlappedEventHandler<GammaPhoton, Coincidence>(sink), cWindow((long long)(cWindow*1E12))
{
	nPrompts = 0;
}

CoincidenceGrouper::~CoincidenceGrouper()
{
}

void CoincidenceGrouper::report()
{
	printf(">> CoincidenceGrouper report\n");
	printf(" prompts passed\n");
	printf("  %10u \n", nPrompts);
	OverlappedEventHandler<GammaPhoton, Coincidence>::report();
}

EventBuffer<Coincidence> * CoincidenceGrouper::handleEvents(EventBuffer<GammaPhoton> *inBuffer)
{
	long long tMin = inBuffer->getTMin();
	long long tMax = inBuffer->getTMax();
	unsigned nEvents =  inBuffer->getSize();
	EventBuffer<Coincidence> * outBuffer = new EventBuffer<Coincidence>(nEvents);
	outBuffer->setTMin(tMin);
	outBuffer->setTMax(tMax);		

	u_int32_t lPrompts = 0;
	for(unsigned i = 0; i < nEvents; i++) {
		GammaPhoton &photon1 = inBuffer->get(i);
		
		for(unsigned j = i+1; j < nEvents; j++) {
			GammaPhoton &photon2 = inBuffer->get(j);			
			if ((photon2.time - photon1.time) > (overlap + cWindow)) break;
						
			if(photon1.region == photon2.region) continue;
			
			if(tAbs(photon1.time - photon2.time) <= cWindow) {
				Coincidence &c = outBuffer->getWriteSlot();
				c.nPhotons = 2;
				
				bool first1 = photon1.region > photon2.region;
				c.photons[0] = first1 ? photon1 : photon2;
				c.photons[1] = first1 ? photon2 : photon1;
				if(c.photons[0].time < tMin || c.photons[0].time >= tMax) continue;		
				outBuffer->pushWriteSlot();
				lPrompts++;
			}		
	
		}
		
	}
	atomicAdd(nPrompts, lPrompts);
	
	delete inBuffer;
	return outBuffer;
}
