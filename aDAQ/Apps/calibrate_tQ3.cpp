#include <TFile.h>
#include <TNtuple.h>
#include <TChain.h>
#include <TProfile.h>
#include <TH1F.h>
#include <TH2F.h>
#include <TF1.h>
#include <TGraph.h>


int main(int argc, char *argv[])
{
	//Arguments (to be automatized...) 
	Int_t nchannels=128;  // number of channels of setup
	Int_t used_channels=1; // the number of channels that will actually be corrected
	Int_t Ch[used_channels];
	Int_t j=0;

	// for (int i=0;i<nchannels;i++){
		
	//   	Ch[j]=i;
	//   	j++;
	
	// }
	Ch[0]=118; 	 
  
	Float_t nBins_tqT[used_channels];   // for channel 118 (has to be read from file)
	Float_t nBins_tqE[used_channels];  // for channel 118  (has to be read from file)

	if (argc != 6){
		printf("\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
		printf("USAGE: ./calibrate_tQ M0_tdc_cal M1_tdc_cal M0_tq_cal M1_tq_cal root_file\n");
		printf( "tdc0_cal - TDC calibration file for board 0\n");
		printf( "tdc1_cal - TDC calibration file for board 1\n");
		printf( "tq0_cal - Output file containing the TQ calibration for board 0\n");
		printf( "tq1_cal - Output file containing the TQ calibration for board 1\n");
		printf( "root_file - File containing the events (single) for a long time aquisition with expeted uniform distribution in tQ\n");
		printf( "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
		return 0;

	}

	const char *filename_MAcal=argv[1] ;
	const char *filename_MBcal= argv[2];

 
	const char *filename_MAtq= argv[3];
	const char *filename_MBtq= argv[4];
	//  const char *filenametqE= "/nscratch/sttagus02/projects/tofpet/data/2014/06/05/M1M5/tqE.cal";

	TFile hfile(argv[5]); 
	//End of arguments
 
	Int_t cal_channel;
	char cal_branch[10];
	Float_t binning;

 
	TTree* lmData= dynamic_cast<TTree*>(hfile.Get("lmData"));

	if(lmData==0){ 
		printf("error: did not find tree!");
		return 1; // or may throw exception here...
	}

	for(int i = 0; i < used_channels; i++) {
		FILE *MA_cal = fopen(filename_MAcal, "r");
		FILE *MB_cal = fopen(filename_MBcal, "r");
		nBins_tqT[i]=0;
		nBins_tqE[i]=0;
		for(int j = 0; j < 512; j++) {
			if(Ch[i]<=63){
				//printf("channel= %s\n", filename_MAcal);
				fscanf(MA_cal, "%5d\t%s\t%*d\t%*f\t%*f\t%f\t%*f\t%*f\t%*f\t%*f\t%*f\n",  &cal_channel, cal_branch, &binning );
				//printf("channel= %d %d\n", cal_channel, Ch[i]);
				if(cal_channel == Ch[i] && cal_branch[0] == 'T'){
					nBins_tqT[i]+=binning;
					// printf("channel= %d %c %f %f %d\n",cal_channel, cal_branch[0], binning, nBins_tqT[i], Ch[i]);
				}
				if(cal_channel == Ch[i] && cal_branch[0] == 'E'){
					nBins_tqE[i]+=binning;
					//printf("channel= %d %s %f %f %d\n",cal_channel, &cal_branch, binning, nBins_tqT[i], Ch[i]);
				} 
			}
			else{
				fscanf(MB_cal, "%5d\t%s\t%*d\t%*f\t%*f\t%f\t%*f\t%*f\t%*f\t%*f\t%*f\n",  &cal_channel, cal_branch, &binning );
			
				if(cal_channel == Ch[i]-64 && cal_branch[0] == 'T'){
					nBins_tqT[i]+=binning;
				}
				if(cal_channel == Ch[i]-64 && cal_branch[0] == 'E'){
					nBins_tqE[i]+=binning;
				} 
				printf("channel= %d %d\n", cal_channel, Ch[i]);
			}

		}
		fclose(MA_cal);
		fclose(MB_cal);
		nBins_tqT[i]/=4.0;
		nBins_tqE[i]/=4.0;
		printf("Channel %d: nbins_t = %d and nbins_e = %d\n", Ch[i],int(nBins_tqT[i]),  int(nBins_tqE[i]) );
	}

	
	Float_t step1;      lmData->SetBranchAddress("step1", &step1);   
	Float_t step2;      lmData->SetBranchAddress("step2", &step2);   
	Long64_t fTime;		lmData->SetBranchAddress("time", &fTime);
	UShort_t channel;	lmData->SetBranchAddress("channel", &channel);
	Float_t tot;		lmData->SetBranchAddress("tot", &tot);
	UShort_t tac;		lmData->SetBranchAddress("tac", &tac);
	Double_t channelIdleTime;lmData->SetBranchAddress("channelIdleTime", &channelIdleTime);
	Double_t tacIdleTime;	lmData->SetBranchAddress("tacIdleTime", &tacIdleTime);
	Float_t tqT;		lmData->SetBranchAddress("tqT", &tqT);
	Float_t tqE;		lmData->SetBranchAddress("tqE", &tqE);

	



	Int_t stepBegin = 0;
	Int_t nEvents = lmData->GetEntries();
	//	printf("nevents= %d\n", nEvents);

	
	char args1[128];
	char args2[128];
	char ch_str[128];
	char totl_str[128];
	char toth_str[128];


	FILE *f = fopen(filename_MAtq, "w");
	FILE *f2 = fopen(filename_MBtq, "w");
	//FILE *fe = fopen(filenametqE, "w");
	Double_t cumul;
	float start_t = 0.99;
	float end_t = 2.99;
	float start_e = 0.94;
	float end_e = 2.835;

	
       	for(int i = 0; i < used_channels ; i++) {
		nBins_tqT[i]*=end_t-start_t;  
		nBins_tqE[i]*=end_e-start_e;
		
		TH1F *htqT = new TH1F("htqT", "tqt",int(nBins_tqT[i]), start_t, end_t);
		TH1F *htqE = new TH1F("htqE", "tqE",int(nBins_tqE[i]), start_e, end_e);
	  
		for(int whichBranch = 0; whichBranch < 2; whichBranch++) {
			for(int j=0;j<6;j++){
				bool isT = (whichBranch == 0);
				sprintf(ch_str,"%d",Ch[i]);   
				sprintf(totl_str, "%d",j*50 );
				sprintf(toth_str, "%d",(j+1)*50);	
				sprintf(args1, "tq%s>>htq%s", isT ? "T" : "E", isT ? "T" : "E");
				sprintf(args2, "tot > %s && tot < %s && channel == %s",totl_str, toth_str, ch_str);
			
				lmData->Draw(args1, args2);  
				
				Double_t C = isT ? (end_t-start_t)/htqT->Integral() : (end_e-start_e)/htqE->Integral() ;
				
				printf("%s\t%s\t%10.6e\t%10.6e\n",ch_str, isT ? "T" : "E", C, isT ? htqT->Integral() : htqE->Integral());

				cumul=0;
				
				int nbins= isT ? int(nBins_tqT[i]) : int(nBins_tqE[i]) ;
				
				for(int bin = 1; bin < nbins+1; bin++) {
					if(bin != 1)cumul+= isT ? htqT->GetBinContent(bin) : htqE->GetBinContent(bin); 
					fprintf(f, "%5d\t%c\t%d\t%d\t%10.6e\t%10.6e\n", Ch[i], isT ? 'T' : 'E', j, bin-1, isT ?  htqT->GetBinContent(bin) :  htqE->GetBinContent(bin), C*cumul); 
				}
			}
		}
		delete htqT;
		delete htqE;
	}

	fclose(f);
	

}
