{
	TGraphErrors *gSummary = new TGraphErrors(128);
	TH1 * hSummary = new TH1F("summary1", "Summary", 1000, 0, 500E-12);
	TH1 * hCounts = new TH1F("counts1", "Counts", 128, 0, 128);

	TCanvas *c = new TCanvas();

	Int_t nPoints = 0;
	for(Int_t asic = 0; asic < 64; asic += 1) {
		for(Int_t channel = 0; channel < 64; channel+= 1) {
			
			char hName[128];
			sprintf(hName, "C%03d_%02d_%d_A_T_control_E", asic, channel, 0);			
			TH1 * hT= (TH1 *) _file0->Get(hName);
			if (hT == NULL) continue;
			
		
			
			for (Int_t tac = 1; tac < 4; tac += 1) {
				sprintf(hName, "C%03d_%02d_%d_A_T_control_E", asic, channel, tac);			
				hT->Add((TH1 *) _file0->Get(hName), 1);
			}
			
			Float_t max_i = hT->GetMaximumBin();
			Float_t max_t = hT->GetBinCenter(max_i);
			hT->Fit("gaus", "", "", max_t - 0.5, max_t + 0.5);
			
			TF1 * gaussianFit = hT->GetFunction("gaus");
			if(gaussianFit == NULL) continue;
			Float_t sigma = gaussianFit->GetParameter(2);
			Float_t sigmaError = gaussianFit->GetParError(2);
			
			gSummary->SetPoint(nPoints, ((64*asic)%128 + channel), sigma * 6.25E-9);			
			gSummary->SetPointError(nPoints, 0, sigmaError * 6.25E-9);
			
			hSummary->Fill(sigma * 6.25E-9);
			
			hCounts->Fill((64*asic)%128 +channel, hT->GetEntries());
			
			nPoints += 1;
			
		}
	}
	c->Close();
	c = new TCanvas();
	c->Divide(2,2);
	c->cd(1);
	gSummary->SetTitle("TDC resolution");
	gSummary->GetXaxis()->SetTitle("Channel");
	gSummary->GetYaxis()->SetTitle("Resolution (s RMS)");
	gSummary->Set(nPoints);
	gSummary->GetXaxis()->SetRangeUser(0, 128);
	gSummary->GetYaxis()->SetRangeUser(0, 0.5E-9);
	gSummary->Draw("AP");
	c->cd(2);
	hSummary->SetTitle("TDC resolution histogram");
	hSummary->GetXaxis()->SetTitle("TDC resolution (s RMS)");
	hSummary->Draw();
	c->cd(3);
	hCounts->GetYaxis()->SetRangeUser(0, hCounts->GetMaximum() * 1.10);
	hCounts->GetXaxis()->SetTitle("Channel");
	hCounts->Draw();
	
	
	c = new TCanvas();
	TGraphErrors *gSummary = new TGraphErrors(128);
	TH1 * hSummary = new TH1F("summary2", "summary", 1000, 0, 500E-12);
	TH1 * hCounts = new TH1F("counts2", "Counts", 128, 0, 128);
	TGraph *gLeakage = new TGraph(4*128);
	
	nPoints = 0;
	for(Int_t asic = 0; asic < 64; asic += 1) {
		for(Int_t channel = 0; channel < 64; channel+= 1) {
			
			char hName[128];
			sprintf(hName, "C%03d_%02d_%d_B_T_control_E", asic, channel, 0);			
			TH1 * hT= (TH1 *) _file0->Get(hName);
			if (hT == NULL) continue;
			
  
			for (Int_t tac = 1; tac < 4; tac += 1) {
				sprintf(hName, "C%03d_%02d_%d_B_T_control_E", asic, channel, tac);			
				hT->Add((TH1 *) _file0->Get(hName), 1);
			}
			
			Float_t max_i = hT->GetMaximumBin();
			Float_t max_t = hT->GetBinCenter(max_i);
			hT->Fit("gaus", "", "", max_t - 0.5, max_t + 0.5);
			
			TF1 * gaussianFit = hT->GetFunction("gaus");
			if(gaussianFit == NULL) continue;
			Float_t sigma = gaussianFit->GetParameter(2);
			Float_t sigmaError = gaussianFit->GetParError(2);
			
			gSummary->SetPoint(nPoints, (64*asic)%128 + channel, sigma * 6.25E-9);			
			gSummary->SetPointError(nPoints, 0, sigmaError * 6.25E-9);
			
			hSummary->Fill(sigma * 6.25E-9);
			
			hCounts->Fill((64*asic)%128 + channel, hT->GetEntries());
			
			nPoints += 1;
		}
	}
	
	nPoints = 0;
	for(Int_t asic = 0; asic < 64; asic += 1) {
		for(Int_t channel = 0; channel < 64; channel+= 1) {
			for (Int_t tac = 0; tac < 4; tac += 1) {
				sprintf(hName, "C%03d_%02d_%d_B_T_pFine_X", asic, channel, tac);
				TProfile *p = (TProfile *)_file0->Get(hName);
				if(p == NULL) continue;
				TF1 *f = p->GetFunction("pol1");
				if(f == NULL) continue;
				Float_t leakage = f->GetParameter(1) / (4*6.4) * 1000; // in ADC/ms
				gLeakage->SetPoint(nPoints, (64*asic + channel)%128 + 0.25*tac, leakage);
				nPoints += 1;
				
			}
		}
	}
				
	
	c->Close();
	c = new TCanvas();
	c->Divide(2,2);
	c->cd(1);
	gSummary->SetTitle("TDC resolution");
	gSummary->GetXaxis()->SetTitle("Channel");
	gSummary->GetYaxis()->SetTitle("Resolution (s RMS)");
	gSummary->Set(nPoints);
	gSummary->GetXaxis()->SetRangeUser(0, 128);
	gSummary->GetYaxis()->SetRangeUser(0, 0.5E-9);
	gSummary->Draw("AP");
	c->cd(2);
	hSummary->SetTitle("TDC resolution histogram");
	hSummary->GetXaxis()->SetTitle("TDC resolution (s RMS)");
	hSummary->Draw();
	c->cd(3);
	hCounts->GetYaxis()->SetRangeUser(0, hCounts->GetMaximum() * 1.10);
	hCounts->GetXaxis()->SetTitle("Channel");
	hCounts->Draw();	
	
	c->cd(4);
	gLeakage->SetTitle("Leakage");
	gLeakage->GetXaxis()->SetTitle("Channel");
	gLeakage->GetYaxis()->SetTitle("Leakage (ADC/ms)");
	gLeakage->GetXaxis()->SetRangeUser(0, 128);
	gLeakage->GetYaxis()->SetRangeUser(0, 10);	
	gLeakage->Draw("AL");
	
}
