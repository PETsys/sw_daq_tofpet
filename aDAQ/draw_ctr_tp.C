{
/*	// Specify ROOT file in command line
	TFile * lmFile = new TFile("R007_c.root", "READ");
	TNtuple * lmData = (TNtuple *)lmFile->Get("lmData");
	*/

	Float_t T = 7.5E-9;
	FILE * ctrTable = fopen("ctr_tp.txt", "w");
	Float_t step1;      lmData->SetBranchAddress("step1", &step1);   
	Float_t step2;      lmData->SetBranchAddress("step2", &step2);   
	Float_t fTime1;		lmData->SetBranchAddress("time1", &fTime1);
	Float_t crystal1;	lmData->SetBranchAddress("crystal1", &crystal1);
	Float_t tot1;		lmData->SetBranchAddress("tot1", &tot1);
	Float_t fTime2;		lmData->SetBranchAddress("time2", &fTime2);
	Float_t crystal2;	lmData->SetBranchAddress("crystal2", &crystal2);
	Float_t tot2;		lmData->SetBranchAddress("tot2", &tot2);
	Float_t delta;		lmData->SetBranchAddress("delta", &delta);
	Float_t fTimediff;
	
	gStyle->SetPalette(1);
	gStyle->SetOptFit(1);
    TCanvas *c = new TCanvas();
    c->Divide(3,1);
    
	Int_t nBinsToT = 250;
		
	Double_t wMax = 10E-9;
	TH1F *hDelta = new TH1F("delta", "delta", 2*wMax/50E-12, -wMax, wMax);	
	TH1F *hDeltaP1 = new TH1F("delta_p1", "delta_p1", 10000, -0.5E-6, 0.5E-6);
	TH1F *hDeltaP2 = new TH1F("delta_p2", "delta_p2", 10000, -0.5E-6, 0.5E-6);

	TH1F *hDeltai = new TH1F("deltai", "Pulse delta", 2*wMax/50E-12, -wMax, wMax);	
	TH1F *hDeltaiP1 = new TH1F("deltai_p1", "Pulse error (ch A)", 10000, -0.5E-6, 0.5E-6);
	TH1F *hDeltaiP2 = new TH1F("deltai_p2", "Pulse error (ch B)", 10000, -0.5E-6, 0.5E-6);

	
	Int_t stepBegin = 0;
      Int_t nEvents = lmData->GetEntries();

    do {
        hDelta->Reset();
	hDeltaP1->Reset();
	hDeltaP2->Reset();
        
        lmData->GetEntry(stepBegin);
        Float_t currentStep1 = step1;
        Float_t currentStep2 = step2;
        
        Int_t stepEnd = stepBegin + 1;
        while(stepEnd < nEvents) {
            lmData->GetEntry(stepEnd);
	    
            if(step1 != currentStep1 || step2 != currentStep2) 
                break;
	    
		hDelta->Fill((fTime1 - fTime2) * 1E-12);
		Float_t offset = 512*T;
		hDeltaP1->Fill(fTime1 * 1E-12 - step2 * T - offset);
		hDeltaP2->Fill(fTime2 * 1E-12 - step2 * T - offset);
//		printf("%g - %g = %g\n", fTime1 * 1E-12, step2 * T, fTime1 * 1E-12 - step2 * T);
                
            
            stepEnd++;
            
        }
        
        printf("Step (%6.3f %6.3f) event range: %d to %d\n", currentStep1, currentStep2, stepBegin, stepEnd);
        
	char functionName[128];
	sprintf(functionName, "fCoincidence_%07d_%07d", int(1000*step1), int(1000*step2));	
	pdf1 = new TF1(functionName, "[0]*exp(-0.5*((x-[1])/[2])**2) + [3]", -90, 90);
	pdf1->SetNpx(hDelta->GetNbinsX());
	pdf1->SetParName(0, "Constant");
	pdf1->SetParName(1, "Mean");
	pdf1->SetParName(2, "Sigma");
	pdf1->SetParName(3, "Base");
	float hdMax = hDelta->GetMaximum();
	float hdMean = hDelta->GetMean();
	float hdRMS = hDelta->GetRMS();
	pdf1->SetParameter(0, hdMax);	pdf1->SetParLimits(0, 0.5 * hdMax, 2.0 * hdMax);
	pdf1->SetParameter(1, hdMean);	pdf1->SetParLimits(1, hdMean - 0.5 * fabs(hdMean), hdMean + 0.5 * fabs(hdMean));
	pdf1->SetParameter(2, hdRMS);	pdf1->SetParLimits(2, 25E-12, 1.2 * hdRMS);
	pdf1->SetParameter(3, 0);	pdf1->SetParLimits(3, 0, 0.01*hdMax);
	
	c->cd(2);
	hDelta->Draw();
	hDelta->Fit(functionName);
	
	c->cd(1);
	hDeltaP1->Draw();
	
	c->cd(3);
	hDeltaP2->Draw();
	
	hDeltai->Add(hDelta);
	hDeltaiP1->Add(hDeltaP1);
	hDeltaiP2->Add(hDeltaP2);
	
	char hName[128];
	sprintf(hName, "hDelta_%07d_%07d", int(1000*step1), int(1000*step2));
	hDelta->Clone(hName);

        TF1 *ff = hDelta->GetFunction(functionName);
        if(ff != NULL) {        
            fprintf(ctrTable, "%f\t%f\t%e\t%e\t%e\t%e\t%e\t%e\t%e\t%e\t\n", 
                step1, step2, 
                hDeltaP1->GetMean(), hDeltaP1->GetRMS(),
		hDeltaP2->GetMean(), hDeltaP2->GetRMS(),
                fabs(ff->GetParameter(2)), ff->GetParError(2),
                ff->GetParameter(1), ff->GetParError(1)
//                ff->GetNFD() != 0 ? (ff->GetChisquare()/ff->GetNDF()) : INFINITY
                );
	    fflush(ctrTable);
	    Float_t sigma = fabs(ff->GetParameter(2));
	    if(sigma > 150E-12) {
	      sprintf(hName, "pdf/hDelta_%07d_%07d.pdf", int(1000*step1), int(1000*step2));
	      c->SaveAs(hName);
	    }
        }          
        
        
        
        stepBegin = stepEnd;   
    } while(stepBegin < nEvents); 

    fclose(ctrTable);
  
    Float_t m = 0;
    Float_t W1 = 400E-12;

    c->cd(2);
    m = hDeltai->GetMean();    
    hDeltai->GetXaxis()->SetRangeUser(m-W1, m+W1);
    hDeltai->GetXaxis()->SetTitle("Delta (s)");
    hDeltai->Draw();	
    

    c->cd(1);
    m = hDeltaiP1->GetMean();
    hDeltaiP1->GetXaxis()->SetRangeUser(m-W1, m+W1);
    hDeltaiP1->GetXaxis()->SetTitle("Delta (s)");
    hDeltaiP1->Draw();
    
    c->cd(3);
    m = hDeltaiP2->GetMean();
    hDeltaiP2->GetXaxis()->SetRangeUser(m-W1, m+W1);
    hDeltaiP2->GetXaxis()->SetTitle("Delta (s)");
    hDeltaiP2->Draw();

}
