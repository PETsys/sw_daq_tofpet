#include "CoarseSorter.hpp"
#include <vector>
#include <algorithm>
#include <functional>

using namespace DAQ::Common;
using namespace DAQ::Core;
using namespace std;

CoarseSorter::CoarseSorter(EventSink<RawPulse> *sink) :
	OverlappedEventHandler<RawPulse, RawPulse>(sink)
{
	nSingleRead = 0;
}

struct SortEntry {
	long long frameID;
        long long time;
        unsigned index;
};

static bool operator< (SortEntry lhs, SortEntry rhs) { return (lhs.frameID < rhs.frameID) ||  (lhs.time < rhs.time); }


EventBuffer<RawPulse> * CoarseSorter::handleEvents (EventBuffer<RawPulse> *inBuffer)
{
	long long tMin = inBuffer->getTMin();
	long long tMax = inBuffer->getTMax();
	unsigned nEvents =  inBuffer->getSize();
	EventBuffer<RawPulse> * outBuffer = new EventBuffer<RawPulse>(nEvents);
	outBuffer->setTMin(tMin);
	outBuffer->setTMax(tMax);	
	u_int32_t lSingleRead = 0;

	vector<SortEntry> sortList;
	sortList.reserve(nEvents);
	
	for(unsigned i = 0; i < nEvents; i++) {
		RawPulse &p = inBuffer->get(i);
		if(p.time < tMin || p.time >=  tMax) continue;
		long frameTime = 1024L * p.T;
		SortEntry entry = { p.time / frameTime, p.time / (overlap/2), i };
		sortList.push_back(entry);
	}
	
	sort(sortList.begin(), sortList.end());
	
	for(unsigned j = 0; j < sortList.size(); j++) {
		unsigned i = sortList[j].index;
		RawPulse &p = inBuffer->get(i);
		outBuffer->push(p);
		lSingleRead++;
	}
	atomicAdd(nSingleRead, lSingleRead);
	//fprintf(stderr, "CoarseSorter:: %4u in, %4u sorted, %4d out\n", nEvents, sortList.size(), lSingleRead);
	
	delete inBuffer;
	return outBuffer;
}

void CoarseSorter::report()
{
	u_int32_t nTotal = nSingleRead;
	fprintf(stderr, ">> CoarseSorter report\n");
	fprintf(stderr, " events passed\n");
	fprintf(stderr, "  %10u\n", nSingleRead);
	OverlappedEventHandler<RawPulse, RawPulse>::report();
}
