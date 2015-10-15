#include "sticv3Handler.hpp"
#include <stdio.h>

using namespace DAQ::Core;
using namespace DAQ::STICv3;
using namespace DAQ::Common;

Sticv3Handler::Sticv3Handler() 
{
	nPassed=0;
}

 bool Sticv3Handler::handleEvent(RawPulse &raw, Pulse &pulse)
{
	// if event is good return true
	// if event is not good (see examples below), return false
	
	atomicAdd(nEvent, 1);
	if(raw.feType != RawPulse::STIC) return false;

        pulse.raw = &raw;
        // WARNING: rounding sensitive!
        pulse.time = raw.time + (raw.d.stic.tfine * raw.T / (4*32));  
        pulse.timeEnd = raw.timeEnd;
        pulse.region = raw.region;
        pulse.channelID = raw.channelID;
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
