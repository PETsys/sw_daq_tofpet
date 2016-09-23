void draw_threshold_scan(Int_t maxAsics, bool Save = false)
{
    
	TFile *hFile = new TFile("draw_threshold_scan.root", "RECREATE");

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
	
	
	TH2F * hRate = new TH2F("hRate", "Rate", maxAsics*64, 0, maxAsics*64, 64, 0, 64);
	TH1F * hMax = new TH1F("hMax", "Max rate", maxAsics*64, 0, maxAsics*64);
	TGraphErrors *gBaseline = new TGraphErrors(maxAsics*64);
	gBaseline->SetTitle("Baseline");
	TH1F * hBaseline = new TH1F("Baseline", "Baseline", 64, 0, 64);
	TH1F * hSigma = new TH1F("Sigma", "Sigma", 20, 0, 2);
	
	Int_t N = nTuple->GetEntries();
	for(Int_t n = 0; n < N; n++) {
		nTuple->GetEntry(n);
		Int_t C = 64*asic + channel;
		hRate->Fill(C, step2, rate);
	}
	
	
	//gROOT->ProcessLine(".L cdf.C");
	TF1 *cdf = new TF1("cdf", "[0]*(ROOT::Math::normal_cdf(x, [2], [1]))", 32, 64);
	cdf->SetParName(0, "Constant");
	cdf->SetParName(1, "x0");
	cdf->SetParName(2, "Sigma");


	FILE * thFile;

	for(Int_t i = 1; i < hRate->GetNbinsX() + 1; i++) {
		
		
		
		Int_t C = i-1;
		char filename[256];
		
		if(C%64==0){
			sprintf(filename, "asic%d.baseline",C/64);
			thFile=fopen(filename, "w");
		}
		char hName[256];
		char hTitle[256];
		sprintf(hName, "hRate_%05d", C);
		sprintf(hTitle, "Channel %5d", C);
		TH1 *h = hRate->ProjectionY(hName, i, i);
		h->SetTitle(hTitle);
		
	       


		//	FILE *thFile = C/64 == 0 ? thFile1 : thFile2;
		
		if(h->GetEntries() < 1) {
		  fprintf(thFile, "%d\t%d\t%f\t%f\n", C/64, C%64, 0.0, 100.0);
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
		
		cdf->SetParameter(0, max);	cdf->SetParLimits(0, 0.8*max, 1.01*max);
		cdf->SetParameter(1, maxJ-0.5);	cdf->SetParLimits(1, firstAboveZeroJ-1.5, 64);
		cdf->SetParameter(2, 0.02);	cdf->SetParLimits(2, 0.01, 5.0);
		h->Fit("cdf", "", "", 32, maxJ+0.25);
		Float_t x0 = cdf->GetParameter(1);
		Float_t x0_e = cdf->GetParError(1);
		Float_t sigma = cdf->GetParameter(2);


		fprintf(thFile, "%d\t%d\t%f\t%f\n", C/64, C%64, x0, sigma);
		hMax->SetBinContent(i, max);
		hBaseline->Fill(x0);
		hSigma->Fill(sigma);
		gBaseline->SetPoint(i-1, i-1, x0);
		gBaseline->SetPointError(i-1, 0.1, sigma);
	}
	fclose(thFile);
	
	
	
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
	hMax->GetXaxis()->SetRangeUser(0, 256);
	hMax->GetXaxis()->SetTitle("Channel ID");
	hMax->GetYaxis()->SetRangeUser(0, 25E6);
	hMax->GetYaxis()->SetTitle("Max trigger rate  (Hz)");
	hMax->Draw();
	
	c1->cd(2);
	gBaseline->Draw("AP");
	gBaseline->GetXaxis()->SetTitle("Channel ID");
	gBaseline->GetXaxis()->SetRangeUser(0, 256);
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
	hFile->Write();
	
	if(Save){
		c1->SaveAs("/tmp/baseline_dummy.pdf");
	}
	
}
