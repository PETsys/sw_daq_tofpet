#include "CoarseExtract.hpp"
#include <math.h>
#include <stdio.h>
#include <assert.h>

using namespace DAQ::Common;
using namespace DAQ::Core;

CoarseExtract::CoarseExtract(bool killZeroToT, EventSink<Pulse> *sink) : 
	OverlappedEventHandler<RawPulse, Pulse>(sink), killZeroToT(killZeroToT)
{
	nEvent = 0;
	nZeroToT = 0;
	nPassed = 0;
}

EventBuffer<Pulse> * CoarseExtract::handleEvents (EventBuffer<RawPulse> *inBuffer)
{
	long long tMin = inBuffer->getTMin();
	long long tMax = inBuffer->getTMax();
	unsigned N =  inBuffer->getSize();
	EventBuffer<Pulse> * outBuffer = new EventBuffer<Pulse>(N);
	outBuffer->setTMin(tMin);
	outBuffer->setTMax(tMax);
	
	u_int32_t lEvent = 0;
	u_int32_t lZeroTot = 0;
	u_int32_t lPassed = 0;	

	
	for(unsigned i = 0; i < N; i++) {
		RawPulse &raw = inBuffer->get(i);
		if(raw.time < tMin || raw.time >= tMax) continue;
		lEvent++;
		
		assert(raw.feType == RawPulse::TOFPET);
		if(raw.feType != RawPulse::TOFPET) continue;
/*
		int coarseToT = raw.d.tofpet.ecoarse >= raw.d.tofpet.tcoarse ? 
				raw.d.tofpet.ecoarse - raw.d.tofpet.tcoarse :
				raw.d.tofpet.ecoarse - raw.d.tofpet.tcoarse + 1024;
				*/
		int coarseToT =	raw.d.tofpet.ecoarse < 384 && raw.d.tofpet.tcoarse > 640 ? 
				1024 + raw.d.tofpet.ecoarse - raw.d.tofpet.tcoarse :
				raw.d.tofpet.ecoarse - raw.d.tofpet.tcoarse;
				

		// WARNING: reduces data, but may discard darks as well
		if(killZeroToT && coarseToT == 0) {
			lZeroTot += 1;
			continue;
		}
		
		Pulse &p = outBuffer->getWriteSlot();
		p.raw = raw;		
		// WARNING: rounding sensitive!
		p.time = raw.time;
		p.region = raw.region;
		p.channelID = raw.channelID;
		p.energy = coarseToT  * (raw.T * 1E-3); // In ns
		
		outBuffer->pushWriteSlot();
		lPassed++;
	}	

	atomicAdd(nEvent, lEvent);
	atomicAdd(nZeroToT, lZeroTot);
	atomicAdd(nPassed, lPassed);
	
	delete inBuffer;
	return outBuffer;
}


void CoarseExtract::report()
{
        fprintf(stderr, ">> CoarseExtract report\n");
        fprintf(stderr, " events received\n");
        fprintf(stderr, "  %10u\n", nEvent);
        fprintf(stderr, " events discarded\n");
        fprintf(stderr, "  %10u (%4.1f%%) zero ToT\n", nZeroToT, 100.0*nZeroToT/nEvent);
        fprintf(stderr, " events passed\n");
        fprintf(stderr, "  %10u (%4.1f%%)\n", nPassed, 100.0*nPassed/nEvent);
	
	OverlappedEventHandler<RawPulse, Pulse>::report();
}
