#include "dsipmHandler.hpp"
#include <stdio.h>

using namespace DAQ::Core;
using namespace DAQ::DSIPM;

DsipmHandler::DsipmHandler() 
{
	
}

bool DsipmHandler::handleEvent(RawPulse &raw, Pulse &pulse)
{
	// if event is good return true
	// if event is not good (see example below), return false
   
	
	if(raw.feType != RawPulse::DSIPM) return false;


	// extract data from rawPulse as you see fit


	return true; 

}


void DsipmHandler::printReport()
{
        printf(">> DSIPM::dsipmHandler report\n");
        printf(" events received\n");
		//...
		//...
     
	
}
