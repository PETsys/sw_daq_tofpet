#include <TFile.h>
#include <TNtuple.h>
#include <TChain.h>
#include <TProfile.h>
#include <TH1F.h>
#include <TH2F.h>
#include <TF1.h>
#include <TGraph.h>
#include <TVirtualFitter.h>
#include <TOFPET/LUT.hpp>
#include <assert.h>
#include <math.h>
#include <boost/lexical_cast.hpp>

static const int nIterations = 2;

static const int nTAC = 128*2*4;
struct TacInfo {
	TH2 *hFine;
	TProfile *pFine;
	TProfile *pLUT;
	TProfile *pControlT;
	TProfile *pControlQ;	
	TH1* pControlE;
	float minY;
	float maxY;
	
	TF1 *al;

	TacInfo() {
		hFine = NULL;
		pFine = NULL;		
		pControlT = NULL;
		pControlQ = NULL;
		al = NULL;
		pControlE = NULL;
	}
};

static const float rangeX = 0.2;

const char * paramNames1[3] =  { "x0", "b", "m" };
static Double_t periodicPol1 (Double_t *xx, Double_t *pp)
{
	float x = fmod(4.0 + xx[0] - pp[0], 2.0);	
	return pp[1] + pp[2]*x;
}

static Double_t aperiodicPol1 (Double_t *xx, Double_t *pp)
{
	float x = xx[0] - pp[0];
	return pp[1] + pp[2]*x;
}



static const int nPar = 7;
const char *paramNamesN[nPar] = { "x0", "p0", "p1", "p2", "p3", "A", "phi" };
static Double_t periodicPolN (Double_t *xx, Double_t *pp)
{
	float x = fmod(4.0 + xx[0] - pp[0], 2.0);	
	return  pp[1] + pp[2]*x + pp[3]*x*x + pp[4]*x*x*x + pp[5] * cos(2*M_PI*2*x + pp[6]);
}

static Double_t aperiodicPolN (Double_t *xx, Double_t *pp)
{
	float x = xx[0] - pp[0];
	return  pp[1] + pp[2]*x + pp[3]*x*x + pp[4]*x*x*x + pp[5] * cos(2*M_PI*2*x + pp[6]);
}


bool isCanonical(int coarse, float q)
{
	bool isEven = (coarse % 2) == 0;
	if (isEven && q >= 1.0 && q <= 2.0) return true;
	else if (!isEven && q >= 2.0 && q <= 3.0) return true;
	else return false;
}

