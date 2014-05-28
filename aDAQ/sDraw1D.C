{
	gStyle->SetPalette(1);
	TGraph *g1 = new TGraph("sptr.txt", "%*lg %lg %*lg %*lg %lg %*lg %*lg %*lg");
	g1->GetXaxis()->SetTitle("praedictio_delay");	
	g1->GetYaxis()->SetTitle("MPTR sigma (s)");	
	g1->Draw("A*");

}