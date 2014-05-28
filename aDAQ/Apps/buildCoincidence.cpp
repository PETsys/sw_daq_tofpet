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
					
/*					if(crystals[0]->crystalID != 56) continue;
					if(crystals[1]->crystalID != 9) continue;*/
						
					long long T = crystals[0]->top.raw.d.tofpet.T;
					long long delta = crystals[0]->time - crystals[1]->time;

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
	TNtuple *lmData = new TNtuple("lmData", "Event List", "step1:step2:frameid:time1:crystal1:tac1:tot1:n1:time2:crystal2:tac2:tot2:n2:delta");
	
	int N = scanner->getNSteps();
	for(int step = 0; step < N; step++) {
		Float_t step1;
		Float_t step2;
		unsigned long long eventsBegin;
		unsigned long long eventsEnd;
		scanner->getStep(step, step1, step2, eventsBegin, eventsEnd);
		printf("Step %3d of %3d: %f %f (%llu to %llu)\n", step+1, scanner->getNSteps(), step1, step2, eventsBegin, eventsEnd);

// 		printf("BIG FAT WARNING: limiting event number\n");
// 		if(eventsEnd > eventsBegin + 10E6) eventsEnd = eventsBegin + 10E6;

		const unsigned nChannels = 2*128; 
		DAQ::TOFPET::RawReaderV2 *reader = new DAQ::TOFPET::RawReaderV2(inputDataFile, 6.25E-9,  eventsBegin, eventsEnd, 

								      
				new P2Extract(P2, false,
				new PulseFilter(-INFINITY, INFINITY, 	
				new SingleReadoutGrouper(
				new FakeCrystalPositions(
				new ComptonGrouper(20, 20E-9, GammaPhoton::maxHits, 0, INFINITY,
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
