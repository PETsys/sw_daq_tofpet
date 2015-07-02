#include <TFile.h>
#include <TNtuple.h>
#include <Common/Constants.hpp>
#include <TOFPET/RawV3.hpp>
#include <TOFPET/RawV2.hpp>
#include <ENDOTOFPET/Raw.hpp>
#include <ENDOTOFPET/Extract.hpp>
#include <STICv3/sticv3Handler.hpp>
#include <Core/OverlappedEventHandler.hpp>
#include <assert.h>
#include <math.h>
#include <string.h>
#include <TH1F.h>
#include <getopt.h>

using namespace DAQ;
using namespace DAQ::Core;
using namespace DAQ::TOFPET;
using namespace std;


class EventReport : public OverlappedEventHandler<RawPulse, RawPulse> {
public:
	EventReport(TNtuple *data, float step1, float step2, EventSink<RawPulse> *sink) 
	: 	OverlappedEventHandler<RawPulse, RawPulse>(sink, true),
		data(data), step1(step1), step2(step2) 
	{
		
	};
	
	~EventReport() {
		
	};

	EventBuffer<RawPulse> * handleEvents(EventBuffer<RawPulse> *inBuffer) {
		long long tMin = inBuffer->getTMin();
		long long tMax = inBuffer->getTMax();
		unsigned nEvents =  inBuffer->getSize();
	
		for(unsigned i = 0; i < nEvents; i++) {
			RawPulse & raw = inBuffer->get(i);
			
			
			if(raw.time < tMin || raw.time >= tMax)
					continue;
			
			data->Fill(step1, step2, 
				raw.channelID, raw.d.tofpet.tac,
				raw.d.tofpet.tcoarse, raw.d.tofpet.tfine, 
				raw.d.tofpet.ecoarse, raw.d.tofpet.efine, 
				float(raw.channelIdleTime), 
				float(raw.d.tofpet.tacIdleTime));
			
			
			
		}
		
		return inBuffer;
	};
	
	void pushT0(double t0) { };	
	void report() { };
private: 
	TNtuple *data;
	float step1;
	float step2;
};
void displayHelp(char * program)
{
	fprintf(stderr, "usage: %s rawfiles_prefix output_file\n", program);
	fprintf(stderr, "\noptional arguments:\n");
	fprintf(stderr,  "  --help \t\t\t Show this help message and exit \n");
#ifndef __ENDOTOFPET__	
	fprintf(stderr,  "  --raw_version=RAW_VERSION\t The version of the raw file to be processed: 2 or 3 (default) \n");
#endif
	fprintf(stderr, "\npositional arguments:\n");
	fprintf(stderr, "  rawfiles_prefix \t\t Path to raw data files prefix\n");
	fprintf(stderr, "  output_file \t\t\t ROOT output file containing raw (not calibrated) single events TTree\n");
};

void displayUsage( char * program)
{
	fprintf(stderr, "usage: %s rawfiles_prefix output_file.root\n", program);
};

int main(int argc, char *argv[])
{


	static struct option longOptions[] = {
		{ "help", no_argument, 0, 0 },
		{ "raw_version", optional_argument,0,0 }
	};
#ifndef __ENDOTOFPET__
	char rawV[128];
	rawV[0]='3';
#endif
	int optionIndex = -1;
	int nOptArgs=0;
	if (int c=getopt_long(argc, argv, "",longOptions, &optionIndex) !=-1) {
		if(optionIndex==0){
			displayHelp(argv[0]);
			return(1);
			
		}
#ifndef __ENDOTOFPET__
		if(optionIndex==1){
			nOptArgs++;
			sprintf(rawV,optarg);
			if(rawV[0]!='2' && rawV[0]!='3'){
				fprintf(stderr, "\n%s: error: Raw version not valid! Please choose 2 or 3\n", argv[0]);
				return(1);
			}
		}
#endif		
		else{
			displayUsage(argv[0]);
			fprintf(stderr, "\n%s: error: Unknown option!\n", argv[0]);
			return(1);
		}
	}
   
	if(argc - nOptArgs < 3){
		displayUsage(argv[0]);
		fprintf(stderr, "\n%s: error: too few arguments!\n", argv[0]);
		return(1);
	}
	else if(argc - nOptArgs> 3){
		displayUsage(argv[0]);
		fprintf(stderr, "\n%s: error: too many arguments!\n", argv[0]);
		return(1);
	}
	char *inputFilePrefix = argv[1];

	DAQ::TOFPET::RawScanner *scanner = NULL;
#ifndef __ENDOTOFPET__ 
	if(rawV[0]=='3')
		scanner = new DAQ::TOFPET::RawScannerV3(inputFilePrefix);
	else if(rawV[0]=='2')
		scanner = new DAQ::TOFPET::RawScannerV2(inputFilePrefix);
#else 
	scanner = new DAQ::ENDOTOFPET::RawScannerE(inputFilePrefix);
#endif
	
	TFile *hFile = new TFile(argv[2], "RECREATE");
	TNtuple *data = new TNtuple("data", "Event List", "step1:step2:channel:tac:tcoarse:tfine:ecoarse:efine:channelIdleTime:tacIdleTime");
	
	int N = scanner->getNSteps();
	for(int step = 0; step < N; step++) {
		Float_t step1;
		Float_t step2;
		unsigned long long eventsBegin;
		unsigned long long eventsEnd;
		scanner->getStep(step, step1, step2, eventsBegin, eventsEnd);
		printf("Step %3d of %3d: %f %f (%llu to %llu)\n", step+1, scanner->getNSteps(), step1, step2, eventsBegin, eventsEnd);
		
		DAQ::TOFPET::RawReader *reader=NULL;

		EventSink<RawPulse> * pipeSink = new EventReport(data, step1, step2,
						                 new NullSink<RawPulse>());

#ifndef __ENDOTOFPET__
		if(rawV[0]=='3') 
			reader = new DAQ::TOFPET::RawReaderV3(inputFilePrefix, SYSTEM_PERIOD,  eventsBegin, eventsEnd, pipeSink);
		else if(rawV[0]=='2')
		    reader = new DAQ::TOFPET::RawReaderV2(inputFilePrefix, SYSTEM_PERIOD,  eventsBegin, eventsEnd, pipeSink);
#else
		reader = new DAQ::ENDOTOFPET::RawReaderE(inputFilePrefix, SYSTEM_PERIOD,  eventsBegin, eventsEnd, pipeSink);
#endif
		
		reader->wait();
		delete reader;
		hFile->Write();
	}
	delete scanner;
	hFile->Close();
	return 0;
	
}
