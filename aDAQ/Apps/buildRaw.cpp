#include <TFile.h>
#include <TNtuple.h>
#include <TOFPET/RawV2.hpp>
#include <Core/OverlappedEventHandler.hpp>
#include <assert.h>
#include <math.h>
#include <string.h>

#include <TH1F.h>

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

int main(int argc, char *argv[])
{
	if (argc != 3) {
		fprintf(stderr, "USAGE: %s <rawfiles_prefix> <output_file.root>\n", argv[0]);
		fprintf(stderr, "rawfiles_prefix - Path to raw data files prefix\n");
		fprintf(stderr, "output_file.root - ROOT output file containing raw (uncalibrated) single events\n");
		return 1;
	}

	assert(argc == 3);
	char *inputFilePrefix = argv[1];

	char dataFileName[512];
	char indexFileName[512];
	sprintf(dataFileName, "%s.raw2", inputFilePrefix);
	sprintf(indexFileName, "%s.idx2", inputFilePrefix);
	FILE *inputDataFile = fopen(dataFileName, "r");
	FILE *inputIndexFile = fopen(indexFileName, "r");
	
	DAQ::TOFPET::RawScannerV2 * scanner = new DAQ::TOFPET::RawScannerV2(inputIndexFile);
	
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
		
//		printf("BIG FAT WARNING: limiting event number\n");
//		if(eventsEnd > eventsBegin + 20E6) eventsEnd = eventsBegin + 20E6;

		const unsigned nChannels = 2*128; 
		DAQ::TOFPET::RawReaderV2 *reader = new DAQ::TOFPET::RawReaderV2(inputDataFile, 6.25E-9,  eventsBegin, eventsEnd, 
				new EventReport(data, step1, step2,
				new NullSink<RawPulse>()

		));
		
		reader->wait();
		delete reader;
		hFile->Write();
	}
	
	hFile->Close();
	fclose(inputDataFile);
	fclose(inputIndexFile);
	return 0;
	
}
