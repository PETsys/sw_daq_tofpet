{
	gStyle->SetPalette(1);

	c = new TCanvas();
	c->Divide(2,2);	
	
	Float_t m = 0;
	Float_t w1 = 500E-12;
	
	c->cd(1);
	TGraph *g1 = new TGraphErrors("ctr_tp.txt", "%*lg %lg %*lg %*lg %*lg %*lg %lg %lg %*lg %*lg");
	g1->SetTitle("Delta = tp ch1 - tp ch2");
	g1->GetXaxis()->SetTitle("Delay (clk)");	
	g1->GetYaxis()->SetTitle("Delta sigma (s)");	
	m = g1->GetMean(2);
	g1->GetYaxis()->SetRangeUser(0, 200E-12);
	g1->GetXaxis()->SetRangeUser(60, 70);
	g1->Draw("AP");

	c->cd(2);
	TGraph *g2 = new TGraphErrors("ctr_tp.txt", "%*lg %lg %*lg %*lg %*lg %*lg %*lg %*lg %lg %lg");
	g2->SetTitle("Delta = tp ch1 - tp ch2");
	g2->GetXaxis()->SetTitle("Delay (clk)");	
	g2->GetYaxis()->SetTitle("Delta mean (s)");	
	m = g2->GetMean(2);
	g2->GetYaxis()->SetRangeUser(m-W1, m+W1);
	g2->GetXaxis()->SetRangeUser(60, 70);
	g2->Draw("AP");

	c->cd(3);
	TGraph *g3 = new TGraphErrors("ctr_tp.txt", "%*lg %lg %lg %lg %*lg %*lg %*lg %*lg %*lg %*lg");
	g3->SetTitle("Delta = tp ch1 - tp set");
	g3->GetXaxis()->SetTitle("Delay (clk)");	
	g3->GetYaxis()->SetTitle("TP1");	
	m = g3->GetMean(2);
	g3->GetYaxis()->SetRangeUser(m-W1, m+W1);
	g3->GetXaxis()->SetRangeUser(60, 70);
	g3->Draw("AP");

	c->cd(4);
	TGraph *g4 = new TGraphErrors("ctr_tp.txt", "%*lg %lg %*lg %*lg %lg %lg %*lg %*lg %*lg %*lg");
	g4->SetTitle("Delta = tp ch2 - tp set");
	g4->GetXaxis()->SetTitle("Delay (clk)");	
	g4->GetYaxis()->SetTitle("TP2");	
	m = g3->GetMean(2);
	g3->GetYaxis()->SetRangeUser(m-W1, m+W1);
	g4->GetXaxis()->SetRangeUser(60, 70);
	g4->Draw("AP");
	
}
