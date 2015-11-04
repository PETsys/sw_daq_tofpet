#include "P2Extract.hpp"
#include <math.h>
#include <stdio.h>
#include <assert.h>

using namespace DAQ::Common;
using namespace DAQ::Core;
using namespace DAQ::TOFPET;

P2Extract::P2Extract(DAQ::TOFPET::P2 *lut, bool killZeroToT, float tDenormalTolerance, float eDenormalTolerance, bool killDenormal, EventSink<Pulse> *sink) : 
	lut(lut), killZeroToT(killZeroToT), tDenormalTolerance(tDenormalTolerance), eDenormalTolerance(eDenormalTolerance), killDenormal(killDenormal),
	OverlappedEventHandler<RawPulse, Pulse>(sink)
{
	nEvent = 0;
	nZeroToT = 0;
	nPassed = 0;
	nNotNormal = 0;
}

 bool P2Extract::handleEvent(RawPulse &raw, Pulse &pulse)
{
	// if event is true return good
	// if event is not good, use atomicIncrement() and return false
	
	//if(raw.time < tMin || raw.time >= tMax) return false;
	//atomicAdd(nEvent, 1);
	atomicAdd(nEvent, 1);
	if(raw.feType != RawPulse::TOFPET) return false;

	short tCoarse = raw.d.tofpet.tcoarse;
	short eCoarse = raw.d.tofpet.ecoarse;
				
		
	int coarseToT =	eCoarse < 384 && tCoarse > 640 ? 
		1024 + eCoarse - tCoarse :
		eCoarse - tCoarse;

		
	// WARNING: reduces data, but may discard darks as well
	if(killZeroToT && coarseToT == 0) {
	   	atomicAdd(nZeroToT, 1);
		return false;
	}
		
	short tfine = raw.d.tofpet.tfine;
	short efine = raw.d.tofpet.efine;
		
	long long tacIdleTime = raw.d.tofpet.tacIdleTime;
		
// 	if((killTDenormals && !lut->isNormal(raw.channelID, raw.d.tofpet.tac, true, tfine,tCoarse, tacIdleTime)) ||
// 	   (killEDenormals && !lut->isNormal(raw.channelID, raw.d.tofpet.tac, false, efine, eCoarse, tacIdleTime))) {
// 		atomicAdd(nNotNormal, 1);
// 		return false;
// 	}
   
	pulse.raw = &raw;
	pulse.region = raw.region;
	pulse.channelID = raw.channelID;

	pulse.tofpet_TQT = lut->getQ(raw.channelID, raw.d.tofpet.tac, true, tfine, tacIdleTime, coarseToT*raw.T/1000);
	pulse.tofpet_TQE = lut->getQ(raw.channelID, raw.d.tofpet.tac, false, efine, tacIdleTime, coarseToT*raw.T/1000);
	// WARNING: P2::geT() returns time with coarse value already added!
	float f_T = lut->getT(raw.channelID, raw.d.tofpet.tac, true, tfine, tCoarse, tacIdleTime, coarseToT*raw.T/1000.0) - tCoarse;
	float f_E = lut->getT(raw.channelID, raw.d.tofpet.tac, false, efine, eCoarse, tacIdleTime, coarseToT*raw.T/1000.0) - eCoarse;
	
	pulse.badEvent = false;
	if(pulse.tofpet_TQT < (1.0 - tDenormalTolerance) || pulse.tofpet_TQT > (3.0 + tDenormalTolerance) ||  pulse.tofpet_TQE < (1.0 - eDenormalTolerance) || pulse.tofpet_TQE > (3.0 + eDenormalTolerance)) {
		atomicAdd(nNotNormal, 1);
		pulse.badEvent = true;
		pulse.time = raw.time;
		pulse.timeEnd = raw.timeEnd;
		if(killDenormal) return false;
	}
	else {
		// WARNING: rounding sensitive!
		pulse.time = raw.time + (long long)((f_T * raw.T) + lut->timeOffset[raw.channelID]*1e12);
		pulse.timeEnd = raw.timeEnd + (long long)((f_E * raw.T) + lut->timeOffset[raw.channelID]*1e12);
	}
	
   	//printf("tot=%f energy = %e\n", 1E-3*(pulse.timeEnd - pulse.time), lut->timeOffset[raw.channelID]);
	pulse.energy = lut->getEnergy(raw.channelID, 1E-3*(pulse.timeEnd - pulse.time));

	atomicAdd(nPassed, 1); 
	return true; 
}

EventBuffer<Pulse> * P2Extract::handleEvents (EventBuffer<RawPulse> *inBuffer)
{
	long long tMin = inBuffer->getTMin();
	long long tMax = inBuffer->getTMax();
	unsigned nEvents =  inBuffer->getSize();
	EventBuffer<Pulse> * outBuffer = new EventBuffer<Pulse>(nEvents, inBuffer);
	outBuffer->setTMin(tMin);
	outBuffer->setTMax(tMax);		
	

	for(unsigned i = 0; i < nEvents; i++) {
                        
		RawPulse &raw = inBuffer->get(i);
		if(raw.time < tMin || raw.time >= tMax)continue; 
		Pulse &p = outBuffer->getWriteSlot();
		if (handleEvent(raw, p)){
			outBuffer->pushWriteSlot();
		}	
	}
	return outBuffer;
}


void P2Extract::printReport()
{
        printf(">> TOFPET::P2Extract report\n");
        printf(" events received\n");
        printf("  %10u\n", nEvent);
        printf(" events discarded\n");
        printf("  %10u (%4.1f%%) zero ToT\n", nZeroToT, 100.0*nZeroToT/nEvent);
		printf("  %10u (%4.1f%%) not normal\n", nNotNormal, 100.0*nNotNormal/nEvent);
        printf(" events passed\n");
        printf("  %10u (%4.1f%%)\n", nPassed, 100.0*nPassed/nEvent);

}

void P2Extract::report()
{
	printReport();	
	OverlappedEventHandler<RawPulse, Pulse>::report();
}
