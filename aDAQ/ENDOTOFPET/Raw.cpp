#include "Raw.hpp"

#include <algorithm>
#include <functional>
#include <math.h>
#include <set>
#include <limits.h>
#include <iostream>

using namespace std;
using namespace DAQ::Core;
using namespace DAQ::ENDOTOFPET;

static const unsigned outBlockSize = 128*1024;
static const unsigned maxEventsPerFrame = 16*1024;

RawReader::RawReader(FILE *dataFile, float T, EventSink<RawPulse> *sink)
	: EventSource<RawPulse>(sink), dataFile(dataFile), T(T)
{
	start();
}

RawReader::~RawReader()
{
}


struct SortEntry {
	short tCoarse;
	RawPulse *event;	
};
static bool operator< (SortEntry lhs, SortEntry rhs) { return lhs.tCoarse < rhs.tCoarse; }



void RawReader::run()
{

	unsigned nWraps = 0;
	
	EventBuffer<RawPulse> *outBuffer = NULL;
	
	RawPulse framePulses[maxEventsPerFrame];
	SortEntry sortArray[maxEventsPerFrame];
	int nEventsInFrame = 0;
	
	long long tMax = 0, lastTMax = 0;
	long long FrameID;
	
	sink->pushT0(0);
	
	long long minFrameID = LLONG_MAX;
	long long maxFrameID = LLONG_MIN;
	uint8_t code;
	
	long long events=0;
	int bytes;
	while (fread(&code, 1, 1, dataFile) == 1) {
		  events++;
		  
		  
		  if(code>0x02){
				fprintf(stderr, "Impossible code: %u, at event %ld\n\n", code, events);
				break;
		  }
		  fseek(dataFile, -1, SEEK_CUR);
		
		
			
		  switch(code){
		  case 0:{
				StartTime rawStartTime;
				bytes= fread(&rawStartTime, sizeof(StartTime), 1, dataFile);
				AcqStartTime=rawStartTime.time;
				//fprintf(stderr, "rawStartTime=%d %d\n",rawStartTime.code,rawStartTime.time);
				continue;
		  }
		  case 1:{
				FrameHeader rawFrameHeader;
				bytes=fread(&rawFrameHeader, sizeof(FrameHeader), 1, dataFile);
				CurrentFrameID=rawFrameHeader.frameID;
				//fprintf(stderr, "Current frameID= %d %lld %lld\n", rawFrameHeader.code,rawFrameHeader.frameID,rawFrameHeader.drift   );
				continue;
		  }
		  case 2:{
				RawTOFPET rawEvent;
		
				bytes=fread(&rawEvent, sizeof(RawTOFPET), 1, dataFile);
				//fprintf(stderr, "tof.struct=%d\n %d\n %d\n %ld\n %ld\n %ld\n %lld\n %lld\n \n\n", rawEvent.code, rawEvent.tac, rawEvent.channelID,rawEvent.tCoarse,  rawEvent.eCoarse, rawEvent.tFine , rawEvent.eFine , rawEvent.tacIdleTime , rawEvent.channelIdleTime );
				if(outBuffer == NULL) {
					  outBuffer = new EventBuffer<RawPulse>(outBlockSize);
				}
				
			
				RawPulse &p = outBuffer->getWriteSlot();
				
				// Carefull with the float/double/integer conversions here..
				p.d.tofpet.T = T * 1E12;
				p.time = (1024LL * CurrentFrameID + rawEvent.tCoarse) * p.d.tofpet.T;
				p.timeEnd = (1024LL * CurrentFrameID + rawEvent.eCoarse) * p.d.tofpet.T;
				if((p.timeEnd - p.time) < -256*p.d.tofpet.T) p.timeEnd += (1024LL * p.d.tofpet.T);
				p.channelID = rawEvent.channelID;
				p.channelIdleTime = rawEvent.channelIdleTime;
				p.region = rawEvent.channelID / 16;
				p.feType = RawPulse::TOFPET;
				p.d.tofpet.frameID = CurrentFrameID;
				p.d.tofpet.tac = rawEvent.tac;
				p.d.tofpet.tcoarse = rawEvent.tCoarse;
				p.d.tofpet.ecoarse = rawEvent.eCoarse;
				p.d.tofpet.tfine =  rawEvent.tFine;
				p.d.tofpet.efine = rawEvent.eFine;
				p.d.tofpet.tacIdleTime = rawEvent.tacIdleTime;
				
		
				if(CurrentFrameID < minFrameID) minFrameID = CurrentFrameID;
				if(CurrentFrameID > maxFrameID) maxFrameID = CurrentFrameID;
				
				if(p.channelID >= 128)
					  continue;
				
				if(p.time > tMax)
					  tMax = p.time;
				
				outBuffer->pushWriteSlot();
				
				if(outBuffer->getSize() >= (outBlockSize - 512)) {
					  outBuffer->setTMin(lastTMax);
					  outBuffer->setTMax(tMax);		
					  sink->pushEvents(outBuffer);
					  outBuffer = NULL;
				}
				continue;
		  }
		  case 3:{
				RawSTICv3 rawEvent2;
				
				fread(&rawEvent2, sizeof(RawSTICv3), 1, dataFile);
				
				if(outBuffer == NULL) {
					  outBuffer = new EventBuffer<RawPulse>(outBlockSize);
				}
				
				
				RawPulse &p = outBuffer->getWriteSlot();
				
				// Add in the following block any other STIC auxiliary fields relevant for data analysis and constructed from raw data 

				
				//	p.d.tofpet.T = T * 1E12;
				p.time = (1024LL * CurrentFrameID + rawEvent2.tCoarse) * p.d.tofpet.T;
				p.timeEnd = (1024LL * CurrentFrameID + rawEvent2.eCoarse) * p.d.tofpet.T;
				if((p.timeEnd - p.time) < -256*p.d.tofpet.T) p.timeEnd += (1024LL * p.d.tofpet.T);
				p.channelID = rawEvent2.channelID;
				p.channelIdleTime = rawEvent2.channelIdleTime;
				p.region = rawEvent2.channelID / 16;
				p.feType = RawPulse::STIC;
				p.d.stic.frameID = CurrentFrameID;
   
				p.d.stic.tcoarse = rawEvent2.tCoarse;
				p.d.stic.ecoarse = rawEvent2.eCoarse;
				p.d.stic.tfine =  rawEvent2.tFine;
				p.d.stic.efine = rawEvent2.eFine;
				p.d.stic.tBadHit = rawEvent2.tBadHit;
				p.d.stic.eBadHit = rawEvent2.eBadHit;
				
		
				if(CurrentFrameID < minFrameID) minFrameID = CurrentFrameID;
				if(CurrentFrameID > maxFrameID) maxFrameID = CurrentFrameID;
				
				if(p.channelID >= 128)
					  continue;
				
				if(p.time > tMax)
					  tMax = p.time;
				
				outBuffer->pushWriteSlot();
				
				if(outBuffer->getSize() >= (outBlockSize - 512)) {
					  outBuffer->setTMin(lastTMax);
					  outBuffer->setTMax(tMax);		
					  sink->pushEvents(outBuffer);
					  outBuffer = NULL;
				}
				continue;
		  }
				
		  }
		
		
	}

	
	if(outBuffer != NULL) {
		outBuffer->setTMin(lastTMax);
		outBuffer->setTMax(tMax);		
		sink->pushEvents(outBuffer);
		outBuffer = NULL;
		
	}
	
	sink->finish();
	
	fprintf(stderr, "RawReaderV2 report\n");
	fprintf(stderr, "\t%16lld minFrameID\n", minFrameID);
	fprintf(stderr, "\t%16lld maxFrameID\n", maxFrameID);
	sink->report();
}



