#include <TFile.h>
#include <TNtuple.h>
#include <TOFPET/RawV2.hpp>
#include <TOFPET/Sanity.hpp>
#include <TOFPET/P2Extract.hpp>
#include <TOFPET/P2.hpp>
#include <Common/Constants.hpp>
#include <Common/Utils.hpp>
#include <Core/SingleReadoutGrouper.hpp>
#include <Core/CrystalPositions.hpp>
#include <assert.h>
#include <math.h>
#include <string.h>

using namespace DAQ;
using namespace DAQ::Core;
using namespace DAQ::TOFPET;
using namespace std;


static float		eventStep1;
static float		eventStep2;
static long long 	stepBegin;
static long long 	stepEnd;

static long long	eventTime;
static unsigned short	eventChannel;
static float		eventToT;
static double		eventChannelIdleTime;
static unsigned short	eventTac;
static double		eventTacIdleTime;
static unsigned short	eventTFine;
static int		eventXi;
static int		eventYi;
static float		eventX;
static float 		eventY;
static float 		eventZ;
static float		eventTQT;
static float		eventTQE;

class EventWriter : public EventSink<Hit> {



public:
	EventWriter(TTree *lmDataTuple) 
		: lmDataTuple(lmDataTuple) {
		
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
			eventTime = raw.time;
			eventChannel = raw.top.channelID;
			eventToT = 1E-3*(raw.top.timeEnd - raw.top.time);
			eventTac = raw.top.raw.d.tofpet.tac;
			eventChannelIdleTime = raw.top.raw.channelIdleTime * T * 1E-12;
			eventTacIdleTime = raw.top.raw.d.tofpet.tacIdleTime * T * 1E-12;
			eventTQT = raw.top.tofpet_TQT;
			eventTQE = raw.top.tofpet_TQE;
			eventX = hit.x;
			eventY = hit.y;
			eventZ = hit.z;
			eventXi = hit.xi;
			eventYi = hit.yi;
			
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

};

int main(int argc, char *argv[])
{
	if (argc != 4) {
		fprintf(stderr, "USAGE: %s <setup_file> <rawfiles_prefix> <output_file.root>\n", argv[0]);
		fprintf(stderr, "setup_file - File containing setup layout and path to tdc calibration files (mezzanines.cal or similar)\n");
		fprintf(stderr, "rawfiles_prefix - Path to raw data files prefix\n");
		fprintf(stderr, "output_file.root - ROOT output file containing single events TTree\n");
		return 1;
	}		
	assert(argc == 4);
	char *inputFilePrefix = argv[2];

	char dataFileName[512];
	char indexFileName[512];
	sprintf(dataFileName, "%s.raw2", inputFilePrefix);
	sprintf(indexFileName, "%s.idx2", inputFilePrefix);
	FILE *inputDataFile = fopen(dataFileName, "r");
	FILE *inputIndexFile = fopen(indexFileName, "r");
	
	DAQ::TOFPET::RawScannerV2 * scanner = new DAQ::TOFPET::RawScannerV2(inputIndexFile);
	
	TOFPET::P2 *lut = new TOFPET::P2(4096);
	if (strcmp(argv[1], "none") == 0) {
		lut->setAll(2.0);
		printf("BIG FAT WARNING: no calibration\n");
	} 
	else {
		lut->loadFiles(argv[1], true,false,0,0);
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
	lmData->Branch("xi", &eventXi, bs);
	lmData->Branch("yi", &eventYi, bs);
	lmData->Branch("x", &eventX, bs);
	lmData->Branch("y", &eventY, bs);
	lmData->Branch("z", &eventY, bs);
	lmData->Branch("tqT", &eventTQT, bs);
	lmData->Branch("tqE", &eventTQE, bs);
	
	
	TTree *lmIndex = new TTree("lmIndex", "Step Index", 2);
	lmIndex->Branch("step1", &eventStep1, bs);
	lmIndex->Branch("step2", &eventStep2, bs);
	lmIndex->Branch("stepBegin", &stepBegin, bs);
	lmIndex->Branch("stepEnd", &stepEnd, bs);	

	stepBegin = 0;
	stepEnd = 0;
	int N = scanner->getNSteps();
	for(int step = 0; step < N; step++) {
		unsigned long long eventsBegin;
		unsigned long long eventsEnd;
		scanner->getStep(step, eventStep1, eventStep2, eventsBegin, eventsEnd);
		
		//if(eventsEnd > eventsBegin + 1000000)
	//		eventsEnd = eventsBegin + 1000000;
		
		printf("Step %3d of %3d: %f %f (%llu to %llu)\n", step+1, scanner->getNSteps(), eventStep1, eventStep2, eventsBegin, eventsEnd);
		if(N!=1){
			if (strcmp(argv[1], "none") == 0) {
				lut->setAll(2.0);
				printf("BIG FAT WARNING: no calibration file\n");
			} 
			else{
				lut->loadFiles(argv[1], true, true,eventStep1,eventStep2);
			}
		}
	
		DAQ::TOFPET::RawReaderV2 *reader = new DAQ::TOFPET::RawReaderV2(inputDataFile, SYSTEM_PERIOD,  eventsBegin, eventsEnd, 

				new Sanity(100E-9, 		      
				new P2Extract(lut, false, 0.0, 0.20,
				new SingleReadoutGrouper(
				new CrystalPositions(SYSTEM_NCRYSTALS, Common::getCrystalMapFileName(),
				new EventWriter(lmData

		))))));
		
		reader->wait();
		delete reader;
		
		stepEnd = lmData->GetEntries();
		lmIndex->Fill();
		stepBegin = stepEnd;
		
		lmFile->Write();
	}
	
	lmFile->Close();
	fclose(inputDataFile);
	fclose(inputIndexFile);
	return 0;
	
}
