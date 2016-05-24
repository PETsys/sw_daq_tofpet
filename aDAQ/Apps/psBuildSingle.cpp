#include <Common/Constants.hpp>
#include <Common/Utils.hpp>
#include <TOFPET/RawV3.hpp>
#include <TOFPET/RawV2.hpp>
#include <ENDOTOFPET/Raw.hpp>
#include <ENDOTOFPET/Extract.hpp>
#include <STICv3/sticv3Handler.hpp>
#include <TOFPET/P2Extract.hpp>
#include <TOFPET/P2.hpp>
#include <Core/CrystalPositions.hpp>
#include <Core/NaiveGrouper.hpp>
#include <assert.h>
#include <math.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>

using namespace DAQ::Core;
using namespace DAQ::TOFPET;
using namespace DAQ::Common;

void displayUsage( char * program)
{
	fprintf(stderr, "usage: %s <setup_file> <rawfiles_prefix> <output_file>\n", program);
}

void displayHelp(char *program)
{
	fprintf(stderr, "usage: %s <setup_file> <rawfiles_prefix> <output_file>\n", program);
	fprintf(stderr, 
	"\npositional arguments:\n"
	"  <setup_file>		File containing paths to tdc calibration file(s) (required), tQ correction file(s) (optional) and Energy calibration file(s) (optional)\n"
	"  <rawfiles_prefix>	Raw data files prefix\n"
	"  <output_file>	\tOutput file containing coincidence event data\n"
	"\n"
	
	"\noptional arguments:\n"
	 "  --help		Show this help message and exit \n"
	 "  --writeBinary	\tWrite a binary file\n"
	 "\n"
	);
}

struct Event {
	long long time;
	float e;
	int id;
};


class EventWriter : public OverlappedEventHandler<Hit, Hit> {
	private:
		FILE *dataFile;
		bool writeBinary;
	public:
		EventWriter (FILE *f, bool writeBinary, EventSink<Hit> *sink) :
		OverlappedEventHandler<Hit, Hit>(sink, true),
		dataFile(f), writeBinary(writeBinary)
		{		
		}
		
		void report() {
			printf(">> EventWriter report\n");
			OverlappedEventHandler<Hit, Hit>::report();
		};

	protected:
		virtual EventBuffer<Hit> * handleEvents (EventBuffer<Hit> *inBuffer) {
			long long tMin = inBuffer->getTMin();
			long long tMax = inBuffer->getTMax();
			unsigned nEvents =  inBuffer->getSize();
			
			for(unsigned i = 0; i < nEvents; i++){ 
				Hit &hit = inBuffer->get(i);
				if(hit.time < tMin || hit.time >= tMax) continue;

				if (writeBinary) {
					Event eo = {
						hit.time,
						hit.energy,
						hit.raw->channelID
					};
					fwrite(&eo, sizeof(eo), 1, dataFile);
				}
				else {
					fprintf(dataFile, "%lld\t%f\t%d\n",
						hit.time,
						hit.energy,
						hit.raw->channelID
					);
				}
		
			}
			
			return inBuffer;
		}
};

int main(int argc, char *argv[])
{
	bool writeBinary = false;
	static struct option longOptions[] = {
		{ "help", no_argument, 0, 0 },
		{ "writeBinary", no_argument, 0, 0 },
		{ NULL, 0, 0, 0 }
	};
	
   	while(1) {
		
		int optionIndex = 0;
		int c =getopt_long(argc, argv, "",longOptions, &optionIndex);
		if(c==-1) break;
		
		switch(optionIndex) {
			case 0: displayHelp(argv[0]); return 0;
			case 1: writeBinary = true; break;
   
			default: 
				displayHelp(argv[0]); return 1;
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
	

	DAQ::TOFPET::P2 *P2 = new DAQ::TOFPET::P2(SYSTEM_NCRYSTALS);
	if (strcmp(setupFileName, "none") == 0) {
		P2->setAll(2.0);
		printf("BIG FAT WARNING: no calibration\n");
	} 
	else {
		P2->loadFiles(setupFileName, true, false,0,0);
	}
	
	DAQ::Common::SystemInformation *systemInformation = new DAQ::Core::SystemInformation();
	

	DAQ::TOFPET::RawScanner *scanner = new DAQ::TOFPET::RawScannerV3(inputFilePrefix);
	if(scanner->getNSteps() > 1 && !writeBinary) {
		fprintf(stderr, "%s only supports single step acquisitions when writing text files\n", argv[0]);
		return 1;
	}
	
	unsigned long long eventsBegin;
	unsigned long long eventsEnd;
	float eventStep1;
	float eventStep2;
	scanner->getStep(0, eventStep1, eventStep2, eventsBegin, eventsEnd);
	if(eventsBegin == eventsEnd) {
		fprintf(stderr, "Empty input file, bailing out\n");
		return 1;
	}
	
	FILE * dataFile = fopen(outputFileName, writeBinary ? "wb" : "w");
	if(dataFile == NULL) {
		int e = errno;
		fprintf(stderr, "Could not open %s for writing : %d %s\n", outputFileName, e, strerror(e));
		return 1;
	}
	FILE *indexFile = NULL;
	long long outputStepBegin = 0;
	if(writeBinary) {
		char indexFileName[512];
		sprintf(indexFileName, "%s.idx", outputFileName);
		indexFile = fopen(indexFileName, "w");
		if(indexFile == NULL) {
			int e = errno;
			fprintf(stderr, "Could not open %s for writing : %d %s\n", indexFileName, e, strerror(e));
			return 1;
		}
	}
	
	for(int step = 0; step < scanner->getNSteps(); step++) {
		unsigned long long eventsBegin;
		unsigned long long eventsEnd;
		float eventStep1;
		float eventStep2;
		scanner->getStep(step, eventStep1, eventStep2, eventsBegin, eventsEnd);
		if(eventsBegin != eventsEnd) {
			// Do not process empty steps, the chain crashes
		
			RawReader *reader = new RawReaderV3(inputFilePrefix, SYSTEM_PERIOD,  eventsBegin, eventsEnd , -1, false,
				new P2Extract(P2, false, 0.0, 0.20, true,
				new EventWriter(dataFile, writeBinary,
				new NullSink<Hit>()
			)));
			reader->wait();
			delete reader;
		}
		
		
		if(writeBinary) {
			long long outputStepEnd = ftell(dataFile);
			fprintf(indexFile, "%f\t%f\t%lld\t%lld\n", eventStep1, eventStep2, outputStepBegin, outputStepEnd);
			outputStepBegin = outputStepEnd;
		}
	}
			
	
	fclose(dataFile);
	if(indexFile != NULL) {
		fclose(indexFile);
	}
	return 0;
}
