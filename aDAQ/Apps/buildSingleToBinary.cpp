#include <TFile.h>
#include <TNtuple.h>
#include <Common/Constants.hpp>
#include <TOFPET/RawV2.hpp>
#include <TOFPET/P2Extract.hpp>
#include <Core/SingleReadoutGrouper.hpp>
#include <Core/FakeCrystalPositions.hpp>
#include <Core/ComptonGrouper.hpp>
#include <Core/CoincidenceGrouper.hpp>
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


class EventWriter : public EventSink<Pulse> {
public:
	EventWriter(FILE *dataFile, float step1, float step2) 
	: dataFile(dataFile), step1(step1), step2(step2) {
		
	};
	
	~EventWriter() {
		
	};

	void pushEvents(EventBuffer<Pulse> *buffer) {
		if(buffer == NULL) return;	
		
		unsigned nEvents = buffer->getSize();
		for(unsigned i = 0; i < nEvents; i++) {
			Pulse & p = buffer->get(i);
			EventOut e = { step1, step2, 
				p.time, 
				(unsigned short)(p.channelID), 
				p.energy, 
				(unsigned char)(p.raw.d.tofpet.tac), 
				(unsigned char)(p.badEvent ? 1 : 0) };
			fwrite(&e, sizeof(e), 1, dataFile);
		}
		
		delete buffer;
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
	fprintf(stderr, "\npositional arguments:\n");
	fprintf(stderr, "  setup_file \t\t\t File containing paths to tdc calibration files (required) and tq correction files (optional)\n");
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
	sprintf(dataFileName, "%s.raw2", inputFilePrefix);
	sprintf(indexFileName, "%s.idx2", inputFilePrefix);
	FILE *inputDataFile = fopen(dataFileName, "r");
	FILE *inputIndexFile = fopen(indexFileName, "r");
	
	DAQ::TOFPET::RawScannerV2 * scanner = new DAQ::TOFPET::RawScannerV2(inputIndexFile);
	

	TOFPET::P2 *lut = new TOFPET::P2(SYSTEM_NCRYSTALS);

	if (strcmp(argv[1], "none") == 0) {
		lut->setAll(2.0);
		printf("BIG FAT WARNING: no calibration\n");
	} 
	else {
		lut->loadFiles(argv[1], true, false, 0,0);
	}
	
	FILE *lmFile = fopen(argv[3], "w");
	
	int N = scanner->getNSteps();
	for(int step = 0; step < N; step++) {
		Float_t step1;
		Float_t step2;
		unsigned long long eventsBegin;
		unsigned long long eventsEnd;
		scanner->getStep(step, step1, step2, eventsBegin, eventsEnd);
		printf("Step %3d of %3d: %f %f (%llu to %llu)\n", step+1, scanner->getNSteps(), step1, step2, eventsBegin, eventsEnd);
		if(N!=1){
			if (strcmp(argv[1], "none") == 0) {
				lut->setAll(2.0);
				printf("BIG FAT WARNING: no calibration file\n");
			} 
			else{
				lut->loadFiles(argv[1], true, true,step1,step2);
			}
		}

		DAQ::TOFPET::RawReaderV2 *reader = new DAQ::TOFPET::RawReaderV2(inputDataFile, 6.25E-9,  eventsBegin, eventsEnd, 
				new P2Extract(lut, false, 0.0, 0.20,
				new EventWriter(lmFile, step1, step2

		)));
		
		reader->wait();
		delete reader;
	}
	

	fclose(lmFile);
	fclose(inputDataFile);
	fclose(inputIndexFile);
	return 0;
	
}
