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
#include <Core/RawHitWriter.hpp>
#include <TOFPET/RawV3.hpp>
#include <ENDOTOFPET/Raw.hpp>
#include <STICv3/sticv3Handler.hpp>

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
	assert(argc == 12);
	char *shmObjectPath = argv[1];
	unsigned long dataFrameSharedMemorySize = boost::lexical_cast<unsigned long>(argv[2]);	
	char outputType = argv[3][0];
	char *outputFilePrefix = argv[4];
	float cWindow = boost::lexical_cast<float>(argv[5]);
	float minToT = boost::lexical_cast<float>(argv[6]);
	float preWindow = boost::lexical_cast<float>(argv[7]);
	float postWindow = boost::lexical_cast<float>(argv[8]);
	char *channelMapFileName = argv[9];
	char *triggerMapFileName = argv[10];
	float cutToT = boost::lexical_cast<float>(argv[11]);

	DAQ::Common::SystemInformation *systemInformation = new DAQ::Core::SystemInformation();
	if(cWindow != 0) {
		systemInformation->loadMapFile(channelMapFileName);
		systemInformation->loadTriggerMapFile(triggerMapFileName);
	}
	
	FILE *rawFrameFile = NULL;

	DAQd::SHM *shm = new DAQd::SHM(shmObjectPath);
		
	AbstractRawHitWriter *writer = NULL;
	bool pipeWriterIsNull = true;
	if(outputType == 'T') {
		writer = new TOFPET::RawWriterV3(outputFilePrefix);
		pipeWriterIsNull = false;
	}
	else if(outputType == 'E') {
		writer = new ENDOTOFPET::RawWriterE(outputFilePrefix, 0);
		pipeWriterIsNull = false;
	}
	else if(outputType == 'R') {
		writer = new NullRawHitWriter();

		char fName[1024];
		sprintf(fName, "%s.rawf", outputFilePrefix);
		rawFrameFile = fopen(fName, "wb");
		assert(rawFrameFile != NULL);
		pipeWriterIsNull = true;
	}
	else {
		writer = new NullRawHitWriter();
		pipeWriterIsNull = true;
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
	
	EventSink<RawHit> *sink = NULL;
	EventBuffer<RawHit> *outBuffer = NULL;
	long long minFrameID = 0x7FFFFFFFFFFFFFFFLL, maxFrameID = 0, lastMaxFrameID = 0;
	
	long long lastFrameID = -1;
	long long stepFirstFrameID = -1;

	while(fread(&blockHeader, sizeof(blockHeader), 1, stdin) == 1) {

		step1 = blockHeader.step1;
		step2 = blockHeader.step2;
		
		if(sink == NULL) {
			writer->openStep(step1, step2);
			if (pipeWriterIsNull) {
				sink = new NullSink<RawHit>();
			}
			else if (cWindow == 0) {
				sink =	new CoarseSorter(
					new RawHitWriterHandler(writer,
					new NullSink<RawHit>()
					));
			}
			else {
				// Round up cWindow and minToT for use in CoincidenceFilter
				float cWindowCoarse = (ceil(cWindow/SYSTEM_PERIOD)) * SYSTEM_PERIOD;
				float minToTCoarse = (floor(minToT/SYSTEM_PERIOD) - 2) * SYSTEM_PERIOD;
				sink =	new CoarseSorter(
					new CoincidenceFilter(systemInformation, cWindowCoarse, minToTCoarse,
					new RawHitWriterHandler(writer,
					new NullSink<RawHit>()
					)));
			}
		}
		
		unsigned bs = shm->getSizeInFrames();
		unsigned rdPointer = blockHeader.rdPointer % (2*bs);
		unsigned wrPointer = blockHeader.wrPointer % (2*bs);
		while(rdPointer != wrPointer) {
			unsigned index = rdPointer % bs;
			
			long long frameID = shm->getFrameID(index);
			if(stepFirstFrameID == -1) stepFirstFrameID = frameID;
			if(frameID <= lastFrameID) {
				fprintf(stderr, "WARNING!! Frame ID reversal: %12lld -> %12lld | %04u %04u %04u\n", 
					lastFrameID, frameID, 
					blockHeader.wrPointer, blockHeader.rdPointer, rdPointer
					);
					
					
				
			}
			else if ((lastFrameID >= 0) && (frameID != (lastFrameID + 1))) {
				// We have skipped one or more frame ID, so 
				// we account them as lost
				long long skippedFrames = (frameID - lastFrameID) - 1;
				stepGoodFrames += skippedFrames;
				stepLostFrames += skippedFrames;
				stepLostFrames0 += skippedFrames;
			}

			lastFrameID = frameID;
			minFrameID = minFrameID < frameID ? minFrameID : frameID;
			maxFrameID = maxFrameID > frameID ? maxFrameID : frameID;
			
			// Simply dump the raw data frame
			int frameSize = shm->getFrameSize(index);
			if (rawFrameFile != NULL) {
				DAQd::DataFrame *dataFrame = shm->getDataFrame(index);
				fwrite((void *)dataFrame->data, sizeof(uint64_t), frameSize, rawFrameFile);
			}

			int nEvents = shm->getNEvents(index);
			bool frameLost = shm->getFrameLost(index);
			
			if(outBuffer == NULL) {
				outBuffer = new EventBuffer<RawHit>(EVENT_BLOCK_SIZE, NULL);
			}
			
			for (int n = 0; !pipeWriterIsNull && n < nEvents; n++) {
				RawHit &p = outBuffer->getWriteSlot();
#ifdef __ENDOTOFPET__
				int feType = shm->getEventType(index, n);
#else
				const int feType = 0;
#endif
				if (feType == 0) {
					p.feType = RawHit::TOFPET;
					unsigned tCoarse = shm->getTCoarse(index, n);
					unsigned eCoarse = shm->getECoarse(index, n);
					p.time = (1024LL * frameID + tCoarse) * T;
					p.timeEnd = (1024LL * frameID + eCoarse) * T;
					if((p.timeEnd - p.time) < -256*T) p.timeEnd += (1024LL * T);
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
					p.feType = RawHit::STIC;
					unsigned tCoarse = shm->getTCoarse(index, n);
					unsigned eCoarse = shm->getECoarse(index, n);
					// Compensate for LFSR's 2^16-1 period
					// and wrap at frame's 6.4 us period
					int ctCoarse = STICv3::Sticv3Handler::compensateCoarse(tCoarse, frameID) % 4096;
					int ceCoarse = STICv3::Sticv3Handler::compensateCoarse(eCoarse, frameID) % 4096;
					p.time = 1024LL * frameID * T + ctCoarse * T/4;
					p.timeEnd = 1024LL * frameID * T + ceCoarse * T/4;
					if((p.timeEnd - p.time) < -256*T) p.timeEnd += (1024LL * T);
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
				
				// Cut events which don't meet cutToT
				if((p.timeEnd - p.time) < (cutToT * 1E12)) {
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

			fprintf(stderr, "writeRaw:: Step had %lld frames with %lld events; %f events/frame avg, %d event/frame max\n", 
					stepGoodFrames, stepEvents, 
					float(stepEvents)/stepGoodFrames,
					stepMaxFrame); fflush(stderr);
			fprintf(stderr, "writeRaw:: %d (%5.1f%%) frames received had missing data; %d (%5.1f%%) of had no data at all\n", 
					stepLostFrames, 100.0 * stepLostFrames / stepGoodFrames,
					stepLostFrames0, 100.0 * stepLostFrames0 / stepGoodFrames
					); 
			fflush(stderr);
			stepGoodFrames = 0;
			stepEvents = 0;
			stepMaxFrame = 0;
			stepLostFrames = 0;
			stepLostFrames0 = 0;
			lastFrameID = -1;
			stepFirstFrameID = -1;
		}

		fwrite(&rdPointer, sizeof(uint32_t), 1, stdout);
		fflush(stdout);

	
	}

	delete writer;
	delete systemInformation;
	if(rawFrameFile != NULL)
		fclose(rawFrameFile);
	
	return 0;
}

