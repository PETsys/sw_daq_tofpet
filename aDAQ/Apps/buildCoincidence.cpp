#include <TFile.h>
#include <TNtuple.h>
#include <Common/Constants.hpp>
#include <Common/Utils.hpp>
#include <TOFPET/RawV3.hpp>
#include <TOFPET/P2Extract.hpp>
#include <Core/PulseFilter.hpp>
#include <Core/SingleReadoutGrouper.hpp>
#include <Core/CrystalPositions.hpp>
#include <Core/NaiveGrouper.hpp>
#include <Core/CoincidenceGrouper.hpp>
#include <Core/CoincidenceFilter.hpp>
#include <assert.h>
#include <math.h>
#include <string.h>
#include <getopt.h>

static float		eventStep1;
static float		eventStep2;
static long long 	stepBegin;
static long long 	stepEnd;

static unsigned short	event1N;
static unsigned short	event1J;
static unsigned 	event1DeltaT;
static long long	event1Time;
static unsigned short	event1Channel;
static float		event1ToT;
static double		event1ChannelIdleTime;
static unsigned short	event1Tac;
static double		event1TacIdleTime;
static unsigned short	event1TFine;
static float		event1TQT;
static float		event1TQE;
static int		event1Xi;
static int		event1Yi;
static float		event1X;
static float 		event1Y;
static float 		event1Z;

static unsigned short	event2N;
static unsigned short	event2J;
static unsigned 	event2DeltaT;
static long long	event2Time;
static unsigned short	event2Channel;
static float		event2ToT;
static double		event2ChannelIdleTime;
static unsigned short	event2Tac;
static double		event2TacIdleTime;
static unsigned short	event2TFine;
static float		event2TQT;
static float		event2TQE;
static int		event2Xi;
static int		event2Yi;
static float		event2X;
static float 		event2Y;
static float 		event2Z;



using namespace DAQ;
using namespace DAQ::Core;
using namespace DAQ::TOFPET;
using namespace std;



class EventWriter : public EventSink<Coincidence> {
public:
	EventWriter(TTree *lmTree, float maxDeltaT, int maxN)
	: lmTree(lmTree), maxDeltaT((long long)(maxDeltaT*1E12)), maxN(maxN)
	{
	};
	
	~EventWriter() {
		
	};

