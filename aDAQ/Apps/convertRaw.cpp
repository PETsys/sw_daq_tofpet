#include <ENDOTOFPET/Raw.hpp>
#include <TOFPET/RawV2.hpp>
#include <TOFPET/LUTExtract.hpp>
#include <TOFPET/LUT.hpp>
#include <Core/SingleReadoutGrouper.hpp>
#include <Core/FakeCrystalPositions.hpp>
#include <Core/ComptonGrouper.hpp>
#include <Core/CoincidenceGrouper.hpp>
#include <assert.h>
#include <math.h>
#include <string.h>


using namespace DAQ;
using namespace DAQ::Core;
using namespace DAQ::TOFPET;
using namespace DAQ::ENDOTOFPET;
using namespace std;


class EventConvert : public OverlappedEventHandler<RawPulse, RawPulse> {
public:
	  EventConvert(FILE *dataFile, float step1, float step2, EventSink<RawPulse> *sink) 
			: 	OverlappedEventHandler<RawPulse, RawPulse>(sink, true),
				dataFile(dataFile), step1(step1), step2(step2) 
	  {
			currentFrameID=0;
	  };
	  
	  ~EventConvert() {
		
	  };

	  EventBuffer<RawPulse> * handleEvents(EventBuffer<RawPulse> *inBuffer) {
			long long tMin = inBuffer->getTMin();
			long long tMax = inBuffer->getTMax();
			unsigned nEvents =  inBuffer->getSize();
			
			for(unsigned i = 0; i < nEvents; i++) {
				  RawPulse & raw = inBuffer->get(i);
				  
					
				  if(raw.time < tMin || raw.time >= tMax)
						continue;
				
				  if(raw.d.tofpet.frameID < currentFrameID){
						/*	fprintf(stderr, "Event FRAME ID is lower than current Frame ID , %ld, %ld\n", events_missed, events_passed);
						fprintf(stderr, "Event FRAME ID= %ld\n",raw.d.tofpet.frameID );
						fprintf(stderr, "current ID= %ld\n",currentFrameID );*/
						events_missed++;
						continue;
						}
				  
				  if(raw.d.tofpet.frameID != currentFrameID){
						DAQ::ENDOTOFPET::FrameHeader FrHeaderOut = {
							  0x01,
							  raw.d.tofpet.frameID,
							  0,
						};
					
					
						fwrite(&FrHeaderOut, sizeof(DAQ::ENDOTOFPET::FrameHeader), 1, dataFile);
					
						/*	if(i<10){
									fprintf(stderr, "events_passed= %u %u %ld %ld\n\n", i, nEvents, raw.d.tofpet.frameID ,currentFrameID);
									}*/
							 currentFrameID=raw.d.tofpet.frameID;
					
				
						
				  }
					
			  
				  DAQ::ENDOTOFPET::RawTOFPET eventOut = {
						  0x02,
						  raw.d.tofpet.tac,
						  raw.channelID,
						  raw.d.tofpet.tcoarse,
						  raw.d.tofpet.ecoarse,
						  raw.d.tofpet.tfine,
						  raw.d.tofpet.efine,
						  raw.d.tofpet.tacIdleTime,
						  raw.channelIdleTime};
				  
				  fwrite(&eventOut, sizeof(RawTOFPET), 1, dataFile);	     
				  //fprintf(stderr, "size raw= %zu\n", sizeof(eventOut) );
			}
	  
			return inBuffer;
	  };

	
	  void pushT0(double t0) { };	
	  void report() { };
private: 
	  FILE *dataFile;
	  float step1;
	  float step2;
	  uint32_t currentFrameID;
	  uint32_t events_missed;
	  uint32_t events_passed;
	  
};

int main(int argc, char *argv[])
{
	  if (argc != 3 && argc != 2) {
		fprintf(stderr, "USAGE: %s <rawfiles_prefix> [outputfile]\n", argv[0]);
		fprintf(stderr, "rawfiles_prefix - Path to raw data files prefix\n");
		fprintf(stderr, "output_file - If ommited, rawfiles_prefix will be considered with a '.rawTOF' suffix \n");
		return 1;
	}

	char *inputFilePrefix = argv[1];
	
	char In_dataFileName[512];
	char In_indexFileName[512];
	char Out_dataFileName[512];

	
	sprintf(In_dataFileName, "%s.raw2", inputFilePrefix);
	sprintf(In_indexFileName, "%s.idx2", inputFilePrefix);

	if(argc == 2){
	  sprintf(Out_dataFileName, "%s.rawTOF", inputFilePrefix);
	}
	if(argc == 3){
	  sprintf(Out_dataFileName, "%s", argv[2]);
	}
	
	  
	FILE *inputDataFile = fopen(In_dataFileName, "r");
	FILE *inputIndexFile = fopen(In_indexFileName, "r");
	FILE *outputDataFile = fopen(Out_dataFileName, "w");

	DAQ::ENDOTOFPET::StartTime StartTimeOut = {
			                0x00,
							0,
	};
	fwrite(&StartTimeOut, sizeof(StartTimeOut), 1, outputDataFile);



	DAQ::TOFPET::RawScannerV2 * scanner = new DAQ::TOFPET::RawScannerV2(inputIndexFile);
	

	
	int N = scanner->getNSteps();
	for(int step = 0; step < N; step++) {
		  float_t step1;
		  float_t step2;
		  unsigned long long eventsBegin;
		  unsigned long long eventsEnd;
		  scanner->getStep(step, step1, step2, eventsBegin, eventsEnd);
		  printf("Step %3d of %3d: %f %f (%llu to %llu)\n", step+1, scanner->getNSteps(), step1, step2, eventsBegin, eventsEnd);
		
		  //		printf("BIG FAT WARNING: limiting event number\n");
		  //		if(eventsEnd > eventsBegin + 20E6) eventsEnd = eventsBegin + 20E6;

		  const unsigned nChannels = 2*128; 
		  DAQ::TOFPET::RawReaderV2 *reader = new DAQ::TOFPET::RawReaderV2(inputDataFile, 6.25E-9,  eventsBegin, eventsEnd, 
				new EventConvert(outputDataFile, step1, step2,
				new NullSink<RawPulse>()
								        ));
		
		reader->wait();
		delete reader;
	
	}
	
	fclose(inputDataFile);
	fclose(inputIndexFile);
	fclose(outputDataFile);
	return 0;
	
}
