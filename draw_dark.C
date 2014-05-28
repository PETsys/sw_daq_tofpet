{
//	gStyle->SetOptTitle(1);
	gStyle->SetOptStat(0);
	C=9;


	TNtuple *nTuple = (TNtuple *)_file0->Get("data");
	Float_t step1; nTuple->SetBranchAddress("step1", &step1);
	Float_t step2; nTuple->SetBranchAddress("step2", &step2);
	Float_t rate; nTuple->SetBranchAddress("rate", &rate);
	Float_t channel; nTuple->SetBranchAddress("channel", &channel);
	Float_t asic; nTuple->SetBranchAddress("asic", &asic);
	
	TH1F * hRateZ = new TH1F("hRateZ", "Vbias < Vbreakdown (50 V)", 64, 0, 64);
	TH2F * hRate = new TH2F("hRate", "Vbias", 200, 60, 80, 64, 0, 64);
	
	Int_t N = nTuple->GetEntries();
	for(Int_t n = 0; n < N; n++) {
		nTuple->GetEntry(n);
		
		if(64*asic+channel != C) continue;
		
		printf("%f; %f; %f\n", step1, step2, rate);
		if (fabs(step1-50.0) < 0.01)  hRateZ->Fill(step2, rate);
		else  hRate->Fill(step1, step2, rate);
	}
	
	TCanvas *c = new TCanvas();
	hRateZ->GetXaxis()->SetRangeUser(20, 63);
	hRateZ->GetYaxis()->SetRangeUser(0, 25E6);
	hRateZ->SetLineStyle(2);
	
	TLegend *l = new TLegend(0.15, 0.6, 0.4, 0.85);
	hRateZ->GetXaxis()->SetTitle("Vth_T increment (ADC)");


	hRateZ->Draw(); l->AddEntry(hRateZ);
	
	
	Int_t nFound = 0;
	Int_t myColors[] = {			
			kBlue,
			kRed,
			kGreen,
			kGray, 
			kMagenta,
			kAzure,
			kOrange,
			kCyan,
			kYellow
			
	};
		
	for(Int_t i = 1; i < hRate->GetNbinsX() - 1; i++) {
		Float_t voltage = hRate->GetXaxis()->GetBinCenter(i) - hRate->GetXaxis()->GetBinWidth(i) * 0.5 ;
		char hName[128];
		char hTitle[128];
		sprintf(hName, "hRate_%05d", int(100*voltage));
		sprintf(hTitle, "Vbias = %1.2f", voltage);
		TH1 *h = hRate->ProjectionY(hName, i, i);
		h->SetTitle(hTitle);
		
		if(h->GetEntries() < 1) {
			delete h;
			continue;
		}
		if(nFound >= sizeof(myColors)) continue;
		
		h->SetLineColor(myColors[nFound]);
		l->AddEntry(h);
		h->Draw("SAME");
		nFound += 1;
		
		
	}
	
	
	
	l->Draw("SAME");

}
