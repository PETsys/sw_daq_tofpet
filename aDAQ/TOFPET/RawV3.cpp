#include "RawV3.hpp"
#include <Common/Constants.hpp>

#include <algorithm>
#include <functional>
#include <math.h>
#include <set>
#include <limits.h>
#include <iostream>
#include <assert.h>

using namespace std;
using namespace DAQ::Core;
using namespace DAQ::TOFPET;

static const unsigned outBlockSize = EVENT_BLOCK_SIZE;
static const unsigned maxEventsPerFrame = 16*1024;

RawReaderV3::RawReaderV3(FILE *dataFile, float T, unsigned long long eventsBegin, unsigned long long eventsEnd, EventSink<RawPulse> *sink)
	: EventSource<RawPulse>(sink), dataFile(dataFile), T(T)
{
	this->eventsBegin = eventsBegin;
	this->eventsEnd = eventsEnd;
	start();
}

RawReaderV3::~RawReaderV3()
{
}




void RawReaderV3::run()
{

	unsigned nWraps = 0;
	
	EventBuffer<RawPulse> *outBuffer = NULL;
	
	int nEventsInFrame = 0;
	
	long long tMax = 0, lastTMax = 0;

	sink->pushT0(0);
	
	long long minFrameID = LLONG_MAX;
	long long maxFrameID = LLONG_MIN;
	
	fprintf(stderr, "Reading %llu to %llu\n", eventsBegin, eventsEnd);
	fseek(dataFile, eventsBegin * sizeof(RawEventV3), SEEK_SET);
	
	int maxReadBlock = 1024*1024;
	RawEventV3 *rawEvents = new RawEventV3[maxReadBlock];

	unsigned long long readPointer = eventsBegin;
	while (readPointer < eventsEnd) {
		unsigned long long count = eventsEnd - readPointer;
		if(count > maxReadBlock) count = maxReadBlock;
		int r = fread(rawEvents, sizeof(RawEventV3), count, dataFile);
		if(r <= 0) break;
		readPointer += r;
	
		//printf("events extracted= %lld\n",readPointer);
	
		for(int j = 0; j < r; j++) {
			RawEventV3 &r = rawEvents[j];

			if(outBuffer == NULL) {
				outBuffer = new EventBuffer<RawPulse>(outBlockSize);
			}

			RawPulse &p = outBuffer->getWriteSlot();
			
			uint64_t frameID = (r >> 92) & 0xFFFFFFFFFULL;
			uint64_t asicID = (r >> 78) & 0x3FFF;
			uint64_t channelID = (r >> 72) & 0x3F;
			uint64_t tacID = (r >> 70) & 0x3;
			uint64_t tCoarse = (r >> 60) & 0x3FF;
			uint64_t eCoarse = (r >> 50) & 0x3FF;
			uint64_t tFine = (r >> 40) & 0x3FF;
			uint64_t eFine = (r >> 30) & 0x3FF;
			uint64_t channelIdleTime = ((r >> 15) & 0x7FFF) * 8192;
			uint64_t tacIdleTime = ((r >> 0) & 0x7FFF) * 8192;
			
			if(eCoarse < (tCoarse - 256)) eCoarse += 1024;

// Carefull with the float/double/integer conversions here..
			p.T = T * 1E12;
			p.time = (1024LL * frameID + tCoarse) * p.T;
			p.timeEnd = (1024LL * frameID + eCoarse) * p.T;
			if((p.timeEnd - p.time) < -256*p.T) p.timeEnd += (1024LL * p.T);
			p.channelID = (64 * asicID) + channelID;
			p.channelIdleTime = channelIdleTime;
			p.feType = RawPulse::TOFPET;
			p.d.tofpet.tac = tacID;
			p.d.tofpet.tcoarse = tCoarse;
			p.d.tofpet.ecoarse = eCoarse;
			p.d.tofpet.tfine =  tFine;
			p.d.tofpet.efine = eFine;
			p.channelIdleTime = channelIdleTime;
			p.d.tofpet.tacIdleTime = tacIdleTime;
		
			if(frameID < minFrameID) minFrameID = frameID;
			if(frameID > maxFrameID) maxFrameID = frameID;
		
			if(p.time > tMax)
				tMax = p.time;
		
			outBuffer->pushWriteSlot();
		
			if(outBuffer->getSize() >= outBlockSize) {
				outBuffer->setTMin(lastTMax);
				outBuffer->setTMax(tMax);		
				sink->pushEvents(outBuffer);
				outBuffer = NULL;
			}
		}
	}
	
	delete [] rawEvents;

	
	if(outBuffer != NULL) {
		outBuffer->setTMin(lastTMax);
		outBuffer->setTMax(tMax);		
		sink->pushEvents(outBuffer);
		outBuffer = NULL;
		
	}
	
	sink->finish();
	
	fprintf(stderr, "RawReaderV3 report\n");
	fprintf(stderr, "\t%16lld minFrameID\n", minFrameID);
	fprintf(stderr, "\t%16lld maxFrameID\n", maxFrameID);
	sink->report();
}

