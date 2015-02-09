#include "sticv3Handler.hpp"
#include <stdio.h>

using namespace DAQ::Core;
using namespace DAQ::STICv3;

Sticv3Handler::Sticv3Handler() 
{
	
}

 bool Sticv3Handler::handleEvent(RawPulse &raw, Pulse &pulse)
{
	// if event is good return true
	// if event is not good (see examples below), return false
	
	
	if(raw.feType != RawPulse::STIC) return false;

        pulse.raw = raw;                
        // WARNING: rounding sensitive!
        pulse.time = raw.time + (raw.d.stic.tfine * raw.d.stic.T / (4*32));  
        pulse.timeEnd = raw.timeEnd;
        pulse.region = raw.region;
        pulse.channelID = raw.channelID;
        pulse.energy = 1E-3*(pulse.timeEnd - pulse.time);

	//printf("%lld %lld %f\n", pulse.time, pulse.timeEnd, pulse.energy);


	return true; 

}


void Sticv3Handler::printReport()
{
        printf(">> STICv3::sticv3Handler report\n");
        printf(" events received\n");
		//...
		//...
     
	
}
