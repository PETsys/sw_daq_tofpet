#include <TFitResult.h>
#include <TFile.h>
#include <TNtuple.h>
#include <TChain.h>
#include <TProfile.h>
#include <TH1F.h>
#include <TH2F.h>
#include <TF1.h>
#include <TGraph.h>
#include <Fit/FitConfig.h>
#include <TVirtualFitter.h>
#include <TMinuit.h>
#include <Math/ProbFuncMathCore.h>
#include <TRandom.h>
#include <TOFPET/LUT.hpp>
#include <assert.h>
#include <math.h>
#include <boost/lexical_cast.hpp>

static const int nIterations = 0;

static const int nTAC = 128*2*4;
struct TacInfo {
	TH2 *hFine;
	TProfile *pFine;
	TProfile *pLUT;
	TProfile *pControlT;
	TProfile *pControlQ;	
	TH1* pControlE;
	
	struct {
		float x0;
		float b;
		float m;
		float p2;
	} shape;
	
	

	TacInfo() {
		hFine = NULL;
		pFine = NULL;		
		pControlT = NULL;
		pControlQ = NULL;
		pControlE = NULL;
	}
};

static const float rangeX = 0.2;

Double_t square(Double_t *x, Double_t *p)
{
	
        Double_t A = p[0];
	Double_t x1 = p[1];
	Double_t x2 = p[1] + p[2];

	if(*x >= x1 && *x <= x2)
		return A;
	else
		return 0;
}


Double_t sigmoid2(Double_t *x, Double_t *p)
{
        Double_t A = p[0];
	Double_t x1 = p[1];
	Double_t x2 = p[1] + p[2];
	Double_t sigma1 = p[3];
	Double_t sigma2 = p[4];
	
	return A * (ROOT::Math::normal_cdf (*x, sigma1, x1) - ROOT::Math::normal_cdf (*x, sigma2, x2));
}

static const int nPar1 = 3;
const char * paramNames1[nPar1] =  { "x0", "b", "m" };
static double aperiodicF1 (double x, double x0, double b, double m)
{
	x = x - x0;
	return b + 2*m - m*x;
}
static Double_t periodicF1 (Double_t *xx, Double_t *pp)
{
	float x = fmod(4.0 + xx[0] - pp[0], 2.0);	
	return pp[1] + pp[2]*2 - pp[2]*x;
}

static const int nPar2 = 4;
const char *paramNames2[nPar2] = { "x0", "b", "m", "p2" };
static Double_t periodicF2 (Double_t *xx, Double_t *pp)
{
	double x = fmod(4.0 + xx[0] - pp[0], 2.0);	
	return  pp[1] + pp[2]*2 - pp[2]*x + pp[3]*(2.0-x)*(2.0-x);
}
static Double_t aperiodicF2 (double x, double x0, double b, double m, double p2)
{
	x = x - x0;
	return b + 2*m - m*x + p2*(2.0-x)*(2.0-x);
}

