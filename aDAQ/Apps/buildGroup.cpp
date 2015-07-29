#include <TFile.h>
#include <TNtuple.h>
#include <TOFPET/RawV3.hpp>
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
#include <getopt.h>

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
static float		eventEnergy;
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
				
				long long T = hit.raw.top.raw.T;
				eventJ = j1;
				eventN = e.nHits;
				eventDeltaT = dt;
				eventTime = hit.time;
				eventChannel = hit.raw.top.channelID;
				eventToT = 1E-3*(hit.raw.top.timeEnd - hit.raw.top.time);
				eventEnergy = hit.raw.top.energy;
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

void displayHelp(char * program)
{
	fprintf(stderr, "usage: %s setup_file rawfiles_prefix output_file\n", program);
	fprintf(stderr, "\noptional arguments:\n");
	fprintf(stderr,  "  --help \t\t\tShow this help message and exit \n");
	fprintf(stderr,  "  --onlineMode\t Use this flag to process data in real time during acquisition\n");
	fprintf(stderr,  "  --acqDeltaTime=ACQDELTATIME\t If online mode is chosen, this variable defines how much data time (in seconds) to process (default is -1 which selects all data for the current step)\n");
	fprintf(stderr,  "  --raw_version=RAW_VERSION\tThe version of the raw file to be processed: 2 or 3 (default) \n");
	fprintf(stderr,  "  --minEnergy=MINENERGY\t\tThe minimum energy (in keV) of an event to be considered valid. If no energy calibration file is available, the entered value will correspond to a minimum TOT in ns (default is 150 ns)\n");
	fprintf(stderr,  "  --maxEnergy=MAXENERGY\t\tThe maximum energy (in keV) of an event to be considered valid. If no energy calibration file is available, the entered value will correspond to a minimum TOT in ns (default is 500 ns)\n");
	fprintf(stderr,  "  --gWindow=gWINDOW\t\tMaximum delta time (in seconds) inside a given multi-hit group (default is 100E-9s)\n");
	fprintf(stderr,  "  --gMaxHits=gMAXHITS\t\tMaximum number of hits inside a given multi-hit group (default is 16)\n");
	fprintf(stderr,  "  --gMaxHitsRoot=gMAXHITSROOT\tMaximum number of hits inside a given multi-hit group to be written to ROOT output file (default is 16)\n");
	fprintf(stderr, "\npositional arguments:\n");
	fprintf(stderr, "  setup_file \t\t\tFile containing paths to tdc calibration file(s) (required), tQ correction file(s) (optional) and Energy calibration file(s) (optional)\n");
	fprintf(stderr, "  rawfiles_prefix\t\tPath to raw data files prefix\n");
	fprintf(stderr, "  output_file_prefix \t\t Output file containing grouped event data (extension .root will be created automatically)\n");
};

void displayUsage( char * program)
{
	fprintf(stderr, "usage: %s setup_file rawfiles_prefix output_file_prefix\n", program);
};

