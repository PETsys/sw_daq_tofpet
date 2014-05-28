#include <TFile.h>
#include <TNtuple.h>
#include <TChain.h>
#include <TProfile.h>
#include <TH1F.h>
#include <TH2F.h>
#include <TF1.h>
#include <TGraph.h>
#include <TVirtualFitter.h>
#include <TOFPET/P2.hpp>
#include <assert.h>
#include <math.h>
#include <boost/lexical_cast.hpp>

static const int nIterations = 2;

static const int nTAC = 128*2*4;
struct TacInfo {
	TH2 *hFine;
	TProfile *pFine;
	TProfile *pP2;
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


int main(int argc, char *argv[])
{
	assert(argc == 3);
//	TVirtualFitter::SetDefaultFitter("Minuit2");

	TFile * dataFile = new TFile(argv[1], "READ");
	TFile * resumeFile = new TFile("tdcSummary2.root", "RECREATE");	
	char *tableFileName = argv[2];
	
	DAQ::TOFPET::P2 myP2(128);
	myP2.loadFiles(tableFileName);


	TacInfo tacInfo[nTAC];
	for(int n = 0; n < nTAC; n++) {
		Int_t channel = (n/2) / 4;
		Int_t tac = (n/2) % 4;
		Bool_t isT = n % 2 == 0;
		
		char hName[128];			
		sprintf(hName, isT ? "htFine_%03d_%02d_%1d" : "heFine_%03d_%02d_%1d" , 
			channel/64, channel%64,	tac);
			
		TacInfo &ti = tacInfo[n];

		TH2 *hFine = (TH2 *)dataFile->Get(hName);
		if(hFine == NULL) continue;
		
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
		ti.pControlE = new TH1F(hName, hName, 2048, -4, 4);
		
	}

	TTree *data = (TTree *)dataFile->Get("data3");
	Float_t fStep2;		data->SetBranchAddress("step2", &fStep2);
	Int_t fAsic;		data->SetBranchAddress("asic", &fAsic);
	Int_t fChannel;		data->SetBranchAddress("channel", &fChannel);
	Int_t fTac;		data->SetBranchAddress("tac", &fTac);
	Int_t ftCoarse;		data->SetBranchAddress("tcoarse", &ftCoarse);
	Int_t feCoarse;		data->SetBranchAddress("ecoarse", &feCoarse);
	Int_t ftFine;		data->SetBranchAddress("tfine", &ftFine);
	Int_t feFine;		data->SetBranchAddress("efine", &feFine);
	Long64_t fTacIdleTime;	data->SetBranchAddress("tacIdleTime", &fTacIdleTime);

	for(int n = 0; n < nTAC; n++) {
		if(tacInfo[n].pControlT == NULL) continue;
		tacInfo[n].pControlT->Reset();
	}

	int nEvents = data->GetEntries();

	Double_t minTCoarse = 1024;
	Double_t minECoarse = 1024;
	
	Double_t totalTCoarse = 0;
	Double_t totalECoarse = 0;

	for(int i = 0; i < nEvents; i++) {
		data->GetEntry(i);
		
	
		if(ftCoarse == 0) continue;

		minTCoarse = (minTCoarse < ftCoarse) ? minTCoarse : ftCoarse;
		minECoarse = (minECoarse < feCoarse) ? minECoarse : feCoarse;
		
		totalTCoarse += ftCoarse;
		totalECoarse += feCoarse;
	}

	
	Double_t avgTCoarse = totalTCoarse / nEvents - 5.5;
	Double_t avgECoarse = totalTCoarse / nEvents - 5.5;

	for(int i = 0; i < nEvents; i++) {
		data->GetEntry(i);
		
		
		int index = 64 * fAsic + fChannel;
		float tT = myP2.getT(index, fTac, true, ftFine, ftCoarse, fTacIdleTime);
		float qT = myP2.getQ(index, fTac, true, ftFine, fTacIdleTime);
		bool isNormalT = myP2.isNormal(index, fTac, true, ftFine, ftCoarse, fTacIdleTime);
		
		float tE = myP2.getT(index, fTac, false, feFine, feCoarse, fTacIdleTime);			
		float qE = myP2.getQ(index, fTac, false, feFine, fTacIdleTime);
		bool isNormalE = myP2.isNormal(index, fTac, false, feFine, feCoarse, fTacIdleTime);
	
		
		tT = tT - avgTCoarse;
		tE = tE - avgECoarse;

		index = 4 * index + fTac;
		
		TacInfo &tiT = tacInfo[2*index + 0];
		TacInfo &tiE = tacInfo[2*index + 1];

		
		if((tiT.pControlT != NULL)) {
			if(isNormalT) {
				tiT.pControlT->Fill(fStep2, tT - fStep2);
				tiT.pControlE->Fill(tT - fStep2);
			}
			tiT.pControlQ->Fill(fStep2, qT);
		}

		if((tiE.pControlT != NULL)) {
			if(isNormalE) {
				tiE.pControlT->Fill(fStep2, tE - fStep2);
				tiE.pControlE->Fill(tE - fStep2);
			}
			tiE.pControlQ->Fill(fStep2, qE);
		}
		
		
	
	}

	
	resumeFile->Write();
	resumeFile->Close();

	return 0;
}
