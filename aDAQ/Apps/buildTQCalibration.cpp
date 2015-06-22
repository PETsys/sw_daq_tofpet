#include <TFile.h>
#include <TNtuple.h>
#include <TChain.h>
#include <TProfile.h>
#include <TH1F.h>
#include <TH2F.h>
#include <TF1.h>
#include <TGraph.h>
#include <TOFPET/RawV2.hpp>
#include <TOFPET/RawV3.hpp>
#include <TOFPET/Sanity.hpp>
#include <TOFPET/P2Extract.hpp>
#include <TOFPET/P2.hpp>
#include <Common/Constants.hpp>
#include <Common/Utils.hpp>
#include <Core/SingleReadoutGrouper.hpp>
#include <Core/CrystalPositions.hpp>
#include <assert.h>
#include <math.h>
#include <string.h>
#include <getopt.h>

using namespace DAQ;
using namespace DAQ::Core;
using namespace DAQ::TOFPET;
using namespace DAQ::Common;
using namespace std;


static float		eventStep1;
static float		eventStep2;
static long long 	stepBegin;
static long long 	stepEnd;

static long long	eventTime;
static unsigned short	eventChannel;
static float		eventToT;
static float		eventTQT;
static float		eventTQE;

class TQCorrWriter : public EventSink<Pulse> {



public:
	TQCorrWriter(FILE *tQcalFile, P2 *lut) 
		: tQcalFile(tQcalFile), lut(lut){
		
		start_t = 1.;
		end_t = 3.;
		start_e = 1.;
		end_e = 3.;
		char hist_T[128];
		char hist_E[128];
		
		for(int channel=0; channel<SYSTEM_NCHANNELS; channel++){
			sprintf(hist_T,"htqT_ch%d",channel);   
			sprintf(hist_E,"htqE_ch%d",channel);   
			if(int(lut->nBins_tqT[channel]) > 0){
				htqT2D[channel] = new TH2D(hist_T, "tqt vs ToT",int(lut->nBins_tqT[channel]),1,3,6,0,300);
			}
			else{
				htqT2D[channel] = new TH2D(hist_T, "tqt vs ToT",300,1,3,6,0,300);
			}
			if(int(lut->nBins_tqE[channel]) > 0){
				htqE2D[channel] = new TH2D(hist_E, "tqE vs ToT",int(lut->nBins_tqE[channel]),1,3,6,0,300);
			}
			else{
				htqE2D[channel] = new TH2D(hist_E, "tqE vs ToT",300,1,3,6,0,300);
			}
		
		}
	};
	
	~TQCorrWriter() {
		for(int channel=0; channel<SYSTEM_NCHANNELS; channel++){
			delete htqT2D[channel];
			delete htqE2D[channel];
		}
		
		fclose(tQcalFile);
	};

	void pushEvents(EventBuffer<Pulse> *buffer) {
		if(buffer == NULL) return;	
		
		unsigned nEvents = buffer->getSize();
		for(unsigned i = 0; i < nEvents; i++) {
			Pulse &e = buffer->get(i);
			
			
			

			long long T = e.raw.T;
			eventTime = e.time;
			eventChannel = e.channelID;
			eventToT = 1E-3*(e.timeEnd - e.time);
			eventTQT = e.tofpet_TQT;
			eventTQE = e.tofpet_TQE;
			htqT2D[eventChannel]->Fill(eventTQT, eventToT);
			htqE2D[eventChannel]->Fill(eventTQE, eventToT);
		
		}
		
		delete buffer;
	};
	
	void pushT0(double t0) { };
	void finish() { 
		for(int channel = 0; channel < SYSTEM_NCHANNELS ; channel++){ 
			for(int whichBranch = 0; whichBranch < 2; whichBranch++) {
				for(int tot_bin=0;tot_bin<6;tot_bin++){
					bool isT = (whichBranch == 0);
					
					//sprintf(args2, "tot > %s && tot < %s && channel == %s && step1==1 &&(fmod(time,6.4e6)<3500e3 || fmod(time,6.4e6)>3600e3)",totl_str, toth_str, ch_str);
					

					TH1D *htq = isT ? htqT2D[channel]->ProjectionX("htqT_proj",tot_bin+1,tot_bin+2) : htqE2D[channel]->ProjectionX("htqE_proj",tot_bin+1,tot_bin+2);
								
					Double_t C = isT ? (end_t-start_t)/htq->Integral() : (end_e-start_e)/htq->Integral() ;
					
					//printf("%5d\t%s\t%10.6e\t%10.6e\n",channel, isT ? "T" : "E", C, isT ? htq->Integral() : htq->Integral());

					cumul=0;
					
					int nbins= isT ? int(lut->nBins_tqT[channel]) : int(lut->nBins_tqE[channel]);
		    
					for(int bin = 1; bin < nbins+1; bin++) {
						if(bin != 1)cumul+= htq->GetBinContent(bin); 
					
						fprintf(tQcalFile, "%5d\t%c\t%d\t%d\t%10.6e\t%10.6e\n", channel, isT ? 'T' : 'E', tot_bin, bin-1,  htq->GetBinContent(bin), C*cumul); 
					}
					
				}
				//htq->reset();
			}
			
		}
	};
	void report() { };
private: 
	FILE *tQcalFile;
	P2 *lut;
	TH2D *htqT2D[SYSTEM_NCRYSTALS];
	TH2D *htqE2D[SYSTEM_NCRYSTALS];
	float start_t;
	float start_e;
	float end_t;
	float end_e;
	float cumul;
	

};

