#include <TFile.h>
#include <TNtuple.h>
#include <TH1F.h>
#include <TSpectrum.h>
#include <TF1.h>
#include <TH2F.h>
#include <TStyle.h>
#include <TCanvas.h>
#include <TProfile.h>

void draw_ctr_stats_x(TFile *lmFile) {

	TNtuple *lmIndex = (TNtuple *)lmFile->Get("lmIndex");
	TNtuple *lmData = (TNtuple *)lmFile->Get("lmData");
	

	bool savePDFs=false;  // CHANGE here to false if you don't want to save the plots for all channel pairs

	Int_t nchannels_A=128;
	Int_t nchannels_B=128;
	

	Int_t maxPairsTotal=1000;
	Int_t maxPairsAsic=150;
	Int_t nDoneTotal;
	Int_t nDoneAsic[32];

	Int_t minCounts=5000;

	char condition[256];
		       
	Float_t T = 6250;
	
	FILE * ctrTable = fopen("ctr_stats_108vin_HVOPAMP.txt", "w");
	
	Float_t step1;      lmData->SetBranchAddress("step1", &step1);   
	Float_t step2;      lmData->SetBranchAddress("step2", &step2);   
	Long64_t fTime1;		lmData->SetBranchAddress("time1", &fTime1);
	UShort_t channel1;	lmData->SetBranchAddress("channel1", &channel1);
	Float_t tot1;		lmData->SetBranchAddress("tot1", &tot1);
	Long64_t fTime2;		lmData->SetBranchAddress("time2", &fTime2);
	UShort_t channel2;	lmData->SetBranchAddress("channel2", &channel2);
	Float_t tot2;		lmData->SetBranchAddress("tot2", &tot2);
	
	gStyle->SetPalette(1);
	gStyle->SetOptFit(1);
	TCanvas *c = new TCanvas();
	c->Divide(3,1);


	
	char hTitle1[128];
	sprintf(hTitle1, "CTR distribution");
	TH1F *h1 =new TH1F("h1",hTitle1,100,100,400);
	TH1F *h2 = new TH1F("h2",hTitle1,100,250,600);
	

		
	Long64_t stepEnd=0;
	Long64_t stepBegin=0;
	Int_t nSteps1=0;
	Int_t nSteps2=0;
	Int_t nLimits=0;
	Float_t step1Array[100];
	Float_t step2Array[100];
	Long64_t stepLimits[10000];
	Long64_t nEvents = lmData->GetEntries();
	printf("nEvents=%lld\n", nEvents);
	lmData->GetEntry(stepEnd);
	step1Array[0]=step1;
	step2Array[0]=step2;
	stepLimits[0]=0;

	while(stepEnd < nEvents){
		lmData->GetEntry(stepEnd);
		//printf("%d %d %d %d %d %d\n", channel1, channel2,xi1, yi1, xi2, yi2);	
		if(step1 != step1Array[nSteps1]){
			nSteps1++;
			nLimits++;
			step1Array[nSteps1]=step1;
			stepLimits[nLimits]=stepEnd;
			
		}
		if(step2 != step2Array[nSteps2]){
			nSteps2++;
			nLimits++;
			step2Array[nSteps2+1]=step2;
			stepLimits[nLimits]=stepEnd;
		
		
		
		}
		//printf("nSteps1=%d nSteps2=%d %lld\n", nSteps1, nSteps2, stepEnd);	
		stepEnd+=10000;
	}
	nSteps1++;
	nSteps2++;
	
	//printf("nSteps1=%d nSteps2=%d\n", nSteps1, nSteps2);	
	
	Int_t nBinsToT = 500;
	TH2F * hToTx = new TH2F("hToTx", "ToT", nBinsToT, 0, nBinsToT, nBinsToT, 0, nBinsToT);
	hToTx->GetXaxis()->SetTitle("Crystal 1 ToT (ns)");
	hToTx->GetYaxis()->SetTitle("Crystal 2 ToT (ns)");
			
	Double_t tBinWidth = 50E-12;
	Double_t wMax = 40E-9;
	TH1F *hDelta = new TH1F("hDelta", "Delta", 2*wMax/tBinWidth, -wMax, wMax);	
	TH1F *hDeltaTW1 = new TH1F("hDelta_tw1", "delta_tw1", 2*wMax/tBinWidth, -wMax, wMax);
	TH1F *hDeltaTW2 = new TH1F("hDlta_tw2", "delta_tw2", 2*wMax/tBinWidth, -wMax, wMax);	
			
	TH2F *hDD = new TH2F("hDD", "hDD", 500, 0, 2*T*1E-12, 160, -2E-9, 2E-9);
	hDD->GetXaxis()->SetTitle("#frac{t1+t2}{2} (s)");
	hDD->GetYaxis()->SetTitle("t1-t2 (s)");
			
	
	TProfile *pTW1 = new TProfile("pTW1", "Time Walk 1", nBinsToT, 0, 500);
	pTW1->GetXaxis()->SetTitle("Crystal 1 ToT (ns)");
	pTW1->GetYaxis()->SetTitle("Time difference (s)");
			
	TProfile *pTW2 = new TProfile("pTW2", "Time Walk 1", nBinsToT, 0, 500);
	pTW2->GetXaxis()->SetTitle("Crystal 2 ToT (ns)");
	pTW2->GetYaxis()->SetTitle("Time difference (s)");


	//for(int i=0;i<10;i++)printf("%d %f %f \n",stepLimits[i], step1Array[i],step2Array[i] );
	stepLimits[nSteps1]= nEvents;
	for(int step_1=0; step_1<nSteps1;step_1++){
		for(int step_2=0; step_2<nSteps2;step_2++){
			Float_t currentStep1= step1Array[step_1];
			Float_t currentStep2= step2Array[step_2];
			//stepBegin=stepLimits[step_2*nSteps1+step_1];
			//if(step_1==(nSteps1-1) && step_2==(nSteps2-1))stepEnd=nEvents;
			//else stepEnd=stepLimits[step_2*nSteps1+step_1+1];
			
		       
			//printf("step_1=%d step_2=%d step1=%f step2=%f\n", step_1,step_2,currentStep1, currentStep2);
			
			TH2F * counts2 = new TH2F("counts2", "counts 2", 2048, 0, 2048, 2048, 0, 2048);
			sprintf(condition, "tot1>0 && tot2 > 0 && step1==%f && step2==%f", currentStep1, currentStep2);
			
			lmData->Project("counts2", "channel1:channel2", condition, "", stepLimits[step_1+1]-stepLimits[step_1], stepLimits[step_1]);
			Int_t maxCounts=counts2->GetMaximum();
			Int_t binMax=counts2->GetMaximumBin();
			Int_t binx, biny, binz;
			counts2->GetBinXYZ(binMax, binx, biny, binz);
			printf("CA max=%d  CB max=%d nCountsMax=%d\n",binx, biny,maxCounts);
			for(Int_t i=0;i<32;i++)nDoneAsic[i]=0;
			nDoneTotal=0;
		
			
			

			
			for(Int_t CA=128; CA<256;CA++){
				for(Int_t CB=0; CB<128;CB++){
					Int_t asicA=CA/64;
					Int_t asicB=CB/64;
					if( counts2->GetBinContent(CB+1,CA+1)< minCounts) continue; 
			 
					//if(CA==136 && CB==60)continue;
					if(nDoneTotal>=maxPairsTotal)exit(0);
				        if(nDoneAsic[asicA]>=maxPairsAsic && nDoneAsic[asicB]>=maxPairsAsic)continue;

					printf("Number of Pairs done: %d\n", nDoneTotal);
					printf("Making coincidence for channels %d and %d (step1=%f and step2=%f)\n", CA, CB, currentStep1, currentStep2);
			
		       
					
					hToTx->Reset();
					hDelta->Reset();
					pTW1->Reset();
					pTW2->Reset();
					hDD->Reset();
					
					sprintf(condition, "channel2==%d && channel1 == %d && step1==%f && step2==%f", CB, CA, currentStep1, currentStep2 );
				
   
					lmData->Project("hToTx", "tot2:tot1", condition, "", stepLimits[step_1+1]-stepLimits[step_1], stepLimits[step_1]);

			
					TH1 *hToT1 = hToTx->ProjectionX("hToT1");
					TH1 *hToT2 = hToTx->ProjectionY("hToT2");
				
					TSpectrum *spectrum = new TSpectrum();
					spectrum->Search(hToT1, 3, " ",  0.2);
					Int_t nPeaks = spectrum->GetNPeaks();
					if (nPeaks == 0) {
						printf("No peaks in hToT1!!!\n");
					        continue;
					}

#if ROOT_VERSION_CODE > ROOT_VERSION(6,0,0)
					Double_t *xPositions1 = spectrum->GetPositionX();
#else
					Float_t *xPositions1 = spectrum->GetPositionX();
#endif

					Double_t x1_psc = 0;
					Double_t x1_pe = 0;
					for(Int_t i = 0; i < nPeaks; i++){
						printf("%lf\n",xPositions1[i]);
						if(xPositions1[i] > x1_pe){
							if(x1_pe > x1_psc)
								x1_psc = x1_pe;
							x1_pe = xPositions1[i];
						}
					} 
					printf("%d\n",nPeaks);
					TSpectrum *spectrum2 = new TSpectrum();
					spectrum2->Search(hToT2, 3, " ",  0.2);
					nPeaks = spectrum2->GetNPeaks();
					if (nPeaks == 0) {
						printf("No peaks in hToT2!!!\n");
						continue;
					}

#if ROOT_VERSION_CODE > ROOT_VERSION(6,0,0)
					Double_t *xPositions2 = spectrum2->GetPositionX();
#else
					Float_t  *xPositions2 = spectrum2->GetPositionX();
#endif
					Double_t x2_psc = 0;
					Double_t x2_pe = 0;
					for(Int_t i = 0; i < nPeaks; i++) {
						printf("%lf\n",xPositions2[i]);
						if(xPositions2[i] > x2_pe) {
							if(x2_pe > x2_psc)
								x2_psc = x2_pe;
							x2_pe = xPositions2[i];
						}
					} 
				
					Double_t x1 = x1_pe;
					Double_t x2 = x2_pe;
					
					//printf("%lf %lf\n",x1_pe, x2_pe);
					hToT1->Fit("gaus", "", "", x1-10, x1+10);
					if(hToT1->GetFunction("gaus") == NULL) {   continue; }
					x1 = hToT1->GetFunction("gaus")->GetParameter(1);
					Float_t sigma1 = hToT1->GetFunction("gaus")->GetParameter(2);
					printf("fit2\n");
					hToT2->Fit("gaus", "", "", x2-10, x2+10);
					if(hToT2->GetFunction("gaus") == NULL) {  continue; }
					x2 = hToT2->GetFunction("gaus")->GetParameter(1);
					Float_t sigma2 = hToT2->GetFunction("gaus")->GetParameter(2);
				
				
				
					Float_t sN = 1.0;
				
					Float_t tot1_min=x1 - sN*sigma1;
					Float_t tot1_max=x1 + sN*sigma1;
					Float_t tot2_min=x2 - sN*sigma2;
					Float_t tot2_max=x2 + sN*sigma2;
					
					sprintf(condition, "channel2==%d && channel1 == %d && step1==%f && step2==%f && tot1> %f && tot1<%f && tot2>%f && tot2<%f", CB, CA, currentStep1, currentStep2, tot1_min , tot1_max, tot2_min, tot2_max);
					lmData->Project("hDelta", "(time1-time2)*1E-12", condition, "",stepLimits[step_1+1]-stepLimits[step_1], stepLimits[step_1]);
					
					int binmax = hDelta->GetMaximumBin();
					Float_t hmax= hDelta->GetXaxis()->GetBinCenter(binmax);
					
					hDelta->Fit("gaus", "", "", hmax-500e-12, hmax+500e-12);
					if(hDelta->GetFunction("gaus") == NULL) {  continue; }
					Float_t hmean = hDelta->GetFunction("gaus")->GetParameter(1);
					Float_t hsigma = hDelta->GetFunction("gaus")->GetParameter(2);
				
				
					if(true) {        
						fprintf(ctrTable, "%f\t%f\t%d\t%d\t%f\t%f\t%f\t%f\t%e\t%e\t%d\n", 
							currentStep1, currentStep2, CA, CB ,x1, sigma1, x2, sigma2, 
							fabs(hsigma)*1.0e12, hmean, hDelta->GetEntries()
						
	                                        );
						fflush(ctrTable);
					
						h1->Fill(fabs(hsigma)*1.0e12);
						h2->Fill(fabs(hsigma)*1.0e12*2.354);
					}     
					if(savePDFs){
						TCanvas *c1 = new TCanvas();
						char pdffilename[128];
						c1->Clear();
						c1->Divide(3,1);
						c1->cd(1);
						hToT1->Draw();
						c1->cd(2);
						hDelta->Draw();
						c1->cd(3);
						hToT2->Draw();
						sprintf(pdffilename,"ctr_plots/CTR_chs_%d_%d.pdf", CB, CA); 
						c1->SaveAs(pdffilename);
						delete c1;
					}	
				
					delete hToT1;
					delete hToT2;
				       
					nDoneTotal++;
					nDoneAsic[asicA]++;
					nDoneAsic[asicB]++;
				}
			}
			delete counts2;
		}
	}
	fclose(ctrTable);
		
		
	TCanvas *c2 = new TCanvas();

	// ctr for all channels
	c2->Clear();
	c2->cd(1);
	h1->Draw();
	h1->GetXaxis()->SetTitle("DeltaTime sigma [ps]");


	TCanvas *c3 = new TCanvas();

	// ctr for all channels
	c3->Clear();
	c3->cd(1);
	h2->Draw();
	h2->GetXaxis()->SetTitle("DeltaTime FWHM [ps]");

	//sprintf(pdf5, "%s_all.pdf",filebase);
	//c->SaveAs(pdf5);
    


}
