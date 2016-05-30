#include <TFile.h>
#include <TTree.h>
#include <TH1F.h>
#include <TH2F.h>
#include <TF1.h>
#include <TSpectrum.h>
#include <TCanvas.h>
#include <TStyle.h>
#include <stdio.h>

#include <stdio.h>
#include <stdint.h>
#include <TH1F.h>
#include <TSpectrum.h>
#include <TF1.h>
#include <TGraphErrors.h>

struct Event {
        uint8_t mh_n1;
        uint8_t mh_j1;
        long long time1;
        float e1;
        int id1;

        uint8_t mh_n2;
        uint8_t mh_j2;
        long long time2;
        float e2;
        int id2;

};


int psDrawCTR(char const *fileName, Int_t channelA=-1, Int_t channelB=-1)
{

	
        FILE * dataFile = fopen(fileName, "rb");
    

		
	Double_t tBinWidth = 50;
	Double_t wMax = 8000;
	TH1F *hDelta = new TH1F("hDelta", "Coincidence time difference", 2*wMax/tBinWidth, -wMax, wMax);
	
	Int_t nBinsToT=600;
	const Int_t N_CHANNELS = 256;
	TH2F *hC2 = new TH2F("hC2", "C2", N_CHANNELS, 0, N_CHANNELS, N_CHANNELS, 0, N_CHANNELS);
	
	struct CEvent {
		Long64_t	time1;
		Long64_t	time2;
		Float_t		tot1;
		Float_t		tot2;
	};

	

	
	const Int_t MAX_EVENTS = 100000;
	const Int_t MAX_EVENTS_TOTAL = 250000000;
	CEvent *eventBuffer = new CEvent[MAX_EVENTS];
	Int_t count = 0;
	
	Event ei;
	int nRead = 0;
      
	if(channelA==-1 && channelB==-1){
		while(fread(&ei, sizeof(ei), 1, dataFile) == 1 && count < MAX_EVENTS_TOTAL) {
			hC2->Fill(ei.id1, ei.id2);
			count++;
			if(count%1000000==0)cout << count <<endl;
		}	
		rewind(dataFile);
		Int_t binMax=hC2->GetMaximumBin();
		Int_t binx, biny, binz;
		hC2->GetBinXYZ(binMax, binx, biny, binz);
		channelA=binx-1;
		channelB=biny-1;
		printf("CA max=%d  CB max=%d\n",channelA, channelB);
	}
	count=0;
	char hName[128];
	sprintf(hName, "ToT spectra (channel %d)", channelA); 
	TH1F * hToT1 = new TH1F("hToT1", hName, nBinsToT, 0, nBinsToT);
	sprintf(hName, "ToT spectra (channel %d)", channelB); 
	TH1F * hToT2 = new TH1F("hToT2", hName, nBinsToT, 0, nBinsToT);

        while(fread(&ei, sizeof(ei), 1, dataFile) == 1) {
		nRead ++;
		if(nRead % 10000 == 0) {
			printf("Read %d\r", nRead);
			fflush(stdout);
		}
		
		hC2->Fill(ei.id1, ei.id2);
		
		bool selectedPair = ((ei.id1 == channelA) || (ei.id1 == channelB)) && ((ei.id2 == channelA) || (ei.id2 == channelB));
		if(!selectedPair) {
			continue;
		}
		
		CEvent &e = eventBuffer[count];
		e.time1 = ei.time1;
		e.time2 = ei.time2;
		e.tot1 = ei.e1;
		e.tot2 = ei.e2;
		count++;
		
		hToT1->Fill(ei.e1);
		hToT2->Fill(ei.e2);
	}
	printf("\n");
	
	TCanvas *c1 = new TCanvas();
	
	//hToT1->Smooth(); hToT2->Smooth();
	
	
	TSpectrum *spectrum = new TSpectrum();
	spectrum->Search(hToT1, 3, " ",  0.2);
	Int_t nPeaks = spectrum->GetNPeaks();
	if (nPeaks == 0) {
		printf("No peaks in hToT1!!!\n");
		return -1;
	}
#if ROOT_VERSION_CODE > ROOT_VERSION(6,0,0)
	Double_t *xPositions1 = spectrum->GetPositionX();
#else
	Float_t  *xPositions1 = spectrum->GetPositionX();
#endif	
	Float_t x1_psc = 0;
	Float_t x1_pe = 0;
	for(Int_t i = 0; i < nPeaks; i++) {
	if(xPositions1[i] > x1_pe) {
		if(x1_pe > x1_psc)
			x1_psc = x1_pe;
			x1_pe = xPositions1[i];
		}
	} 


	spectrum = new TSpectrum();
	spectrum->Search(hToT2, 3, " ",  0.2);
	nPeaks = spectrum->GetNPeaks();
	if (nPeaks == 0) {
		printf("No peaks in hToT2!!!\n");
		return -1;
	}
#if ROOT_VERSION_CODE > ROOT_VERSION(6,0,0)
	Double_t *xPositions2 = spectrum->GetPositionX();
#else
	Float_t  *xPositions2 = spectrum->GetPositionX();
#endif

	Float_t x2_psc = 0;
	Float_t x2_pe = 0;
	for(Int_t i = 0; i < nPeaks; i++) {
	if(xPositions2[i] > x2_pe) {
		if(x2_pe > x2_psc)
			x2_psc = x2_pe;
			x2_pe = xPositions2[i];
		}
	} 
		
	Float_t x1 = x1_pe;
	Float_t x2 = x2_pe;
	

	hToT1->Fit("gaus", "", "", x1-10, x1+10);
	if(hToT1->GetFunction("gaus") == NULL) {
		return -1;
	}
	x1 = hToT1->GetFunction("gaus")->GetParameter(1);
	Float_t sigma1 = hToT1->GetFunction("gaus")->GetParameter(2);

	hToT2->Fit("gaus", "", "", x2-10, x2+10);
	if(hToT2->GetFunction("gaus") == NULL) { 
		return -1;
	}
	x2 = hToT2->GetFunction("gaus")->GetParameter(1);
	Float_t sigma2 = hToT2->GetFunction("gaus")->GetParameter(2);

	Float_t sN = 1.0;
	
	for(Int_t i = 0; i < count; i++) {
		CEvent &e = eventBuffer[i];

		if((e.tot1 < (x1 - sN*sigma1)) || (e.tot1 > (x1 + sN*sigma1))) 
			continue;
		
		if((e.tot2 < (x2 - sN*sigma2)) || (e.tot2 > (x2 + sN*sigma2))) 
			continue;
		
		Float_t delta = e.time1 - e.time2;
		//delta *= 1E-12;
		
		hDelta->Fill(delta);
	}
	delete [] eventBuffer;
	int binmax = hDelta->GetMaximumBin();
	Float_t hmax= hDelta->GetXaxis()->GetBinCenter(binmax);
	
	hDelta->Fit("gaus", "", "", hmax-350, hmax+300);
	delete c1;


	gStyle->SetPalette(1);
	gStyle->SetOptFit(1);
	c1 = new TCanvas();
	c1->Divide(3, 1);
	c1->cd(1); hToT1->Draw();
	hToT1->GetXaxis()->SetTitle("ToT [ns]");
	c1->cd(2); hDelta->Draw();
	hDelta->GetXaxis()->SetTitle("time1-time2 [ps]");
	c1->cd(3); hToT2->Draw();
	hToT2->GetXaxis()->SetTitle("ToT [ns]");
	c1->Modified();
	return 0;

}
