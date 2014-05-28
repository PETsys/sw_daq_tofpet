#include "PulseFilter.hpp"
#include <vector>

using namespace DAQ::Common;
using namespace DAQ::Core;
using namespace std;

PulseFilter::PulseFilter(float eMin, float eMax, EventSink<Pulse> *sink) :
	OverlappedEventHandler<Pulse, Pulse>(sink),
	eMin(eMin), eMax(eMax)
{
	nPassed = 0;
}
 
EventBuffer<Pulse> * PulseFilter::handleEvents (EventBuffer<Pulse> *inBuffer)
{
	long long tMin = inBuffer->getTMin();
	long long tMax = inBuffer->getTMax();
	unsigned nEvents =  inBuffer->getSize();
	
	
	u_int32_t lPassed = 0;

	unsigned N = inBuffer->getSize();
	for(unsigned i = 0; i < N; i++) {
		Pulse &e = inBuffer->get(i);
		if(e.time < tMin || e.time >=  tMax) continue;		
		
		if(e.energy < eMin || e.energy > eMax)  {
			e.time = -1;
			continue;
		}
		lPassed++;

	}

	return inBuffer;
	
}

void PulseFilter::report()
{
	u_int32_t nTotal = nPassed;
	fprintf(stderr, ">> PulseFilter report\n");
	fprintf(stderr, " events passed\n");
	fprintf(stderr, "  %10u\n", nPassed);
 	OverlappedEventHandler<Pulse, Pulse>::report();
}
