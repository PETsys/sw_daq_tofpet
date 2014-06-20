{
	gStyle->SetPalette(1);
	gStyle->SetOptFit(1);
	gStyle->SetOptStat(0);
	TCanvas * c1= new TCanvas();
	TNtuple *nTuple = (TNtuple *)_file0->Get("data");
	Float_t step1; nTuple->SetBranchAddress("step1", &step1);
	Float_t step2; nTuple->SetBranchAddress("step2", &step2);
	Float_t rate; nTuple->SetBranchAddress("rate", &rate);
	Float_t asic; nTuple->SetBranchAddress("asic", &asic);
	Float_t channel; nTuple->SetBranchAddress("channel", &channel);
	
	
	TH2F * hRate = new TH2F("hRate", "Rate", 128, 0, 128, 64, 0, 64);
	TH1F * hMax = new TH1F("hMax", "Max rate", 128, 0, 128);
	TGraphErrors *gBaseline = new TGraphErrors(128);
	gBaseline->SetTitle("Baseline");
	TH1F * hBaseline = new TH1F("Baseline", "Baseline", 64, 0, 64);
	TH1F * hSigma = new TH1F("Sigma", "Sigma", 20, 0, 2);
	
	Int_t N = nTuple->GetEntries();
	for(Int_t n = 0; n < N; n++) {
		nTuple->GetEntry(n);
		
		if(fabs(step1 - 50.0) > 0.01) continue;
		Int_t C = 64*asic + channel;
		hRate->Fill(C, step2, rate);
	}
	
	
	gROOT->ProcessLine(".L cdf.C");
	TF1 *f = new TF1("cdf", cdf, 32, 64, 3);
	f->SetParName(0, "Constant");
	f->SetParName(1, "x0");
	f->SetParName(2, "Sigma");

	FILE *thFile1 = fopen("MA.baseline", "w");
	FILE *thFile2 = fopen("MB.baseline", "w");
	for(Int_t i = 1; i < hRate->GetNbinsX() + 1; i++) {
		Int_t C = i-1;
		char hName[128];
		char hTitle[128];
		sprintf(hName, "hRate_%05d", C);
		sprintf(hTitle, "Channel %5d", C);
		TH1 *h = hRate->ProjectionY(hName, i, i);
		h->SetTitle(hTitle);

		FILE *thFile = C/64 == 0 ? thFile1 : thFile2;
		if(h->GetEntries() < 1) {
		  fprintf(thFile, "%d\t%d\t%f\t%f\n", C/64, C%64, 0, 100);
			delete h;
			continue;
		}
		
// 		Int_t maxJ = 1;
// 		Float_t max = h->GetBinContent(1);
// 		Int_t j = 2;
// 		for(j = 2; j < h->GetNbinsX()+1; j++) {
// 			Float_t r = h->GetBinContent(j);
// 			if(r > 1.2*max) {
// 				maxJ = j;
// 				max = h->GetBinContent(j);
// 			}
// 			else if (r < 0.9*max && max > 2E6) {
// 				break;
// 			}
// 		}
		Float_t max = h->GetMaximum();
		Int_t maxJ = h->GetMaximumBin();
		
		Int_t firstAboveZeroJ = h->FindFirstBinAbove(0);
		

		cout << i << " " << C << " " << " " << maxJ << firstAboveZeroJ << endl;
		
		f->SetParameter(0, max);	f->SetParLimits(0, 0.8*max, 1.01*max);
		f->SetParameter(1, maxJ-0.5);	f->SetParLimits(1, firstAboveZeroJ-1.5, 64);
		f->SetParameter(2, 0.02);	f->SetParLimits(2, 0.01, 5.0);
		h->Fit("cdf", "", "", 32, maxJ+0.25);
		Float_t x0 = f->GetParameter(1);
		Float_t x0_e = f->GetParError(1);
		Float_t sigma = f->GetParameter(2);


		fprintf(thFile, "%d\t%d\t%f\t%f\n", C/64, C%64, x0, sigma);
		hMax->SetBinContent(i, max);
		hBaseline->Fill(x0);
		hSigma->Fill(sigma);
		gBaseline->SetPoint(i-1, i-1, x0);
		gBaseline->SetPointError(i-1, 0.1, sigma);
	}
	fclose(thFile1);
	fclose(thFile2);
	
	
	delete c1;
	
	c1 = new TCanvas();
	c1->Divide(2, 2);
	gStyle->SetOptStat(0);
	
	c1->cd(1);
/*	hRate->GetXaxis()->SetTitle("Channel");
	hRate->GetYaxis()->SetTitle("vth_T (ADC)");
	hRate->GetYaxis()->SetRangeUser(32, 64);
	hRate->GetZaxis()->SetRangeUser(0, 25E6);
	hRate->Draw("COLZ");*/
	hMax->GetXaxis()->SetRangeUser(0, 128);
	hMax->GetXaxis()->SetTitle("Channel ID");
	hMax->GetYaxis()->SetRangeUser(0, 25E6);
	hMax->GetYaxis()->SetTitle("Max trigger rate  (Hz)");
	hMax->Draw();
	
	c1->cd(2);
	gBaseline->Draw("AP");
	gBaseline->GetXaxis()->SetTitle("Channel ID");
	gBaseline->GetXaxis()->SetRangeUser(0, 128);
	gBaseline->GetYaxis()->SetTitle("vth_T (ADC)");
	gBaseline->GetYaxis()->SetRangeUser(32, 64);
	
	gStyle->SetOptStat(1);
	c1->cd(3);
	hBaseline->SetStats(1);
	hBaseline->GetXaxis()->SetTitle("vth_T (ADC)");
	hBaseline->GetYaxis()->SetTitle("Number of channels");
	hBaseline->Draw();
	c1->cd(4);
	hSigma->GetXaxis()->SetTitle("vth_T (ADC)");
	hSigma->GetYaxis()->SetTitle("Number of channels");
	hSigma->Draw();
}
