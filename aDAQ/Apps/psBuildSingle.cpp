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
	fprintf(stderr, "usage: %s setup_file rawfiles_prefix output_file_prefix\n", program);
}

void displayHelp(char *program)
{
	fprintf(stderr, "usage: %s setup_file rawfiles_prefix output_file_prefix\n", program);
	fprintf(stderr, 
	
	"\noptional arguments:\n"
	 "  --help \t\t\t Show this help message and exit \n"
	 "\n"
	"\npositional arguments:\n"
	"  setup_file \t\t\t File containing paths to tdc calibration file(s) (required), tQ correction file(s) (optional) and Energy calibration file(s) (optional)\n"
	"  rawfiles_prefix \t\t Raw data files prefix\n"
	"  output_file \t\t Output file containing coincidence event data\n"
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
	public:
		EventWriter (FILE *f, EventSink<Hit> *sink) :
		OverlappedEventHandler<Hit, Hit>(sink, true),
		dataFile(f) 
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
				
				Event eo = { 
			        
					hit.time, hit.energy,
					hit.raw->channelID,
				};
					
				fwrite(&eo, sizeof(eo), 1, dataFile);
				
		
			}
			
			return inBuffer;
		}
};

int main(int argc, char *argv[])
{
	
	static struct option longOptions[] = {
		{ "help", no_argument, 0, 0 },
		{ NULL, 0, 0, 0 }
	};
	
   	while(1) {
		
		int optionIndex = 0;
		int c =getopt_long(argc, argv, "",longOptions, &optionIndex);
		if(c==-1) break;
		
		switch(optionIndex) {
			case 0: displayHelp(argv[0]); return 0;
   
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
	char *outputFilePrefix = argv[optind+2];
	

	DAQ::TOFPET::P2 *P2 = new DAQ::TOFPET::P2(SYSTEM_NCRYSTALS);
	if (strcmp(setupFileName, "none") == 0) {
		P2->setAll(2.0);
		printf("BIG FAT WARNING: no calibration\n");
	} 
	else {
		P2->loadFiles(setupFileName, true, false,0,0);
	}
	
	DAQ::Common::SystemInformation *systemInformation = new DAQ::Core::SystemInformation();
	systemInformation->loadMapFile(DAQ::Common::getCrystalMapFileName());
	

	DAQ::TOFPET::RawScanner *scanner = new DAQ::TOFPET::RawScannerV3(inputFilePrefix);
	if(scanner->getNSteps() != 1) {
		fprintf(stderr, "%s only supports single step acquisitions\n", argv[0]);
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
	
	FILE * dataFile = fopen(outputFilePrefix, "wb");
	if(dataFile == NULL) {
		int e = errno;
		fprintf(stderr, "Could not open %s for writing : %d %s\n", outputFilePrefix, e, strerror(e));
		return 1;
	}
	
	RawReader *reader = new RawReaderV3(inputFilePrefix, SYSTEM_PERIOD,  eventsBegin, eventsEnd , -1, false,
		new P2Extract(P2, false, 0.0, 0.20, true,
		new CrystalPositions(systemInformation,
		new EventWriter(dataFile, 
		new NullSink<Hit>()
	))));
	
	reader->wait();
	delete reader;
	fclose(dataFile);
	return 0;
}
