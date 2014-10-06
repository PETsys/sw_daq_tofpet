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
#include <TOFPET/P2.hpp>
#include <assert.h>
#include <math.h>
#include <boost/lexical_cast.hpp>

static const int nBoard = 2;
static const int nASIC = 2 * nBoard;
static const int nTAC = nASIC*64*2*4;
struct TacInfo {
	TH2 *hA_Fine;
	TProfile *pA_Fine;
	TProfile *pA_P2;
	TProfile *pA_ControlADC_E;
	TH1* hA_ControlADC_E;
	TProfile *pA_ControlT;
	TProfile *pA_ControlQ;	
	TH1* pA_ControlE;

	
	TProfile *pB_ControlADC_E;
	TH1* hB_ControlADC_E;
	TProfile *pB_ControlT;
	TProfile *pB_ControlQ;	
	TH1* pB_ControlE;
	
	
	struct {
		float tEdge;
		float tB;
		float m;
		float p2;
	} shape;
	
	struct {
		float tQ;
		float a0;
		float a1;
		float a2;
	} leakage;
	

	TacInfo() {
		hA_Fine = NULL;
		pA_Fine = NULL;
		pA_ControlADC_E = NULL;
		hA_ControlADC_E = NULL;
		pA_ControlT = NULL;
		pA_ControlQ = NULL;
		pA_ControlE = NULL;
		
		shape.tEdge = 0;
		shape.tB = 0;
		shape.m = 0;
		shape.p2 = 0;
		
		leakage.tQ = 0;
		leakage.a0 = 0;
		leakage.a1 = 0;
		leakage.a2 = 0;
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
const char *paramNames2[nPar2] = {  "tEdge", "tB", "m", "p2" };
static Double_t aperiodicF2 (double tDelay, double tEdge, double tB, double m, double p2)
{
	
	double tQ = 3 - (tDelay - tEdge);
	double tFine = m * (tQ - tB) + p2 * (tQ - tB) * (tQ - tB);
	return tFine;
}
static Double_t periodicF2 (Double_t *xx, Double_t *pp)
{
	double tDelay = xx[0];	
	double tEdge = pp[0];
	double tB = pp[1];
	double m = pp[2];
	double p2 = pp[3];
	
	double tQ = 3 - fmod(4.0 + tDelay - tEdge, 2.0);
	double tFine = m * (tQ - tB) + p2 * (tQ - tB) * (tQ - tB);
	return tFine;
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
	if (argc != 6) {
		fprintf(stderr, "USAGE: %s t_branch_data.root e_branch_data.root"
				" 128"
				" mezzanine1.tdc.cal mezzanine2.tdc.cal"
				"\n",  argv[0]);
		fprintf(stderr, "t_branch_data.root -- Data to be used for T branch calibration");
		fprintf(stderr, "E_branch_data.root -- Data to be used for E branch calibration");
		fprintf(stderr, "128 -- Nominal TDC interpolation factor");
		fprintf(stderr, "board1.tdc.cal -- Output calibration constants for board 1");
		fprintf(stderr, "board2.tdc.cal -- Output calibration constants for board 2");
		return 1;
	}	
	assert(argc == 6);
//	TVirtualFitter::SetDefaultFitter("Minuit2");
	
	TRandom *random = new TRandom();

	TFile * tDataFile = new TFile(argv[1], "READ");
	TFile * eDataFile = new TFile(argv[2], "READ");
	TFile * resumeFile = new TFile("tdcSummary.root", "RECREATE");	
	float nominalM = boost::lexical_cast<float>(argv[3]);
	char *tableFileName1 = argv[4];
	char *tableFileName2 = argv[5];
	
	DAQ::TOFPET::P2 myP2(64*nASIC);


	TacInfo tacInfo[nTAC];
	for(int asic = 0; asic < nASIC; asic++) {
		for(int channel = 0; channel < 64; channel++) {
			for(int whichBranch = 0; whichBranch < 2; whichBranch++) {
				bool isT = (whichBranch == 0);
				
				
				for(int tac = 0; tac < 4; tac++) {					
					int index2 = 4 * (64 * asic + channel) + tac;				
					TacInfo &ti = tacInfo[2*index2 + (isT ? 0 : 1)];		
	
					char hName[128];			
					sprintf(hName, isT ? "htFine_%03d_%02d_%1d" : "heFine_%03d_%02d_%1d" , 
						asic, channel,	tac);
						

					TH2 *hA_Fine = isT ? (TH2 *)tDataFile->Get(hName) : (TH2 *)eDataFile->Get(hName);
					ti.hA_Fine = hA_Fine;

					if(ti.hA_Fine == NULL) continue;
					if(ti.hA_Fine->GetEntries() == 0) continue;
					
					int nBinsX = hA_Fine->GetXaxis()->GetNbins();
					float xMin = hA_Fine->GetXaxis()->GetXmin();
					float xMax = hA_Fine->GetXaxis()->GetXmax();
			
					sprintf(hName, isT ? "C%03d_%02d_%d_A_T_pFine_X" : "C%03d_%02d_%d_A_E_pFine_X", 
						asic, channel,	tac);
					
					hA_Fine->GetYaxis()->SetRangeUser(0.5 * nominalM, 4.0 * nominalM);
					TProfile *pA_Fine = hA_Fine->ProfileX(hName, 1, -1, "s");
					ti.pA_Fine = pA_Fine;
					
					float minADCY = 1024;
					float minADCX = 0;
					float minADCJ = 0;
					for(int j = 1; j < nBinsX-1; j++) {
						float adc = pA_Fine->GetBinContent(j);			

						if(adc > 3.0 * nominalM) 
							continue;		
						
						float error = pA_Fine->GetBinError(j);
						if (error > 2.0)
							continue;
						
						if(adc < 1.0 * nominalM)
							continue;;
						
						if(adc < minADCY) {
							minADCY = adc;
							minADCX = pA_Fine->GetBinCenter(j);
							minADCJ = j;
						}			
					}
					
					float maxADCY = 0;
					float maxADCX = 0;
					float maxADCJ = 0;
					for(int j = pA_Fine->FindBin(minADCX + 1.0); j > minADCJ; j--) {
						float adc = pA_Fine->GetBinContent(j);

						if(adc < (minADCY + 1.0 * nominalM))
							continue;
						
						float error = pA_Fine->GetBinError(j);
						if (error > 2.0)
							continue;
				
						if(adc > (minADCY + 2.5 * nominalM))
							continue;
						
						if(adc > maxADCY) {
							maxADCY = adc;
							maxADCX = pA_Fine->GetBinCenter(j);
							maxADCJ = 0;
						}
						
					}
					
					
					//printf("Coordinates (1): (%f %f) -- (%f %f)\n", minADCX, minADCY, maxADCX, maxADCY);
					while(minADCX > 2.0) minADCX -= 2.0;
					while(maxADCX > minADCX + 2.0) maxADCX -= 2.0;
					//printf("Coordinates(2): (%f %f) -- (%f %f)\n", minADCX, minADCY, maxADCX, maxADCY);
					
					float estimatedM = (maxADCY - minADCY)/(minADCX - maxADCX + 2.0);
					
					float lowerT0 = maxADCX;
					float upperT0 = minADCX;
						
					
					if(lowerT0 > upperT0) {
						float temp = upperT0;
						upperT0 = lowerT0;
						lowerT0 = temp;
					}
			// 		printf("tEdge limits are %f .. %f\n",  lowerT0, upperT0);		
					while(lowerT0 > 2.0) lowerT0 -= 2.0;
			// 		printf("tEdge limits are %f .. %f\n",  lowerT0, upperT0);		
					while(upperT0 > (lowerT0 + 2.0)) upperT0 -= 2.0;

					
					float estimatedT0 = lowerT0;
					lowerT0 -= 0.5 * pA_Fine->GetBinWidth(0);
					upperT0 += 0.5 * pA_Fine->GetBinWidth(0);			
					//printf("tEdge limits are %f .. %f\n",  lowerT0, upperT0);		
					
					
					//printf("estimated tEdge = %f, m = %f\n", estimatedT0, estimatedM);
					
					TF1 *pf = new TF1("periodicF1", periodicF1, xMin, xMax, nPar1);
					for(int p = 0; p < nPar1; p++) pf->SetParName(p, paramNames1[p]);
					
					pf->SetNpx(256 * (xMax - xMin));
					float fitMin = maxADCX;
					while (fitMin > 2.0) {
						fitMin -= 2.0;
					}
					fitMin = 0.5;
					
					float tEdge;
					float b;
					float m;
					float tB;
					float p2;
					float currChi2 = INFINITY;
					float prevChi2 = INFINITY;
					int nTry = 0;
					do {
						pf->SetParameter(0, estimatedT0);	pf->SetParLimits(0, lowerT0, upperT0);
						pf->SetParameter(1, minADCY);		pf->SetParLimits(1, 0.95 * minADCY, 1.0 * minADCY);
						pf->SetParameter(2, estimatedM);	pf->SetParLimits(2, 0.95 * estimatedM, 1.05 * estimatedM);
						pA_Fine->Fit("periodicF1", "Q", "", fitMin, xMax);
						
						TF1 *pf_ = pA_Fine->GetFunction("periodicF1");
						if(pf_ != NULL) {
							prevChi2 = currChi2;
							currChi2 = pf_->GetChisquare() / pf_->GetNDF();	
							
							if(currChi2 < prevChi2) {
								tEdge = pf->GetParameter(0);
								b  = pf->GetParameter(1);
								m  = pf->GetParameter(2);		
								tB = - (b/m - 1.0);
								p2 = 0;								
							}
						}
						estimatedT0 += 0.001;
						nTry += 1;
						
					} while((currChi2 <= 0.95*prevChi2) && (nTry < 10) && (estimatedT0 < upperT0));
					
					if(prevChi2 > 1E6) {
						fprintf(stderr, "WARNING: No or very bad fit. Skipping TAC.\n");
						continue;
					}
					
					TF1 *pf2 = new TF1("periodicF2", periodicF2, xMin, xMax,  nPar2);		
					pf2->SetNpx(256 * (xMax - xMin));
					for(int p = 0; p < nPar2; p++) pf2->SetParName(p, paramNames2[p]);
					
					currChi2 = INFINITY;
					prevChi2 = INFINITY;
					nTry = 0;
					do {
						pf2->SetParameter(0, tEdge);		pf2->SetParLimits(0, tEdge-0.1, tEdge+0.1);
						pf2->SetParameter(1, tB);		pf2->SetParLimits(1, tB, 0);
						pf2->SetParameter(2, m);		pf2->SetParLimits(2, 1.00 * m, 1.15 * m);
						pf2->SetParameter(3, -1.0);		pf2->SetParLimits(3, -5.0, 0);
						pA_Fine->Fit("periodicF2", "Q", "", fitMin, xMax);

						TF1 *pf_ = pA_Fine->GetFunction("periodicF2");
						if(pf_ != NULL) {
							prevChi2 = currChi2;
							currChi2 = pf_->GetChisquare() / pf_->GetNDF();	
							
							if(currChi2 < prevChi2) {
								tEdge = pf->GetParameter(0);
								b  = pf->GetParameter(1);
								m  = pf->GetParameter(2);		
								tB = - (b/m - 1.0);
								p2 = 0;								
							}
						}
						nTry += 1;
						
					} while((currChi2 <= 0.95*prevChi2) && (nTry < 10));
					
					
					if(prevChi2 > 1E6) {
						fprintf(stderr, "WARNING: No or very bad fit. Skipping TAC.\n");
						continue;
					}
					if(prevChi2 > 10) {						
						fprintf(stderr, "WARNING: BAD FIT\n");
						
					}
					
					ti.shape.tEdge = tEdge = pf2->GetParameter(0);
					ti.shape.tB = tB = pf2->GetParameter(1);
					ti.shape.m  = m  = pf2->GetParameter(2);
					ti.shape.p2 = p2 = pf2->GetParameter(3);
					
				
					myP2.setShapeParameters((64*asic+channel), tac, isT, tB, m, p2);
					myP2.setT0(channel, tac, isT, tEdge);

					// Allocate the control histograms
					sprintf(hName, isT ? "C%03d_%02d_%d_A_T_control_pADC_E" : "C%03d_%02d_%d_A_E_control_pADC_E", 
						asic, channel, tac);
					ti.pA_ControlADC_E = new TProfile(hName, hName, nBinsX, xMin, xMax, "s");

					sprintf(hName, isT ? "C%03d_%02d_%d_A_T_control_hADC_E" : "C%03d_%02d_%d_A_E_control_hADC_E", 
							asic, channel, tac);
					ti.hA_ControlADC_E = new TH1F(hName, hName, 256, -128, 128);		

					
					sprintf(hName, isT ? "C%03d_%02d_%d_A_T_control_T" : "C%03d_%02d_%d_A_E_control_T", 
							asic, channel, tac);
					ti.pA_ControlT = new TProfile(hName, hName, nBinsX, xMin, xMax, "s");
					sprintf(hName, isT ? "C%03d_%02d_%d_A_T_control_Q" : "C%03d_%02d_%d_A_E_control_Q", 
							asic, channel, tac);
					ti.pA_ControlQ = new TProfile(hName, hName, nBinsX, xMin, xMax, "s");
					
					sprintf(hName, isT ? "C%03d_%02d_%d_A_T_control_E" : "C%03d_%02d_%d_A_E_control_E", 
							asic, channel, tac);
					ti.pA_ControlE = new TH1F(hName, hName, 256, -0.5, 0.5);
				}

				
				TFile *dataFile = isT ? tDataFile : eDataFile;
				TH1 * hTPoint = (TH1 *)dataFile->Get("hTPoint");
				if(hTPoint == NULL)
					continue;
				
				for(int tac = 0; tac < 4; tac++) {
					int index2 = 4 * (64 * asic + channel) + tac;				
					TacInfo &ti = tacInfo[2*index2 + (isT ? 0 : 1)];				
					
					if(ti.pA_Fine == NULL) {
						continue;
					}
					// We use the same delay for all (ASIC, TAC, branch) combinations for a given channel
					float x = hTPoint->GetBinContent(channel + 1);
					float tQ = 3 - fmod(4.0 + x - ti.shape.tEdge, 2.0);
					
					char hName[128];
					sprintf(hName, isT ? "htFineB_%03d_%02d_%1d" : "heFineB_%03d_%02d_%1d", asic, channel, tac);
					TH2 * hB_Fine = (TH2 *)dataFile->Get(hName);					
					if(hB_Fine == NULL) {
						continue;
					}
					
					sprintf(hName, isT ? "C%03d_%02d_%d_B_T_pFine_X" : "C%03d_%02d_%d_B_E_pFine_X", asic, channel, tac);
					TProfile *pB_Fine = hB_Fine->ProfileX(hName, 1, -1, "s");
					
					int nTry = 0;
					while(nTry < 10){
						pB_Fine->Fit("pol1", "Q");
						TF1 *fit = pB_Fine->GetFunction("pol1");
						if(fit == NULL) break;
						float chi2 = fit->GetChisquare();
						float ndf = fit->GetNDF();
						if(chi2/ndf < 2.0) break;
						nTry += 1;
					}
					TF1 *fit = pB_Fine->GetFunction("pol1");
					if(fit == NULL) continue;
					
					ti.leakage.tQ = tQ;
					ti.leakage.a0 = fit->GetParameter(0);
					ti.leakage.a1 = fit->GetParameter(1) / (1024 * 4);
					ti.leakage.a2 = 0;//fit->GetParameter(2) / ((1024 * 4)*(1024*4));
					
					myP2.setLeakageParameters((64 * asic + channel), tac, isT, ti.leakage.tQ, ti.leakage.a0, ti.leakage.a1, ti.leakage.a2);

					int nBinsX = hB_Fine->GetXaxis()->GetNbins();
					float xMin = hB_Fine->GetXaxis()->GetXmin();
					float xMax = hB_Fine->GetXaxis()->GetXmax();
					
					sprintf(hName, isT ? "C%03d_%02d_%d_B_T_control_pADC_E" : "C%03d_%02d_%d_B_E_control_pADC_E", 
							asic, channel, tac);
					ti.pB_ControlADC_E = new TProfile(hName, hName, nBinsX, xMin, xMax, "s");

					sprintf(hName, isT ? "C%03d_%02d_%d_B_T_control_hADC_E" : "C%03d_%02d_%d_B_E_control_hADC_E", 
							asic, channel, tac);
					ti.hB_ControlADC_E = new TH1F(hName, hName, 256, -128, 128);		

					sprintf(hName, isT ? "C%03d_%02d_%d_B_T_control_T" : "C%03d_%02d_%d_B_E_control_T", 
							asic, channel, tac);
					ti.pB_ControlT = new TProfile(hName, hName, nBinsX, xMin, xMax, "s");
					sprintf(hName, isT ? "C%03d_%02d_%d_B_T_control_Q" : "C%03d_%02d_%d_B_E_control_Q", 
							asic, channel, tac);
					ti.pB_ControlQ = new TProfile(hName, hName, nBinsX, xMin, xMax, "s");
					
					sprintf(hName, isT ? "C%03d_%02d_%d_B_T_control_E" : "C%03d_%02d_%d_B_E_control_E", 
							asic, channel, tac);
					ti.pB_ControlE = new TH1F(hName, hName, 256, -0.5, 0.5);		

				}
				
				for(int tac = 0; tac < 4; tac++) {
					int index2 = 4 * (64 * asic + channel) + tac;				
					TacInfo &ti = tacInfo[2*index2 + (isT ? 0 : 1)];				
					
					if(ti.pA_Fine == NULL) 
						continue;
					
					printf("%2d:%2d:%c:%d | %5.4f %5.4f %5.1f %6.4f | %5.4f %5.1f %5.2f %5.4f |\n",
						asic, channel, isT ? 'T' : 'E', tac,
					       ti.shape.tEdge, ti.shape.tB, ti.shape.m, ti.shape.p2,
					       ti.leakage.tQ, ti.leakage.a0, ti.leakage.a1*1E6, ti.leakage.a2
					       );
					
				}
				
			}
		}
	}
	
	
	// Correct t0 and build shape quality control histograms
	const int nIterations = 2;
	// Need two iterations to correct t0, then one more to build final histograms
	for(int iter = 0; iter <= nIterations; iter++) {
		for(int n = 0; n < nTAC; n++) {
			if(tacInfo[n].pA_ControlT == NULL) continue;
			tacInfo[n].pA_ControlT->Reset();
			tacInfo[n].pA_ControlE->Reset();
			tacInfo[n].pA_ControlQ->Reset();
			
		}


		for(int whichBranch = 0; whichBranch < 2; whichBranch++) {
			bool isT = (whichBranch == 0);
			
			Float_t fStep2;
			Int_t fAsic;
			Int_t fChannel;
			Int_t fTac;
			Int_t fCoarse;
			Int_t fFine;
			Long64_t fTacIdleTime;

			TTree *data = NULL;
			if (isT) {
				data = (TTree *)tDataFile->Get("data3");
				data->SetBranchAddress("step2", &fStep2);
				data->SetBranchAddress("asic", &fAsic);
				data->SetBranchAddress("channel", &fChannel);
				data->SetBranchAddress("tac", &fTac);
				data->SetBranchAddress("tcoarse", &fCoarse);
				data->SetBranchAddress("tfine", &fFine);
				data->SetBranchAddress("tacIdleTime", &fTacIdleTime);
			}
			else {
				data = (TTree *)eDataFile->Get("data3");
				data->SetBranchAddress("step2", &fStep2);
				data->SetBranchAddress("asic", &fAsic);
				data->SetBranchAddress("channel", &fChannel);
				data->SetBranchAddress("tac", &fTac);
				data->SetBranchAddress("ecoarse", &fCoarse);
				data->SetBranchAddress("efine", &fFine);
				data->SetBranchAddress("tacIdleTime", &fTacIdleTime);
			}

			int nEvents = data->GetEntries();
			for(int i = 0; i < nEvents; i++) {
				data->GetEntry(i);				
				int index2 = 4 * (64 * fAsic + fChannel) + fTac;			
				if(fAsic >= nASIC) continue;
				if(fChannel >= 64) continue;
				if(fTac >= 4) continue;
				
				
				
				TacInfo &ti = tacInfo[2*index2 + (isT ? 0 : 1)];
				
				if (ti.pA_ControlT == NULL) continue;
				if (ti.pB_ControlT == NULL) continue;

				float step2Width = ti.pA_ControlT->GetBinWidth(1);
		
				//TF1 * f = ti.pA_Fine->GetFunction("periodicF2");

				float t = fStep2;				
				float t_ = fmod(4.0 + t - ti.shape.tEdge, 2.0);
				float adcEstimate = aperiodicF2(t_, 0, ti.shape.tB, ti.shape.m, ti.shape.p2);
				
				float adcError = fFine - adcEstimate;

				if(fabs(adcError) <= ti.shape.m) {
					ti.pA_ControlADC_E->Fill(fStep2, adcError);
					ti.hA_ControlADC_E->Fill(adcError);
				}
				
				
				float tEstimate = myP2.getT((64 * fAsic + fChannel), fTac, isT, fFine, fCoarse, fTacIdleTime);
				float qEstimate = myP2.getQ((64 * fAsic + fChannel), fTac, isT, fFine, fTacIdleTime);
				bool isNormal = myP2.isNormal((64 * fAsic + fChannel), fTac, isT, fFine, fCoarse, fTacIdleTime);				
				float tError = tEstimate - fStep2;
	
				
				if(!isNormal) continue;
				ti.pA_ControlT->Fill(fStep2, tError);
				ti.pA_ControlE->Fill(tError);
				ti.pA_ControlQ->Fill(fStep2, qEstimate);
			}
		}
		
		if(iter >= nIterations) continue;
		for(int n = 0; n < nTAC; n++) {
			Int_t channel = (n/2) / 4;
			Int_t tac = (n/2) % 4;
			Bool_t isT = (n % 2 == 0);
			
			TacInfo &ti = tacInfo[n];
			if(ti.pA_ControlT == NULL) continue;
			if(ti.pB_ControlT == NULL) continue;

			float offset = INFINITY;
			if(iter == 0 ) 
				offset = ti.pA_ControlT->GetMean(2);
			else
				offset = ti.pA_ControlE->GetMean();
			
	
			float tEdge = myP2.getT0(channel, tac, isT);
			myP2.setT0(channel, tac, isT, tEdge - offset);
			

			
		}
	}
	
	// Build leakage quality control
	for(int whichBranch = 0; whichBranch < 2; whichBranch++) {
		bool isT = (whichBranch == 0);
		
		Float_t fStep1;
		Float_t fStep2;
		Int_t fAsic;
		Int_t fChannel;
		Int_t fTac;
		Int_t fCoarse;
		Int_t fFine;
		Long64_t fTacIdleTime;

		TTree *data = NULL;
		if (isT) {
			data = (TTree *)tDataFile->Get("data3B");
			data->SetBranchAddress("step1", &fStep1);
			data->SetBranchAddress("step2", &fStep2);
			data->SetBranchAddress("asic", &fAsic);
			data->SetBranchAddress("channel", &fChannel);
			data->SetBranchAddress("tac", &fTac);
			data->SetBranchAddress("tcoarse", &fCoarse);
			data->SetBranchAddress("tfine", &fFine);
			data->SetBranchAddress("tacIdleTime", &fTacIdleTime);
		}
		else {
			data = (TTree *)eDataFile->Get("data3B");
			data->SetBranchAddress("step1", &fStep1);
			data->SetBranchAddress("step2", &fStep2);
			data->SetBranchAddress("asic", &fAsic);
			data->SetBranchAddress("channel", &fChannel);
			data->SetBranchAddress("tac", &fTac);
			data->SetBranchAddress("ecoarse", &fCoarse);
			data->SetBranchAddress("efine", &fFine);
			data->SetBranchAddress("tacIdleTime", &fTacIdleTime);
		}

		int nEvents = data->GetEntries();
		for(int i = 0; i < nEvents; i++) {
			data->GetEntry(i);				
			int index2 = 4 * (64 * fAsic + fChannel) + fTac;			
			if(fAsic >= nASIC) continue;
			if(fChannel >= 64) continue;
			if(fTac >= 4) continue;
			
			
			
			TacInfo &ti = tacInfo[2*index2 + (isT ? 0 : 1)];
			
			if (ti.pA_ControlT == NULL) continue;	
			float step2Width = ti.pA_ControlT->GetBinWidth(1);
			
			//printf("%d %d %d %d %d\n", fAsic, fChannel, fTac, fCoarse, fFine);
			
			float adcEstimate = ti.leakage.a0 + ti.leakage.a1 * fTacIdleTime + ti.leakage.a2 * fTacIdleTime * fTacIdleTime;
			float adcError = fFine - adcEstimate;
			//printf("%f %lld : %d - %f = %f\n", fStep1*1024*4, fTacIdleTime, fFine, adcEstimate, adcError);
			if(fabs(adcError)  < ti.shape.m) {
				// WARNING: since we use more than 1 value of step2, this histogram isn't value
				//ti.pB_ControlADC_E->Fill(fStep1, adcError);
				//ti.hB_ControlADC_E->Fill(adcError);
			}
			
			float tEstimate = myP2.getT((64 * fAsic + fChannel), fTac, isT, fFine, fCoarse, fTacIdleTime);
			float qEstimate = myP2.getQ((64 * fAsic + fChannel), fTac, isT, fFine, fTacIdleTime);
			bool isNormal = myP2.isNormal((64 * fAsic + fChannel), fTac, isT, fFine, fCoarse, fTacIdleTime);			
			float tError = tEstimate - fStep2;
			
// 			if(fAsic == 0 && fChannel == 0 && fTac == 0 && isT) 
// 				printf("%f %f %d %d %d => %f %f\n", 
// 					fStep1, fStep2, fCoarse, fFine, fTacIdleTime, 
// 					qEstimate, tEstimate);			
			
			if(!isNormal) continue;
			ti.pB_ControlT->Fill(fStep1, tError);
			ti.pB_ControlE->Fill(tError);
			
			// WARNING: since we use more than 1 value of step2, this histogram isn't value
			//ti.pB_ControlQ->Fill(fStep1, qEstimate);

		}
	}
	
	
	for(int asic = 0; asic < nASIC; asic++) {
		for(int channel = 0; channel < 64; channel++) {			
		
			float t0_adjust_sum = 0;
			int t0_adjust_N = 0;	
			for(int tac = 0; tac < 4; tac++) {
				int index2 = 4 * (64 * asic + channel) + tac;				
				TacInfo &tiT = tacInfo[2*index2 + 0];				
				TacInfo &tiE = tacInfo[2*index2 + 1];				
				
				if(tiT.pA_Fine == NULL || tiE.pA_Fine == NULL) 
					continue;
			
				float t0_T = myP2.getT0((64 * asic + channel), tac, true);
				float t0_E = myP2.getT0((64 * asic + channel), tac, false);
		
				t0_adjust_sum += (t0_T - t0_E);
				t0_adjust_N += 1;				
			}
			
			float t0_adjust = t0_adjust_sum / t0_adjust_N;
			for(int tac = 0; tac < 4; tac++) {
				int index2 = 4 * (64 * asic + channel) + tac;				
				TacInfo &tiT = tacInfo[2*index2 + 0];				
				TacInfo &tiE = tacInfo[2*index2 + 1];				
				
				if(tiT.pA_Fine == NULL || tiE.pA_Fine == NULL) 
					continue;
			
				float t0_T = myP2.getT0((64 * asic + channel), tac, true);
				float t0_T_adjusted = t0_T - t0_adjust;
				
				fprintf(stderr, "%d %2d %d adjusting %f to %f\n", asic, channel, tac, t0_T, t0_T_adjusted);
				
				myP2.setT0((64 * asic + channel), tac, true, t0_T_adjusted);
			}
			
		}
	}
	
	
	
	
	myP2.storeFile(  0, 128, tableFileName1);
	myP2.storeFile(128, 256, tableFileName2);
	resumeFile->Write();
	resumeFile->Close();

	return 0;
}

