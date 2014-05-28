#include <TFile.h>
#include <TNtuple.h>
#include <TOFPET/RawV2.hpp>
#include <TOFPET/P2Extract.hpp>
#include <TOFPET/P2.hpp>
#include <Core/SingleReadoutGrouper.hpp>
#include <Core/FakeCrystalPositions.hpp>
#include <Core/ComptonGrouper.hpp>
#include <Core/CoincidenceGrouper.hpp>
#include <assert.h>
#include <math.h>
#include <string.h>

using namespace DAQ;
using namespace DAQ::Core;
using namespace DAQ::TOFPET;
using namespace std;


static float		eventStep1;
static float		eventStep2;
static long long	eventTime;
static unsigned short	eventChannel;
static float		eventToT;
static double		eventChannelIdleTime;
static unsigned short	eventTac;
static double		eventTacIdleTime;
static unsigned short	eventTFine;
static float		eventTQT;
static float		eventTQE;

class EventWriter : public EventSink<Hit> {



public:
	EventWriter(TTree *lmDataTuple, float step1, float step2) 
	: lmDataTuple(lmDataTuple), step1(step1), step2(step2) {
		
	};
	
	~EventWriter() {
		
	};

	void pushEvents(EventBuffer<Hit> *buffer) {
		if(buffer == NULL) return;	
		
		unsigned nEvents = buffer->getSize();
		for(unsigned i = 0; i < nEvents; i++) {
			Hit &hit = buffer->get(i);
			
			RawHit &raw= hit.raw;
			long long T = raw.top.raw.d.tofpet.T;
			eventStep1 = step1;
			eventStep2 = step2;
			eventTime = raw.time;
			eventChannel = raw.top.channelID;
			eventToT = raw.energy;
			eventChannelIdleTime = 0;
			eventTac = raw.top.raw.d.tofpet.tac;
			eventChannelIdleTime = raw.top.raw.d.tofpet.channelIdleTime * T * 1E-12;
			eventTacIdleTime = raw.top.raw.d.tofpet.tacIdleTime * T * 1E-12;
			eventTQT = raw.top.tofpet_TQT;
			eventTQE = raw.top.tofpet_TQE;
			
			//printf("%lld %e %f\n", raw.top.raw.d.tofpet.tacIdleTime, eventTacIdleTime, eventTQ);
			
			lmDataTuple->Fill();
		}
		
		delete buffer;
	};
	
	void pushT0(double t0) { };
	void finish() { };
	void report() { };
private: 
	TTree *lmDataTuple;
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
	
	TOFPET::P2 *lut = new TOFPET::P2(128);
	if (strcmp(argv[1], "none") == 0) {
		lut->setAll(2.0);
		printf("BIG FAT WARNING: no calibration\n");
	} 
	else {
		lut->loadFiles(argv[1]);
	}
	
	TFile *lmFile = new TFile(argv[3], "RECREATE");	
	TTree *lmData = new TTree("lmData", "Event List", 2);
	int bs = 512*1024;
	lmData->Branch("step1", &eventStep1, bs);
	lmData->Branch("step2", &eventStep2, bs);
	lmData->Branch("time", &eventTime, bs);
	lmData->Branch("channel", &eventChannel, bs);
	lmData->Branch("tot", &eventToT, bs);
	lmData->Branch("tac", &eventTac, bs);
	lmData->Branch("channelIdleTime", &eventChannelIdleTime, bs);
	lmData->Branch("tacIdleTime", &eventTacIdleTime, bs);
	lmData->Branch("tqT", &eventTQT, bs);
	lmData->Branch("tqE", &eventTQE, bs);

	int N = scanner->getNSteps();
	for(int step = 0; step < N; step++) {
		Float_t step1;
		Float_t step2;
		unsigned long long eventsBegin;
		unsigned long long eventsEnd;
		scanner->getStep(step, step1, step2, eventsBegin, eventsEnd);
		printf("Step %3d of %3d: %f %f (%llu to %llu)\n", step+1, scanner->getNSteps(), step1, step2, eventsBegin, eventsEnd);

		const unsigned nChannels = 2*128; 
		DAQ::TOFPET::RawReaderV2 *reader = new DAQ::TOFPET::RawReaderV2(inputDataFile, 6.25E-9,  eventsBegin, eventsEnd, 

								      
				new P2Extract(lut, false,
				new SingleReadoutGrouper(
				new FakeCrystalPositions(
				new EventWriter(lmData, step1, step2

		)))));
		
		reader->wait();
		delete reader;
		lmFile->Write();
	}
	
	lmFile->Close();
	fclose(inputDataFile);
	fclose(inputIndexFile);
	return 0;
	
}
