{
	Float_t T = 6250;
	FILE * ctrTable = fopen("str.txt", "w");
	TFile *rootFile = new TFile("str.root", "RECREATE");
	
	TNtuple *data = (TNtuple *)_file0->Get("data");
	Float_t step1;      	data->SetBranchAddress("step1", &step1);   
	Float_t step2;      	data->SetBranchAddress("step2", &step2);   
	Float_t channel;		data->SetBranchAddress("channel", &channel);
	Float_t tac;		data->SetBranchAddress("tac", &tac);
	Float_t tcoarse; 		data->SetBranchAddress("tcoarse", &tcoarse);
	Float_t tfine; 		data->SetBranchAddress("tfine", &tfine);
	Float_t ecoarse; 		data->SetBranchAddress("ecoarse", &ecoarse);
	Float_t efine; 		data->SetBranchAddress("efine", &efine);
	
	
	gStyle->SetPalette(1);
	gStyle->SetOptFit(1);

	TCanvas *c = new TCanvas();
	c->Divide(2,1);
    
	TH1F * hToT = new TH1F("hToT", "ToT", 1024, 0, 1024*T/1000);
	hToT->GetXaxis()->SetTitle("ToT (ns)");

	TH1F * hTFine = new TH1F("hTFine", "TFine", 1024, 0, 1024);
	hTFine->GetXaxis()->SetTitle("TFine (LSB)");
	
	TH1F * hEFine = new TH1F("hEFine", "EFine", 1024, 0, 1024);
	hEFine->GetXaxis()->SetTitle("EFine (LSB)");


	
	Int_t stepBegin = 0;
	Int_t nEvents = data->GetEntries();

	while (stepBegin < nEvents) {
		hToT->Reset();
		hTFine->Reset();
		hEFine->Reset();
		
		data->GetEntry(stepBegin);
		
		Float_t currentStep1 = step1;
		Float_t currentStep2 = step2;
		printf("Step (%6.3f %6.3f)\n", currentStep1, currentStep2);
		
		Int_t stepEnd = stepBegin + 1;
		while(stepEnd < nEvents) {
			data->GetEntry(stepEnd);	    
			if(step1 != currentStep1 || step2 != currentStep2) 
				break;			
			stepEnd++;			
			if((Int_t(channel) / 64) != ASIC) continue;
			if(Int_t(tac) != 0) continue;
			
			Int_t tot = ecoarse - tcoarse;
			hToT->Fill(tot * T / 1000);
			hTFine->Fill(tfine);
			hEFine->Fill(efine);
		}
		
		Int_t thisStepBegin = stepBegin;
		Int_t thisStepEnd = stepEnd;
		stepBegin = stepEnd;

		printf("Event range: %d to %d\n", stepBegin, stepEnd);


		hToT->Fit("gaus");
		hTFine->Fit("gaus");
		hEFine->Fit("gaus");
		
		
		char hName[1024];
		sprintf(hName, "hToT_%05d_%05d", int(step1*1000), int(step2*1000));
		hToT->Clone(hName);
		sprintf(hName, "hTFine_%05d_%05d", int(step1*1000), int(step2*1000));
		hTFine->Clone(hName);
		sprintf(hName, "hEFine_%05d_%05d", int(step1*1000), int(step2*1000));
		hEFine->Clone(hName);
		

		TF1 *fToT = hToT->GetFunction("gaus");
		TF1 *fTFine = hTFine->GetFunction("gaus");
		TF1 *fEFine = hEFine->GetFunction("gaus");
		
		if(fToT != NULL && fTFine != NULL && fEFine != NULL) {        
			fprintf(ctrTable, "%f\t%f\t%f\t%f\t%e\t%e\t%e\t%e\t\n", 
				step1, step2, 
				fToT->GetParameter(1), fEFine->GetParameter(2),
				fabs(fTFine->GetParameter(2)), fTFine->GetParError(2),
				fTFine->GetParameter(1), fTFine->GetParError(1)
				);
				
			fflush(ctrTable);
			
		
		}          
		
	}

    fclose(ctrTable);
    
    rootFile->Write();
}