static const int nPar3 = 4;
const char *paramNames3[nPar3] = { "x0", "b", "m", "p2" };
static Double_t periodicF3 (Double_t *xx, Double_t *pp)
{
	double x = fmod(4.0 + xx[0] - pp[0], 2.0);	
	return  pp[1] + pp[2]*x + pp[3]*x*x;
}
static Double_t aperiodicF3 (double x, double x0, double b, double m, double p2)
{
	x = x - x0;
	return b + + m*x + p2*x*x;
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
	
	TRandom *random = new TRandom();

	TFile * tdcaDataFile = new TFile(argv[1], "READ");
	TFile * fetpDataFile = new TFile(argv[2], "READ");
	TFile * resumeFile = new TFile("tdcSummary.root", "RECREATE");	
	float nominalM = boost::lexical_cast<float>(argv[3]);
	char *tableFileName1 = argv[4];
	char *tableFileName2 = argv[5];
	
	DAQ::TOFPET::LUT myLUT(128);


	TacInfo tacInfo[nTAC];
	for(int n = 0; n < nTAC; n++) {
		Int_t channel = (n/2) / 4;
		Int_t tac = (n/2) % 4;
		Bool_t isT = n % 2 == 0;
		
		if(channel != 0) continue;
		
		char hName[128];			
		sprintf(hName, isT ? "htFine_%03d_%02d_%1d" : "heFine_%03d_%02d_%1d" , 
			channel/64, channel%64,	tac);
			
		TacInfo &ti = tacInfo[n];

		TH2 *hFine = isT ? (TH2 *)fetpDataFile->Get(hName) : (TH2 *)tdcaDataFile->Get(hName);
		ti.hFine = hFine;

		if(ti.hFine == NULL) continue;

		int nBinsX = hFine->GetXaxis()->GetNbins();
		int xMin = hFine->GetXaxis()->GetXmin();
		int xMax = hFine->GetXaxis()->GetXmax();

		printf(">>>> Fitting %3d %d %c (%p)\n", channel, tac, isT ? 'T' : 'E', hFine);
		
		sprintf(hName, isT ? "C%03d_%02d_%d_T_hFine_Y" : "C%03d_%02d_%d_E_hFine_Y", 
			channel/64, channel%64, tac);
		TH1 * hFineY = hFine->ProjectionY(hName);
		float minADCY = hFineY->GetBinCenter(hFineY->FindFirstBinAbove(0.5*hFineY->GetMaximum()));
		float maxADCY = hFineY->GetBinCenter(hFineY->FindLastBinAbove(0.5*hFineY->GetMaximum()));
		
		sprintf(hName, isT ? "C%03d_%02d_%d_T_pFine_X" : "C%03d_%02d_%d_E_pFine_X", 
			channel/64, channel%64, tac);
		hFine->GetYaxis()->SetRangeUser(0.90 * minADCY, 1.10 * maxADCY);
		TProfile *pFine = hFine->ProfileX(hName, 1, -1);
		ti.pFine = pFine;
		
		int j = pFine->FindBin(1.0);
		float maxADCX = 0;
		for(; j < nBinsX; j++) {
			float x = pFine->GetBinCenter(j);
			float y = pFine->GetBinContent(j);
			float e = pFine->GetBinError(j);
			
			if (fabs(y - maxADCY) < 0.02 * maxADCY) {
				maxADCX = x;
				break;
			}
			
		}
		
		float minADCX = j;
		for(; j < nBinsX; j++) {
			float x = pFine->GetBinCenter(j);
			float y = pFine->GetBinContent(j);
			float e = pFine->GetBinError(j);
			
			if (fabs(y - minADCY) < 0.02 * minADCY) {				
				minADCX = x;
				break;
			}
		}
		
		printf("Coordinates: (%f %f) -- (%f %f)\n", maxADCX, maxADCY, minADCX, minADCY);
		
		float estimatedM = (maxADCY - minADCY)/(minADCX - maxADCX);
		
		float lowerX0 = maxADCX;
		float upperX0 = minADCX - 2.0;
			
		
		if(lowerX0 > upperX0) {
			float temp = upperX0;
			upperX0 = lowerX0;
			lowerX0 = temp;
		}
/*		lowerX0 -= 0.5 * pFine->GetBinWidth(0);
		upperX0 += 0.5 * pFine->GetBinWidth(0);		*/
		float estimatedX0 = lowerX0;
		printf("x0 limits are %f .. %f\n",  lowerX0, upperX0);		
		
		
		printf("estimated x0 = %f, m = %f\n", estimatedX0, estimatedM);
		
 		TF1 *pf = new TF1("periodicF1", periodicF1, xMin, xMax, nPar1);
		for(int p = 0; p < nPar1; p++) pf->SetParName(p, paramNames1[p]);
		
		pf->SetNpx(256 * (xMax - xMin));
		
		
		bool goodFit = false;
		int nTry = 0;
		do {
			
			
			pf->SetParameter(0, estimatedX0);	pf->SetParLimits(0, lowerX0, upperX0);
			pf->SetParameter(1, minADCY);		pf->SetParLimits(1, 0.95 * minADCY, 1.0 * minADCY + 0.5);
			pf->SetParameter(2, estimatedM);	pf->SetParLimits(2, 0.95 * estimatedM, 1.05 * estimatedM);
			
			pFine->Fit("periodicF1", "", "", maxADCX, xMax);			
			goodFit = gMinuit->fCstatu.Data()[0] == 'C';

			estimatedX0 += 0.001;
			nTry += 1;
			
		} while(!goodFit && nTry < 10 && estimatedX0 < upperX0);
		
		//if(!goodFit) break;

		pf = pFine->GetFunction("periodicF1");
		if(pf == NULL) continue;
		
		float x0 = ti.shape.x0 = pf->GetParameter(0);
		float b  = ti.shape.b = pf->GetParameter(1);
		float m  = ti.shape.m = pf->GetParameter(2);
				
		TF1 *pfN = new TF1("periodicF2", periodicF2, xMin, xMax,  nPar2);
		pfN->SetNpx(256 * (xMax - xMin));
		for(int p = 0; p < nPar2; p++) pfN->SetParName(p, paramNames2[p]);
		pfN->FixParameter(0, x0);
		pfN->FixParameter(1, b);
		pfN->SetParameter(2, m);		pfN->SetParLimits(2, 1.00 * m, 1.05 * m);
		pfN->SetParameter(3, 0);		
		
		
		
		pFine->Fit("periodicF2", "", "", x0, xMax);		  
		
		pfN = pFine->GetFunction("periodicF2");
		if(pfN == NULL) continue;
		
		float pfNx0  = ti.shape.x0 = pfN->GetParameter(0);
		float pfNb  = ti.shape.b = pfN->GetParameter(1);
		float pfNm  = ti.shape.m = pfN->GetParameter(2);
		float pfNp2  = ti.shape.p2 = pfN->GetParameter(3);
		
		printf("**** ****\n");
		printf("%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\n", maxADCX, maxADCY, minADCX, minADCY,
				x0, pfNb, pfNm, pfNp2);
		
		printf("At x0		=> %f vs %f\n", aperiodicF1(x0, x0, b, m), aperiodicF2(x0, x0, pfNb, pfNm, pfNp2));
		double endx = 1.999;
		printf("At x0+%5.3f	=> %f vs %f\n", endx, aperiodicF1(x0+endx, x0, b, m), aperiodicF2(x0+endx, x0, pfNb, pfNm, pfNp2));
		
		sprintf(hName, isT ? "C%03d_%02d_%d_T_pLUT" : "C%03d_%02d_%d_E_pLUT", 
			channel/64, channel%64, tac);
		TProfile *pLUT = new TProfile(hName, hName, 1024, 0, 1024);
		ti.pLUT = pLUT;
		
		for(float t = - 3.2; t <= 3.2; t += 0.002) {
			//float adc = aperiodicF1(t, 0, ti.shape.b, ti.shape.m);
			float adc = aperiodicF2(t, 0, ti.shape.b, ti.shape.m, ti.shape.p2);
			float q = 3.0 - t;
			pLUT->Fill(adc, q);
		}
		for(int adc = 0; adc < 1024; adc++) {
			float q = pLUT->GetBinContent(pLUT->FindBin(adc));
			myLUT.setQ(channel, tac, isT, adc, q);
		}
		myLUT.setT0(channel, tac, isT, +x0);

		// Allocate the control histograms
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
	
	float tSum = 0, eSum = 0;
	int tNum = 0, eNum = 0;

	for(int iter = -1; iter <= nIterations; iter++) {
		for(int n = 0; n < nTAC; n++) {
			if(tacInfo[n].pControlT == NULL) continue;
			tacInfo[n].pControlT->Reset();
			tacInfo[n].pControlE->Reset();
			tacInfo[n].pControlQ->Reset();
			
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
		
		tSum = 0; tNum = 0; eSum = 0; eNum = 0;
		for(int n = 0; n < nTAC; n++) {
			Int_t channel = (n/2) / 4;
			Int_t tac = (n/2) % 4;
			Bool_t isT = n % 2 == 0;
			
			TacInfo &ti = tacInfo[n];
			if(ti.pControlT == NULL) continue;
			
			float offset = ti.pControlT->GetMean(2);
			
			float t0 = myLUT.getT0(channel, tac, isT);
			myLUT.setT0(channel, tac, isT, t0 - offset);
			
			if(isT) {
				tSum += (t0 - offset);
				tNum += 1;
			}
			else {
				eSum += (t0 - offset);
				eNum += 1;
			}

			
		}
		
		if(iter < 0) continue;
	
		continue;
		for(int n = 0; n < nTAC; n++) {
			Int_t channel = (n/2) / 4;
			Int_t tac = (n/2) % 4;
			Bool_t isT = n % 2 == 0;
			
			TacInfo &ti = tacInfo[n];
			if(ti.pControlT == NULL) continue;
		
			float xMin = ti.pControlT->GetXaxis()->GetXmin();
			float xMax = ti.pControlT->GetXaxis()->GetXmax();
			TF1 *pf = new TF1("periodicF3", periodicF3, xMin, xMax, nPar3);
			for(int p = 0; p < nPar3; p++)  pf->SetParName(p, paramNames3[p]);
			pf->SetNpx(256*(xMax-xMin));
		
			pf->FixParameter(0, ti.shape.x0);
			pf->FixParameter(2, 0);

			
			ti.pControlT->Fit("periodicF3", "", "", ti.shape.x0, xMax);
			
		}

	}


	resumeFile->Write();
	resumeFile->Close();

	return 0;
}

