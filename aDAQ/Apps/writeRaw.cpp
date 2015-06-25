#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <boost/lexical_cast.hpp>
#include <SHM.hpp>
#include <TFile.h>
#include <TTree.h>
#include <vector>
#include <algorithm>
#include <functional>
#include <Common/Constants.hpp>
#include <Common/Utils.hpp>
#include <Core/Event.hpp>
#include <Core/CoarseSorter.hpp>
#include <Core/CoincidenceFilter.hpp>
#include <Core/RawPulseWriter.hpp>
#include <TOFPET/RawV3.hpp>
#include <ENDOTOFPET/Raw.hpp>

using namespace std;
using namespace DAQ;
using namespace DAQ::Core;

const long long T = (long long)(SYSTEM_PERIOD * 1E12);

struct BlockHeader  {
	float step1;
	float step2;	
	uint32_t wrPointer;
	uint32_t rdPointer;
	int32_t endOfStep;
};



int main(int argc, char *argv[])
{
	assert(argc == 7);
	char *shmObjectPath = argv[1];
	unsigned long dataFrameSharedMemorySize = boost::lexical_cast<unsigned long>(argv[2]);	
	float cWindow = boost::lexical_cast<float>(argv[3]);
	float minToT = boost::lexical_cast<float>(argv[4]);
	char outputType = argv[5][0];
	char *outputFilePrefix = argv[6];
	
	DAQd::SHM *shm = new DAQd::SHM(shmObjectPath);
	
	AbstractRawPulseWriter *writer = NULL;
	if(outputType == 'T') {
		writer = new TOFPET::RawWriterV3(outputFilePrefix);
	}
	else {
		writer = new NullRawPulseWriter();
	}

	bool firstBlock = true;
	float step1;
	float step2;
	BlockHeader blockHeader;
	
	long long stepGoodFrames = 0;
	long long stepEvents = 0;
	long long stepMaxFrame = 0;
	long long stepLostFrames = 0;
	long long stepLostFrames0 = 0;
	
	EventSink<RawPulse> *sink = NULL;
	EventBuffer<RawPulse> *outBuffer = NULL;
	long long maxFrameID = 0, lastMaxFrameID = 0;
	
	long long lastFrameID = -1;

	while(fread(&blockHeader, sizeof(blockHeader), 1, stdin) == 1) {

		step1 = blockHeader.step1;
		step2 = blockHeader.step2;
		
		if(sink == NULL) {
			writer->openStep(step1, step2);
			if (outputType == 'N') {
				sink = new NullSink<RawPulse>();
			}
			else if (cWindow == 0) {
				sink =	new CoarseSorter(
					new RawPulseWriterHandler(writer,
					new NullSink<RawPulse>()
					));
			}
			else {
				// Round up cWindow and minToT for use in CoincidenceFilter
				float cWindowCoarse = (ceil(cWindow/SYSTEM_PERIOD) + 1) * SYSTEM_PERIOD;
				float minToTCoarse = (ceil(minToT/SYSTEM_PERIOD) + 2) * SYSTEM_PERIOD;
				sink =	new CoarseSorter(
					new CoincidenceFilter(Common::getCrystalMapFileName(), cWindowCoarse, minToTCoarse,
					new RawPulseWriterHandler(writer,
					new NullSink<RawPulse>()
					)));
			}
		}
		
		unsigned bs = shm->getSizeInFrames();
		unsigned rdPointer = blockHeader.rdPointer % (2*bs);
		unsigned wrPointer = blockHeader.wrPointer % (2*bs);
		while(rdPointer != wrPointer) {
			unsigned index = rdPointer % bs;
			
			long long frameID = shm->getFrameID(index);
			if(frameID <= lastFrameID) {
				fprintf(stderr, "WARNING!! Frame ID reversal: %12lld -> %12lld | %04u %04u %04u\n", 
					lastFrameID, frameID, 
					blockHeader.wrPointer, blockHeader.rdPointer, rdPointer
					);
					
					
				
			}
			lastFrameID = frameID;
			maxFrameID = maxFrameID > frameID ? maxFrameID : frameID;
			
			int nEvents = shm->getNEvents(index);
			bool frameLost = shm->getFrameLost(index);
			
			if(outBuffer == NULL) {
				outBuffer = new EventBuffer<RawPulse>(EVENT_BLOCK_SIZE);
			}
			
			for (int n = 0; outputType != 'N' && n < nEvents; n++) {
				RawPulse &p = outBuffer->getWriteSlot();
#ifdef __ENDOTOFPET__
				int feType = shm->getEventType(index, n);
#else
				const int feType = 0;
#endif
				if (feType == 0) {
					p.feType = RawPulse::TOFPET;
					p.T = T;
					unsigned tCoarse = shm->getTCoarse(index, n);
					unsigned eCoarse = shm->getECoarse(index, n);
					p.time = (1024LL * frameID + tCoarse) * p.T;
					p.timeEnd = (1024LL * frameID + eCoarse) * p.T;
					if((p.timeEnd - p.time) < -256*p.T) p.timeEnd += (1024LL * p.T);
					p.channelID = 64 * shm->getAsicID(index, n) + shm->getChannelID(index, n);
					p.d.tofpet.tac = shm->getTACID(index, n);
					p.d.tofpet.tcoarse = tCoarse;
					p.d.tofpet.ecoarse = eCoarse;
					p.d.tofpet.tfine =  shm->getTFine(index, n);
					p.d.tofpet.efine = shm->getEFine(index, n);
					p.channelIdleTime = shm->getChannelIdleTime(index, n);
					p.d.tofpet.tacIdleTime = shm->getTACIdleTime(index, n);
				}
				else if (feType == 1) {
					p.feType = RawPulse::STIC;
					p.T = T;
					unsigned tCoarse = shm->getTCoarse(index, n);
					unsigned eCoarse = shm->getECoarse(index, n);
					p.time = (1024LL * frameID + ((tCoarse>>2) & 0x3FF)) * p.T;
					p.timeEnd = (1024LL * frameID + ((eCoarse>>2) & 0x3FF)) * p.T;
					if((p.timeEnd - p.time) < -256*p.T) p.timeEnd += (1024LL * p.T);
					p.channelID = 64 * shm->getAsicID(index, n) + shm->getChannelID(index, n);
					p.d.stic.tcoarse = tCoarse;
					p.d.stic.ecoarse = eCoarse;
					p.d.stic.tfine =  shm->getTFine(index, n);
					p.d.stic.efine = shm->getEFine(index, n);
					p.channelIdleTime = shm->getChannelIdleTime(index, n);
					p.d.stic.tBadHit = shm->getTBadHit(index, n);
					p.d.stic.eBadHit = shm->getEBadHit(index, n);
				} else {
					continue;
				}
				
				outBuffer->pushWriteSlot();
				
			}
			
			if(outBuffer->getSize() >= (EVENT_BLOCK_SIZE - DAQd::MaxDataFrameSize)) {
				long long tMin = lastMaxFrameID * 1024 * T;
				long long tMax = (maxFrameID+1) * 1024 * T - 1;
				outBuffer->setTMin(tMin);
				outBuffer->setTMax(tMax);
				lastMaxFrameID = maxFrameID;
				sink->pushEvents(outBuffer);
				outBuffer = NULL;
				
			}

			stepEvents += nEvents;
			stepMaxFrame = stepMaxFrame > nEvents ? stepMaxFrame : nEvents;
			if(frameLost) {
				stepLostFrames += 1;
				if(nEvents == 0) 
					stepLostFrames0 += 1;
			}			
			stepGoodFrames += 1;
			
			rdPointer = (rdPointer+1) % (2*bs);
		}		
		
		fwrite(&rdPointer, sizeof(uint32_t), 1, stdout);
		fflush(stdout);

		if(blockHeader.endOfStep != 0) {
			if(sink != NULL) {
				if(outBuffer != NULL) {
					long long tMin = lastMaxFrameID * 1024 * T;
					long long tMax = (maxFrameID+1) * 1024 * T - 1;
					outBuffer->setTMin(tMin);
					outBuffer->setTMax(tMax);
					lastMaxFrameID = maxFrameID;
					sink->pushEvents(outBuffer);
					outBuffer = NULL;
				}
			
				sink->finish();
				sink->report();
				delete sink;
				sink = NULL;
				writer->closeStep();
			}

			fprintf(stderr, "writeRaw:: Step had %d frames with %d events; %f events/frame avg, %d event/frame max\n", 
					stepGoodFrames, stepEvents, 
					float(stepEvents)/stepGoodFrames,
					stepMaxFrame); fflush(stderr);
			fprintf(stderr, "writeRaw:: %d frames received had missing data; %d of these had no data\n", stepLostFrames, stepLostFrames0); fflush(stderr);
			stepGoodFrames = 0;
			stepEvents = 0;
			stepMaxFrame = 0;
			stepLostFrames = 0;
			stepLostFrames0 = 0;
		}
	}

	delete writer;
	
	return 0;
}
