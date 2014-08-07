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


	// extract data from rawPulse as you see fit


	return true; 

}


void Sticv3Handler::printReport()
{
        printf(">> STICv3::sticv3Handler report\n");
        printf(" events received\n");
		//...
		//...
     
	
}
