{
 	c0 = new TCanvas("c1","multigraph L3",200,10,700,500);
//	c0->SetFrameFillColor(30);
  
	TMultiGraph *mg = new TMultiGraph();
	
	gStyle->SetPalette(1);
	TGraph *g1 = new TGraphErrors("ctr_g1.txt", "%*lg %lg %*lg %*lg %*lg %*lg %*lg %*lg %lg %lg");
	g1->SetLineColor(kBlue);
// 	g1->Draw("AP");
	TGraph *g2 = new TGraphErrors("ctr_g2.txt", "%*lg %lg %*lg %*lg %*lg %*lg %*lg %*lg %lg %lg");
	g2->SetLineColor(kRed);	
// 	g2->Draw("AP");
	TGraph *g3 = new TGraphErrors("ctr_g3.txt", "%*lg %lg %*lg %*lg %*lg %*lg %*lg %*lg %lg %lg");
	g3->SetLineColor(kGreen);	


	mg->Add(g1);
	mg->Add(g2); 
 	mg->Add(g3); 
//	mg->BuildLegend();

	mg->SetTitle("Coincidence Mean Value (s) vs. Vth_T vs. TIA Gain (HV=68.5V, T=19.0C) - TTB1, 160MHz, Vth[t][2,63]=[scan,48], Vth[e]=[5], vib[1,2]=[20,48]");

 	mg->Draw("ALP");
 	TLegend* l=new TLegend(0.6,0.7,0.85,0.9);
 	l->AddEntry(g1,"3.5 KOhm","lep");
 	l->AddEntry(g2,"4.5 KOhm","lep");
 	l->AddEntry(g3,"5.0 KOhm","lep");
 	l->SetHeader("TIA Gain");
 	l->Draw();	

	mg->GetXaxis()->SetTitle("Vth_T (ADCs)");
	mg->GetYaxis()->SetTitle("CMV (s)");
	
	/*	g1->GetXaxis()->SetTitle("Vth_T (ADCs)");	
	g1->GetYaxis()->SetTitle("CTR sigma (s)");	
	g1->SetTitle("CTR sigma (s) vs. Vth (HV=70.0V, T=20.0C) - TTB1, 160MHz, Vth[t][2,63]=[scan,48], Vth[e]=[5], vib[1,2]=[20,48]");*/
	
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
