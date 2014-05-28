{
	gStyle->SetPalette(1);
//	TGraph2D *g = new TGraph2D("ctr.txt", "%lg %lg %*lg %*lg %*lg %*lg %lg %*lg %*lg %*lg");
//	TGraph *g1 = new TGraphErrors("ctr.txt", "%lg %*lg %lg %lg %*lg %*lg %*lg %*lg %*lg %*lg");
	TGraph *g1 = new TGraphErrors("ctr.txt", "%lg %*lg %*lg %**g %*lg %*lg %*lg %*lg %lg %lg");
	//g1->GetZaxis()->SetRangeUser(50E-12, 300E-12);
//	g1->GetXaxis()->SetTitle("VthT[53] (ADCs)");	
//	g1->GetYaxis()->SetTitle("CTR sigma (s)");	
//	g1->SetTitle("CTR vs VthT (CH53) (LIPv2, 160MHz, [HV1,HV4]=[69.00,68.75], T=18.5C), activechannels=ALL32, VthT[7,53]=[52,scan], VthE=[5]");	
	g1->Draw("AP");
	//g1->GetZaxis()->SetRangeUser(50E-12, 300E-12);
	g1->Draw("AP");
/*	g->GetYaxis()->SetTitle("vib2 (ADCs)");	
	g->GetXaxis()->SetTitle("vib1 (ADCs)");	
	g->GetZaxis()->SetTitle("CTR - internal calib FE TP (s)");
	g->GetXaxis()->SetTitleOffset(2);
	g->GetYaxis()->SetTitleOffset(2);*/
//	g1->GetYaxis()->SetRangeUser(0, 500E-12);
//	g->Draw("SURF1");
/*	g1->SetTitle("CTR sigma (s) vs. Vbl (HV=73.2V, T=21.3C, trim range 500mV) - TTB1, 160MHz, Vth[t][2,63]=[51,46], Vth[e]=[5], vib[1,2]=[20,48]");
	g1->SetTitle("CTR (s) vs. Vth_E[CH02] - TTB1 [CH2,CH63], 160MHz, Vth[t,e][CH02]=[51,scan], Vth[t,e][CH63]=[46,5], vib[1,2]=[0,48], HV=73.0V, Vbl=52");*/
//	g->SetTitle("CTR (s) vs. Vib1 vs. Vib2 (global params)");// - TTB1 [CH2,CH63], 160MHz, Vth[t,e][CH02]=[51,5], Vth[t,e][CH63]=[46,5], vib[1,2]=[scan,scan], HV=73.0V, Vbl=52");
	
	
	
/*	g1->SetTitle("ToT (s) vs. Vbl[CH02]");
	g2->SetTitle("ToT (s) vs. Vbl[CH63]");
	TGraph *g1 = new TGraph("ctr.txt", "%lg %*lg %*lg %*lg %lg %*lg %*lg %*lg %*lg %*lg");
	TGraph *g2 = new TGraph("ctr.txt", "%lg %*lg %lg %*lg %*lg %*lg %*lg %*lg %*lg %*lg");
	c1.Divide(1,2)
	c1.cd(1)
	g1->Draw("A*");
	c1.cd(2)
	g2->Draw("A*");*/
	
}
