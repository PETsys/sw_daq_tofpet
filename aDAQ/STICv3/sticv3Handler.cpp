#include "sticv3Handler.hpp"
#include <Common/SystemInformation.hpp>
#include <stdio.h>

using namespace DAQ::Core;
using namespace DAQ::STICv3;
using namespace DAQ::Common;

Sticv3Handler::Sticv3Handler() 
{
	nPassed=0;
}

 bool Sticv3Handler::handleEvent(RawHit &raw, Hit &pulse)
{
	// if event is good return true
	// if event is not good (see examples below), return false
	
	atomicAdd(nEvent, 1);
	if(raw.feType != RawHit::STIC) return false;

        pulse.raw = &raw;
	long long T = SYSTEM_PERIOD * 1E12;
        // WARNING: rounding sensitive!
        pulse.time = raw.time + (raw.d.stic.tfine * T / (4*32));
	pulse.timeEnd = raw.timeEnd - (raw.d.stic.efine * T / (4*32));
        pulse.energy = 1E-3*(pulse.timeEnd - pulse.time);
		
	//printf("%lld %lld %f\n", pulse.time, pulse.timeEnd, pulse.energy);
	   
	atomicAdd(nPassed, 1);
	return true; 

}


void Sticv3Handler::printReport()
{
        printf(">> STICv3::sticv3Handler report\n");
        printf(" events received\n");
	    printf("  %10u\n", nEvent);
		printf(" events passed\n");
        printf("  %10u (%4.1f%%)\n", nPassed, 100.0*nPassed/nEvent);
	
}

int Sticv3Handler::compensateCoarse(unsigned coarse, unsigned long long frameID)
{
	int coarse_i = coarse;
	long long clocksElapsed = (frameID % 256) *1024*4ULL;	// Periodic reset every 256 frames
	long long wrapNumber	= clocksElapsed / 32767;
	long long wrapRemainder	= clocksElapsed % 32767;
	if(coarse_i < wrapRemainder) coarse_i += 32767;
		coarse_i -= wrapRemainder;

	return coarse_i;
}