int main(int argc, char *argv[])
{


	static struct option longOptions[] = {
		{ "help", no_argument, 0, 0 },
		{ "onlineMode", no_argument,0,0 },
		{ "acqDeltaTime", optional_argument,0,0 },
		{ "raw_version", optional_argument,0,0 },
		{ "minEnergy", optional_argument,0,0 },
		{ "maxEnergy", optional_argument,0,0 },
		{ "gWindow", optional_argument,0,0 },
		{ "gMaxHits", optional_argument,0,0 },
		{ "gMaxHitsRoot", optional_argument,0,0 },
		{ NULL, 0, 0, 0 }
	};

	char rawV[128];
	rawV[0]='3';
	bool onlineMode=false;
	float readBackTime=-1;
	char * setupFileName=argv[1];
	char *inputFilePrefix = argv[2];
	char *outputFilePrefix = argv[3];
	char outputFileName[256];
	
	float gWindow = 100E-9; // s
	float minEnergy = 150; // keV or ns (if energy=tot)
	float maxEnergy = 500; // keV or ns (if energy=tot)
	int gMaxHits=16;
	int gMaxHitsRoot=16;

	int nOptArgs=0;
	while(1) {
		
		int optionIndex = 0;
		int c =getopt_long(argc, argv, "",longOptions, &optionIndex);
		if(c==-1) break;

		if(optionIndex==0){
			displayHelp(argv[0]);
			return(1);
		}
		else if(optionIndex==1){
			nOptArgs++;
			onlineMode=true;
		}
		else if(optionIndex==2){
			nOptArgs++;
			readBackTime=atof(optarg);
		}
		else if(optionIndex==3){
			nOptArgs++;
			sprintf(rawV,optarg);
			if(rawV[0]!='2' && rawV[0]!='3'){
				fprintf(stderr, "\n%s: error: Raw version not valid! Please choose 2 or 3\n", argv[0]);
				return(1);
			}
		}
		else if(optionIndex==4){
			nOptArgs++;
			minEnergy=atof(optarg);
		}
		else if(optionIndex==5){
			nOptArgs++;
			maxEnergy=atof(optarg);
		}
		else if(optionIndex==6){
			nOptArgs++;
			gWindow=atof(optarg);
		}
		else if(optionIndex==7){
			nOptArgs++;
			gMaxHits=atoi(optarg);
		}
		else if(optionIndex==8){
			nOptArgs++;
			gMaxHitsRoot=atoi(optarg);
		}
		else{
			displayUsage(argv[0]);
			fprintf(stderr, "\n%s: error: Unknown option!\n", argv[0]);
			return(1);
		}
	}
   
	if(argc - nOptArgs < 4){
		displayUsage(argv[0]);
		fprintf(stderr, "\n%s: error: too few arguments!\n", argv[0]);
		return(1);
	}
	if(argc - nOptArgs < 4){
		displayUsage(argv[0]);
		fprintf(stderr, "\n%s: error: too many arguments!\n", argv[0]);
		return(1);
	}


   

	DAQ::TOFPET::RawScanner *scanner = NULL;
	if(rawV[0]=='3')
		scanner = new DAQ::TOFPET::RawScannerV3(inputFilePrefix);
	else
		scanner = new DAQ::TOFPET::RawScannerV2(inputFilePrefix);

	TOFPET::P2 *P2 = new TOFPET::P2(SYSTEM_NCRYSTALS);
	if (strcmp(setupFileName, "none") == 0) {
		P2->setAll(2.0);
		printf("BIG FAT WARNING: no calibration\n");
	} 
	else {
		P2->loadFiles(setupFileName, true, false,0,0);
	}
	
	sprintf(outputFileName,"%s.root",outputFilePrefix);
	TFile *lmFile = new TFile(outputFileName, "RECREATE");
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
	lmData->Branch("Energy", &eventEnergy, bs);
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
		if(onlineMode)step=N-1;
		scanner->getStep(step, eventStep1, eventStep2, eventsBegin, eventsEnd);
		if(!onlineMode)printf("Step %3d of %3d: %f %f (%llu to %llu)\n", step+1, scanner->getNSteps(), eventStep1, eventStep2, eventsBegin, eventsEnd);
		if(N!=1){
			if (strcmp(argv[1], "none") == 0) {
				P2->setAll(2.0);
				printf("BIG FAT WARNING: no calibration file\n");
			} 
			else{
				P2->loadFiles(setupFileName, true, true,eventStep1,eventStep2);
			}
		}
		
	
		float gRadius = 20; // mm

		EventSink<RawPulse> * pipeSink =new P2Extract(P2, false, 0.0, 0.20, true,
				new SingleReadoutGrouper(
				new CrystalPositions(SYSTEM_NCRYSTALS, Common::getCrystalMapFileName(),
				new NaiveGrouper(gRadius, gWindow, minEnergy, maxEnergy, gMaxHits,
				new EventWriter(lmData, gWindow, gMaxHitsRoot 
								)))));

		DAQ::TOFPET::RawReader *reader=NULL;
	
		if(rawV[0]=='3') 
			reader = new DAQ::TOFPET::RawReaderV3(inputFilePrefix, SYSTEM_PERIOD,  eventsBegin, eventsEnd, readBackTime, onlineMode,pipeSink);
		else 
		    reader = new DAQ::TOFPET::RawReaderV2(inputFilePrefix, SYSTEM_PERIOD,  eventsBegin, eventsEnd, pipeSink);
		reader->wait();
		delete reader;
		
		stepEnd = lmData->GetEntries();
		lmIndex->Fill();
		stepBegin = stepEnd;

		lmFile->Write();
		
	}
	delete scanner;
	lmFile->Close();
	return 0;
	
}
