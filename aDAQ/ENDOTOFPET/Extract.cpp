#include "Extract.hpp"
#include <stdio.h>

using namespace DAQ::Common;
using namespace DAQ::Core;
using namespace DAQ::TOFPET;
using namespace DAQ::STICv3;
using namespace DAQ::DSIPM;
using namespace DAQ::ENDOTOFPET;

Extract::Extract(DAQ::TOFPET::P2Extract *tofpetH, DAQ::STICv3::Sticv3Handler *sticv3H, DAQ::DSIPM::DsipmHandler *dsipmH,  EventSink<Pulse> *sink) :
	tofpetH(tofpetH), sticv3H(sticv3H), dsipmH(dsipmH), 
	OverlappedEventHandler<RawPulse, Pulse>(sink)
{
	nEvent = 0;
	nPassed = 0;
}



EventBuffer<Pulse> * Extract::handleEvents (EventBuffer<RawPulse> *inBuffer)
{
	long long tMin = inBuffer->getTMin();
	long long tMax = inBuffer->getTMax();
	unsigned nEvents =  inBuffer->getSize();
	EventBuffer<Pulse> * outBuffer = new EventBuffer<Pulse>(nEvents, inBuffer);
	outBuffer->setTMin(tMin);
	outBuffer->setTMax(tMax);	


	u_int32_t lEvent = 0;
	u_int32_t lPassed = 0;

	for(unsigned i = 0; i < nEvents; i++) {
		
		
		RawPulse &raw = inBuffer->get(i);
	   	if(raw.time < tMin || raw.time >= tMax)continue; 
		lEvent++;  
		Pulse &p = outBuffer->getWriteSlot();
		if (raw.feType == RawPulse::TOFPET &&  tofpetH != NULL){
			if (tofpetH->handleEvent(raw, p)){
				outBuffer->pushWriteSlot();
				lPassed++;
			}
		}
		else if (raw.feType == RawPulse::STIC &&  sticv3H != NULL){
			if (sticv3H->handleEvent(raw, p)){
				outBuffer->pushWriteSlot();
				lPassed++;
			}
		}
		else if (raw.feType == RawPulse::DSIPM && dsipmH != NULL){
			if (dsipmH->handleEvent(raw, p)){
				outBuffer->pushWriteSlot();	
				lPassed++;
			}
		}
  
	}
	atomicAdd(nEvent, lEvent);
	atomicAdd(nPassed, lPassed);

	return outBuffer;
}

	void Extract::report()
{
        printf(">> ENDOTOFPET::Extract report\n");
        printf(" events received\n");
        printf("  %10u\n", nEvent);
        printf(" events passed\n");
        printf("  %10u (%4.1f%%)\n", nPassed, 100.0*nPassed/nEvent);
		if (tofpetH != NULL){
			tofpetH->printReport();
		}
		if (sticv3H != NULL){
			sticv3H->printReport();
		}
		if (dsipmH != NULL){
			dsipmH->printReport();
		}	
	OverlappedEventHandler<RawPulse, Pulse>::report();
}
