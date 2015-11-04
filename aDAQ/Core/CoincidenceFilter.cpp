#include "CoincidenceFilter.hpp"
#include <Common/Constants.hpp>
#include <vector>
#include <assert.h>

using namespace std;
using namespace DAQ::Common;
using namespace DAQ::Core;

CoincidenceFilter::CoincidenceFilter(const char *mapFileName, float cWindow, float minToT,
	EventSink<RawPulse> *sink)
	: OverlappedEventHandler<RawPulse, RawPulse>(sink), cWindow((long long)(cWindow*1E12)), minToT((long long)(minToT*1E12))
{
	regionMap = std::vector<short>(SYSTEM_NCHANNELS, -1);
	FILE *mapFile = fopen(mapFileName, "r");
	assert(mapFile != NULL);
	
	int channel;
	int region;
	int xi;
	int yi;
	float x;
	float y;
	float z;
	int hv;
	
	int nLoaded = 0;
	while(fscanf(mapFile, "%d\t%d\t%d\t%d\t%f\t%f\t%f\t%d\n", &channel, &region, &xi, &yi, &x, &y, &z, &hv) == 8) {
		assert(channel < SYSTEM_NCHANNELS);
		regionMap[channel] = region;
		nLoaded++;
	}
	fclose(mapFile);
	fprintf(stderr, "%d valid channel coordinates found\n", nLoaded);	
	
	nEventsIn = 0;
	nTriggersIn = 0;
	nEventsOut = 0;
}

CoincidenceFilter::~CoincidenceFilter()
{
}

void CoincidenceFilter::report()
{
	fprintf(stderr, ">> CoincidenceFilter report\n");
	fprintf(stderr, "  cWindow: %lld\n", cWindow);
	fprintf(stderr, "  min ToT: %lld\n", minToT);
	fprintf(stderr, "  %10u events received\n", nEventsIn);
	fprintf(stderr, "  %10u (%5.1f%%) events met minimum ToT \n", nTriggersIn, 100.0 * nTriggersIn / nEventsIn);
	fprintf(stderr, "  %10u (%5.1f%%) events passed\n", nEventsOut, 100.0 * nEventsOut / nEventsIn);
	OverlappedEventHandler<RawPulse, RawPulse>::report();
}

EventBuffer<RawPulse> * CoincidenceFilter::handleEvents(EventBuffer<RawPulse> *inBuffer)
{
	long long tMin = inBuffer->getTMin();
	long long tMax = inBuffer->getTMax();
	unsigned nEvents =  inBuffer->getSize();
	
	u_int32_t lEventsIn = 0;
	u_int32_t lTriggersIn = 0;
	u_int32_t lEventsOut = 0;
	
	vector<bool> meetsMinToT(nEvents, false);
	vector<bool> coincidenceMatched(nEvents, false);
	for(unsigned i = 0; i < nEvents; i++) {
		RawPulse &p1 = inBuffer->get(i);

		if((p1.timeEnd - p1.time) >= minToT) {
			meetsMinToT[i] = true;
		}
		else {
			meetsMinToT[i] = false;
			continue;
		}
		
		for (unsigned j = i+1; j < nEvents; j++) {
			RawPulse &p2 = inBuffer->get(j);
			if((p2.time - p1.time) > (overlap + cWindow)) break;		// No point in looking further
			if((p2.timeEnd - p2.time) < minToT) continue;			// Does not meet min ToT
			if(tAbs(p2.time - p1.time) > cWindow) continue;			// Does not meet cWindow
			if(regionMap[p1.channelID] == regionMap[p2.channelID]) continue;// Same region, no coincidence

			coincidenceMatched[i] = true;
			coincidenceMatched[j] = true;
		}
	}
	
	vector<bool> accepted(nEvents, false);	
	const long long dWindow = 100000; // 100 ns acceptance window for events which come after the first
	for(unsigned i = 0; i < nEvents; i++) {
		RawPulse &p1 = inBuffer->get(i);

		if(coincidenceMatched[i]) {
			accepted[i] = true;
		}
		else {
			continue;
		}

		for(unsigned j = i; j > 0; j--) {	// Look for events before p1
			RawPulse &p2 = inBuffer->get(j);
			if(p1.region != p2.region) continue;			// Only search within trigger's region
			if(p2.time < (p1.time - cWindow - overlap)) break;	// No point in looking further
			if(p2.time < (p1.time - cWindow)) continue;		// Doesn't meet cWindow
			accepted[j] = true;
		}
		
		for (unsigned j = i; j < nEvents; j++) {// Look for events after p1
			RawPulse &p2 = inBuffer->get(j);
			if(p1.region != p2.region) continue;			// Only search within trigger's region
			if(p2.time > (p1.time + dWindow + overlap)) break;	// No point in looking further
			if(p2.time > (p1.time + dWindow)) continue;		// Doesn't meet dWindow
			accepted[j] = true;
		}
	}
	
	// Filter unaccepted events by setting their time to -1
	for(unsigned i = 0; i < nEvents; i++) {
		RawPulse &p1 = inBuffer->get(i);
		if(p1.time < tMin || p1.time >= tMax) {
			p1.time = -1;
			continue;
		}
		lEventsIn += 1;
		
		if(meetsMinToT[i]) lTriggersIn += 1;
		
		if (!accepted[i]) {
			p1.time = -1;
			continue;
		}
		lEventsOut += 1;
	}
	
	atomicAdd(nEventsIn, lEventsIn);
	atomicAdd(nTriggersIn, lTriggersIn);
	atomicAdd(nEventsOut, lEventsOut);
	return inBuffer;
}
