#include <TFile.h>
#include <TNtuple.h>
#include <ENDOTOFPET/Raw.hpp>
#include <ENDOTOFPET/Extract.hpp>
#include <Common/Constants.hpp>
#include <Common/Utils.hpp>
#include <TOFPET/RawV2.hpp>
#include <TOFPET/P2Extract.hpp>
#include <Core/PulseFilter.hpp>
#include <Core/SingleReadoutGrouper.hpp>
#include <Core/CrystalPositions.hpp>
#include <Core/NaiveGrouper.hpp>
#include <Core/CoincidenceGrouper.hpp>
#include <assert.h>
#include <math.h>
#include <string.h>
#include <getopt.h>


static unsigned short	event1J;
static long long	event1Time;
static unsigned short	event1Channel;
static float		event1ToT;
static double		event1ChannelIdleTime;
static unsigned short	event1Tac;
static double		event1TacIdleTime;
static unsigned short	event1TFine;
static float		event1TQT;
static float		event1TQE;

static unsigned short	event2J;
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
using namespace DAQ::ENDOTOFPET;
using namespace DAQ::TOFPET;
using namespace std;



class EventWriter : public EventSink<Coincidence> {
public:
	EventWriter(TTree *lmDataTuple) 
	: lmTuple(lmDataTuple) {
		  events_passed=0;
	};
	
	~EventWriter() {
		
	};

	void pushEvents(EventBuffer<Coincidence> *buffer) {
		if(buffer == NULL) return;	
		
		unsigned nEvents = buffer->getSize();
		for(unsigned i = 0; i < nEvents; i++) {
			Coincidence &c = buffer->get(i);

			for(int j1 = 0; j1 < 1; j1 ++) {
				for(int j2 = 0; j2 < 1; j2++) {
					RawHit *crystals[2] = { 
						&c.photons[0].hits[j1].raw, 
						&c.photons[1].hits[j2].raw 
					};
					
					long long T = crystals[0]->top.raw.T;
					
					event1J	= j1;
					
					event1Time = crystals[0]->time;

					event1Channel = crystals[0]->top.channelID;
					event1ToT = 1E-3*(crystals[0]->top.timeEnd - crystals[0]->top.time),
					event1Tac = crystals[0]->top.raw.d.tofpet.tac;
					event1ChannelIdleTime = crystals[0]->top.raw.channelIdleTime * T * 1E-12;
					event1TacIdleTime = crystals[0]->top.raw.d.tofpet.tacIdleTime * T * 1E-12;
					event1TQT = crystals[0]->top.tofpet_TQT;
					event1TQE = crystals[0]->top.tofpet_TQE;
					
					event2J = j2;
					event2Time = crystals[1]->time;
				
					event2Channel = crystals[1]->top.channelID;
					event2ToT = 1E-3*(crystals[1]->top.timeEnd - crystals[1]->top.time),
					event2Tac = crystals[1]->top.raw.d.tofpet.tac;
					event2ChannelIdleTime = crystals[1]->top.raw.channelIdleTime * T * 1E-12;
					event2TacIdleTime = crystals[1]->top.raw.d.tofpet.tacIdleTime * T * 1E-12;
					event2TQT = crystals[1]->top.tofpet_TQT;
					event2TQE = crystals[1]->top.tofpet_TQE;	




				
					lmTuple->Fill();
					events_passed++;
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
	  long long events_passed;
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
	sprintf(dataFileName, "%s", inputFilePrefix);

	FILE *inputDataFile = fopen(dataFileName, "r");

	
	
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
	lmData->Branch("j1", &event1J, bs);
	lmData->Branch("time1", &event1Time, bs);
	lmData->Branch("channel1", &event1Channel, bs);
	lmData->Branch("tot1", &event1ToT, bs);
	lmData->Branch("tac1", &event1Tac, bs);
	lmData->Branch("channelIdleTime1", &event1ChannelIdleTime, bs);
	lmData->Branch("tacIdleTime1", &event1TacIdleTime, bs);
	lmData->Branch("tqT1", &event1TQT, bs);
	lmData->Branch("tqE1", &event1TQE, bs);	
	lmData->Branch("j2", &event2J, bs);
	lmData->Branch("time2", &event2Time, bs);
	lmData->Branch("channel2", &event2Channel, bs);
	lmData->Branch("tot2", &event2ToT, bs);
	lmData->Branch("tac2", &event2Tac, bs);
	lmData->Branch("channelIdleTime2", &event2ChannelIdleTime, bs);
	lmData->Branch("tacIdleTime2", &event2TacIdleTime, bs);
	lmData->Branch("tqT2", &event2TQT, bs);
	lmData->Branch("tqE2", &event2TQE, bs);	
	
	TTree *lmIndex = new TTree("lmIndex", "Step Index", 2);
	

	//		eventsEnd=1000000000+eventsBegin;
// 		printf("BIG FAT WARNING: limiting event number\n");
// 		if(eventsEnd > eventsBegin + 10E6) eventsEnd = eventsBegin + 10E6;
	
	const unsigned nChannels = 2*128; 
	DAQ::ENDOTOFPET::RawReader *reader = new DAQ::ENDOTOFPET::RawReader(inputDataFile, SYSTEM_PERIOD, new Extract(
																												  new P2Extract(P2, false, true, true, NULL),
							NULL,
							NULL,	
				new SingleReadoutGrouper(
				new CrystalPositions(SYSTEM_NCRYSTALS, Common::getCrystalMapFileName(),
				new NaiveGrouper(20, 100E-9,
				new CoincidenceGrouper(20E-9,
				new EventWriter(lmData
								)))))));
		
	reader->wait();
	delete reader;
									 
	lmIndex->Fill();
	lmFile->Write();
		

	
	lmFile->Close();
	fclose(inputDataFile);

	return 0;
	
}
