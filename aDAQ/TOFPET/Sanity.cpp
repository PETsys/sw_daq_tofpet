#include "Sanity.hpp"
#include <math.h>
#include <stdio.h>
#include <assert.h>

using namespace DAQ::Common;
using namespace DAQ::Core;
using namespace DAQ::TOFPET;

Sanity::Sanity(float deadtimeWindow, EventSink<RawPulse> *sink) : 
	deadtimeWindow((long long)(deadtimeWindow * 1E12)),
	OverlappedEventHandler<RawPulse, RawPulse>(sink)
{
	nEvent = 0;
	nPassed = 0;
	nDeadtime = 0;
	nOverlapped = 0;
}

EventBuffer<RawPulse> * Sanity::handleEvents (EventBuffer<RawPulse> *inBuffer)
{
	long long tMin = inBuffer->getTMin();
	long long tMax = inBuffer->getTMax();
	unsigned nEvents =  inBuffer->getSize();
	
	u_int32_t lEvent = 0;
	u_int32_t lPassed = 0;
	u_int32_t lDeadtime = 0;
	u_int32_t lOverlapped = 0;

	vector<bool> deadtime(nEvents, false);
	vector<bool> overlapped(nEvents, false);

	for(unsigned i = 0; i < nEvents; i++) {
		RawPulse &raw1 = inBuffer->get(i);
		if(raw1.time < tMin || raw1.time >= tMax) continue;

		long long T = raw1.d.tofpet.T;

		for(unsigned j = i+1; j < nEvents; j++) {
			RawPulse &raw2 = inBuffer->get(j);
			if(raw2.time >= (raw1.timeEnd + overlap + deadtimeWindow)) break;
			
			if(raw1.channelID != raw2.channelID) continue;

			
			// Deadtime: event begins shortly after another ends	
			if((raw2.time > (raw1.time - T)) && (raw2.time < (raw1.timeEnd + deadtimeWindow))) {
				deadtime[j] = true;
			}
			if((raw1.time > (raw2.time - T)) && (raw1.time < (raw2.timeEnd + deadtimeWindow))) {
				deadtime[i] = true;
			}

			// Overalpped: event starts or stops in the middle of another event
			if((raw1.time > (raw2.time - T)) && (raw1.time < (raw2.timeEnd + T))) {
				overlapped[i] = true;
				overlapped[j] = true;
			}
			if((raw1.timeEnd > (raw2.time - T)) && (raw1.timeEnd < (raw2.timeEnd + T))) {
				overlapped[i] = true;
				overlapped[j] = true;
			}
	
			if((raw2.time > (raw1.time - T)) && (raw2.time < (raw1.timeEnd + T))) {
				overlapped[i] = true;
				overlapped[j] = true;
			}
			if((raw2.timeEnd > (raw1.time - T)) && (raw2.timeEnd < (raw1.timeEnd - T))) {
				overlapped[i] = true;
				overlapped[j] = true;
			}

/*			printf("A = %8.1f to %8.1f %c %c, B = %8.1f to %8.1f %c %c\n", 
				1E-3*(raw1.time-raw1.time), 1E-3*(raw1.timeEnd-raw1.time), 
				deadtime[i] ? 'D' : ' ', overlapped[i] ? 'O' : ' ',
				1E-3*(raw2.time-raw1.time), 1E-3*(raw2.timeEnd-raw1.time),
				deadtime[j] ? 'D' : ' ', overlapped[j] ? 'O' : ' '
			);*/
		}
		
	}	

	for(unsigned i = 0; i < nEvents; i++) {
		RawPulse &raw = inBuffer->get(i);
		if(raw.time < tMin || raw.time >= tMax) continue;
		lEvent += 1;

		if(deadtime[i]) {
			raw.time = -1;
			lDeadtime += 1;
		}

		if(overlapped[i]) {
			raw.time = -1;
			lOverlapped += 1;
		}

		if(!deadtime[i] && !overlapped[i])
			lPassed += 1;

	}

	atomicAdd(nEvent, lEvent);
	atomicAdd(nPassed, lPassed);
	atomicAdd(nDeadtime, lDeadtime);
	atomicAdd(nOverlapped, lOverlapped);
	
	return inBuffer;
}


void Sanity::report()
{
        printf(">> Sanity report\n");
        printf(" events received\n");
        printf("  %10u\n", nEvent);
        printf(" events discarded\n");
        printf("  %10u (%4.1f%%) deadtime\n", nDeadtime, 100.0*nDeadtime/nEvent);
	printf("  %10u (%4.1f%%) overlapped\n", nOverlapped, 100.0*nOverlapped/nEvent);
        printf(" events passed\n");
        printf("  %10u (%4.1f%%)\n", nPassed, 100.0*nPassed/nEvent);
	
	OverlappedEventHandler<RawPulse, RawPulse>::report();
}
