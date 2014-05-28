{
	Float_t T = 6250;
	FILE * ctrTable = fopen("str.txt", "w");
	TFile *rootFile = new TFile("str.root", "RECREATE");
	
	TNtuple *lmData = (TNtuple *)_file0->Get("lmData");
	Float_t step1;      	lmData->SetBranchAddress("step1", &step1);   
	Float_t step2;      	lmData->SetBranchAddress("step2", &step2);   
	Float_t fTime;		lmData->SetBranchAddress("time", &fTime);
	Float_t channel;	lmData->SetBranchAddress("channel", &channel);
	Float_t tot;		lmData->SetBranchAddress("tot", &tot);

	gStyle->SetPalette(1);
	gStyle->SetOptFit(1);

	TCanvas *c = new TCanvas();
	c->Divide(2,1);
    
	Int_t nBinsToT = 10000;
	TH1F * hToT = new TH1F("hToT", "ToT", nBinsToT, 0, 1E3);
	hToT->GetXaxis()->SetTitle("ToT (ns)");
	

	Float_t htMin = 0;
	Float_t htMax = 6.4E6;
	TH1F *hTime = new TH1F("hTime", "Time", (htMax - htMin)/50, htMin, htMax);
	hTime->GetXaxis()->SetTitle("Time (ps)");
	
	Float_t tMin = 3.206E6-100E3;
	Float_t tMax = 3.206E6+100E3;

	
	Int_t stepBegin = 0;
	Int_t nEvents = lmData->GetEntries();

	while (stepBegin < nEvents) {
		hToT->Reset();
		hTime->GetXaxis()->SetRangeUser(htMin, htMax);
		hToT->GetXaxis()->SetRangeUser(0, 1000);
		
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
			if((Int_t(channel) / 64) != ASIC) continue;
			
			hToT->Fill(tot);
		}
		
		Int_t thisStepBegin = stepBegin;
		Int_t thisStepEnd = stepEnd;
		stepBegin = stepEnd;

		printf("Event range: %d to %d\n", stepBegin, stepEnd);

		hTime->Reset();
		for(Int_t i = thisStepBegin; i < thisStepEnd; i++) {
			lmData->GetEntry(i);
			if((Int_t(channel) / 64) != ASIC) continue;
			
		
			hTime->Fill(fTime);
		}
			
		char functionName[128];
		
		TSpectrum *spectrum = new TSpectrum();
		c->cd(1);
		spectrum->Search(hTime);
		Int_t nPeaks = spectrum->GetNPeaks();
		if (nPeaks == 0) {
			printf("No peaks in hTime!!!\n");
			continue;
		}
		Float_t *xPositions = spectrum->GetPositionX();
		Float_t tPeak = xPositions[0];
		
		hTime->GetXaxis()->SetRangeUser(tPeak - 1000, tPeak + 1000);		
		hTime->Fit("gaus");
		hTime->Draw();
		
		c->cd(2);
		
		spectrum->Search(hToT);
		nPeaks = spectrum->GetNPeaks();
		if (nPeaks == 0) {
			printf("No peaks in hToT!!!\n");
			continue;
		}
		xPositions = spectrum->GetPositionX();
		Float_t totPeak = xPositions[0];
		hToT->GetXaxis()->SetRangeUser(totPeak - 1.0, totPeak + 1.0);
		hToT->Fit("gaus");
		hToT->Draw();
		
		
		
			
		char hName[128];
		sprintf(hName, "hToT_%07d_%07d", int(1000*step1), int(1000*step2));
		hToT->Clone(hName);
		sprintf(hName, "hTime_%07d_%07d", int(1000*step1), int(1000*step2));
		hTime->Clone(hName);	

		TF1 *ff1 = hTime->GetFunction("gaus");
		TF1 *ff2 = hToT->GetFunction("gaus");
		if(ff1 != NULL && ff2 != NULL) {        
			fprintf(ctrTable, "%f\t%f\t%f\t%f\t%e\t%e\t%e\t%e\t\n", 
				step1, step2, 
				ff2->GetParameter(1), ff2->GetParameter(2),
				fabs(ff1->GetParameter(2)), ff1->GetParError(2),
				ff1->GetParameter(1), ff1->GetParError(1)
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
