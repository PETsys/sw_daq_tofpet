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


using namespace std;
using namespace DAQ;
using namespace DAQ::Core;

const double T = SYSTEM_PERIOD;
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


class EventWriter_GP : public OverlappedEventHandler<GammaPhoton, GammaPhoton> {
public:
	EventWriter_GP(FILE *dataFile, FILE *indexFile, float step1, float step2, EventSink<GammaPhoton> *sink) 
	: dataFile(dataFile), indexFile(indexFile), step1(step1), step2(step2), 
	OverlappedEventHandler<GammaPhoton, GammaPhoton>(sink, true)
	{
		long position = ftell(dataFile);
		stepBegin = position / sizeof(DAQ::TOFPET::RawEventV2);
		nEventsPassed = 0;
	};
	
	
	~EventWriter_GP() {
		
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
// 					rawPulse.channelIdleTime,
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
					rawPulse.channelIdleTime,
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
		fprintf(stderr, ">> EventWriter_GP report\n");
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

class EventWriter_RP : public OverlappedEventHandler<RawPulse, RawPulse> {
public:
	EventWriter_RP(FILE *dataFile, FILE *indexFile, float step1, float step2, EventSink<RawPulse> *sink) 
	: dataFile(dataFile), indexFile(indexFile), step1(step1), step2(step2), 
	OverlappedEventHandler<RawPulse, RawPulse>(sink, true)
	{
		long position = ftell(dataFile);
		stepBegin = position / sizeof(DAQ::TOFPET::RawEventV2);
		nEventsPassed = 0;
	};
	
	
	~EventWriter_RP() {
		
	};

	EventBuffer<RawPulse> * handleEvents(EventBuffer<RawPulse> *inBuffer) {
		
		long long tMin = inBuffer->getTMin();
		long long tMax = inBuffer->getTMax();
		unsigned nEvents =  inBuffer->getSize();
	
		for(unsigned i = 0; i < nEvents; i++) {
			RawPulse &rawPulse = inBuffer->get(i);
					
			DAQ::TOFPET::RawEventV2 eventOut = {
				rawPulse.d.tofpet.frameID,
				rawPulse.channelID / 64,
				rawPulse.channelID % 64,
				rawPulse.d.tofpet.tac,
				rawPulse.d.tofpet.tcoarse,
				rawPulse.d.tofpet.ecoarse,
				rawPulse.d.tofpet.tfine,
				rawPulse.d.tofpet.efine,
				rawPulse.channelIdleTime,
				rawPulse.d.tofpet.tacIdleTime
			};
			
			
			
			fwrite(&eventOut, sizeof(eventOut), 1, dataFile);			
			nEventsPassed++;			
		}
		
		return inBuffer;
	};
	
	void pushT0(double t0) { };
	void finish() {
		OverlappedEventHandler<RawPulse, RawPulse>::finish();
		long position = ftell(dataFile);
		long stepEnd = position/sizeof(DAQ::TOFPET::RawEventV2);
		fflush(dataFile);
		fprintf(indexFile, "%f %f %ld %ld\n", step1, step2, stepBegin, stepEnd);
		fflush(indexFile);
	};
	void report() { 
		fprintf(stderr, ">> EventWriter_RP report\n");
		fprintf(stderr, " events passed\n");
		fprintf(stderr, "  %10ld total\n", nEventsPassed);
		OverlappedEventHandler<RawPulse, RawPulse>::report();
	};
private: 
	FILE *dataFile;
	FILE *indexFile;
	float step1;
	float step2;
	long stepBegin;
	long nEventsPassed;
};

class EventWriter_ENDOTOFPET_RP : public OverlappedEventHandler<RawPulse, RawPulse> {
public:
	  EventWriter_ENDOTOFPET_RP(FILE *dataFile, float step1, float step2, EventSink<RawPulse> *sink) 
			: 	OverlappedEventHandler<RawPulse, RawPulse>(sink, true),
				dataFile(dataFile), step1(step1), step2(step2) 
	  {
			currentFrameID=0;
	  };
	  
	  ~EventWriter_ENDOTOFPET_RP() {
		
	  };

	  EventBuffer<RawPulse> * handleEvents(EventBuffer<RawPulse> *inBuffer) {
			long long tMin = inBuffer->getTMin();
			long long tMax = inBuffer->getTMax();
			unsigned nEvents =  inBuffer->getSize();
			
			for(unsigned i = 0; i < nEvents; i++) {
				RawPulse & raw = inBuffer->get(i);
				
				
				if(raw.time < tMin || raw.time >= tMax)
					continue;
			
				if (raw.feType == RawPulse::TOFPET) {
					if(raw.d.tofpet.frameID < currentFrameID) {					
						events_missed++;
						continue;
					}
					
					if(raw.d.tofpet.frameID != currentFrameID){
						DAQ::ENDOTOFPET::FrameHeader FrHeaderOut = {
								0x01,
								raw.d.tofpet.frameID,
								0,
						};				
						fwrite(&FrHeaderOut, sizeof(DAQ::ENDOTOFPET::FrameHeader), 1, dataFile);
						currentFrameID=raw.d.tofpet.frameID;
					}				
			
					DAQ::ENDOTOFPET::RawTOFPET eventOut = {
							0x02,
							raw.d.tofpet.tac,
							raw.channelID,
							raw.d.tofpet.tcoarse,
							raw.d.tofpet.ecoarse,
							raw.d.tofpet.tfine,
							raw.d.tofpet.efine,
							raw.d.tofpet.tacIdleTime,
							raw.channelIdleTime};
					
					fwrite(&eventOut, sizeof(DAQ::ENDOTOFPET::RawTOFPET), 1, dataFile);	     
				}
				
				else if (raw.feType == RawPulse::STIC) {\
					if(raw.d.stic.frameID < currentFrameID) {					
						events_missed++;
						continue;
					}
					
					if(raw.d.stic.frameID != currentFrameID){
						DAQ::ENDOTOFPET::FrameHeader FrHeaderOut = {
								0x01,
								raw.d.stic.frameID,
								0,
						};				
						fwrite(&FrHeaderOut, sizeof(DAQ::ENDOTOFPET::FrameHeader), 1, dataFile);
						currentFrameID=raw.d.stic.frameID;
					}				
			
					DAQ::ENDOTOFPET::RawSTICv3 eventOut = {
							0x03,
							raw.channelID,
							raw.d.stic.tcoarse,
							raw.d.stic.ecoarse,
							raw.d.stic.tfine,
							raw.d.stic.efine,
							raw.d.stic.tBadHit,
							raw.d.stic.eBadHit,
							raw.channelIdleTime};
					
					fwrite(&eventOut, sizeof(DAQ::ENDOTOFPET::RawSTICv3), 1, dataFile);	     
				}
					
			}
	  
			return inBuffer;
	  };

	
	  void pushT0(double t0) { };	
	  void report() { };
private: 
	  FILE *dataFile;
	  float step1;
	  float step2;
	  uint32_t currentFrameID;
	  uint32_t events_missed;
	  uint32_t events_passed;
	  
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
	
	

/*	char dataFileName[512];
	char indexFileName[512];
	sprintf(dataFileName, "%s.raw2", outputFilePrefix);
	sprintf(indexFileName, "%s.idx2", outputFilePrefix);
	
	FILE *outputDataFile = fopen(dataFileName, "w");
	FILE *outputIndexFile = fopen(indexFileName, "w");*/

	FILE *outputDataFile = fopen(outputFilePrefix, "w");
		
	
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
			assert (cWindow == 0); // We don't support coincidence filtering when writing to endotofpet files yet
			if (cWindow == 0) {
				sink = new EventWriter_ENDOTOFPET_RP(outputDataFile, step1, step2,
					new NullSink<RawPulse>()
					);
			}
			else {
// 				sink = new CoarseExtract(false,
// 					new SingleReadoutGrouper(
// 					new CrystalPositions(SYSTEM_NCRYSTALS, Common::getCrystalMapFileName(),
// 					new NaiveGrouper(20, 100E-9,
// 					new CoincidenceFilter(cWindow, 0,
// 					new EventWriter_GP(outputDataFile, outputIndexFile, step1, step2,
// 					new NullSink<GammaPhoton>()
// 					))))));		
			}
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
				Event &eventIn = dataFrame.events[i];
				SortEntry entry = { eventIn.tCoarse, i };
				sortList.push_back(entry);				
			}
			
			//fprintf(stderr, "Frames has %d events, sort list has %u entries\n", dataFrame.nEvents, sortList.size());
			
			sort(sortList.begin(), sortList.end());

			if(outBuffer == NULL) {
				outBuffer = new EventBuffer<RawPulse>(outBlockSize);
			}

			for(unsigned j = 0; j < sortList.size(); j++) {
				Event &eventIn = dataFrame.events[sortList[j].index];			
				
				long long correctedFrameID = dataFrame.frameID + nWraps * 0x100000000LL;				
				RawPulse &p = outBuffer->getWriteSlot();

				if (eventIn.type == 0) {
					p.feType = RawPulse::TOFPET;
					p.d.tofpet.T = T * 1E12;
					p.time = (1024LL * correctedFrameID + eventIn.tCoarse) * p.d.tofpet.T;
					p.channelID = 64 * eventIn.asicID + eventIn.channelID;
					p.region = (64 * eventIn.asicID + eventIn.channelID) / 16;
					p.d.tofpet.frameID = correctedFrameID;
					p.d.tofpet.tac = eventIn.d.tofpet.tacID;
					p.d.tofpet.tcoarse = eventIn.tCoarse;
					p.d.tofpet.ecoarse = eventIn.d.tofpet.eCoarse;
					p.d.tofpet.tfine =  eventIn.d.tofpet.tFine;
					p.d.tofpet.efine = eventIn.d.tofpet.eFine;
					p.channelIdleTime = eventIn.d.tofpet.channelIdleTime;				
					p.d.tofpet.tacIdleTime = eventIn.d.tofpet.tacIdleTime;
				}
				else if (eventIn.type == 1) {
					p.feType = RawPulse::STIC;					
					p.d.stic.T = T * 1E12;
					unsigned tCoarse = (eventIn.d.sticv3.tCoarseL  >> 2) & 0x3FF;					
					p.time = (1024LL * correctedFrameID + tCoarse) * p.d.stic.T;
					p.channelID = 64 * eventIn.asicID + eventIn.channelID;
					p.region = (64 * eventIn.asicID + eventIn.channelID) / 16;
					p.d.stic.frameID = correctedFrameID;
					p.d.stic.tcoarse = eventIn.d.sticv3.tCoarseL;
					p.d.stic.ecoarse = eventIn.d.sticv3.eCoarseL;
					p.d.stic.tfine =  eventIn.d.sticv3.tFine;
					p.d.stic.efine = eventIn.d.sticv3.eFine;
					p.channelIdleTime = eventIn.d.sticv3.channelIdleTime;
					p.d.stic.tBadHit = eventIn.d.sticv3.tBadHit;
					p.d.stic.eBadHit = eventIn.d.sticv3.eBadHit;
				}
				
				
									
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
	

	
	//fclose(outputIndexFile);
	fclose(outputDataFile);
	
	return 0;
}