int main(int argc, char *argv[])
{
	assert(argc == 6);
//	TVirtualFitter::SetDefaultFitter("Minuit2");

	TFile * tdcaDataFile = new TFile(argv[1], "READ");
	TFile * fetpDataFile = new TFile(argv[2], "READ");
	TFile * resumeFile = new TFile("tdcSummary.root", "RECREATE");	
	float timeGain = boost::lexical_cast<float>(argv[3]);
	char *tableFileName1 = argv[4];
	char *tableFileName2 = argv[5];
	
	DAQ::TOFPET::LUT myLUT(128);


	TacInfo tacInfo[nTAC];
	for(int n = 0; n < nTAC; n++) {
		Int_t channel = (n/2) / 4;
		Int_t tac = (n/2) % 4;
		Bool_t isT = n % 2 == 0;
		
		char hName[128];			
		sprintf(hName, isT ? "htFine_%03d_%02d_%1d" : "heFine_%03d_%02d_%1d" , 
			channel/64, channel%64,	tac);
			
		TacInfo &ti = tacInfo[n];

		TH2 *hFine = isT ? (TH2 *)fetpDataFile->Get(hName) : (TH2 *)tdcaDataFile->Get(hName);
		ti.hFine = hFine;

		if(ti.hFine == NULL) continue;
		printf("Fitting %3d %d %c (%p)\n", channel, tac, isT ? 'T' : 'E', hFine);

		sprintf(hName, isT ? "C%03d_%02d_%d_T_pFine_X" : "C%03d_%02d_%d_E_pFine_X", 
			channel/64, channel%64, tac);
		TProfile * pFine = hFine->ProfileX(hName, 1, -1, "s");
		ti.pFine = pFine;

		
		float yMean = pFine->GetMean(2);
		float yRMS = pFine->GetRMS(2);
		float MaxRMS = 10.0;
		Float_t maxI = 0;
		Float_t maxY = 0;
		Float_t startX = pFine->GetXaxis()->GetXmin() + 1.0;
		for(Int_t i = 1; pFine->GetBinCenter(i) <= (startX + 2.0); i++) {
			if(pFine->GetBinError(i) > MaxRMS) continue;
			Int_t j = pFine->FindBin(pFine->GetBinCenter(i) + 0.5);
			Float_t y = pFine->GetBinContent(i);
			if (y < yMean - 2 * yRMS || y > yMean + 2 * yRMS) continue;
			if (y < 0.5 * timeGain || y > 3.5 * timeGain) continue;
			
			if(y > maxY) {
				maxI = i;
				maxY = y;
			}	  
		}
		
		Float_t maxX = pFine->GetBinCenter(maxI);
		printf("maxX = %f, maxY = %f\n", maxX, maxY);

		Float_t minI = maxI;
		Float_t minY = 1024;
		for(Int_t i = pFine->FindBin(maxX + 1.6);  pFine->GetBinCenter(i) <= (maxX + 2.4); i++) {
			
			if (pFine->GetBinError(i) > MaxRMS) continue;
			Float_t y = pFine->GetBinContent(i);
			if(y < minY) {
				minI = i;
				minY = y;
			}
			else {
				break;
			}
		}
		Float_t minX = pFine->GetBinCenter(minI);
		printf("minX = %f, minY = %f\n", minX, minY);
		

		// Rebuild
		delete pFine;
		hFine->GetYaxis()->SetRangeUser(0.90 * minY, 1.10 * maxY);
		pFine = hFine->ProfileX(hName, 1, -1, "s");
		
		ti.maxY = maxY;
		ti.minY = minY;
		


		TF1 *pf = new TF1("periodicPol1", periodicPol1,  
				pFine->GetXaxis()->GetXmin() + 1.0, 
				  pFine->GetXaxis()->GetXmax(), 
				  3);
		for(int p = 0; p < 3; p++)
			pf->SetParName(p, paramNames1[p]);
		pf->SetNpx((pFine->GetXaxis()->GetXmax() - pFine->GetXaxis()->GetXmin()) * 256);
		pf->SetParameter(0, (minX-2.0 + maxX)/2);	pf->SetParLimits(0, minX-2.0, maxX);
		pf->SetParameter(1, 1.10*pFine->GetMaximum());	pf->SetParLimits(1, pFine->GetMaximum(), 1.20 * pFine->GetMaximum());
		pf->SetParameter(2, -timeGain);			pf->SetParLimits(2, -1.2*timeGain, -0.8*timeGain);
		pFine->Fit("periodicPol1", "R");
		pf = pFine->GetFunction("periodicPol1");
		if(pf == NULL) continue;
		
		TF1 *af = new TF1("aperiodicPol1", aperiodicPol1,  pFine->GetXaxis()->GetXmin(), pFine->GetXaxis()->GetXmax(), 3);
		for(int i = 0; i < 3; i++)
			af->SetParameter(i, pf->GetParameter(i));
		ti.al = af;
		
		float x0 = pf->GetParameter(0);
		
		
		sprintf(hName, isT ? "C%03d_%02d_%d_T_pLUT" : "C%03d_%02d_%d_E_pLUT", 
			channel/64, channel%64, tac);
		TProfile *pLUT = new TProfile(hName, hName, 1024, 0, 1024);
		ti.pLUT = pLUT;
		
		for(float x = x0 - 3.2; x <= x0 + 3.2; x += 0.002) {
			float adc = af->Eval(x);
			pLUT->Fill(adc, x - x0);
		}
		for(int adc = 0; adc < 1024; adc++) {
			float x = pLUT->GetBinContent(pLUT->FindBin(adc));
			float q = 3.0 - x;
			myLUT.setQ(channel, tac, isT, adc, q);
		}
		myLUT.setT0(channel, tac, isT, +x0);

		// Allocate the control histogram
		int nBinsX = hFine->GetXaxis()->GetNbins();
		int xMin = hFine->GetXaxis()->GetXmin();
		int xMax = hFine->GetXaxis()->GetXmax();

		sprintf(hName, isT ? "C%03d_%02d_%d_T_control_T" : "C%03d_%02d_%d_E_control_T", 
				channel/64, channel%64, tac);
		ti.pControlT = new TProfile(hName, hName, nBinsX, xMin, xMax, "");
		sprintf(hName, isT ? "C%03d_%02d_%d_T_control_Q" : "C%03d_%02d_%d_E_control_Q", 
				channel/64, channel%64, tac);
		ti.pControlQ = new TProfile(hName, hName, nBinsX, xMin, xMax, "");
		
		sprintf(hName, isT ? "C%03d_%02d_%d_T_control_E" : "C%03d_%02d_%d_E_control_E", 
				channel/64, channel%64, tac);
		ti.pControlE = new TH1F(hName, hName, 512, -1, 1);
		
	}

	float tSum, eSum;
	int tNum, eNum;

	for(int iter = 0; iter <= nIterations; iter++) {
		for(int n = 0; n < nTAC; n++) {
			if(tacInfo[n].pControlT == NULL) continue;
			tacInfo[n].pControlT->Reset();
		}

		TTree *data = (TTree *)fetpDataFile->Get("data3");
		Float_t fStep2;		data->SetBranchAddress("step2", &fStep2);
		Int_t fAsic;		data->SetBranchAddress("asic", &fAsic);
		Int_t fChannel;		data->SetBranchAddress("channel", &fChannel);
		Int_t fTac;		data->SetBranchAddress("tac", &fTac);
		Int_t ftCoarse;		data->SetBranchAddress("tcoarse", &ftCoarse);
		Int_t feCoarse;		data->SetBranchAddress("ecoarse", &feCoarse);
		Int_t fxSoC;		data->SetBranchAddress("xsoc", &fxSoC);
		Int_t ftEoC;		data->SetBranchAddress("teoc", &ftEoC);
		Int_t feEoC;		data->SetBranchAddress("eeoc", &feEoC);

		int nEvents = data->GetEntries();
		for(int i = 0; i < nEvents; i++) {
			data->GetEntry(i);
			
			Int_t ftFine = (ftEoC >= fxSoC) ? (ftEoC - fxSoC) : (1024 + ftEoC - fxSoC);
			
			int index = 64 * fAsic + fChannel;
			float tT = myLUT.getT(index, fTac, true, ftFine, ftCoarse);
			float qT = myLUT.getQ(index, fTac, true, ftFine);
			bool isNormalT = myLUT.isNormal(index, fTac, true, ftFine, ftCoarse);
			
			index = 4 * index + fTac;
			
			TacInfo &tiT = tacInfo[2*index + 0];
			
			if((tiT.pControlT != NULL)) {
				if(isNormalT) {
					tiT.pControlT->Fill(fStep2, tT - fStep2);
					tiT.pControlE->Fill(tT - fStep2);
				}
				tiT.pControlQ->Fill(fStep2, qT);
			}
		}


		data = (TTree *)tdcaDataFile->Get("data3");
		data->SetBranchAddress("step2", &fStep2);
		data->SetBranchAddress("asic", &fAsic);
		data->SetBranchAddress("channel", &fChannel);
		data->SetBranchAddress("tac", &fTac);
		data->SetBranchAddress("tcoarse", &ftCoarse);
		data->SetBranchAddress("ecoarse", &feCoarse);
		data->SetBranchAddress("xsoc", &fxSoC);
		data->SetBranchAddress("teoc", &ftEoC);
		data->SetBranchAddress("eeoc", &feEoC);

		nEvents = data->GetEntries();
		for(int i = 0; i < nEvents; i++) {
			data->GetEntry(i);
			
			Int_t feFine = (feEoC >= fxSoC) ? (feEoC - fxSoC) : (1024 + feEoC - fxSoC);
			
			int index = 64 * fAsic + fChannel;
			float tE = myLUT.getT(index, fTac, false, feFine, feCoarse);			
			float qE = myLUT.getQ(index, fTac, false, feFine);
			bool isNormalE = myLUT.isNormal(index, fTac, false, feFine, feCoarse);

			index = 4 * index + fTac;
			
			TacInfo &tiE = tacInfo[2*index + 1];

			if((tiE.pControlT != NULL)) {
				if(isNormalE) {
					tiE.pControlT->Fill(fStep2, tE - fStep2);
					tiE.pControlE->Fill(tE - fStep2);
				}
				tiE.pControlQ->Fill(fStep2, qE);
			}
		}



		if(iter >= nIterations) continue;
		
		tSum = 0;
		tNum = 0;
		eSum = 0;
		eNum = 0;

		for(int n = 0; n < nTAC; n++) {
			Int_t channel = (n/2) / 4;
			Int_t tac = (n/2) % 4;
			Bool_t isT = n % 2 == 0;
			
			TacInfo &ti = tacInfo[n];
			if(ti.pControlT == NULL) continue;
			
			
			TF1 *pf = new TF1("periodicPolN", periodicPolN, 
						ti.pControlT->GetXaxis()->GetXmin() + 1.0,
						ti.pControlT->GetXaxis()->GetXmax(),
						nPar);
			for(int p = 0; p < nPar; p++)
				pf->SetParName(p, paramNamesN[p]);
			pf->SetNpx((ti.pControlT->GetXaxis()->GetXmax() - ti.pControlT->GetXaxis()->GetXmin()) * 256);
			TF1 *al = ti.al;
			float x0 = al->GetParameter(0);
			float b = al->GetParameter(1);
			float m = al->GetParameter(2);
			pf->FixParameter(0, x0);
			if(nPar > 5) {
				pf->SetParameter(5, 0);
				//pf->SetParLimits(5, 0, 0.005);
				pf->FixParameter(5, 0);
			}
						    
			ti.pControlT->Fit("periodicPolN", "R");
			char hName[128];
			sprintf(hName, "%s_it%d", ti.pControlT->GetName(), iter);
			ti.pControlT->Clone(hName);
			
			printf("Adjusting %3d %d %c\n", channel, tac, isT ? 'T' : 'E');
			TF1 *af = new TF1("aperiodicPolN", aperiodicPolN, 
						ti.pControlT->GetXaxis()->GetXmin(),
						ti.pControlT->GetXaxis()->GetXmax(),
						nPar);
			for(int i = 0; i < nPar; i++) {
				af->SetParameter(i, pf->GetParameter(i));
			}
			
			ti.pLUT->Reset();			
			float bias = ti.pControlT->GetMean(2);
			for(float x = x0 - 3.2; x <= x0 + 3.2; x += 0.002) {
				int adc = roundf(al->Eval(x));
				float q = myLUT.getQ(channel, tac, isT, adc);
				float error = (1.0 * af->Eval(x)) - bias;
				float newQ = (q + error);
				ti.pLUT->Fill(adc, 3.0 - newQ);
				
			}
			for(int adc = 0; adc < 1024; adc++) {
				float x = ti.pLUT->GetBinContent(ti.pLUT->FindBin(adc));
				float q = 3.0 - x;
				myLUT.setQ(channel, tac, isT, adc, q);
			}
			
			float t0 = myLUT.getT0(channel, tac, isT);
			myLUT.setT0(channel, tac, isT, t0 - bias);

			if(isT) {
				tSum += (t0 - bias);
				tNum += 1;
			}
			else {
				eSum += (t0 - bias);
				eNum += 1;
			}
			
		}
	}

	float avgTt0 = tSum/tNum;
	float avgEt0 = eSum/eNum;

	for(int n = 0; n < nTAC; n++) {
		Int_t channel = (n/2) / 4;
		Int_t tac = (n/2) % 4;
		Bool_t isT = n % 2 == 0;

		if(!isT) continue;

		float t0 = myLUT.getT0(channel, tac, isT);
		myLUT.setT0(channel, tac, isT, t0 - (avgTt0 - avgEt0));
	}

	
	myLUT.storeFile(0, 64, tableFileName1);
	myLUT.storeFile(64, 128, tableFileName2);
	resumeFile->Write();
	resumeFile->Close();

	return 0;
}
