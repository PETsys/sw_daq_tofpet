{
	TFile *f1 = new TFile("data/2014.03.20/R001_tdca.root");
	TFile *f2 = new TFile("data/2014.03.20/R002_tdca.root");
	TFile *f3 = new TFile("data/2014.03.20/R003_tdca.root");
	TFile *f4 = new TFile("data/2014.03.20/R004_tdca_E.root");
	TFile *f5 = new TFile("data/2014.03.20/R005_fetp.root");
	TFile *f6 = new TFile("data/2014.03.20/R006_fetp.root");
	TFile *f7 = new TFile("data/2014.03.20/R007_fetp.root");
	TFile *f8 = new TFile("data/2014.03.20/R008_fetp.root");
	
	
	
	
	TLegend *l = new TLegend(0.65, 0.65, 0.99, 0.99);
	
/*	TH2 *h = (TH2 *)f1->Get("htFine_001_09_3");	
	TH1 *py = h->ProjectionY("r1");	
 	py->SetTitle("htFine_001_09_3");
	py->Rebin(2);
	py->GetXaxis()->SetRangeUser(100, 500);
	py->GetYaxis()->SetRangeUser(0, 2500);
	py->SetLineColor(kBlack);
	py->Draw();
	l->AddEntry(py, "T Fine (TDCA, 1 FI, 510 ps step)");
	
	h = (TH2 *)f2->Get("htFine_001_09_3");
	py = h->ProjectionY("r2");	
	py->Rebin(2);
	py->GetYaxis()->SetRangeUser(0, 1500);
	py->SetLineColor(kBlue);
	py->Draw("SAME");
	l->AddEntry(py, "T Fine (TDCA, 1 FI, 127 ps step)");*/
	
	h = (TH2 *)f3->Get("htFine_001_09_3");
	py = h->ProjectionY("r3");	
	py->Rebin(2);
	py->GetYaxis()->SetRangeUser(0, 1500);
	py->SetLineColor(kRed);
	py->GetYaxis()->SetRangeUser(0, 2500);
	py->Draw("SAME");
	l->AddEntry(py, "T Fine (TDCA, 16 FI, 127 ps step)");

// 	h = (TH2 *)f4->Get("htFine_001_09_3");
// 	py = h->ProjectionY("r4");	
// 	py->Rebin(2);
// 	py->GetYaxis()->SetRangeUser(0, 1500);
// 	py->SetLineColor(kGreen);
// 	py->Draw("SAME");
// 	l->AddEntry(py, "T Fine (TDCA E only, 16 FI, 127 ps step)");
	
	h = (TH2 *)f5->Get("htFine_001_09_3");
 	py = h->ProjectionY("r5");	
	py->Rebin(2);
	py->GetYaxis()->SetRangeUser(0, 1500);
	py->SetLineColor(kGreen);
	py->Draw("SAME");
	l->AddEntry(py, "T Fine (FETP (32, 5V), 16 FI, 127 ps step)");

	h = (TH2 *)f6->Get("htFine_001_09_3");
 	py = h->ProjectionY("r6");	
	py->Rebin(2);
	py->GetYaxis()->SetRangeUser(0, 1500);
	py->SetLineColor(kBlack);
	py->Draw("SAME");
	l->AddEntry(py, "T Fine (FETP (48, 5V), 16 FI, 127 ps step)");

	h = (TH2 *)f7->Get("htFine_001_09_3");
 	py = h->ProjectionY("r7");	
	py->Rebin(2);
	py->GetYaxis()->SetRangeUser(0, 1500);
	py->SetLineColor(kBlue);
	py->Draw("SAME");
	l->AddEntry(py, "EFine (FETP (16, 5V), 16 FI, 127 ps step)");


	l->Draw("SAME");
}
