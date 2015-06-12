#include "RawPulseWriter.hpp"
#include <vector>
#include <algorithm>
#include <functional>

using namespace DAQ::Common;
using namespace DAQ::Core;
using namespace std;



RawPulseWriterHandler::RawPulseWriterHandler(AbstractRawPulseWriter *writer, EventSink<RawPulse> *sink) :
	OverlappedEventHandler<RawPulse, RawPulse>(sink, true),
	writer(writer)
{
	nSingleRead = 0;
}




EventBuffer<RawPulse> * RawPulseWriterHandler::handleEvents (EventBuffer<RawPulse> *inBuffer)
{
	long long tMin = inBuffer->getTMin();
	long long tMax = inBuffer->getTMax();
	
	u_int32_t lSingleRead = 0;
	unsigned N = inBuffer->getSize();
	for(unsigned i = 0; i < N; i++) {
		RawPulse &p = inBuffer->get(i);
		if(p.time < tMin || p.time >=  tMax) continue;
		writer->addEvent(p);
		lSingleRead++;
	}
	atomicAdd(nSingleRead, lSingleRead);
	return inBuffer;
}

void RawPulseWriterHandler::report()
{
	u_int32_t nTotal = nSingleRead;
	fprintf(stderr, ">> RawPulseWriterHandler report\n");
	fprintf(stderr, " events passed\n");
	fprintf(stderr, "  %10u\n", nSingleRead);
	OverlappedEventHandler<RawPulse, RawPulse>::report();
}

AbstractRawPulseWriter::~AbstractRawPulseWriter()
{
}

NullRawPulseWriter::NullRawPulseWriter()
{
}

NullRawPulseWriter::~NullRawPulseWriter()
{
}

void NullRawPulseWriter::openStep(float step1, float step2)
{
}

void NullRawPulseWriter::closeStep()
{
}

void NullRawPulseWriter::addEvent(RawPulse &p)
{
}