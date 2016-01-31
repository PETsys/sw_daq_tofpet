#include "RawV3.hpp"
#include <Common/Constants.hpp>

#include <algorithm>
#include <functional>
#include <math.h>
#include <set>
#include <limits.h>
#include <iostream>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

using namespace std;
using namespace DAQ::Core;
using namespace DAQ::TOFPET;

static const unsigned outBlockSize = EVENT_BLOCK_SIZE;
static const unsigned maxEventsPerFrame = 16*1024;

RawReaderV3::RawReaderV3(char *dataFilePrefix, float T, unsigned long long eventsBegin, unsigned long long eventsEnd, float deltaTime, bool onlineMode, EventSink<RawHit> *sink)
	: EventSource<RawHit>(sink),T(T), deltaTime(deltaTime), onlineMode(onlineMode)
{
	char dataFileName[512];
	sprintf(dataFileName, "%s.raw3", dataFilePrefix);
	dataFile = fopen(dataFileName, "rb");
	if(dataFile == NULL) {
		int e = errno;
		fprintf(stderr, "Could not open '%s for reading' : %d %s\n", dataFileName, e, strerror(e));
		exit(e);
	}
	
	this->eventsBegin = eventsBegin;
	this->eventsEnd = eventsEnd;
	start();
}

RawReaderV3::~RawReaderV3()
{
	fclose(dataFile);
}




void RawReaderV3::run()
{


	

	unsigned nWraps = 0;
	
	EventBuffer<RawHit> *outBuffer = NULL;
	
	int nEventsInFrame = 0;
	
	long long tMax = 0, lastTMax = 0;

	sink->pushT0(0);
   	
	int maxReadBlock = 1024*1024;
	RawEventV3 *rawEvents = new RawEventV3[maxReadBlock];

	if(onlineMode){		
		eventsBegin=eventsEnd;
		RawEventV3 Event;
		fseek(dataFile, -sizeof(RawEventV3), SEEK_END);
		int r = fread(&Event, sizeof(RawEventV3), 1, dataFile);

		eventsEnd=  ftell(dataFile) / sizeof(DAQ::TOFPET::RawEventV3);

		uint64_t frameID = (Event >> 92) & 0xFFFFFFFFFULL;
		uint64_t tCoarse = (Event >> 60) & 0x3FF;
		long long last_time= (1024LL * frameID + tCoarse) * T * 1E12;
		
		
		//long long nEventsStart=  ftell(dataFile) / sizeof(DAQ::TOFPET::RawEventV3);
		if(deltaTime!=-1){
			fseek(dataFile, eventsBegin * sizeof(RawEventV3), SEEK_SET); 
			r = fread(&Event, sizeof(RawEventV3), 1, dataFile);		
			frameID = (Event >> 92) & 0xFFFFFFFFFULL;
			tCoarse = (Event >> 60) & 0x3FF;
			long long start_time= (1024LL * frameID + tCoarse) * T * 1E12;		
			double runningTime= double(last_time- start_time)/1.0e12;
			unsigned long nEvents =  deltaTime*(eventsEnd-eventsBegin)/runningTime;		
			if(eventsBegin+nEvents<eventsEnd)eventsBegin=eventsEnd-nEvents;
		}
	}
	fprintf(stderr, "Reading %llu to %llu\n", eventsBegin, eventsEnd);

	fseek(dataFile, eventsBegin*sizeof(RawEventV3), SEEK_SET);
	unsigned long long readPointer = eventsBegin;
	unsigned long long nBlocks = 0;
	while (readPointer < eventsEnd) {
		unsigned long long count = eventsEnd - readPointer;
		if(count > maxReadBlock) count = maxReadBlock;
		int r = fread(rawEvents, sizeof(RawEventV3), count, dataFile);
		if(r <= 0) break;
		readPointer += r;
		
		//printf("events extracted= %lld\r",readPointer-eventsBegin);
	
		for(int j = 0; j < r; j++) {
			RawEventV3 &r = rawEvents[j];

			if(outBuffer == NULL) {
				outBuffer = new EventBuffer<RawHit>(outBlockSize, NULL);
			}

			RawHit &p = outBuffer->getWriteSlot();
			
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
			
// Carefull with the float/double/integer conversions here..
			long long pT = T * 1E12;
			p.time = (1024LL * frameID + tCoarse) * pT;
			p.timeEnd = (1024LL * frameID + eCoarse) * pT;
			if((p.timeEnd - p.time) < -256*pT) p.timeEnd += (1024LL * pT);
			p.channelID = (64 * asicID) + channelID;
			p.channelIdleTime = channelIdleTime;
			p.feType = RawHit::TOFPET;
			p.d.tofpet.tac = tacID;
			p.d.tofpet.tcoarse = tCoarse;
			p.d.tofpet.ecoarse = eCoarse;
			p.d.tofpet.tfine =  tFine;
			p.d.tofpet.efine = eFine;
			p.channelIdleTime = channelIdleTime;
			p.d.tofpet.tacIdleTime = tacIdleTime;
// 			fprintf(stderr, "%4d %d >> %4d %4d %4d %4d \n",
// 				p.channelID, p.d.tofpet.tac,
// 				p.d.tofpet.tcoarse,
// 				p.d.tofpet.ecoarse,
// 				p.d.tofpet.tfine,
// 				p.d.tofpet.efine
// 				);

			if(p.time > tMax)
				tMax = p.time;
		
			outBuffer->pushWriteSlot();
		
			if(outBuffer->getSize() >= outBlockSize) {
				outBuffer->setTMin(lastTMax);
				outBuffer->setTMax(tMax);
				lastTMax=tMax;
				sink->pushEvents(outBuffer);
				outBuffer = NULL;
				nBlocks += 1;
			}
		}
	}
	
	delete [] rawEvents;

	
	if(outBuffer != NULL) {
		outBuffer->setTMin(lastTMax);
		outBuffer->setTMax(tMax);		
		sink->pushEvents(outBuffer);
		outBuffer = NULL;
		nBlocks += 1;
		
	}
	
	sink->finish();
	
	fprintf(stderr, "RawReaderV3 report\n");
	fprintf(stderr, " %10lu events processed\n", eventsEnd - eventsBegin);
	fprintf(stderr, " %10lu blocks processed\n",  nBlocks);
	fprintf(stderr, " %10lu events/block\n",  (eventsEnd - eventsBegin)/nBlocks);
	sink->report();
}

