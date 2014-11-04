{
	gStyle->SetPalette(1);
	TH2F * c2 = new TH2F("counts2", "counts 2", 256, 0, 256, 256, 0, 256);
	lmData->Project("counts2", "channel1:channel2");
	c2->Draw("COLZ");

	
}