void displayHelp(char * program)
{
	fprintf(stderr, "usage: %s setup_file rawfiles_prefix output_file\n", program);
	fprintf(stderr, "\noptional arguments:\n");
	fprintf(stderr,  "  --help \t\t\t Show this help message and exit \n");
	fprintf(stderr,  "  --raw_version=RAW_VERSION\t The version of the raw file to be processed: 2 or 3 (default) \n");
	fprintf(stderr, "\npositional arguments:\n");
	fprintf(stderr, "  setup_file \t\t\t File containing paths to tdc calibration files (required) and tq correction files (optional)\n");
	fprintf(stderr, "  rawfiles_prefix \t\t Path to raw data files prefix\n");
	fprintf(stderr, "  output_file \t\t\t Text output file containing tq calibration data\n");
};

void displayUsage( char * program)
{
	fprintf(stderr, "usage: %s setup_file rawfiles_prefix output_file.root\n", program);
};

int main(int argc, char *argv[])
{


   	static struct option longOptions[] = {
		{ "help", no_argument, 0, 0 },
		{ "raw_version", optional_argument,0,0 }
	};

	char rawV[128];
	sprintf(rawV,"3");
	int optionIndex = -1;
	int nOptArgs=0;
	if (int c=getopt_long(argc, argv, "",longOptions, &optionIndex) !=-1) {
		if(optionIndex==0){
			displayHelp(argv[0]);
			return(1);
			
		}
		if(optionIndex==1){
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
   
	if(argc - nOptArgs < 4){
		displayUsage(argv[0]);
		fprintf(stderr, "\n%s: error: too few arguments!\n", argv[0]);
		return(1);
	}
	else if(argc - nOptArgs> 4){
		displayUsage(argv[0]);
		fprintf(stderr, "\n%s: error: too many arguments!\n", argv[0]);
		return(1);
	}

	char *inputFilePrefix = argv[2];

	DAQ::TOFPET::RawScanner *scanner = NULL;
	if(rawV[0]=='3')
		scanner = new DAQ::TOFPET::RawScannerV3(inputFilePrefix);
	else
		scanner = new DAQ::TOFPET::RawScannerV2(inputFilePrefix);
	
	TOFPET::P2 *lut = new TOFPET::P2(SYSTEM_NCRYSTALS);
	if (strcmp(argv[1], "none") == 0) {
		lut->setAll(2.0);
		printf("BIG FAT WARNING: no calibration\n");
	} 
	else {
		lut->loadFiles(argv[1], false, false, 0, 0);
	}
	

	char filename_tq[256];
	FILE *f;
	stepBegin = 0;
	stepEnd = 0;
	int N = scanner->getNSteps();

	for(int step = 0; step < N; step++) {
		printf("hello1\n");
		unsigned long long eventsBegin;
		unsigned long long eventsEnd;
		scanner->getStep(step, eventStep1, eventStep2, eventsBegin, eventsEnd);
		printf("Step %3d of %3d: %f %f (%llu to %llu)\n", step+1, scanner->getNSteps(), eventStep1, eventStep2, eventsBegin, eventsEnd);
       
		if(N==1){
			sprintf(filename_tq,"%s",argv[3]);
			printf(filename_tq);
		}
		else{
			sprintf(filename_tq,"%s.stp1_%f_stp2_%f",argv[3],eventStep1, eventStep2); 
		}
		f = fopen(filename_tq, "w");

		EventSink<RawPulse> * pipeSink = 	new Sanity(100E-9, 		      
				new P2Extract(lut, false, 0.0, 0.20,
				new TQCorrWriter(f, lut
        )));
		DAQ::TOFPET::RawReader *reader=NULL;
		if(rawV[0]=='3') 
			reader = new DAQ::TOFPET::RawReaderV3(inputFilePrefix, SYSTEM_PERIOD,  eventsBegin, eventsEnd, pipeSink);
		else 
		    reader = new DAQ::TOFPET::RawReaderV2(inputFilePrefix, SYSTEM_PERIOD,  eventsBegin, eventsEnd, pipeSink);
	
		reader->wait();
		delete reader;
	
	}
	
	delete scanner;
	return 0;
	
}
