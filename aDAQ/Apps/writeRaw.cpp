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
#include <Protocol.hpp>
#include <TFile.h>
#include <TTree.h>
#include <TOFPET/RawV2.hpp>
#include <vector>
#include <algorithm>
#include <functional>

#include <Core/Event.hpp>
#include <Core/CoarseExtract.hpp>
#include <Core/PulseFilter.hpp>
#include <Core/SingleReadoutGrouper.hpp>
#include <Core/FakeCrystalPositions.hpp>
#include <Core/NaiveGrouper.hpp>
#include <Core/CoincidenceFilter.hpp>


using namespace std;
using namespace DAQ;
using namespace DAQ::Core;

const double T = 6.25E-9;
static const unsigned outBlockSize = 128*1024;


struct BlockHeader  {
	float step1;
	float step2;	
	int32_t nFrames;
	int32_t endOfStep;
};


bool isBefore(uint32_t frameID1, uint32_t frameID2)
{
	if(frameID1 < frameID2)
		return true;
	
	if(frameID1 > 4294967295 && frameID2 < 4294967295)
		return true;
	
	return false;
	
}

struct SortEntry {
	uint16_t tCoarse;
	short index;
};
static bool operator< (SortEntry lhs, SortEntry rhs) { return lhs.tCoarse < rhs.tCoarse; }


class EventWriter : public OverlappedEventHandler<GammaPhoton, GammaPhoton> {
public:
	EventWriter(FILE *dataFile, FILE *indexFile, float step1, float step2, EventSink<GammaPhoton> *sink) 
	: dataFile(dataFile), indexFile(indexFile), step1(step1), step2(step2), 
	OverlappedEventHandler<GammaPhoton, GammaPhoton>(sink, true)
	{
		long position = ftell(dataFile);
		stepBegin = position / sizeof(DAQ::TOFPET::RawEventV2);
		nEventsPassed = 0;
	};
	
	
	~EventWriter() {
		
	};

	EventBuffer<GammaPhoton> * handleEvents(EventBuffer<GammaPhoton> *inBuffer) {
		
		long long tMin = inBuffer->getTMin();
		long long tMax = inBuffer->getTMax();
		unsigned nEvents =  inBuffer->getSize();
	
		for(unsigned i = 0; i < nEvents; i++) {
			GammaPhoton &photon = inBuffer->get(i);
			if(photon.time < tMin || photon.time > tMax)
				continue;
			
			for(int k = 0; k < photon.nHits; k++) {
				RawHit &rawHit= photon.hits[k].raw;
				RawPulse &rawPulse = rawHit.top.raw;
				
				if(rawPulse.time < tMin || rawPulse.time >= tMax)
					continue;
				
				
// 				fprintf(stderr, "writing: %d %d %d %d %d %d %d %d %lld %lld\n", 
// 				       rawPulse.d.tofpet.frameID,
// 					rawPulse.channelID / 64,
// 					rawPulse.channelID % 64,
// 					rawPulse.d.tofpet.tac,
// 					rawPulse.d.tofpet.tcoarse,
// 					rawPulse.d.tofpet.ecoarse,
// 					rawPulse.d.tofpet.tfine,
// 					rawPulse.d.tofpet.efine,
// 					rawPulse.d.tofpet.channelIdleTime,
// 					rawPulse.d.tofpet.tacIdleTime
// 					);
				DAQ::TOFPET::RawEventV2 eventOut = {
					rawPulse.d.tofpet.frameID,
					rawPulse.channelID / 64,
					rawPulse.channelID % 64,
					rawPulse.d.tofpet.tac,
					rawPulse.d.tofpet.tcoarse,
					rawPulse.d.tofpet.ecoarse,
					rawPulse.d.tofpet.tfine,
					rawPulse.d.tofpet.efine,
					rawPulse.d.tofpet.channelIdleTime,
					rawPulse.d.tofpet.tacIdleTime
				};
				
				
				
				fwrite(&eventOut, sizeof(eventOut), 1, dataFile);			
				nEventsPassed++;
			}			
		}
		
		return inBuffer;
	};
	
