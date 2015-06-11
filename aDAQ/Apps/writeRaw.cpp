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
#include <TOFPET/RawV2.hpp>
#include <vector>
#include <algorithm>
#include <functional>

#include <Common/Constants.hpp>
#include <Common/Utils.hpp>
#include <Core/Event.hpp>
#include <Core/CoarseExtract.hpp>
#include <Core/PulseFilter.hpp>
#include <Core/SingleReadoutGrouper.hpp>
#include <Core/CrystalPositions.hpp>
#include <Core/NaiveGrouper.hpp>
#include <Core/CoincidenceFilter.hpp>
#include <ENDOTOFPET/Raw.hpp>
// #include <Core/RawPulseWriter.hpp>

using namespace std;
using namespace DAQ;
using namespace DAQ::Core;

const double T = SYSTEM_PERIOD;
static const unsigned outBlockSize = 128*1024;


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
// 	
// 	AbstractRawPulseWriter *writer = NULL;
// 	if (outputType = 'T') {
// 		writer = new TOFPET::RawWriterV2(outputFilePrefix);
// 	}
// 	else {
// 		writer = new NullRawPulseWriter();
// 	}

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
	long long tMax = 0, lastTMax = 0;

	while(fread(&blockHeader, sizeof(blockHeader), 1, stdin) == 1) {

		step1 = blockHeader.step1;
		step2 = blockHeader.step2;
		
		
// 		if(sink == NULL) {
// 			
// 			
// 			
// 			sink =	new RawPulseWriterHandler(writer,
// 				new NullSink<RawPulse>()
// 				);
// 	
// 		}
/*		
		if(outBuffer == NULL) {
			outBuffer = new EventBuffer<RawPulse>(outBlockSize);
		}*/
		
		unsigned rdPointer = blockHeader.rdPointer;
		unsigned wrPointer = blockHeader.wrPointer;
		unsigned bs = shm->getSizeInFrames();
		for(; rdPointer != wrPointer; rdPointer = (rdPointer + 1) % (2*bs) ) {
			unsigned index = rdPointer % bs;

			
			unsigned long long frameID = shm->getFrameID(index);
			int nEvents = shm->getNEvents(index);
			bool frameLost = shm->getFrameLost(index);
			
// 			for (int n = 0; n < nEvents; n++) {
// 				RawPulse &p = outBuffer->getWriteSlot();
// 				
// #ifdef __ENDOTOFPET__
// 				int feType = shm->getEventType(index, n);
// #else
// 				const int feType = 0;
// #endif
// 
// 				if (feType == 0) {
// 					p.feType = RawPulse::TOFPET;
// 					p.T = T * 1E12;
// 					unsigned tCoarse = shm->getTCoarse(index, n);
// 					unsigned eCoarse = shm->getECoarse(index, n);
// 					p.time = (1024LL * frameID + tCoarse) * p.T;
// 					p.timeEnd = (1024LL * frameID + eCoarse) * p.T;
// 					p.channelID = 64 * shm->getAsicID(index, n) + shm->getChannelID(index, n);
// 					p.d.tofpet.tac = shm->getTACID(index, n);
// 					p.d.tofpet.tcoarse = tCoarse;
// 					p.d.tofpet.ecoarse = eCoarse;
// 					p.d.tofpet.tfine =  shm->getTFine(index, n);
// 					p.d.tofpet.efine = shm->getEFine(index, n);
// 					p.channelIdleTime = shm->getChannelIdleTime(index, n);
// 					p.d.tofpet.tacIdleTime = shm->getTACIdleTime(index, n);
// 				}
// 				else if (feType == 1) {
// 					p.feType = RawPulse::STIC;
// 					p.T = T * 1E12;
// 					unsigned tCoarse = shm->getTCoarse(index, n);
// 					unsigned eCoarse = shm->getECoarse(index, n);
// 					p.time = (1024LL * frameID + ((tCoarse>>2) & 0x3FF)) * p.T;
// 					p.timeEnd = (1024LL * frameID + ((eCoarse>>2) & 0x3FF)) * p.T;
// 					p.channelID = 64 * shm->getAsicID(index, n) + shm->getChannelID(index, n);
// 					p.d.stic.tcoarse = tCoarse;
// 					p.d.stic.ecoarse = eCoarse;
// 					p.d.stic.tfine =  shm->getTFine(index, n);
// 					p.d.stic.efine = shm->getEFine(index, n);
// 					p.channelIdleTime = shm->getChannelIdleTime(index, n);
// 					p.d.stic.tBadHit = shm->getTBadHit(index, n);
// 					p.d.stic.eBadHit = shm->getEBadHit(index, n);
// 				}
// 				
// 				if(p.time > tMax) {
// 					tMax = p.time;
// 				}
// 				
// 				outBuffer->pushWriteSlot();
// 				
// 			}
// 			
// 			if(outBuffer->getSize() >= (outBlockSize - MaxDataFrameSize)) {
// 				outBuffer->setTMin(lastTMax);
// 				outBuffer->setTMax(tMax);
// 				lastTMax = tMax;
// 				sink->pushEvents(outBuffer);
// 				outBuffer = NULL;
// 				
// 			}

			stepEvents += nEvents;
			stepMaxFrame = stepMaxFrame > nEvents ? stepMaxFrame : nEvents;
			if(frameLost) {
				stepLostFrames += 1;
				if(nEvents == 0) 
					stepLostFrames0 += 1;
			}			
			stepGoodFrames += 1;
		}		
		
		fwrite(&rdPointer, sizeof(uint32_t), 1, stdout);
		fflush(stdout);

		if(blockHeader.endOfStep != 0) {
			if(sink != NULL) {

				if(outBuffer != NULL) {
					outBuffer->setTMin(lastTMax);
					outBuffer->setTMax(tMax);
					lastTMax = tMax;
					sink->pushEvents(outBuffer);
					outBuffer = NULL;
				}
			
				sink->finish();
				sink->report();
				delete sink;
				sink = NULL;
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
	


//	delete writer;
	
	return 0;
}
