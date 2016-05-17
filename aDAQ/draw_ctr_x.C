#include <TFile.h>
#include <TNtuple.h>
#include <TH1F.h>
#include <TSpectrum.h>
#include <TF1.h>
#include <TH2F.h>
#include <TStyle.h>
#include <TCanvas.h>
#include <TProfile.h>

void draw_ctr_x(TFile *lmFile) {
	TNtuple *lmIndex = (TNtuple *)lmFile->Get("lmIndex");
	TNtuple *lmData = (TNtuple *)lmFile->Get("lmData");
		
	Int_t CA = 901;
	Int_t CB = 122;
	
	Float_t T = 6250;
	FILE * ctrTable = fopen("ctr.txt", "w");
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
	
	
    
	Int_t nBinsToT = 500;
	TH2F * hToTx = new TH2F("hToTx", "ToT", nBinsToT, 0, 500, nBinsToT, 0, 500);
	hToTx->GetXaxis()->SetTitle("Crystal 1 ToT (ns)");
	hToTx->GetYaxis()->SetTitle("Crystal 2 ToT (ns)");
		
	Double_t tBinWidth = 50E-12;
	Double_t wMax = 5E-9;
	TH1F *hDelta = new TH1F("hDelta", "Delta", 2*wMax/tBinWidth, -wMax, wMax);	
	TH1F *hDeltaTW1 = new TH1F("hDelta_tw1", "delta_tw1", 2*wMax/tBinWidth, -wMax, wMax);
	TH1F *hDeltaTW2 = new TH1F("hDlta_tw2", "delta_tw2", 2*wMax/tBinWidth, -wMax, wMax);	
	
	TH2F *hDD = new TH2F("hDD", "hDD", 500, 0, 2*T*1E-12, 160, -2E-9, 2E-9);
	hDD->GetXaxis()->SetTitle("#frac{t1+t2}{2} (s)");
	hDD->GetYaxis()->SetTitle("t1-t2 (s)");



	
	Int_t stepBegin = 0;
      

      Int_t nEvents = lmData->GetEntries();

    do {
        hToTx->Reset();
        hDelta->Reset();
	hDD->Reset();
        
        lmData->GetEntry(stepBegin);
        Float_t currentStep1 = step1;
        Float_t currentStep2 = step2;
      

	printf("STEP: %lf %lf %lf %lf\n", step1, step2, currentStep1, currentStep2);
        Int_t stepEnd = stepBegin + 1;
        while(stepEnd < nEvents) {
		lmData->GetEntry(stepEnd);	    
		if(step1 != currentStep1 || step2 != currentStep2) 
			break;
		
		if((CA == -1 || channel1 == CA) && (CB == -1 || channel2 == CB)) {
			hToTx->Fill(tot1, tot2);
		}
            
            stepEnd++;
            
        }
        
        printf("Step (%6.3f %6.3f) event range: %d to %d\n", currentStep1, currentStep2, stepBegin, stepEnd);
		TH1 *hToT1 = hToTx->ProjectionX("hToT1");
		TH1 *hToT2 = hToTx->ProjectionY("hToT2");
        
        TSpectrum *spectrum = new TSpectrum();
        spectrum->Search(hToT1, 3, " ",  0.2);
        Int_t nPeaks = spectrum->GetNPeaks();
        if (nPeaks == 0) {
            printf("No peaks in hToT1!!!\n");
            stepBegin = stepEnd; continue;
        }
#if ROOT_VERSION_CODE > ROOT_VERSION(6,0,0)
	Double_t *xPositions1 = spectrum->GetPositionX();
#else
	Float_t  *xPositions1 = spectrum->GetPositionX();
#endif
    
        Float_t x1_psc = 0;
	Float_t x1_pe = 0;
        for(Int_t i = 0; i < nPeaks; i++) {
            if(xPositions1[i] > x1_pe) {
				if(x1_pe > x1_psc)
				x1_psc = x1_pe;
				x1_pe = xPositions1[i];
			}
        } 


        TSpectrum *spectrum2 = new TSpectrum();
        spectrum2->Search(hToT2, 3, " ",  0.2);
        nPeaks = spectrum->GetNPeaks();
        if (nPeaks == 0) {
            printf("No peaks in hToT2!!!\n");
            stepBegin = stepEnd; continue;
        }
#if ROOT_VERSION_CODE > ROOT_VERSION(6,0,0)
	Double_t *xPositions2 = spectrum2->GetPositionX();
#else
	Float_t  *xPositions2 = spectrum2->GetPositionX();
#endif
       
        Float_t x2_psc = 0;
	Float_t x2_pe = 0;
        for(Int_t i = 0; i < nPeaks; i++) {
            if(xPositions2[i] > x2_pe) {
				if(x2_pe > x2_psc)
					x2_psc = x2_pe;
				x2_pe = xPositions2[i];
			}
        } 
		
	Float_t x1 = x1_pe;
	Float_t x2 = x2_pe;
	

	hToT1->Fit("gaus", "", "", x1-10, x1+10);
	if(hToT1->GetFunction("gaus") == NULL) { stepBegin = stepEnd;   continue; }
	x1 = hToT1->GetFunction("gaus")->GetParameter(1);
        Float_t sigma1 = hToT1->GetFunction("gaus")->GetParameter(2);

	hToT2->Fit("gaus", "", "", x2-10, x2+10);
	if(hToT2->GetFunction("gaus") == NULL) { stepBegin = stepEnd;   continue; }
	x2 = hToT2->GetFunction("gaus")->GetParameter(1);
        Float_t sigma2 = hToT2->GetFunction("gaus")->GetParameter(2);
	
	
	Float_t sN=1;
        for(Int_t i = stepBegin; i < stepEnd; i++) {
            lmData->GetEntry(i);
	    if((CA != -1 && channel1 != CA) || (CB != -1 && channel2 != CB)) continue;
     
            if((tot1 < (x1 - sN*sigma1)) || (tot1 > (x1 + sN*sigma1))) 
                continue;
        
            if((tot2 < (x2 - sN*sigma2)) || (tot2 > (x2 + sN*sigma2))) 
                continue;
	    
		Float_t delta = fTime1 - fTime2;
		Float_t correctedDelta = delta*1E-12;
		
 		Float_t tw1 = 0;//+fTW1->Eval(tot1);
 		Float_t tw2 = 0;//-fTW2->Eval(tot2);
 		correctedDelta -= (tw1 + tw2);

		
		hDelta->Fill(correctedDelta);
		hDD->Fill(fmod(((fTime1+fTime2)/2), 2*T)*1E-12, correctedDelta);
        }
        
        //pDD = hDD->ProfileX("hDD_pfx_2");
        
	char functionName[128];
	sprintf(functionName, "fCoincidence_%07d_%07d", int(1000*step1), int(1000*step2));	
	TF1 *pdf1 = new TF1(functionName, "[0]*exp(-0.5*((x-[1])/[2])**2) + [3]", -wMax, wMax);
	pdf1->SetNpx(hDelta->GetNbinsX());
	pdf1->SetParName(0, "Constant");
	pdf1->SetParName(1, "Mean");
	pdf1->SetParName(2, "Sigma");
	pdf1->SetParName(3, "Base");
	float hdMax = hDelta->GetMaximum();
	float hdMean = hDelta->GetMean();
	float hdRMS = hDelta->GetRMS();
	float hdAvg = hDelta->GetEntries()/hDelta->GetNbinsX();
	pdf1->SetParameter(0, hdMax);	pdf1->SetParLimits(0, 0.5 * hdMax, 1.5 * hdMax);
	pdf1->SetParameter(1, hdMean);	pdf1->SetParLimits(1, hdMean - 1.0 * hdRMS, hdMean + 1.0 * hdRMS);
	pdf1->SetParameter(2, hdRMS);	pdf1->SetParLimits(2, 25E-12,  1.0 * hdRMS);
	pdf1->SetParameter(3, 0);	pdf1->SetParLimits(3, 0, hdAvg);
	
	hDelta->Fit(functionName);
        hDelta->Draw();
	
	char hName[128];
	sprintf(hName, "hDelta_%07d_%07d", int(1000*step1), int(1000*step2));
	hDelta->Clone(hName);
	sprintf(hName, "hToT1_%07d_%07d", int(1000*step1), int(1000*step2));
	hToT1->Clone(hName);	
	sprintf(hName, "ToT2_%07d_%07d", int(1000*step1), int(1000*step2));
	hToT2->Clone(hName);	

        TF1 *ff = hDelta->GetFunction(functionName);
        if(ff != NULL) {        
            fprintf(ctrTable, "%f\t%f\t%f\t%f\t%f\t%f\t%e\t%e\t%e\t%e\t\n", 
                step1, step2, 
                x1, sigma1, x2, sigma2, 
                fabs(ff->GetParameter(2))*1.0e12, ff->GetParError(2),
                ff->GetParameter(1), ff->GetParError(1)
//                ff->GetNFD() != 0 ? (ff->GetChisquare()/ff->GetNDF()) : INFINITY
                );
	    fflush(ctrTable);
        }          
        
        stepBegin = stepEnd;  
	//if(currentStep1 == 44 && currentStep2 ==52)break;
       
	//c = new TCanvas();
	c->Clear();
	c->Divide(3,1);
	c->cd(1);
	hToT1->Draw();
	c->cd(2);
	hDelta->Draw();
	c->cd(3);
	hToT2->Draw();
      	char pdffilename[128];
	sprintf(pdffilename,"pdfs/postamp%d_vth%d.pdf",int(step1), int(step2));
	c->SaveAs(pdffilename);
        //delete c;
    } while(stepBegin < nEvents); 

    fclose(ctrTable);
    
}
