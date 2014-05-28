{
	gStyle->SetPalette(1);
	TH2F * c2 = new TH2F("counts2", "counts 2", 128, 0, 128, 128, 0, 128);
	lmData->Project("counts2", "crystal1:crystal2");
	c2->Draw("COLZ");

	
}
