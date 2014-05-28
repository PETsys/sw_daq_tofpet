{
	gStyle->SetPalette(1);
	gStyle->SetOptFit(1);

/*	TFile * lmFile = new TFile("lmData.root", "READ");
	TNtuple * lmData = (TNtuple *)lmFile->Get("lmData");*/
	Float_t crystal1;	lmData->SetBranchAddress("crystal1", &crystal1);
	Float_t tot1;		lmData->SetBranchAddress("tot1", &tot1);
	Float_t crystal2;	lmData->SetBranchAddress("crystal2", &crystal2);
	Float_t tot2;		lmData->SetBranchAddress("tot2", &tot2);
	Float_t delta;		lmData->SetBranchAddress("delta", &delta);


	TH2F * hToT = new TH2F("hToT", "hToT", 128, 0, 127, 160, 0, 1000);
	TH1F * hToT1 = new TH1F("hToT1", "hToT1", 160, 0, 1000);
	TH1F * hToT2 = new TH1F("hToT2", "hToT2", 160, 0, 1000);
	Double_t wMax = 5E-9;
	TH1F *hDelta = new TH1F("delta", "delta", 2*wMax/50E-12, -wMax, wMax);

	Int_t nEntries = lmData->GetEntries();

	for (Int_t i = 0; i < nEntries; i++) {
		lmData->GetEntry(i);
//		printf("%lf\n", tot1*1E6);
		hToT->Fill(crystal1, tot1);
		hToT->Fill(crystal2, tot2);
		
	    hToT1->Fill(tot1);
	    hToT2->Fill(tot2);
		
//		if(tot1 < 10 || tot2 < 10) continue;
        if(tot1 < 300 || tot2 < 300) continue;
//		if (tot1 < 335 || tot1 > 345) continue;
//		if (tot2 < 350 || tot2 > 375) continue;
   		hDelta->Fill(delta*1E-12);

	}
	
	
	hToT->Draw("COLZ");

	pdf1 = new TF1("pdf1", "[0]*exp(-0.5*((x-[1])/[2])**2) + [3]", -90, 90);
	pdf1->SetParName(0, "Constant"); pdf1->SetParameter(0, hDelta->GetMaximum());
	pdf1->SetParName(1, "Mean"); pdf1->SetParameter(1, hDelta->GetMean());
	pdf1->SetParName(2, "Sigma"); pdf1->SetParameter(2, hDelta->GetRMS());
	pdf1->SetParName(3, "Base"); pdf1->SetParameter(3, 0);
	
	hDelta->Fit("pdf1");
	hDelta->Draw();
}
