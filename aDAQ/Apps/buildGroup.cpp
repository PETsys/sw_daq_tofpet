#include <TFile.h>
#include <TNtuple.h>
#include <TOFPET/RawV2.hpp>
#include <TOFPET/P2Extract.hpp>
#include <Core/PulseFilter.hpp>
#include <Core/SingleReadoutGrouper.hpp>
#include <Core/CrystalPositions.hpp>
#include <Core/ComptonGrouper.hpp>
#include <Core/NaiveGrouper.hpp>
#include <Core/CoincidenceFilter.hpp>
#include <Core/CoincidenceGrouper.hpp>
#include <Common/Constants.hpp>
#include <Common/Utils.hpp>
#include <assert.h>
#include <math.h>
#include <string.h>


static float		eventStep1;
static float		eventStep2;
static long long 	stepBegin;
static long long 	stepEnd;

static unsigned short	eventN;
static unsigned short	eventJ;
static unsigned 	eventDeltaT;
static long long	eventTime;
static unsigned short	eventChannel;
static float		eventToT;
static double		eventChannelIdleTime;
static unsigned short	eventTac;
static double		eventTacIdleTime;
static unsigned short	eventTFine;
static float		eventTQT;
static float		eventTQE;
static int		eventXi;
static int		eventYi;
static float		eventX;
static float 		eventY;
static float 		eventZ;

using namespace DAQ;
using namespace DAQ::Core;
using namespace DAQ::TOFPET;
using namespace std;



class EventWriter : public EventSink<GammaPhoton> {
public:
	EventWriter(TTree *lmTree, float maxDeltaT, int maxN) 
	: lmTree(lmTree), maxDeltaT((long long)(maxDeltaT*1E12)), maxN(maxN)
	{		
	};
	
	~EventWriter() {
		
	};

	void pushEvents(EventBuffer<GammaPhoton> *buffer) {
		if(buffer == NULL) return;	
		
		unsigned nEvents = buffer->getSize();
		for(unsigned i = 0; i < nEvents; i++) {
			GammaPhoton &e = buffer->get(i);

			long long t0 = e.hits[0].time;
			for(int j1 = 0; (j1 < e.nHits) && (j1 < maxN); j1 ++) {
				Hit &hit = e.hits[j1];
				
				float dt = hit.time - t0;
				if(dt > maxDeltaT) continue;
				
				long long T = hit.raw.top.raw.d.tofpet.T;
				eventJ = j1;
				eventN = e.nHits;
				eventDeltaT = dt;
				eventTime = hit.time;
				eventChannel = hit.raw.top.channelID;
				eventToT = 1E-3*(hit.raw.top.timeEnd - hit.raw.top.time);
				eventTac = hit.raw.top.raw.d.tofpet.tac;
				eventChannelIdleTime = hit.raw.top.raw.channelIdleTime * T * 1E-12;
				eventTacIdleTime = hit.raw.top.raw.d.tofpet.tacIdleTime * T * 1E-12;
				eventTQT = hit.raw.top.tofpet_TQT;
				eventTQE = hit.raw.top.tofpet_TQE;
				eventX = hit.x;
				eventY = hit.y;
				eventZ = hit.z;
				eventXi = hit.xi;
				eventYi = hit.yi;
				
				//printf("%20lld difference: %8lld tot: %6f\n", t0, hit.time - t0, eventToT);

				
				lmTree->Fill();
			}
				
			
		}
		
		delete buffer;
	};
	
	void pushT0(double t0) { };
	void finish() { };
	void report() { };
private: 
	TTree *lmTree;
	long long maxDeltaT;
	int maxN;
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
	lmData->Branch("mh_n", &eventN, bs);
	lmData->Branch("mh_j", &eventJ, bs);
	lmData->Branch("mt_dt", &eventDeltaT, bs);
	lmData->Branch("time", &eventTime, bs);
	lmData->Branch("channel", &eventChannel, bs);
	lmData->Branch("tot", &eventToT, bs);
	lmData->Branch("tac", &eventTac, bs);
	lmData->Branch("channelIdleTime", &eventChannelIdleTime, bs);
	lmData->Branch("tacIdleTime", &eventTacIdleTime, bs);
	lmData->Branch("tqT", &eventTQT, bs);
	lmData->Branch("tqE", &eventTQE, bs);
	lmData->Branch("xi", &eventXi, bs);
	lmData->Branch("yi", &eventYi, bs);
	lmData->Branch("x", &eventX, bs);
	lmData->Branch("y", &eventY, bs);
	lmData->Branch("z", &eventZ, bs);
	
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
		printf("Step %3d of %3d: %f %f (%llu to %llu)\n", step+1, scanner->getNSteps(), eventStep1, eventStep2, eventsBegin, eventsEnd);
		//		eventsEnd=1000000000+eventsBegin;
// 		printf("BIG FAT WARNING: limiting event number\n");
// 		if(eventsEnd > eventsBegin + 10E6) eventsEnd = eventsBegin + 10E6;

		DAQ::TOFPET::RawReaderV2 *reader = new DAQ::TOFPET::RawReaderV2(inputDataFile, SYSTEM_PERIOD,  eventsBegin, eventsEnd, 
								      
				new P2Extract(P2, false, 1.0, 1.0,
				new SingleReadoutGrouper(
				new CrystalPositions(SYSTEM_NCRYSTALS, Common::getCrystalMapFileName(),
				new NaiveGrouper(20, 100E-9,
				new EventWriter(lmData, 5E-9, 4

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
