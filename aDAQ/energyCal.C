#include "Riostream.h"
#include <cmath> 

void energyCal(char fileNameIn[256],char fileNameOut[256], Int_t nChannels, bool savePlots){
	
	if(savePlots){
		gSystem->Exec("mkdir ecal_plot");
	}

	FILE* flist= fopen(fileNameIn,"r");
	FILE * fOut = fopen(fileNameOut, "w");

	
	char pdfFilename[128];
	
	Int_t nfiles=0;	
	char fileName[256];
	char condition[256];
 
	TF1 * myfunc2 = new TF1("myfunc2", "[0]*exp([1]*x^[2])", 0, 350);
	
	Float_t energy;

	TSpectrum *spectrum;

#if ROOT_VERSION_CODE > ROOT_VERSION(6,0,0)
	Double_t *xPositions;
	Double_t T[nChannels][5];
	Double_t E[nChannels][5];
#else
	Float_t  *xPositions;	
	Double_t **T = new Double_t*[nChannels];
	for(int i = 0; i < nChannels; ++i) {
		T[i] = new Double_t[5];
	}
	Double_t **E = new Double_t*[nChannels];
	for(int i = 0; i < nChannels; ++i) {
		E[i] = new Double_t[5];
	}

#endif
       

	Int_t nPeaks;
	Float_t x_pe;
	
	Float_t totMax=512;
 
	for(Int_t i=0;i<nChannels;i++){
		for(Int_t j=0;j<5;j++){
			T[i][j]=0;
			E[i][j]=0;
		}
	}


	while(!feof(flist)){
		fscanf(flist,"%s\n", fileName);

		TFile *f = new TFile(fileName);
		TTree *lmData = (TTree*)f->Get("lmData");
		Float_t tot;		lmData->SetBranchAddress("tot", &tot);
		UShort_t channel;	lmData->SetBranchAddress("channel", &channel);
		
		
		//Ba133 peak searching
		if(nfiles==0){
			for(Int_t ch=0;ch<nChannels;ch++){
				printf("Searching ToT peaks for Ba133 in channel %d\n", ch);
				TH1F *hToT1 = new TH1F("hToT1", "hToT1", totMax, 0, totMax);
				
				sprintf(condition, "channel==%d", ch);
				lmData->Project("hToT1", "tot",condition);
				hToT1->Smooth();
				hToT1->GetXaxis()->SetRangeUser(20,150);
			

				spectrum = new TSpectrum();
				spectrum->Search(hToT1, 3, " ",  0.85);
				nPeaks = spectrum->GetNPeaks();
				if (nPeaks == 0) {
					printf("No peak found when searching for 31 keV!!!\n");
					delete hToT1;
					continue;
				}
				xPositions = spectrum->GetPositionX();
				x_pe=0;
				for(Int_t i = 0; i < nPeaks; i++){
					x_pe+=xPositions[i];
				} 	
				x_pe/=nPeaks;			       
				T[ch][0]=x_pe;
				E[ch][0]=31;

				hToT1->GetXaxis()->SetRangeUser(x_pe+20,x_pe+100);
				spectrum = new TSpectrum();
				spectrum->Search(hToT1, 3, " ",  0.85);
				nPeaks = spectrum->GetNPeaks();
				if (nPeaks == 0) {
					printf("No peak found when searching for 81 keV!!!\n");
					delete hToT1;
					continue;
				}
				xPositions = spectrum->GetPositionX();
				x_pe=0;
				for(Int_t i = 0; i < nPeaks; i++){
					x_pe+=xPositions[i];
				} 	
				x_pe/=nPeaks;			       
				T[ch][1]=x_pe;
				E[ch][1]=81;
				hToT1->GetXaxis()->SetRangeUser(x_pe+20,400);
				spectrum = new TSpectrum();
				spectrum->Search(hToT1, 3, " ",  0.85);
				nPeaks = spectrum->GetNPeaks();
				if (nPeaks == 0) {
					printf("No peak found when searching for 356 keV!!!\n");
					delete hToT1;
					continue;
				}
				xPositions = spectrum->GetPositionX();
				x_pe=0;
				for(Int_t i = 0; i < nPeaks; i++){
					x_pe+=xPositions[i];
				} 	
				x_pe/=nPeaks;			       
				
				
				hToT1->Fit("gaus", "Q", "", x_pe-8, x_pe+8);
				if(hToT1->GetFunction("gaus") == NULL) {   continue; }
				x_pe = hToT1->GetFunction("gaus")->GetParameter(1);
				T[ch][2]=x_pe;
				E[ch][2]=356;
				hToT1->GetXaxis()->SetRangeUser(0,400);
				if(savePlots){
					TCanvas *c1 = new TCanvas;
					c1 -> cd();
					hToT1->Draw();
					hToT1->GetXaxis()->SetTitle("ToT (ns)");
					sprintf(pdfFilename,"ecal_plots/ba133_%d.pdf",ch);
					c1->SaveAs(pdfFilename);
					delete c1;
				}
				delete hToT1;
				
			}
		
		}

		//Na22 peak searching
		if(nfiles==1){
			for(Int_t ch=0;ch<nChannels;ch++){
				printf("Searching ToT peaks for Na22 in channel %d\n", ch);
				TH1F *hToT1 = new TH1F("hToT1", "hToT1", 400, 0, 400);
			
				sprintf(condition, "channel==%d", ch);
			
				lmData->Project("hToT1", "tot",condition);
				hToT1->Smooth();
						

				spectrum = new TSpectrum();
				spectrum->Search(hToT1, 3, " ",  0.9);
				nPeaks = spectrum->GetNPeaks();
				if (nPeaks == 0) {
					printf("No peak found when searching for 511 keV!!!\n");
					delete hToT1;
					continue;
				}

				xPositions = spectrum->GetPositionX();
				Float_t x1_psc = 0;
				Float_t x1_pe = 0;
			

				for(Int_t i = 0; i < nPeaks; i++){
					if(xPositions[i] > x1_pe){
						if(x1_pe > x1_psc)
							x1_psc = x1_pe;
						x1_pe = xPositions[i];
					}
				} 	
						
				Float_t x1 = x1_pe;
		    
				hToT1->Fit("gaus", "Q", "", x1-10, x1+10);
				if(hToT1->GetFunction("gaus") == NULL) {   continue; }
				x1 = hToT1->GetFunction("gaus")->GetParameter(1);
				Float_t sigma1 = hToT1->GetFunction("gaus")->GetParameter(2);
		
				T[ch][3]=x1;
				E[ch][3]=511;
				if(savePlots){
					TCanvas *c1 = new TCanvas;
					c1 -> cd();
					hToT1->Draw();
					hToT1->GetXaxis()->SetTitle("ToT (ns)");
					sprintf(pdfFilename,"ecal_plots/na22_%d.pdf",ch);
					c1->SaveAs(pdfFilename);
					delete c1;
				}
				delete hToT1;
			}
		
		}
		//cs137 peak searching
		if(nfiles==2){
			for(Int_t ch=0;ch<nChannels;ch++){
				printf("Searching ToT peaks for Cs137 in channel %d\n", ch);
				TH1F *hToT1 = new TH1F("hToT1", "hToT1", 400, 50, 400);
			
				sprintf(condition, "channel==%d", ch);
				//printf(condition);
				lmData->Project("hToT1", "tot",condition);
				hToT1->Smooth();
				//hToT1->GetXaxis()->SetRangeUser(3,150);
			

				spectrum = new TSpectrum();
				spectrum->Search(hToT1, 3, " ",  0.8);
				nPeaks = spectrum->GetNPeaks();
				if (nPeaks == 0) {
					printf("No peak found when searching for 662 keV!!!\n");
					delete hToT1;
					continue;
				}

				xPositions = spectrum->GetPositionX();
				Float_t x1_psc = 0;
				Float_t x1_pe = 0;
			

				for(Int_t i = 0; i < nPeaks; i++){
					if(xPositions[i] > x1_pe){
						if(x1_pe > x1_psc)
							x1_psc = x1_pe;
						x1_pe = xPositions[i];
					}
				} 	

			
			
				Float_t x1 = x1_pe;

				hToT1->Fit("gaus", "Q", "", x1-10, x1+10);
				if(hToT1->GetFunction("gaus") == NULL) {   continue; }
				x1 = hToT1->GetFunction("gaus")->GetParameter(1);
				Float_t sigma1 = hToT1->GetFunction("gaus")->GetParameter(2);
		
				T[ch][4]=x1;
				E[ch][4]=662;
				if(savePlots){
					TCanvas *c1 = new TCanvas;
					c1 -> cd();
					hToT1->Draw();
					hToT1->GetXaxis()->SetTitle("ToT (ns)");
					sprintf(pdfFilename,"ecal_plots/cs137_%d.pdf",ch);
					c1->SaveAs(pdfFilename);
					delete c1;
				}

				delete hToT1;
			}
		
		}

	
		nfiles++;
	}

	Int_t nPoints;
	if (nfiles==2)
		nPoints=4;	
	if (nfiles==3)
		nPoints=5;	
	TGraph *e_vs_tot = new TGraph(nPoints);
	//Fit function for all channels and write ToT to Energy file
	for(Int_t ch=0;ch<nChannels;ch++){
		for(Int_t i=0;i<nPoints;i++){
			e_vs_tot->SetPoint(i,T[ch][i], E[ch][i]);				
		}

		myfunc2->SetParameter(0,exp(1.7));
	   	myfunc2->SetParameter(1,0.0001);
		myfunc2->SetParameter(2,2);

		e_vs_tot->Fit("myfunc2","Q","",50,350);
		
       
		for(Int_t j=0;j<int(totMax/0.5);j++){
			if(myfunc2==NULL)
				energy=0;
			else 
				energy= myfunc2->Eval(j*0.5);
			fprintf(fOut,"%d\t%0.1f\t%f\n", ch,j*0.5, energy );
		}
		
		if(savePlots){
			TCanvas *c2 = new TCanvas;
			c2 -> cd();
			e_vs_tot->Draw("AP*");
			e_vs_tot->GetXaxis()->SetTitle("ToT (ns)");
			e_vs_tot->GetYaxis()->SetTitle("Energy (keV)");
			e_vs_tot->SetTitle(" ");
			sprintf(pdfFilename,"ecal_plots/fit_%d.pdf",ch);
			c2->SaveAs(pdfFilename);
			delete c2;
		}
	}
	


}