RawScannerV3::RawScannerV3(char *indexFilePrefix) :
	steps(vector<Step>())
{
	float step1=0;
	float step2=0;
	unsigned long stepBegin=0;
	unsigned long stepEnd=0;
	
	char indexFileName[512];
	sprintf(indexFileName, "%s.idx3", indexFilePrefix);
	indexFile = fopen(indexFileName, "rb");
	if(indexFile == NULL) {
		int e = errno;
		fprintf(stderr, "Could not open '%s for reading' : %d %s\n", indexFileName, e, strerror(e));
		exit(e);
	}

	while(fscanf(indexFile, "%f %f %llu %llu\n", &step1, &step2, &stepBegin, &stepEnd) == 4) {
		Step step = { step1, step2, stepBegin, stepEnd };
		steps.push_back(step);
	}
	if(steps.size()==0){
		Step step = { step1, step2, stepBegin, stepEnd };
		steps.push_back(step);
	}
}

RawScannerV3::~RawScannerV3()
{
	fclose(indexFile);
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
	if(outputDataFile == NULL) {
		int e = errno;
		fprintf(stderr, "Could not open '%s' for writing : %d %s\n", dataFileName, e, strerror(e));
		exit(1);
	}

	outputIndexFile = fopen(indexFileName, "w");
	if(outputIndexFile == NULL) {
		int e = errno;
		fprintf(stderr, "Could not open '%s' for writing : %d %s\n", indexFileName, e, strerror(e));
		exit(1);
	}

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

u_int32_t RawWriterV3::addEventBuffer(long long tMin, long long tMax, EventBuffer<RawHit> *inBuffer)
{
	u_int32_t lSingleRead = 0;
	unsigned N = inBuffer->getSize();
	for(unsigned i = 0; i < N; i++) {
		RawHit &p = inBuffer->get(i);
		if((p.time < tMin) || (p.time >= tMax)) continue;
		long long T = SYSTEM_PERIOD * 1E12;
		uint64_t frameID = p.time / (1024L * T);
		
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

		int r = fwrite(&eventOut, sizeof(eventOut), 1, outputDataFile);
		if(r != 1) {
			int e = errno;
			fprintf(stderr, "RawWriterV3:: error writing to data file : %d %s\n", 
				e, strerror(e));
		}
		lSingleRead++;
	}
	return lSingleRead;
}
