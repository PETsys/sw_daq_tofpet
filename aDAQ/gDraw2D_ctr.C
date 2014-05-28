{
        gStyle->SetPalette(1);
	c1 = new TCanvas();
//	c1->Divide(2,2);


//	c1->cd(1);
        TGraph2D *g1 =  new TGraph2D("ctr.txt", "%lg %lg %*lg %*lg %*lg %*lg %lg %*lg %*lg %*lg");   
	g1->Draw("COLZ");

/*	c1->cd(3);
        TGraph2D *g3 =  new TGraph2D("ctr.txt", "%lg %lg %lg %*lg %*lg %*lg %*lg %*lg %*lg %*lg");   
	g3->Draw("COLZ");
	
	c1->cd(4);
        TGraph2D *g4=  new TGraph2D("ctr.txt", "%lg %lg %*lg %*lg %lg %*lg %*lg %*lg %*lg %*lg");   
	g4->Draw("COLZ");
*/
	c1->SaveAs("dummy.pdf");
	
	
	g1->SetTitle("CTR");
	//g1->GetXaxis()->SetRangeUser(0, 64);
	g1->GetXaxis()->SetTitle("ib1");
	
	///g1->GetYaxis()->SetRangeUser(0, 64);
	g1->GetYaxis()->SetTitle("vbl");
	
//	g1->GetZaxis()->SetRangeUser(50E-12, 300E-12);

//	c1->cd(1);        
	g1->Draw("COLZ");
//	c1->cd(3);        g3->Draw("COLZ");
//	c1->cd(4);        g4->Draw("COLZ");
	
	
	


}
