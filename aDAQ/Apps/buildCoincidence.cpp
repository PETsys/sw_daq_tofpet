#include <TFile.h>
#include <TNtuple.h>
#include <TOFPET/RawV2.hpp>
#include <TOFPET/P2Extract.hpp>
#include <Core/PulseFilter.hpp>
#include <Core/SingleReadoutGrouper.hpp>
#include <Core/FakeCrystalPositions.hpp>
#include <Core/ComptonGrouper.hpp>
#include <Core/CoincidenceFilter.hpp>
#include <Core/CoincidenceGrouper.hpp>
#include <assert.h>
#include <math.h>
#include <string.h>


static float		eventStep1;
static float		eventStep2;

static long long	event1Time;
static unsigned short	event1Channel;
static float		event1ToT;
static double		event1ChannelIdleTime;
static unsigned short	event1Tac;
static double		event1TacIdleTime;
static unsigned short	event1TFine;
static float		event1TQT;
static float		event1TQE;
static long long	event2Time;
static unsigned short	event2Channel;
static float		event2ToT;
static double		event2ChannelIdleTime;
static unsigned short	event2Tac;
static double		event2TacIdleTime;
static unsigned short	event2TFine;
static float		event2TQT;
static float		event2TQE;



using namespace DAQ;
using namespace DAQ::Core;
using namespace DAQ::TOFPET;
using namespace std;



class EventWriter : public EventSink<Coincidence> {
public:
	EventWriter(TTree *lmDataTuple, float step1, float step2) 
	: lmTuple(lmDataTuple), step1(step1), step2(step2) {
		
	};
	
	~EventWriter() {
		
	};

	void pushEvents(EventBuffer<Coincidence> *buffer) {
		if(buffer == NULL) return;	
		
		unsigned nEvents = buffer->getSize();
		for(unsigned i = 0; i < nEvents; i++) {
			Coincidence &c = buffer->get(i);

			for(int j1 = 0; j1 < c.photons[0].nHits; j1 ++) {
				for(int j2 = 0; j2 < c.photons[1].nHits; j2++) {
					RawHit *crystals[2] = { 
						&c.photons[0].hits[j1].raw, 
						&c.photons[1].hits[j2].raw 
					};
					
					long long T = crystals[0]->top.raw.d.tofpet.T;
					
					eventStep1 = step1;
					eventStep2 = step2;
	       
					event1Time = crystals[0]->time;
					event1Channel = crystals[0]->top.channelID;
					event1ToT = 1E-3*(crystals[0]->top.timeEnd - crystals[0]->top.time),
					event1Tac = crystals[0]->top.raw.d.tofpet.tac;
					event1ChannelIdleTime = crystals[0]->top.raw.d.tofpet.channelIdleTime * T * 1E-12;
					event1TacIdleTime = crystals[0]->top.raw.d.tofpet.tacIdleTime * T * 1E-12;
					event1TQT = crystals[0]->top.tofpet_TQT;
					event1TQE = crystals[0]->top.tofpet_TQE;
					
					event2Time = crystals[1]->time;
					event2Channel = crystals[1]->top.channelID;
					event2ToT = 1E-3*(crystals[1]->top.timeEnd - crystals[1]->top.time),
					event2Tac = crystals[1]->top.raw.d.tofpet.tac;
					event2ChannelIdleTime = crystals[1]->top.raw.d.tofpet.channelIdleTime * T * 1E-12;
					event2TacIdleTime = crystals[1]->top.raw.d.tofpet.tacIdleTime * T * 1E-12;
					event2TQT = crystals[1]->top.tofpet_TQT;
					event2TQE = crystals[1]->top.tofpet_TQE;				

					lmTuple->Fill();
				}
					
			}
			
		}
		
		delete buffer;
	};
	
	void pushT0(double t0) { };
	void finish() { };
	void report() { };
private: 
	TTree *lmTuple;
	float step1;
	float step2;
};

int main(int argc, char *argv[])
{
	assert(argc == 4);
	char *inputFilePrefix = argv[2];

	char dataFileName[512];
	char indexFileName[512];
	sprintf(dataFileName, "%s.raw2", inputFilePrefix);
	sprintf(indexFileName, "%s.idx2", inputFilePrefix);
	FILE *inputDataFile = fopen(dataFileName, "r");
	FILE *inputIndexFile = fopen(indexFileName, "r");
	
	DAQ::TOFPET::RawScannerV2 * scanner = new DAQ::TOFPET::RawScannerV2(inputIndexFile);
	
	TOFPET::P2 *P2 = new TOFPET::P2(128);
	if (strcmp(argv[1], "none") == 0) {
		P2->setAll(2.0);
		printf("BIG FAT WARNING: no calibration\n");
	} 
	else {
		P2->loadFiles(argv[1]);
	}
	
	TFile *lmFile = new TFile(argv[3], "RECREATE");
	TTree *lmData = new TTree("lmData", "Event List", 2);
	int bs = 512*1024;
	lmData->Branch("step1", &eventStep1, bs);
	lmData->Branch("step2", &eventStep2, bs);
	lmData->Branch("time1", &event1Time, bs);
	lmData->Branch("channel1", &event1Channel, bs);
	lmData->Branch("tot1", &event1ToT, bs);
	lmData->Branch("tac1", &event1Tac, bs);
	lmData->Branch("channelIdleTime1", &event1ChannelIdleTime, bs);
	lmData->Branch("tacIdleTime1", &event1TacIdleTime, bs);
	lmData->Branch("tqT1", &event1TQT, bs);
	lmData->Branch("tqE1", &event1TQE, bs);	
	lmData->Branch("time2", &event2Time, bs);
	lmData->Branch("channel2", &event2Channel, bs);
	lmData->Branch("tot2", &event2ToT, bs);
	lmData->Branch("tac2", &event2Tac, bs);
	lmData->Branch("channelIdleTime2", &event2ChannelIdleTime, bs);
	lmData->Branch("tacIdleTime2", &event2TacIdleTime, bs);
	lmData->Branch("tqT2", &event2TQT, bs);
	lmData->Branch("tqE2", &event2TQE, bs);	
	
	int N = scanner->getNSteps();
	for(int step = 0; step < N; step++) {
		Float_t step1;
		Float_t step2;
		unsigned long long eventsBegin;
		unsigned long long eventsEnd;
		scanner->getStep(step, step1, step2, eventsBegin, eventsEnd);
		printf("Step %3d of %3d: %f %f (%llu to %llu)\n", step+1, scanner->getNSteps(), step1, step2, eventsBegin, eventsEnd);
		//		eventsEnd=1000000000+eventsBegin;
// 		printf("BIG FAT WARNING: limiting event number\n");
// 		if(eventsEnd > eventsBegin + 10E6) eventsEnd = eventsBegin + 10E6;

		const unsigned nChannels = 2*128; 
		DAQ::TOFPET::RawReaderV2 *reader = new DAQ::TOFPET::RawReaderV2(inputDataFile, 6.25E-9,  eventsBegin, eventsEnd, 

								      
				new P2Extract(P2, false, true, true,
				new PulseFilter(-INFINITY, INFINITY, 	
				new SingleReadoutGrouper(
				new FakeCrystalPositions(
				new ComptonGrouper(20, 20E-9, GammaPhoton::maxHits, -INFINITY, INFINITY,
				new CoincidenceGrouper(20E-9,
				new EventWriter(lmData, step1, step2

		))))))));
		
		reader->wait();
		delete reader;
		lmFile->Write();
	}
	
	lmFile->Close();
	fclose(inputDataFile);
	fclose(inputIndexFile);
	return 0;
	
}
