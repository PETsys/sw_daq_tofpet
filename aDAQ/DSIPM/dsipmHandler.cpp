#include "dsipmHandler.hpp"
#include <stdio.h>

using namespace DAQ::Core;
using namespace DAQ::DSIPM;

DsipmHandler::DsipmHandler() 
{
	
}

bool DsipmHandler::handleEvent(RawHit &raw, Hit &pulse)
{
	// if event is good return true
	// if event is not good (see example below), return false
   
	
	if(raw.feType != RawHit::DSIPM) return false;


	// extract data from rawHit as you see fit


	return true; 

}


void DsipmHandler::printReport()
{
        printf(">> DSIPM::dsipmHandler report\n");
        printf(" events received\n");
		//...
		//...
     
	
}
