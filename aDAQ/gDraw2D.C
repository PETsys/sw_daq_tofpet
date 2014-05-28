{
	TCanvas *c = new TCanvas();
        gStyle->SetPalette(1);
        TGraph2D *g1 =  new TGraph2D("ctr.txt", "%lg %lg %*lg %*lg %lg %*lg %*lg %*lg %*lg %*lg");   
        g1->Draw("COLZ");
	c->SaveAs("dummy.pdf");
	
	g1->SetTitle("Peak ToT (M2)");
	g1->GetXaxis()->SetTitle("ib1 (ADC)");
	g1->GetYaxis()->SetTitle("vbl (ADC)");
        g1->Draw("COLZ");


}
