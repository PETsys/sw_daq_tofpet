{
/*	// Specify ROOT file in command line
	TFile * lmFile = new TFile("R007_c.root", "READ");
	TNtuple * lmData = (TNtuple *)lmFile->Get("lmData");
	*/

	Float_t T = 6250;
	FILE * ctrTable = fopen("sptr.txt", "w");
	TFile *rootFile = new TFile("sptr.root", "RECREATE");
	
	TNtuple *lmData = (TNtuple *)_file0->Get("lmData");
	Float_t step1;      	lmData->SetBranchAddress("step1", &step1);   
	Float_t step2;      	lmData->SetBranchAddress("step2", &step2);   
	Float_t fTime;		lmData->SetBranchAddress("time", &fTime);
	Float_t crystal;	lmData->SetBranchAddress("crystal", &crystal);
	Float_t tot;		lmData->SetBranchAddress("tot", &tot);

	gStyle->SetPalette(1);
	gStyle->SetOptFit(1);
	TCanvas *c = new TCanvas();
	c->Divide(2,1);
    
	Int_t nBinsToT = 2*506;
	TH1F * hToT = new TH1F("hToT", "ToT", nBinsToT, -6, 500);
	hToT->GetXaxis()->SetTitle("ToT (ns)");
	

	Float_t htMin = 5.0E-6;
	Float_t htMax = 6.0E-6;
	TH1F *hTime = new TH1F("hTime", "Time", (htMax - htMin)/50E-12, htMin, htMax);

/*WARNING: change according to 10 ns delay (after Run R025)*/
	float tMin = 5.757E-6 - 0.5E-9;
	float tMax = 5.757E-6 + 0.5E-9;
//	float tMin = 5.747E-6 - 1E-9;
//	float tMax = 5.747E-6 + 1E-9;
//	float totMin = 28.5;
//	float totMax = 30;
	float totMin = 18;
	float totMax = 18.5;
	
	Int_t stepBegin = 0;
	Int_t nEvents = lmData->GetEntries();

	while (stepBegin < nEvents) {
	    hToT->Reset();
	    
	    lmData->GetEntry(stepBegin);
	    
	    Float_t currentStep1 = step1;
	    Float_t currentStep2 = step2;
	    printf("Step (%6.3f %6.3f)\n", currentStep1, currentStep2);
	    
	    Int_t stepEnd = stepBegin + 1;
	    while(stepEnd < nEvents) {
		lmData->GetEntry(stepEnd);	    
		if(step1 != currentStep1 || step2 != currentStep2) 
		    break;
		
		stepEnd++;

		float t = fTime*1e-12;
		bool inPeak = (t >= tMin) && (t <= tMax);
		if(!inPeak) continue;
	
		hToT->Fill(tot);
	}
	Int_t thisStepBegin = stepBegin;
	Int_t thisStepEnd = stepEnd;
	stepBegin = stepEnd;
	
	if(currentStep1 != 70.00) continue;
        
        printf("Event range: %d to %d\n", stepBegin, stepEnd);

	TSpectrum *sp = new TSpectrum();
	int nToTpeaks = sp->Search(hToT, 3.0, "", 0.05);
	Float_t *xx = sp->GetPositionX();
	Float_t *yy = sp->GetPositionY();
	Int_t peakIndex = 0;
	
// 	for(Int_t peakIndex = 0; peakIndex < nToTpeaks; peakIndex++) {
// 	  printf("Peak %d of %d\n", peakIndex+1, nToTpeaks);
// 	  
// 	  TF1 *peakFunction = new TF1("g", "gaus");
// 	  peakFunction->SetParLimits(0, yy[peakIndex]*0.8, yy[peakIndex]*1.2);
// 	  peakFunction->SetParLimits(1, xx[peakIndex]-5, yy[peakIndex]+5);
// 	  hToT->Fit("g");
	  
	  Float_t integral = 0;
	  for(Int_t i = hToT->FindBin(totMin); i <= hToT->FindBin(totMax); i++) {
	    integral += hToT->GetBinContent(i);
	  }
	  printf("Integral is %f\n", integral);

	  hTime->Reset();
	  for(Int_t i = thisStepBegin; i < thisStepEnd; i++) {
	      lmData->GetEntry(i);
	      if(tot < totMin|| tot > totMax) continue;
	      hTime->Fill(fTime * 1E-12);
	  }
	  
	  char functionName[128];
	  float hdMax = hTime->GetMaximum();
	  float hdMean = hTime->GetMean();
	  float hdRMS = hTime->GetRMS();
	  hTime->GetXaxis()->SetRangeUser(tMin, tMax);

	  hdMax = hTime->GetMaximum();
	  hdMean = hTime->GetMean();
	  hdRMS = hTime->GetRMS();
	  
	  sprintf(functionName, "fCoincidence_%07d_%07d_%03d", int(1000*step1), int(1000*step2), peakIndex);	
	  pdf1 = new TF1(functionName, "[0]*exp(-0.5*((x-[1])/[2])**2) + [3]", tMin, tMax);
  //	pdf1->SetNpx(hTime->GetNbinsX());
	  pdf1->SetParName(0, "Constant");
	  pdf1->SetParName(1, "Mean");
	  pdf1->SetParName(2, "Sigma");
	  pdf1->SetParName(3, "Base");
	  pdf1->SetParameter(0, hdMax);	pdf1->SetParLimits(0, 0.5 * hdMax, 1.5 * hdMax);
	  pdf1->SetParameter(1, hdMean);pdf1->SetParLimits(1, tMin, tMax);
	  pdf1->SetParameter(2, hdRMS);	pdf1->SetParLimits(2, 25E-12, 3 * hdRMS);
  /*	pdf1->SetParameter(3, 0);	pdf1->SetParLimits(3, 0, hdMax);*/
	  
	  hTime->Fit(functionName, "R", "", tMin, tMax);
	  hTime->Draw();
	  
	  char hName[128];
	  sprintf(hName, "hToT_%07d_%07d_%03d", int(1000*step1), int(1000*step2), peakIndex);
	  hToT->Clone(hName);
	  sprintf(hName, "hTime_%07d_%07d_%03d", int(1000*step1), int(1000*step2), peakIndex);
	  hTime->Clone(hName);	
//	}

        TF1 *ff = hTime->GetFunction(functionName);
        if(ff != NULL) {        
            fprintf(ctrTable, "%f\t%f\t%f\t%f\t%e\t%e\t%e\t%e\t\n", 
                step1, step2, 
                hToT->GetMean(), hToT->GetRMS(),		    
                fabs(ff->GetParameter(2)), ff->GetParError(2),
                ff->GetParameter(1), ff->GetParError(1)
                );
	    fflush(ctrTable);
        }          
        
    }

    fclose(ctrTable);
    
    c->Close();
    c = new TCanvas();
    c->Divide(2,1);
    c->cd(1);
    hToT->Draw();
    c->cd(2);
    hTime->Draw();
    rootFile->Write();
}