RawScannerV3::RawScannerV3(FILE *indexFile) :
	steps(vector<Step>())
{
	float step1;
	float step2;
	unsigned long stepBegin;
	unsigned long stepEnd;

	
	while(fscanf(indexFile, "%f %f %llu %llu\n", &step1, &step2, &stepBegin, &stepEnd) == 4) {
		Step step = { step1, step2, stepBegin, stepEnd };
		steps.push_back(step);
	}
}

RawScannerV3::~RawScannerV3()
{
}

int RawScannerV3::getNSteps()
{
	return steps.size();
}


void RawScannerV3::getStep(int stepIndex, float &step1, float &step2, unsigned long long &eventsBegin, unsigned long long &eventsEnd)
{
	Step &step = steps[stepIndex];
	step1 = step.step1;
	step2 = step.step2;
	eventsBegin = step.eventsBegin;
	eventsEnd = step.eventsEnd;
}


RawWriterV3::RawWriterV3(char *fileNamePrefix)
{
	char dataFileName[512];
	char indexFileName[512];
	sprintf(dataFileName, "%s.raw3", fileNamePrefix);
	sprintf(indexFileName, "%s.idx3", fileNamePrefix);

	outputDataFile = fopen(dataFileName, "wb");
	outputIndexFile = fopen(indexFileName, "w");
	assert(outputDataFile != NULL);
	assert(outputIndexFile != NULL);
	stepBegin = 0;
}

RawWriterV3::~RawWriterV3()
{
 	fclose(outputDataFile);
 	fclose(outputIndexFile);
}

void RawWriterV3::openStep(float step1, float step2)
{
	this->step1 = step1;
	this->step2 = step2;
	stepBegin = ftell(outputDataFile) / sizeof(DAQ::TOFPET::RawEventV3);
}

void RawWriterV3::closeStep()
{
	long stepEnd = ftell(outputDataFile) / sizeof(DAQ::TOFPET::RawEventV3);	
	fprintf(outputIndexFile, "%f %f %ld %ld\n", step1, step2, stepBegin, stepEnd);
	fflush(outputDataFile);
	fflush(outputIndexFile);
}

void RawWriterV3::addEvent(RawPulse &p)
{
	uint64_t frameID = p.time / (1024L * p.T);
	RawEventV3 eventOut = 0;
	eventOut |= RawEventV3(frameID & 0xFFFFFFFFFULL) << 92;
	eventOut |= RawEventV3((p.channelID / 64) & 0x3FFF) << 78;
	eventOut |= RawEventV3((p.channelID % 64) & 0x3F) << 72;
	eventOut |= RawEventV3(p.d.tofpet.tac & 0x3) << 70;
	eventOut |= RawEventV3(p.d.tofpet.tcoarse & 0x3FF) << 60;
	eventOut |= RawEventV3(p.d.tofpet.ecoarse & 0x3FF) << 50;
	eventOut |= RawEventV3(p.d.tofpet.tfine & 0x3FF) << 40;
	eventOut |= RawEventV3(p.d.tofpet.efine & 0x3FF) << 30;
	eventOut |= RawEventV3(uint16_t(p.channelIdleTime / 8192) & 0x7FFF) << 15;
	eventOut |= RawEventV3(uint16_t(p.d.tofpet.tacIdleTime / 8192) & 0x7FFF) << 0;

	fwrite(&eventOut, sizeof(eventOut), 1, outputDataFile);
}