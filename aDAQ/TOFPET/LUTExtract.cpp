#include "LUTExtract.hpp"
#include <math.h>
#include <stdio.h>
#include <assert.h>

using namespace DAQ::Common;
using namespace DAQ::Core;
using namespace DAQ::TOFPET;

LUTExtract::LUTExtract(DAQ::TOFPET::LUT *lut, bool killZeroToT, EventSink<Pulse> *sink) : 
	lut(lut), killZeroToT(killZeroToT),
	OverlappedEventHandler<RawPulse, Pulse>(sink)
{
	nEvent = 0;
	nZeroToT = 0;
	nPassed = 0;
	nNotNormal = 0;
}

EventBuffer<Pulse> * LUTExtract::handleEvents (EventBuffer<RawPulse> *inBuffer)
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

/*		// Discard TOFPET staturated events
		// Needs to be 1400?!?!?! for 25 ps binning
		if(raw.d.tofpet.tfine > 700) continue;*/
		
/*		int coarseToT = raw.d.tofpet.ecoarse >= raw.d.tofpet.tcoarse ? 
				raw.d.tofpet.ecoarse - raw.d.tofpet.tcoarse :
				raw.d.tofpet.ecoarse - raw.d.tofpet.tcoarse + 1024;*/
				
		int coarseToT =	raw.d.tofpet.ecoarse < 384 && raw.d.tofpet.tcoarse > 640 ? 
				1024 + raw.d.tofpet.ecoarse - raw.d.tofpet.tcoarse :
				raw.d.tofpet.ecoarse - raw.d.tofpet.tcoarse;

				

		// WARNING: reduces data, but may discard darks as well
		if(killZeroToT && coarseToT == 0) {
			lZeroTot += 1;
			continue;
		}
		
		short tfine = raw.d.tofpet.tfine;
		short efine = raw.d.tofpet.efine;
		
		if(!lut->isNormal(raw.channelID, raw.d.tofpet.tac, true, tfine, raw.d.tofpet.tcoarse)) {
			lNotNormal += 1;
			continue;
		}
	
// 		if(!lut->isNormal(raw.channelID, raw.d.tofpet.tac, false, raw.d.tofpet.efine, raw.d.tofpet.ecoarse))
// 			continue;

		// WARNING: LUT::geT() returns time with coarse value already added!
		float f_T = lut->getT(raw.channelID, raw.d.tofpet.tac, true, tfine, raw.d.tofpet.tcoarse) - raw.d.tofpet.tcoarse;
		float f_E = lut->getT(raw.channelID, raw.d.tofpet.tac, false, efine, raw.d.tofpet.ecoarse) - raw.d.tofpet.ecoarse;
		
	
		Pulse &p = outBuffer->getWriteSlot();
		p.raw = raw;		
		// WARNING: rounding sensitive!
		p.time = raw.time + (long long)((f_T * raw.d.tofpet.T));
		p.region = raw.region;
		p.channelID = raw.channelID;
		p.energy = (coarseToT + f_E - f_T) * (raw.d.tofpet.T * 1E-3); // In ns
		
//		printf("%20lld %4d %4d %7.4f * %lld => %lld\n", raw.time, raw.d.tofpet.tcoarse, raw.d.tofpet.tfine, f_T, raw.d.tofpet.T, p.time);

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


void LUTExtract::report()
{
        printf(">> LUTExtract report\n");
        printf(" events received\n");
        printf("  %10u\n", nEvent);
        printf(" events discarded\n");
        printf("  %10u (%4.1f%%) zero ToT\n", nZeroToT, 100.0*nZeroToT/nEvent);
	printf("  %10u (%4.1f%%) not normal\n", nNotNormal, 100.0*nNotNormal/nEvent);
        printf(" events passed\n");
        printf("  %10u (%4.1f%%)\n", nPassed, 100.0*nPassed/nEvent);
	
	OverlappedEventHandler<RawPulse, Pulse>::report();
}
