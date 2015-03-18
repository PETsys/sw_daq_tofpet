#include <TFile.h>
#include <TNtuple.h>
#include <TChain.h>
#include <TProfile.h>
#include <TH1F.h>
#include <TH2F.h>
#include <TF1.h>
#include <TGraph.h>
#include <TOFPET/RawV2.hpp>
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
			
			
			

			long long T = e.raw.d.tofpet.T;
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

int main(int argc, char *argv[])
{
	if (argc != 4) {
		fprintf(stderr, "USAGE: %s <setup_file> <rawfiles_prefix> <output_file.root>\n", argv[0]);
		fprintf(stderr, "setup_file - File containing setup layout and path to tdc calibration files (mezzanines.cal or similar)\n");
		fprintf(stderr, "rawfiles_prefix - Path to raw data files prefix\n");
		fprintf(stderr, "output_file.root - Text output file containing tq calibration data\n");
		return 1;
	}		
	assert(argc == 4);
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

		
		DAQ::TOFPET::RawReaderV2 *reader = new DAQ::TOFPET::RawReaderV2(inputDataFile, SYSTEM_PERIOD,  eventsBegin, eventsEnd, 

				new Sanity(100E-9, 		      
				new P2Extract(lut, false, 0.0, 0.20,
				new TQCorrWriter(f, lut

		))));
	
		reader->wait();
		delete reader;
	
	}

	fclose(inputDataFile);
	fclose(inputIndexFile);
	return 0;
	
}
