{
	TCanvas *c = new TCanvas();
	gStyle->SetPalette(1);
	TH2F * hTCoarse = new TH2F("hTCoarse", "T Coarse", 128, 0, 128, 1024, 0, 1024);
	TH2F * hECoarse = new TH2F("hECoarse", "E Coarse", 128, 0, 128, 1024, 0, 1024);
	TH2F * hToTCoarse = new TH2F("hToTCoarse", "ToT Coarse", 128, 0, 128, 1024, 0, 1024*6.25);
	
	data3->Project("hTCoarse", "tcoarse:64*asic+channel");
	data3->Project("hECoarse", "ecoarse:64*asic+channel");
	data3->Project("hToTCoarse", "6.25*(ecoarse-tcoarse):64*asic+channel");
	
//	c->Divide(2,1);
//	c->cd(1); hTCoarse->Draw("COLZ");
//	c->cd(2); hECoarse->Draw("COLZ");
	hToTCoarse->Draw("COLZ");
}
