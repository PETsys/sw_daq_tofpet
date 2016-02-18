#include <Common/Constants.hpp>
#include <Common/Utils.hpp>
#include <TOFPET/RawV3.hpp>
#include <TOFPET/RawV2.hpp>
#include <ENDOTOFPET/Raw.hpp>
#include <ENDOTOFPET/Extract.hpp>
#include <STICv3/sticv3Handler.hpp>
#include <TOFPET/P2Extract.hpp>
#include <Core/CrystalPositions.hpp>
#include <Core/NaiveGrouper.hpp>
#include <Core/CoincidenceGrouper.hpp>
#include <Core/CoincidenceFilter.hpp>
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
	fprintf(stderr, "usage: %s setup_file rawfiles_prefix output_file\n", program);
}

void displayHelp(char *program)
{
	fprintf(stderr, "usage: %s setup_file rawfiles_prefix output_file\n", program);
	fprintf(stderr, 
	
	"\noptional arguments:\n"
	 "  --help \t\t\t Show this help message and exit \n"
	 "  --cWindow=CWINDOW\t\t Maximum delta time (in seconds) for two events to be considered in coincidence (default is 20E-9s)\n"
	 "  --minToT=MINTOT\t\t The minimum TOT (in ns) to be considered for a preliminary coincidence filtering. Default is 100 ns\n"
	 "  --minEnergy=MINENERGY\t\t The minimum energy (in keV) of an event to be considered a valid coincidence. If no energy calibration file is available, the entered value will correspond to a minimum TOT in ns (default is 150 ns)\n"
	 "  --maxEnergy=MAXENERGY\t\t The maximum energy (in keV) of an event to be considered a valid coincidence. If no energy calibration file is available, the entered value will correspond to a minimum TOT in ns (default is 500 ns)\n"
	 "  --gWindow=gWINDOW\t\t Maximum delta time (in seconds) inside a given multi-hit group (default is 100E-9s)\n"
	 "  --writeMultipleHits\t\t Write multiple hit information.\n"
	 "\n"
	"\npositional arguments:\n"
	"  setup_file \t\t\t File containing paths to tdc calibration file(s) (required), tQ correction file(s) (optional) and Energy calibration file(s) (optional)\n"
	"  rawfiles_prefix \t\t Raw data files prefix\n"
	"  output_file \t\t Output file containing coincidence event data\n"
	);
}

struct Event {
	uint8_t mh_n1; 
	uint8_t mh_j1;
	long long time1;
	float e1;
	int id1;

	
	uint8_t mh_n2; 
	uint8_t mh_j2;
	long long time2;
	float e2;
	int id2;

};


class EventWriter : public OverlappedEventHandler<Coincidence, Coincidence> {
	private:
		FILE *dataFile;
		bool writeMultipleHits;
	public:
		EventWriter (FILE *f, bool writeMultipleHits, EventSink<Coincidence> *sink) :
		OverlappedEventHandler<Coincidence, Coincidence>(sink, true),
		dataFile(f), writeMultipleHits(writeMultipleHits)
		{		
		}
		
		void report() {
			printf(">> EventWriter report\n");
			OverlappedEventHandler<Coincidence, Coincidence>::report();
		};

	protected:
		virtual EventBuffer<Coincidence> * handleEvents (EventBuffer<Coincidence> *inBuffer) {
			long long tMin = inBuffer->getTMin();
			long long tMax = inBuffer->getTMax();
			unsigned nEvents =  inBuffer->getSize();
			
			for(unsigned i = 0; i < nEvents; i++) {
				Coincidence &e = inBuffer->get(i);
				if(e.nPhotons != 2) continue;
				if(e.photons[0]->time < tMin || e.photons[0]->time >= tMax) continue;
				
				GammaPhoton &p1 = *e.photons[0];
				GammaPhoton &p2 = *e.photons[1];
				
				int limit1 = writeMultipleHits ? p1.nHits : 1;
				int limit2 = writeMultipleHits ? p2.nHits : 1;
				for(int m = 0; m < limit1; m++) for(int n = 0; n < limit2; n++) {
					if(m != 0 && n != 0) continue;
					
					Hit &h1 = *p1.hits[m];
					Hit &h2 = *p2.hits[n];

					fprintf(dataFile, "%u\t%u\t%u\t%u\t%u\t%f\t%f\t %u\t%f\t%f\t%f\t%f\t%lld\t%lld\n",
						p1.nHits, m, p2.nHits, n,
						h1.raw->channelID, h1.x, h1.y,
						h2.raw->channelID, h2.x, h2.y,
						h1.energy, h2.energy,
						h1.time, h2.time
					);
				}
			}
			
			return inBuffer;
		}
};

int main(int argc, char *argv[])
{
	float cWindow = 20E-9;		// s
	float gWindow = 100E-9;		// s
	float minEnergy = 150; // keV or ns (if energy=tot)
	float maxEnergy = 500; // keV or ns (if energy=tot)
	float minToT = 100E-9; //s
	
	float acqAngle = 0;
	float ctrEstimate = 200E-12;
	
	bool writeMultipleHits = false;
	
	static struct option longOptions[] = {
		{ "help", no_argument, 0, 0 },
		{ "angle", required_argument,0,0 },
		{ "ctrEstimate", required_argument,0,0 },
		{ "cWindow", required_argument,0,0 },
		{ "minToT", required_argument,0,0 },
		{ "minEnergy", required_argument,0,0 },
		{ "maxEnergy", required_argument,0,0 },
		{ "gWindow", required_argument,0,0 },
		{ "writeMultipleHits", no_argument,0,0 },
		{ NULL, 0, 0, 0 }
	};
	
   	while(1) {
		
		int optionIndex = 0;
		int c =getopt_long(argc, argv, "",longOptions, &optionIndex);
		if(c==-1) break;
		
		switch(optionIndex) {
			case 0: displayHelp(argv[0]); return 0;
			case 1: acqAngle = atof(optarg); break;
			case 2: ctrEstimate = atof(optarg) * 1E12; break;
			case 3: cWindow = atof(optarg)* 1E-9; break;
			case 4: minToT = atof(optarg) * 1E-9; break;
			case 5: minEnergy = atof(optarg); break;
			case 6: maxEnergy = atof(optarg); break;
			case 7: gWindow = atof(optarg) * 1E-9; break;
			case 8: writeMultipleHits = true; break;
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
	
	float gRadius = 20; // mm 
	// Round up cWindow and minToT for use in CoincidenceFilter
	float cWindowCoarse = (ceil(cWindow/SYSTEM_PERIOD)) * SYSTEM_PERIOD;
	float minToTCoarse = (ceil(minToT/SYSTEM_PERIOD) + 2) * SYSTEM_PERIOD;

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
		new NaiveGrouper(gRadius, gWindow, minEnergy, maxEnergy, GammaPhoton::maxHits,
		new CoincidenceGrouper(cWindow,
		new EventWriter(dataFile, writeMultipleHits,
		new NullSink<Coincidence>()
	))))));
	
	reader->wait();
	delete reader;
	fclose(dataFile);
	return 0;
}
