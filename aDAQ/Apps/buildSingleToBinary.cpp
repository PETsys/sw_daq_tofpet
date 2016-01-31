#include <TFile.h>
#include <TNtuple.h>
#include <Common/Constants.hpp>
#include <TOFPET/RawV3.hpp>
#include <TOFPET/RawV2.hpp>
#include <TOFPET/P2Extract.hpp>
#include <assert.h>
#include <math.h>
#include <string.h>
#include <getopt.h>

using namespace DAQ;
using namespace DAQ::Core;
using namespace DAQ::TOFPET;
using namespace std;

struct EventOut {
	float		step1;
	float 		step2;
	long long	time;			// Absolute event time, in ps
	unsigned short	channel;		// Channel ID
	float		tot;			// Time-over-Threshold, in ns
	unsigned char	tac;			// TAC ID
	unsigned char 	badEvent;		// 0 if OK, 1 if bad
} __attribute__((packed));


class EventWriter : public EventSink<Hit>, EventSource<Hit> {
public:
	EventWriter(FILE *dataFile, float step1, float step2, EventSink<Hit> *sink) 
	:  EventSource<Hit>(sink), dataFile(dataFile), step1(step1), step2(step2) {
		
	};
	
	~EventWriter() {
		
	};

	void pushEvents(EventBuffer<Hit> *buffer) {
		if(buffer == NULL) return;	
		
		unsigned nEvents = buffer->getSize();
		for(unsigned i = 0; i < nEvents; i++) {
			Hit & p = buffer->get(i);
			EventOut e = { step1, step2, 
				p.time, 
				(unsigned short)(p.raw->channelID), 
				p.energy, 
				(unsigned char)(p.raw->d.tofpet.tac), 
				(unsigned char)(p.badEvent ? 1 : 0) };
			fwrite(&e, sizeof(e), 1, dataFile);
		}
		
		sink->pushEvents(buffer);
	};
	
	void pushT0(double t0) { };
	void finish() { };
	void report() { };
private: 
	FILE *dataFile;
	float step1;
	float step2;
};

void displayHelp(char * program)
{
	fprintf(stderr, "usage: %s setup_file rawfiles_prefix output_file\n", program);
	fprintf(stderr, "\noptional arguments:\n");
	fprintf(stderr,  "  --help \t\t\t Show this help message and exit \n");
	fprintf(stderr,  "  --onlineMode\t Use this flag to process data in real time during acquisition\n");
	fprintf(stderr,  "  --acqDeltaTime=ACQDELTATIME\t If online mode is chosen, this variable defines how much data time (in seconds) to process (default is -1 which selects all data for the current step)\n");
	fprintf(stderr,  "  --raw_version=RAW_VERSION\t The version of the raw file to be processed: 2 or 3 (default) \n");
	fprintf(stderr, "\npositional arguments:\n");
	fprintf(stderr, "  setup_file \t\t\t File containing paths to tdc calibration file(s) (required), tQ correction file(s) (optional) and Energy calibration file(s) (optional)\n");
	fprintf(stderr, "  rawfiles_prefix \t\t Path to raw data files prefix\n");
	fprintf(stderr, "  output_file \t\t\t Binary output file containing all events\n");
};

void displayUsage( char * program)
{
	fprintf(stderr, "usage: %s setup_file rawfiles_prefix output_file\n", program);
};

int main(int argc, char *argv[])
{

	

	static struct option longOptions[] = {
		{ "help", no_argument, 0, 0 },
		{ "onlineMode", no_argument,0,0 },
		{ "acqDeltaTime", required_argument,0,0 },
		{ "raw_version", required_argument,0,0 }
	};

	char rawV[128];
	sprintf(rawV,"3");
	bool onlineMode=false;
	float readBackTime=-1;

	int nOptArgs=0;

	while(1) {
		int optionIndex = 0;
	    int c=getopt_long(argc, argv, "",longOptions, &optionIndex);
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
		if(optionIndex==3){
			nOptArgs++;
			sprintf(rawV,optarg);
			if(rawV[0]!='2' && rawV[0]!='3'){
				fprintf(stderr, "\n%s: error: Raw version not valid! Please choose 2 or 3\n", argv[0]);
				return(1);
			}
		}		
		else{
			displayUsage(argv[0]);
			fprintf(stderr, "\n%s: error: Unknown option!\n", argv[0]);
			return(1);
		}
	}
   
	if(argc - optind < 3){
		displayUsage(argv[0]);
		fprintf(stderr, "\n%s: error: too few positional arguments!\n", argv[0]);
		return(1);
	}
	else if(argc - optind > 3){
		displayUsage(argv[0]);
		fprintf(stderr, "\n%s: error: too many positional arguments!\n", argv[0]);
		return(1);
	}

	char * setupFileName=argv[optind];
	char *inputFilePrefix = argv[optind+1];
	char *outputFileName = argv[optind+2];
	


	DAQ::TOFPET::RawScanner *scanner = NULL;
	if(rawV[0]=='3')
		scanner = new DAQ::TOFPET::RawScannerV3(inputFilePrefix);
	else
		scanner = new DAQ::TOFPET::RawScannerV2(inputFilePrefix);
	
	

	TOFPET::P2 *lut = new TOFPET::P2(SYSTEM_NCRYSTALS);

	if (strcmp(setupFileName, "none") == 0) {
		lut->setAll(2.0);
		printf("BIG FAT WARNING: no calibration\n");
	} 
	else {
		lut->loadFiles(setupFileName, true, false, 0,0);
	}
	
	FILE *lmFile = fopen(outputFileName, "w");
	
	int N = scanner->getNSteps();
	for(int step = 0; step < N; step++) {
		Float_t step1;
		Float_t step2;
		unsigned long long eventsBegin;
		unsigned long long eventsEnd;
		if(onlineMode)step=N-1;
		scanner->getStep(step, step1, step2, eventsBegin, eventsEnd);
		if(eventsBegin==eventsEnd)continue;
		if(!onlineMode)printf("Step %3d of %3d: %f %f (%llu to %llu)\n", step+1, scanner->getNSteps(), step1, step2, eventsBegin, eventsEnd);
		if(N!=1){
			if (strcmp(setupFileName, "none") == 0) {
				lut->setAll(2.0);
				printf("BIG FAT WARNING: no calibration file\n");
			} 
			else{
				lut->loadFiles(setupFileName, true, true,step1,step2);
			}
		}

		
		EventSink<RawHit> * pipeSink = 		new P2Extract(lut, false, 0.0, 0.20, false,
				new EventWriter(lmFile, step1, step2, 
				new NullSink<Hit>()
		));

		DAQ::TOFPET::RawReader *reader=NULL;
	
		if(rawV[0]=='3') 
			reader = new DAQ::TOFPET::RawReaderV3(inputFilePrefix, SYSTEM_PERIOD,  eventsBegin, eventsEnd, readBackTime, onlineMode, pipeSink);
		else 
		    reader = new DAQ::TOFPET::RawReaderV2(inputFilePrefix, SYSTEM_PERIOD,  eventsBegin, eventsEnd, pipeSink);



		reader->wait();
		delete reader;
	}
	
	delete scanner;
	fclose(lmFile);

	return 0;
	
}
