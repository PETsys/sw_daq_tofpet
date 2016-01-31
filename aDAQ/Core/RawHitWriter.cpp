#include "RawHitWriter.hpp"
#include <vector>
#include <algorithm>
#include <functional>

using namespace DAQ::Common;
using namespace DAQ::Core;
using namespace std;



RawHitWriterHandler::RawHitWriterHandler(AbstractRawHitWriter *writer, EventSink<RawHit> *sink) :
	OverlappedEventHandler<RawHit, RawHit>(sink, true),
	writer(writer)
{
	nSingleRead = 0;
}




EventBuffer<RawHit> * RawHitWriterHandler::handleEvents (EventBuffer<RawHit> *inBuffer)
{
	long long tMin = inBuffer->getTMin();
	long long tMax = inBuffer->getTMax();
	
	u_int32_t lSingleRead = writer->addEventBuffer(tMin, tMax, inBuffer);
	atomicAdd(nSingleRead, lSingleRead);
	return inBuffer;
}

void RawHitWriterHandler::report()
{
	u_int32_t nTotal = nSingleRead;
	fprintf(stderr, ">> RawHitWriterHandler report\n");
	fprintf(stderr, " events passed\n");
	fprintf(stderr, "  %10u\n", nSingleRead);
	OverlappedEventHandler<RawHit, RawHit>::report();
}

AbstractRawHitWriter::~AbstractRawHitWriter()
{
}

NullRawHitWriter::NullRawHitWriter()
{
}

NullRawHitWriter::~NullRawHitWriter()
{
}

void NullRawHitWriter::openStep(float step1, float step2)
{
}

void NullRawHitWriter::closeStep()
{
}

u_int32_t NullRawHitWriter::addEventBuffer(long long tMin, long long tMax, EventBuffer<RawHit> *inBuffer)
{
	return 0;
}