	void pushT0(double t0) { };
	void finish() {
		OverlappedEventHandler<GammaPhoton, GammaPhoton>::finish();
		long position = ftell(dataFile);
		long stepEnd = position/sizeof(DAQ::TOFPET::RawEventV2);
		fflush(dataFile);
		fprintf(indexFile, "%f %f %ld %ld\n", step1, step2, stepBegin, stepEnd);
		fflush(indexFile);
	};
	void report() { 
		fprintf(stderr, ">> EventWriter report\n");
		fprintf(stderr, " events passed\n");
		fprintf(stderr, "  %10ld total\n", nEventsPassed);
		OverlappedEventHandler<GammaPhoton, GammaPhoton>::report();
	};
private: 
	FILE *dataFile;
	FILE *indexFile;
	float step1;
	float step2;
	long stepBegin;
	long nEventsPassed;
};

int main(int argc, char *argv[])
{
	assert(argc == 5);
	char *shmObjectPath = argv[1];
	unsigned long dataFrameSharedMemorySize = boost::lexical_cast<unsigned long>(argv[2]);	
	float cWindow = boost::lexical_cast<float>(argv[3]);
	char *outputFilePrefix = argv[4];
	
	int dataFrameSharedMemory_fd = shm_open(shmObjectPath, O_RDONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
	
	if(dataFrameSharedMemory_fd < 0) {
		perror("Error opening shared memory");
		exit(1);
	}
	
//	ftruncate(dataFrameSharedMemory_fd, dataFrameSharedMemorySize);
	
	DataFrame *dataFrameSharedMemory = (DataFrame *)mmap(NULL, 
						  dataFrameSharedMemorySize, 
						  PROT_READ, 
						  MAP_SHARED, 
						  dataFrameSharedMemory_fd, 
						  0);
	if(dataFrameSharedMemory == NULL) {
		perror("Error mmaping() shared memory");
		exit(1);
	}
	
	

	char dataFileName[512];
	char indexFileName[512];
	sprintf(dataFileName, "%s.raw2", outputFilePrefix);
	sprintf(indexFileName, "%s.idx2", outputFilePrefix);
	
	FILE *outputDataFile = fopen(dataFileName, "w");
	FILE *outputIndexFile = fopen(indexFileName, "w");
		
	
	bool firstBlock = true;
	float step1;
	float step2;
	BlockHeader blockHeader;
	
	int stepGoodFrames = 0;
	int stepEvents = 0;
	int stepMaxFrame = 0;
	int stepLostFrames = 0;
	unsigned long lastFrameID = 0;
	vector<int32_t> shmIndexList;
	vector<SortEntry> sortList;
	
	EventSink<RawPulse> *sink = NULL;
	EventBuffer<RawPulse> *outBuffer = NULL;
	long long tMax = 0, lastTMax = 0;
	long nWraps = 0;
	
	while(fread(&blockHeader, sizeof(blockHeader), 1, stdin) == 1) {
 		//fprintf(stderr, "writeRaw:: Got %f %f %d %u\n", blockHeader.step1, blockHeader.step2, blockHeader.nFrames, unsigned(blockHeader.endOfStep));
		
		if(blockHeader.endOfStep != 0) {
			if(outBuffer != NULL) {
				outBuffer->setTMin(lastTMax);
				outBuffer->setTMax(tMax);
				lastTMax = tMax;
				sink->pushEvents(outBuffer);
				outBuffer = NULL;
			}
			
			if(sink != NULL) {
				sink->finish();
				sink->report();
				delete sink;
				sink = NULL;
			}

			fprintf(stderr, "writeRaw:: Step had %d frames with %d events; %f events/frame avg, %d event/frame max\n", 
					stepGoodFrames, stepEvents, 
					float(stepEvents)/stepGoodFrames,
					stepMaxFrame); fflush(stderr);
			fprintf(stderr, "writeRaw:: %d frames received were lost frames\n", stepLostFrames); fflush(stderr);
			stepGoodFrames = 0;
			stepEvents = 0;
			stepMaxFrame = 0;
			stepLostFrames = 0;

			//fprintf(stderr, "writeRaw:: EoS found with %d frames. Returning ", blockHeader.nFrames);
			for(int n = 0; n < blockHeader.nFrames; n++) {
	                        int32_t shmIndex = -1;
        	                fread(&shmIndex, sizeof(int32_t), 1, stdin);
				fwrite(&shmIndex, sizeof(int32_t), 1, stdout);
				//fprintf(stderr, "%d ", shmIndex); 
			}
			//fprintf(stderr, "\n"); fflush(stderr);
			fflush(stdout);
			continue;
		}				


		if(blockHeader.nFrames <= 0)
			continue;

		shmIndexList.clear();
		shmIndexList.reserve(1024);	
		
		bool hasInvalid = false;
		for(int n = 0; n < blockHeader.nFrames; n++) {
			int32_t shmIndex = -1;			
			fread(&shmIndex, sizeof(int32_t), 1, stdin);
			if(shmIndex < 0) hasInvalid = true;
			shmIndexList.push_back(shmIndex);
		}
		assert(blockHeader.nFrames == shmIndexList.size());

		if(hasInvalid) {
			fprintf(stderr, "writeRaw:: invalid frame index found in %d frames. Returning ", blockHeader.nFrames);
			for(int n = 0; n < blockHeader.nFrames; n++) {
				int32_t shmIndex = shmIndexList[n];
				fwrite(&shmIndex, sizeof(int32_t), 1, stdout);
				//fprintf(stderr, "%d ", shmIndex);   
			}
			fprintf(stderr, "\n"); fflush(stderr);
			fflush(stdout);
			continue;
		}

		// FrameID order check
		int32_t shmIndex = shmIndexList[0];
		DataFrame &dataFrame = dataFrameSharedMemory[shmIndex];	
		bool blockOK = isBefore(lastFrameID, dataFrame.frameID);		
		for(int n = 1; n < blockHeader.nFrames; n++)
		{
			int32_t shmIndex1 = shmIndexList[n-1];
			int32_t shmIndex2 = shmIndexList[n];
			DataFrame &dataFrame1 = dataFrameSharedMemory[shmIndex1];
			DataFrame &dataFrame2 = dataFrameSharedMemory[shmIndex2];
			blockOK &= isBefore(dataFrame1.frameID, dataFrame2.frameID);			
		}
		
		if(!blockOK) {
			fprintf(stderr, "writeRaw:: Discarding block with %d frames. Returning ", blockHeader.nFrames);			
			for(int n = 0; n < blockHeader.nFrames; n++) {
				int32_t shmIndex = shmIndexList[n];
				fwrite(&shmIndex, sizeof(int32_t), 1, stdout);
				//fprintf(stderr, "%d ", shmIndex);   
			}	
			fprintf(stderr, "\n"); fflush(stderr);
			fflush(stdout);
			continue;
		}
		
		

		step1 = blockHeader.step1;
		step2 = blockHeader.step2;
		
		
		if(sink == NULL) {			
			sink = new CoarseExtract(false,
				new PulseFilter(-INFINITY, INFINITY, 
				new SingleReadoutGrouper(
				new FakeCrystalPositions(
				new NaiveGrouper(20, 100E-9,
				new CoincidenceFilter(cWindow, 0,
				new EventWriter(outputDataFile, outputIndexFile, step1, step2,
				new NullSink<GammaPhoton>()
				)))))));		
		}
		
		
		for(int n = 0; n < blockHeader.nFrames; n++) {
			int32_t shmIndex = shmIndexList[n];	
			DataFrame &dataFrame = dataFrameSharedMemory[shmIndex];	
			
			if(dataFrame.frameID < lastFrameID) {
				nWraps += 1;
			}			
			lastFrameID = dataFrame.frameID;
			
			sortList.clear();
			sortList.reserve(512);
			for(unsigned i = 0; i < dataFrame.nEvents; i++) {
				EventTOFPET &eventIn = dataFrame.events[i].d.tofpet;
				SortEntry entry = { eventIn.tCoarse, i };
				sortList.push_back(entry);				
			}
			
			//fprintf(stderr, "Frames has %d events, sort list has %u entries\n", dataFrame.nEvents, sortList.size());
			
			sort(sortList.begin(), sortList.end());

			if(outBuffer == NULL) {
				outBuffer = new EventBuffer<RawPulse>(outBlockSize);
			}

			for(unsigned j = 0; j < sortList.size(); j++) {
				EventTOFPET &eventIn = dataFrame.events[sortList[j].index].d.tofpet;			
				
				long long correctedFrameID = dataFrame.frameID + nWraps * 0x100000000LL;				
				RawPulse &p = outBuffer->getWriteSlot();
				
				p.d.tofpet.T = T * 1E12;
				p.time = (1024LL * correctedFrameID + eventIn.tCoarse) * p.d.tofpet.T;
				p.channelID = 64 * eventIn.asicID + eventIn.channelID;
				p.region = (64 * eventIn.asicID + eventIn.channelID) / 16;
				p.feType = RawPulse::TOFPET;
				p.d.tofpet.frameID = correctedFrameID;
				p.d.tofpet.tac = eventIn.tacID;
				p.d.tofpet.tcoarse = eventIn.tCoarse;
				p.d.tofpet.ecoarse = eventIn.eCoarse;
				p.d.tofpet.tfine =  eventIn.tFine;
				p.d.tofpet.efine = eventIn.eFine;
				p.d.tofpet.channelIdleTime = eventIn.channelIdleTime;				
				p.d.tofpet.tacIdleTime = eventIn.tacIdleTime;				
									
				if(p.time > tMax) {
					tMax = p.time;
				}
				
				//fprintf(stderr, "outbuffer is %p\n", outBuffer);
				outBuffer->pushWriteSlot();
				stepEvents += 1;
				
			}
			
			
			if(outBuffer->getSize() >= (outBlockSize - 512)) {
// 				fprintf(stderr, "Pushing a buffer, %lld to %lld\n", lastTMax, tMax);
				outBuffer->setTMin(lastTMax);
				outBuffer->setTMax(tMax);
				lastTMax = tMax;
				sink->pushEvents(outBuffer);
				outBuffer = NULL;
			
			}
			
			stepMaxFrame = stepMaxFrame > dataFrame.nEvents ? stepMaxFrame : dataFrame.nEvents;
			if(dataFrame.frameLost) 
				stepLostFrames += 1;
			else
				stepGoodFrames += 1;
			
		}

			
		//fprintf(stderr, "writeRaw:: Finished processing %d frames. Returning ", blockHeader.nFrames);
               	for(int n = 0; n < blockHeader.nFrames; n++) {
			int32_t shmIndex = shmIndexList[n];
                        DataFrame &dataFrame = dataFrameSharedMemory[shmIndex];
                        //fprintf(stderr, "%d ", shmIndex);
                        fwrite(&shmIndex, sizeof(int32_t), 1, stdout);
		}
                //fprintf(stderr, "\n"); fflush(stderr);
                fflush(stdout);
	}
	
	if(stepEvents > 0) {
		fprintf(stderr, "writeRaw:: Step had %d frames received with %d events; %f events/frame avg, %d event/frame max\n", 
				stepGoodFrames, stepEvents, 
				float(stepEvents)/stepGoodFrames,
				stepMaxFrame); fflush(stderr);
		fprintf(stderr, "writeRaw:: %d frames received were lost frames\n", stepLostFrames); fflush(stderr);
	}

	
	if(outBuffer != NULL) {
//		fprintf(stderr, "Pushing last buffer, %lld to %lld\n", lastTMax, tMax);						
				
		outBuffer->setTMin(lastTMax);
		outBuffer->setTMax(tMax);
		lastTMax = tMax;
		sink->pushEvents(outBuffer);
		outBuffer = NULL;
	}
	
	if(sink != NULL) {
		sink->finish();
		sink->report();
		delete sink;
		sink = NULL;
	}
	

	
	fclose(outputIndexFile);
	fclose(outputDataFile);
	
	return 0;
}