	void pushEvents(EventBuffer<Coincidence> *buffer) {
		if(buffer == NULL) return;	
		
		unsigned nEvents = buffer->getSize();
		for(unsigned i = 0; i < nEvents; i++) {
			Coincidence &c = buffer->get(i);
			
			for(int j1 = 0; (j1 < c.photons[0].nHits) && (j1 < maxN); j1 ++) {
				long long t0_1 = c.photons[0].hits[0].time;		
				
				for(int j2 = 0; (j2 < c.photons[1].nHits) && (j2 < maxN); j2++) {
					long long t0_2 = c.photons[1].hits[0].time;
		
					Hit &hit1 = c.photons[0].hits[j1];
					Hit &hit2 = c.photons[1].hits[j2];
					
					long long T = hit1.raw.top.raw.T;
					
					float dt1 = hit1.time - t0_1;
					if(dt1 > maxDeltaT) continue;
					
					float dt2 = hit2.time - t0_1;
					if(dt2 > maxDeltaT) continue;
					
					
					event1J = j1;
					event1N = c.photons[0].nHits;
					event1DeltaT = dt1;
					event1Time = hit1.time;
					event1Channel = hit1.raw.top.channelID;
					event1ToT = 1E-3*(hit1.raw.top.timeEnd - hit1.raw.top.time);
					event1Tac = hit1.raw.top.raw.d.tofpet.tac;
					event1ChannelIdleTime = hit1.raw.top.raw.channelIdleTime * T * 1E-12;
					event1TacIdleTime = hit1.raw.top.raw.d.tofpet.tacIdleTime * T * 1E-12;
					event1TQT = hit1.raw.top.tofpet_TQT;
					event1TQE = hit1.raw.top.tofpet_TQE;
					event1X = hit1.x;
					event1Y = hit1.y;
					event1Z = hit1.z;
					event1Xi = hit1.xi;
					event1Yi = hit1.yi;			
						
					event2J = j2;
					event2N = c.photons[1].nHits;
					event2DeltaT = dt2;
					event2Time = hit2.time;
					event2Channel = hit2.raw.top.channelID;
					event2ToT = 1E-3*(hit2.raw.top.timeEnd - hit2.raw.top.time);
					event2Tac = hit2.raw.top.raw.d.tofpet.tac;
					event2ChannelIdleTime = hit2.raw.top.raw.channelIdleTime * T * 1E-12;
					event2TacIdleTime = hit2.raw.top.raw.d.tofpet.tacIdleTime * T * 1E-12;
					event2TQT = hit2.raw.top.tofpet_TQT;
					event2TQE = hit2.raw.top.tofpet_TQE;
					event2X = hit2.x;
					event2Y = hit2.y;
					event2Z = hit2.z;
					event2Xi = hit2.xi;
					event2Yi = hit2.yi;					

					lmTree->Fill();
				}
					
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

void displayHelp(char * program)
{
	fprintf(stderr, "usage: %s setup_file rawfiles_prefix output_file\n", program);
	fprintf(stderr, "\noptional arguments:\n");
	fprintf(stderr,  "  --help \t\t\t Show this help message and exit \n");
	fprintf(stderr, "\npositional arguments:\n");
	fprintf(stderr, "  setup_file \t\t\t File containing paths to tdc calibration files (required) and tq correction files (optional)\n");
	fprintf(stderr, "  rawfiles_prefix \t\t Path to raw data files prefix\n");
	fprintf(stderr, "  output_file \t\t\t ROOT output file containing coincidence events TTree\n");
};

void displayUsage( char * program)
{
	fprintf(stderr, "usage: %s setup_file rawfiles_prefix output_file.root\n", program);
};

int main(int argc, char *argv[])
{


	static struct option longOptions[] = {
		{ "help", no_argument, 0, 0 }
	};
	int optionIndex = -1;
	if (int c=getopt_long(argc, argv, "",longOptions, &optionIndex) !=-1) {
		if(optionIndex==0){
			displayHelp(argv[0]);
			return(1);
		}
		else{
			displayUsage(argv[0]);
			fprintf(stderr, "\n%s: error: Unknown option!\n", argv[0]);
			return(1);
		}
	}
   
	if(argc < 4){
		displayUsage(argv[0]);
		fprintf(stderr, "\n%s: error: too few arguments!\n", argv[0]);
		return(1);
	}
	else if(argc > 4){
		displayUsage(argv[0]);
		fprintf(stderr, "\n%s: error: too many arguments!\n", argv[0]);
		return(1);
	}


	char *inputFilePrefix = argv[2];

	char dataFileName[512];
	char indexFileName[512];
	sprintf(dataFileName, "%s.raw3", inputFilePrefix);
	sprintf(indexFileName, "%s.idx3", inputFilePrefix);
	FILE *inputDataFile = fopen(dataFileName, "rb");
	FILE *inputIndexFile = fopen(indexFileName, "r");
	
	DAQ::TOFPET::RawScannerV3 * scanner = new DAQ::TOFPET::RawScannerV3(inputIndexFile);
	
	TOFPET::P2 *P2 = new TOFPET::P2(SYSTEM_NCRYSTALS);
	if (strcmp(argv[1], "none") == 0) {
		P2->setAll(2.0);
		printf("BIG FAT WARNING: no calibration\n");
	} 
	else {
		P2->loadFiles(argv[1], true, false,0,0);
	}
	
	TFile *lmFile = new TFile(argv[3], "RECREATE");
	TTree *lmData = new TTree("lmData", "Event List", 2);
	int bs = 512*1024;
	lmData->Branch("step1", &eventStep1, bs);
	lmData->Branch("step2", &eventStep2, bs);
	
	lmData->Branch("mh_n1", &event1N, bs);
	lmData->Branch("mh_j1", &event1J, bs);
	lmData->Branch("mt_dt1", &event1DeltaT, bs);
	lmData->Branch("time1", &event1Time, bs);
	lmData->Branch("channel1", &event1Channel, bs);
	lmData->Branch("tot1", &event1ToT, bs);
	lmData->Branch("tac1", &event1Tac, bs);
	lmData->Branch("channelIdleTime1", &event1ChannelIdleTime, bs);
	lmData->Branch("tacIdleTime1", &event1TacIdleTime, bs);
	lmData->Branch("tqT1", &event1TQT, bs);
	lmData->Branch("tqE1", &event1TQE, bs);
	lmData->Branch("xi1", &event1Xi, bs);
	lmData->Branch("yi1", &event1Yi, bs);
	lmData->Branch("x1", &event1X, bs);
	lmData->Branch("y1", &event1Y, bs);
	lmData->Branch("z1", &event1Z, bs);
	
	lmData->Branch("mh_n2", &event2N, bs);
	lmData->Branch("mh_j2", &event2J, bs);
	lmData->Branch("mt_dt2", &event2DeltaT, bs);
	lmData->Branch("time2", &event2Time, bs);
	lmData->Branch("channel2", &event2Channel, bs);
	lmData->Branch("tot2", &event2ToT, bs);
	lmData->Branch("tac2", &event2Tac, bs);
	lmData->Branch("channelIdleTime2", &event2ChannelIdleTime, bs);
	lmData->Branch("tacIdleTime2", &event2TacIdleTime, bs);
	lmData->Branch("tqT2", &event2TQT, bs);
	lmData->Branch("tqE2", &event2TQE, bs);
	lmData->Branch("xi2", &event2Xi, bs);
	lmData->Branch("yi2", &event2Yi, bs);
	lmData->Branch("x2", &event2X, bs);
	lmData->Branch("y2", &event2Y, bs);
	lmData->Branch("z2", &event2Z, bs);	
	
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
		if(N!=1){
			if (strcmp(argv[1], "none") == 0) {
				P2->setAll(2.0);
				printf("BIG FAT WARNING: no calibration file\n");
			} 
			else{
				P2->loadFiles(argv[1], true, true,eventStep1,eventStep2);
			}
		}
		
		float cWindow = 20E-9; // s
		float gWindow = 100E-9; // s
		float gRadius = 20; // mm 
		float minToT = 150E-9; // s
		
		// Round up cWindow and minToT for use in CoincidenceFilter
		float cWindowCoarse = (ceil(cWindow/SYSTEM_PERIOD) + 1) * SYSTEM_PERIOD;
		float minToTCoarse = (ceil(minToT/SYSTEM_PERIOD) + 2) * SYSTEM_PERIOD;
		
		float minEnergy = minToT * 1E9; // In the abscense of energy calibration, energy values are just ToT converted to ns
		DAQ::TOFPET::RawReaderV3 *reader = new DAQ::TOFPET::RawReaderV3(inputDataFile, SYSTEM_PERIOD,  eventsBegin, eventsEnd, 
				new CoincidenceFilter(Common::getCrystalMapFileName(), cWindowCoarse, minToTCoarse,
				new P2Extract(P2, false, 0.0, 0.20,
				new SingleReadoutGrouper(
				new CrystalPositions(SYSTEM_NCRYSTALS, Common::getCrystalMapFileName(),
				new NaiveGrouper(gRadius, gWindow, minEnergy,
				new CoincidenceGrouper(cWindow,
				new EventWriter(lmData, gWindow, 1

		))))))));
		
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
