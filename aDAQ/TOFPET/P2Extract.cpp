#include "P2Extract.hpp"
#include <math.h>
#include <stdio.h>
#include <assert.h>

using namespace DAQ::Common;
using namespace DAQ::Core;
using namespace DAQ::TOFPET;

P2Extract::P2Extract(DAQ::TOFPET::P2 *lut, bool killZeroToT, EventSink<Pulse> *sink) : 
	lut(lut), killZeroToT(killZeroToT),
	OverlappedEventHandler<RawPulse, Pulse>(sink)
{
	nEvent = 0;
	nZeroToT = 0;
	nPassed = 0;
	nNotNormal = 0;
}

EventBuffer<Pulse> * P2Extract::handleEvents (EventBuffer<RawPulse> *inBuffer)
{
	long long tMin = inBuffer->getTMin();
	long long tMax = inBuffer->getTMax();
	unsigned nEvents =  inBuffer->getSize();
	EventBuffer<Pulse> * outBuffer = new EventBuffer<Pulse>(nEvents);
	outBuffer->setTMin(tMin);
	outBuffer->setTMax(tMax);		
	
	u_int32_t lEvent = 0;
	u_int32_t lZeroTot = 0;
	u_int32_t lPassed = 0;
	u_int32_t lNotNormal = 0;

	for(unsigned i = 0; i < nEvents; i++) {
		RawPulse &raw = inBuffer->get(i);
//		printf("%20lld <= %20lld < %20lld\n", tMin, raw.time, tMax);
		if(raw.time < tMin || raw.time >= tMax) continue;
		lEvent++;
		
		assert(raw.feType == RawPulse::TOFPET);
		if(raw.feType != RawPulse::TOFPET) continue;

		short tCoarse = raw.d.tofpet.tcoarse;
		short eCoarse = raw.d.tofpet.ecoarse;
				
		
		int coarseToT =	eCoarse < 384 && tCoarse > 640 ? 
				1024 + eCoarse - tCoarse :
				eCoarse - tCoarse;

		
		// WARNING: reduces data, but may discard darks as well
		if(killZeroToT && coarseToT == 0) {
			lZeroTot += 1;
			continue;
		}
		
		short tfine = raw.d.tofpet.tfine;
		short efine = raw.d.tofpet.efine;
		
		long long tacIdleTime = raw.d.tofpet.tacIdleTime;
		
		if(!lut->isNormal(raw.channelID, raw.d.tofpet.tac, true, tfine,tCoarse, tacIdleTime)) {
			lNotNormal += 1;
			//continue;
		}
		
		// WARNING: P2::geT() returns time with coarse value already added!
		float f_T = lut->getT(raw.channelID, raw.d.tofpet.tac, true, tfine, tCoarse, tacIdleTime) - tCoarse;
		float f_E = lut->getT(raw.channelID, raw.d.tofpet.tac, false, efine, eCoarse, tacIdleTime) - eCoarse;
		
	
		Pulse &p = outBuffer->getWriteSlot();
		p.raw = raw;		
		// WARNING: rounding sensitive!
		p.time = raw.time + (long long)((f_T * raw.d.tofpet.T));
		p.region = raw.region;
		p.channelID = raw.channelID;
		
		
		p.energy = (coarseToT + f_E - f_T) * (raw.d.tofpet.T * 1E-3); // In ns
		p.tofpet_TQT = lut->getQ(raw.channelID, raw.d.tofpet.tac, true, tfine, tacIdleTime);
		p.tofpet_TQE = lut->getQ(raw.channelID, raw.d.tofpet.tac, false, efine, tacIdleTime);

		outBuffer->pushWriteSlot();
		lPassed++;
	}	

	atomicAdd(nEvent, lEvent);
	atomicAdd(nZeroToT, lZeroTot);
	atomicAdd(nPassed, lPassed);
	atomicAdd(nNotNormal, lNotNormal);
	
	delete inBuffer;
	return outBuffer;
}


void P2Extract::report()
{
        printf(">> P2Extract report\n");
        printf(" events received\n");
        printf("  %10u\n", nEvent);
        printf(" events discarded\n");
        printf("  %10u (%4.1f%%) zero ToT\n", nZeroToT, 100.0*nZeroToT/nEvent);
	printf("  %10u (%4.1f%%) not normal\n", nNotNormal, 100.0*nNotNormal/nEvent);
        printf(" events passed\n");
        printf("  %10u (%4.1f%%)\n", nPassed, 100.0*nPassed/nEvent);
	
	OverlappedEventHandler<RawPulse, Pulse>::report();
}
