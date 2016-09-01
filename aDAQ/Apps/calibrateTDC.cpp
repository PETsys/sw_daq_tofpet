#include <TFitResult.h>
#include <TFile.h>
#include <TNtuple.h>
#include <TChain.h>
#include <TProfile.h>
#include <TH1F.h>
#include <TH2F.h>
#include <TH2S.h>
#include <TF1.h>
#include <TGraph.h>
#include <TGraphErrors.h>
#include <Fit/FitConfig.h>
#include <TVirtualFitter.h>
#include <TMinuit.h>
#include <Math/ProbFuncMathCore.h>
#include <TRandom.h>
#include <TCanvas.h>
#include <TStyle.h>
#include <TOFPET/P2.hpp>
#include <assert.h>
#include <math.h>
#include <boost/lexical_cast.hpp>
#include <sys/mman.h>
#include <Common/Constants.hpp>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <getopt.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/sysinfo.h>
#include <vector>
#include <boost/tuple/tuple.hpp>
#include <boost/random.hpp>
#include <boost/nondet_random.hpp>
#include <fcntl.h>
#include <sys/stat.h>

static const int MAX_N_ASIC = DAQ::Common::SYSTEM_NCHANNELS / 64;
static const float ErrorHistogramRange = 0.5;


struct TacInfo {
	TProfile *pA_Fine;
	TProfile *pA_ControlT;
	TH1* pA_ControlE;
	
	TProfile *pB_ControlT;
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
		pA_Fine = NULL;
		pA_ControlT = NULL;
		pA_ControlE = NULL;
		pB_ControlT = NULL;
		pB_ControlE = NULL;
		
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
	float x = fmod(1024.0 + xx[0] - pp[0], 2.0);	
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
	
	double tQ = 3 - fmod(1024.0 + tDelay - tEdge, 2.0);
	double tFine = m * (tQ - tB) + p2 * (tQ - tB) * (tQ - tB);
	return tFine;
}

static const int nPar3 = 4;
const char *paramNames3[nPar3] = { "x0", "b", "m", "p2" };
static Double_t periodicF3 (Double_t *xx, Double_t *pp)
{
	double x = fmod(1024.0 + xx[0] - pp[0], 2.0);	
	return  pp[1] + pp[2]*x + pp[3]*x*x;
}
static Double_t aperiodicF3 (double x, double x0, double b, double m, double p2)
{
	x = x - x0;
	return b + + m*x + p2*x*x;
}

struct Event {
	unsigned gid;
	unsigned short coarse;
	unsigned short fine;
	long long tacIdleTime;
	float stage;
};
static const int READ_BUFFER_SIZE = 32*1024*1024 / sizeof(Event);

void sortData(char *tDataFileName, char *eDataFileName, char *tableFileNamePrefix, int nAsicsPerFile);

int calibrate(
	int asicStart, int asicEnd,
	int linearityDataFile, int linearityNbins, float linearityRangeMinimum, float linearityRangeMaximum,
	int eakageDataFile, int leakageNbins, float leakageRangeMinimum, float leakageRangeMaximum, float *leakagePhasePoint,
	TacInfo *tacInfo, DAQ::TOFPET::P2 &myP2,
	float nominalM
);

void qualityControl(
	int asicStart, int asicEnd, 
	int linearityDataFile,
	int leakageDataFile, float *leakagePhasePoint,
	TacInfo *tacInfo, DAQ::TOFPET::P2 &myP2,
	char *plotFilePrefix
);

void displayHelp(char * program)
{
	fprintf(stderr, "usage: %s [--int-factor=INT_FACTOR] [--asics_per_file=ASICS_PER_FILE] t_branch_data_file e_branch_data_file"
			" tdc_calibration_prefix"
			"\n", program);
	fprintf(stderr, "\noptional arguments:\n");
	fprintf(stderr, "  --help \t\t\t\t Show this help message and exit \n");
	fprintf(stderr, "  --int-factor [=INT_FACTOR] \t\t Nominal TDC interpolation factor (Default is 128)\n");
	fprintf(stderr, "  --asics_per_file [=ASICS_PER_FILE] \t Number of asics to be stored per calibration file (Default is 2). If set to ALL, only one file will be created with calibration for all ASICS with valid data.\n");
	fprintf(stderr, "  --no-sorting \t\t\t Assumes the temporary data file have already been created and skips the sorting stage.\n");
	fprintf(stderr, "\npositional arguments:\n");
	fprintf(stderr, "  t_branch_data_file \t\t\t Data to be used for T branch calibration\n");
	fprintf(stderr, "  E_branch_data_file \t\t\t Data to be used for E branch calibration\n");
	fprintf(stderr, "  tdc_calibration_prefix \t\t Prefix for output calibration files\n");
};

void displayUsage( char * program)
{
	fprintf(stderr, "usage: %s [--int-factor=INT_FACTOR] [--asics_per_file=ASICS_PER_FILE] t_branch_data_file e_branch_data_file"
			" tdc_calibration_prefix"
			"\n", program);
};

int main(int argc, char *argv[])
{


	float nominalM = 128;
	int nAsicsPerFile=2;
	int readInd=0;
	int posInd=0;        
	bool keepTemporary = false;
        bool doSorting = true;

	// Choose the default number of workers based on CPU and RAM
	// Assume we need 4 GiB of system RAM per worker for good performance
	int nCPU = sysconf(_SC_NPROCESSORS_ONLN);
	struct sysinfo si;
	sysinfo(&si);
	int maxMemWorkers = si.totalram * si.mem_unit / (4LL * 1024*1024*1024);
	int maxWorkers = nCPU < maxMemWorkers ? nCPU : maxMemWorkers;
	
	static struct option longOptions[] = {
		{ "asics_per_file", required_argument, 0, 0 },
		{ "int-factor", required_argument, 0, 0 },
		{ "help", no_argument, 0, 0 },
		{ "no-sorting", no_argument, 0, 0 },
		{ "max-workers", required_argument, 0, 0 },
		{ "keep-temporary", no_argument, 0, 0 }
	};

	
	while (true) {
		int optionIndex = 0;
		int c=getopt_long(argc, argv, "",longOptions, &optionIndex);
		
		// Option argument
		if(c == -1) 
			break;
		
		else if(optionIndex==0 && strcmp (optarg,"ALL") != 0)
			nAsicsPerFile = atoi(optarg);
		
		else if(optionIndex==0 && strcmp (optarg,"ALL") == 0)
			nAsicsPerFile = MAX_N_ASIC;
		
		else if(optionIndex==1)
			nominalM = atof(optarg);
		
		else if(optionIndex==2) {
			displayHelp(argv[0]);
			return(1);
		}
		else if((optionIndex==0 || optionIndex==1) and optarg==NULL) {
			displayUsage(argv[0]);
			fprintf(stderr, "\n%s: error: must assign a proper value to optional argument!\n", argv[0]);
			return(1);	
		}	
		else if(optionIndex == 3) {
                    doSorting = false;
                }
                else if(optionIndex == 4) {
			maxWorkers = atoi(optarg);
		}
		else if(optionIndex == 5) {
			keepTemporary = true;
		}
		else {
			displayUsage(argv[0]);
			fprintf(stderr, "\n%s: error: Unknown option!\n", argv[0]);
			return(1);
		}

	}
	
	if(argc - optind < 3){
		displayUsage(argv[0]);
		fprintf(stderr, "\n%s: error: too few positional arguments!\n", argv[0]);
		return(1);
	}
	else if(argc - optind > 3){
		displayUsage(argv[0]);
		fprintf(stderr, "\n%s: error: too many positional arguments!\n", argv[0]);
		return(1);
	}
	char *tDataFileName = argv[optind+0];
	char *eDataFileName = argv[optind+1];
	char *tableFileNamePrefix = argv[optind+2];
	
	if(doSorting) {
		sortData(tDataFileName, eDataFileName, tableFileNamePrefix, nAsicsPerFile);
	}

	char fName[1024];
	sprintf(fName, "%s_ranges.tmp", tableFileNamePrefix);
	FILE *rangeFile = fopen(fName, "r");
	if(rangeFile == NULL) {
		int e = errno;
		fprintf(stderr, "Could not open '%s' for reading : %d %s\n", fName, e, strerror(errno));
		exit(1);
	}	
	
	int linearityNbins = 0;
	float linearityRangeMinimum, linearityRangeMaximum;
	int leakageNbins;
	float leakageRangeMinimum, leakageRangeMaximum;	
	
	assert(fscanf(rangeFile, "%d %f %f\n", &linearityNbins, &linearityRangeMinimum, &linearityRangeMaximum) == 3);
	assert(fscanf(rangeFile, "%d %f %f\n", &leakageNbins, &leakageRangeMinimum, &leakageRangeMaximum) == 3);
	
	fclose(rangeFile);
	
	
	sprintf(fName, "%s_list.tmp", tableFileNamePrefix);
	FILE *listFile = fopen(fName, "r");
	if(listFile == NULL) {
		int e = errno;
		fprintf(stderr, "Could not open '%s' for reading : %d %s\n", fName, e, strerror(errno));
		exit(1);
	}	
	
	
	std::vector<boost::tuple<int, int, int> > list;
	int fileID, asicStart, asicEnd;
	int asicMin = MAX_N_ASIC;
	int asicMax = 0;
	while(fscanf(listFile, "%d %d %d\n", &fileID, &asicStart, &asicEnd) == 3) {
		list.push_back(boost::tuple<int, int, int>(fileID, asicStart, asicEnd));
		asicMin = asicMin < asicStart ? asicMin : asicStart;
		asicMax = asicMax > asicEnd ? asicMax : asicEnd;
	}
	
	int nChannels = asicMax * 64;
	int nTAC = asicMax * 64 * 2 * 4;
	TacInfo *tacInfo = (TacInfo *)mmap(NULL, sizeof(TacInfo)*nTAC, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
	for(int tac = 0; tac < nTAC; tac++)
		tacInfo[tac] = TacInfo();
	
	
	

	maxWorkers = maxWorkers > 1 ? maxWorkers : 1;
	for(unsigned n = 0; n < list.size();) {
		int nWorkers = list.size() - n;
		nWorkers = nWorkers < maxWorkers ? nWorkers : maxWorkers;

		printf("Calibrating ASICs %4d to %4d (%d workers)...\n",
		       list[n].get<1>(), list[n + nWorkers - 1].get<2>() - 1,
		       nWorkers
		);

		pid_t children[nWorkers];
		for(int worker = 0; worker < nWorkers; worker++, n++) {
			pid_t pid = fork();
			if (pid == 0) {
				int fileID = list[n].get<0>();
				int asicStart = list[n].get<1>();
				int asicEnd = list[n].get<2>();

				sprintf(fName,"%s_%d_linearity.tmp", tableFileNamePrefix, fileID);
				int linearityDataFile = open(fName, O_RDONLY);
				if(linearityDataFile == -1) 
					exit(1);

				sprintf(fName,"%s_%d_leakage.tmp", tableFileNamePrefix, fileID);
				int leakageDataFile = open(fName, O_RDONLY);
				if(leakageDataFile == -1) 
					exit(1);
				
				posix_fadvise(linearityDataFile, 0, 0, POSIX_FADV_SEQUENTIAL);
				posix_fadvise(leakageDataFile, 0, 0, POSIX_FADV_SEQUENTIAL);
				
				float *leakagePhasePoint = new float[nTAC];
				sprintf(fName,"%s_%d_leakage_phase.tmp", tableFileNamePrefix, fileID);
				FILE *leakagePhaseFile = fopen(fName, "r");
				if(leakagePhaseFile == NULL) 
					exit(1);
				
				for(int i = asicStart * 64 * 4 * 2; i < asicEnd * 64 * 4 * 2; i++) {
					assert(fscanf(leakagePhaseFile, "%f\n", &leakagePhasePoint[i]) == 1);
				}

				DAQ::TOFPET::P2 *myP2 = new DAQ::TOFPET::P2((asicEnd - asicStart) * 64);
				
				sprintf(fName,"%s_asics%02d-%02d.tdc.root", tableFileNamePrefix, asicStart, asicEnd-1);
				
				char plotFilePrefix[1024];
				sprintf(plotFilePrefix, "%s_asics%02d-%02d", tableFileNamePrefix, asicStart, asicEnd-1);
				
				TFile * resumeFile = new TFile(fName, "RECREATE", "", 1);
				// Create a un-used canvas, just to avoid the fits automatically creating one and printing a warning
				TCanvas *blank = new TCanvas();
				
				int hasData = calibrate(
							asicStart, asicEnd,
							linearityDataFile, linearityNbins, linearityRangeMinimum, linearityRangeMaximum,
							leakageDataFile, leakageNbins, leakageRangeMinimum, leakageRangeMaximum, leakagePhasePoint,
							tacInfo, *myP2, nominalM
						);

				if(hasData == 0){
					resumeFile->Close();
					remove(fName);
					exit(0);
				}
				else{	
					qualityControl(
						asicStart, asicEnd, 
						linearityDataFile,
						leakageDataFile, leakagePhasePoint,
						tacInfo, *myP2,
						plotFilePrefix
					);
					resumeFile->Write();
					resumeFile->Close();
					
				}
				delete myP2;
				
				posix_fadvise(linearityDataFile, 0, 0, POSIX_FADV_DONTNEED);
				close(linearityDataFile);
				posix_fadvise(leakageDataFile, 0, 0, POSIX_FADV_DONTNEED);
				close(leakageDataFile);
				
				exit(0);
			} 
			else {
				children[worker] = pid;
				
			}			
		}
		for(int worker = 0; worker < nWorkers; worker++) {
 			waitpid(children[worker], NULL, 0);
		} 	
	}
	


	// Copy parameters from shared memory to global P2 
	DAQ::TOFPET::P2 myP2(nChannels);
	for(int asic = asicMin; asic < asicMax; asic++) {
		for(int channel = 0; channel < 64; channel++) {
			for(int tOrE = 0; tOrE < 2; tOrE++) {
				bool isT = (tOrE == 0);
				for(int tac = 0; tac < 4; tac++) {
					unsigned gid = ((64*asic + channel) << 3) | (tOrE << 2) | (tac & 0x3);
					TacInfo &ti = tacInfo[gid];
					if(ti.pA_Fine == NULL)	continue;
					
					myP2.setShapeParameters((64*asic+channel), tac, isT, ti.shape.tB, ti.shape.m, ti.shape.p2);
					myP2.setT0((64*asic+channel), tac, isT, ti.shape.tEdge);
					myP2.setLeakageParameters((64 * asic + channel), tac, isT, ti.leakage.tQ, ti.leakage.a0, ti.leakage.a1, ti.leakage.a2);
				}
			}
		}
	}

	printf("Final adjustments\n");
	for(int asic = asicMin; asic < asicMax; asic++) {
		for(int channel = 0; channel < 64; channel++) {			
		
			float t0_adjust_sum = 0;
			int t0_adjust_N = 0;	
			for(int tac = 0; tac < 4; tac++) {
				unsigned gidT = ((64*asic + channel) << 3) | (0 << 2) | (tac & 0x3);
				unsigned gidE = ((64*asic + channel) << 3) | (1 << 2) | (tac & 0x3);
				TacInfo &tiT = tacInfo[gidT];
				TacInfo &tiE = tacInfo[gidE];
				
				if(tiT.pA_Fine == NULL || tiE.pA_Fine == NULL) 
					continue;
			
				float t0_T = myP2.getT0((64 * asic + channel), tac, true);
				float t0_E = myP2.getT0((64 * asic + channel), tac, false);
				
				t0_adjust_sum += (t0_T - t0_E);
				t0_adjust_N += 1;				
			}
			
			float t0_adjust = t0_adjust_sum / t0_adjust_N;
			for(int tac = 0; tac < 4; tac++) {
				unsigned gidT = ((64*asic + channel) << 3) | (0 << 2) | (tac & 0x3);
				unsigned gidE = ((64*asic + channel) << 3) | (1 << 2) | (tac & 0x3);
				TacInfo &tiT = tacInfo[gidT];
				TacInfo &tiE = tacInfo[gidE];
				
				if(tiT.pA_Fine == NULL || tiE.pA_Fine == NULL) 
					continue;
			
				float t0_T = myP2.getT0((64 * asic + channel), tac, true);
				float t0_T_adjusted = t0_T - t0_adjust;
				
				//fprintf(stderr, "%d %2d %d adjusting %f to %f\n", asic, channel, tac, t0_T, t0_T_adjusted);
				
				myP2.setT0((64 * asic + channel), tac, true, t0_T_adjusted);
			}
			
		}
	}

	printf("Saving calibration tables\n");
	for(unsigned n = asicMin/nAsicsPerFile; n <  asicMax/nAsicsPerFile; n++) {

		char tableFileName[1024];
		int asicStart = n * nAsicsPerFile;
		int asicEnd = asicStart + nAsicsPerFile;
		sprintf(tableFileName, "%s_asics%02u-%02u.tdc.cal", tableFileNamePrefix, asicStart, asicEnd-1);
		myP2.storeFile(  64*nAsicsPerFile*n, 64*nAsicsPerFile*(n+1), tableFileName);
	}
	
	if(!keepTemporary) {
		// Remove temporary files
		for(unsigned n = 0; n < list.size(); n++) {
			int fileID = list[n].get<0>();
			sprintf(fName,"%s_%d_linearity.tmp", tableFileNamePrefix, fileID);
			unlink(fName);
			sprintf(fName,"%s_%d_leakage.tmp", tableFileNamePrefix, fileID);
			unlink(fName);
			sprintf(fName, "%s_%d_leakage_phase.tmp", tableFileNamePrefix, n);
			unlink(fName);
		}
		
		sprintf(fName, "%s_list.tmp", tableFileNamePrefix);
		unlink(fName);

		sprintf(fName, "%s_ranges.tmp", tableFileNamePrefix);
		unlink(fName);
	}
	
	return 0;
}

int calibrate(	int asicStart, int asicEnd,
		int linearityDataFile, int linearityNbins, float linearityRangeMinimum, float linearityRangeMaximum,
		int leakageDataFile, int leakageNbins, float leakageRangeMinimum, float leakageRangeMaximum, float *leakagePhasePoint,
		TacInfo *tacInfo, DAQ::TOFPET::P2 &myP2,
		float nominalM
)
{
	unsigned nASIC = asicEnd - asicStart;
	unsigned gidStart = asicStart * 64 * 2 * 4;
	unsigned gidEnd = asicEnd * 64 * 4 * 2;
	unsigned nTAC = gidEnd - gidStart;
	unsigned channelStart = asicStart * 64;

	// Build the histograms
	TH2S **hA_Fine2List = new TH2S *[nTAC];
	TProfile **pB_FineList = new TProfile *[nTAC];
	for(int n = 0; n < nTAC; n++) {
		hA_Fine2List[n] = NULL;
		pB_FineList[n] = NULL;
	}

	bool asicPresent[nASIC];
	for(int n = 0; n < nASIC; n++)
		asicPresent[n] = false;

	for(unsigned gid = gidStart; gid < gidEnd; gid++) {
		unsigned tac = gid & 0x3;
		bool isT = ((gid >> 2) & 0x1) == 0;
		unsigned channel = (gid >> 3) & 63;
		unsigned asic = gid >> 9;
		char hName[128];
		
		sprintf(hName, isT ? "C%03d_%02d_%d_A_T_hFine2" : "C%03d_%02d_%d_A_E_hFine2",
			asic, channel, tac);
		hA_Fine2List[gid-gidStart] = new TH2S(hName, hName, linearityNbins, linearityRangeMinimum, linearityRangeMaximum, 1024, 0, 1024);

		sprintf(hName, isT ? "C%03d_%02d_%d_B_T_pFine_X" : "C%03d_%02d_%d_B_E_pFine_X",
			asic, channel, tac);
		pB_FineList[gid-gidStart] = new TProfile(hName, hName, leakageNbins, leakageRangeMinimum, leakageRangeMaximum);
	}
	struct timespec t0;
	clock_gettime(CLOCK_REALTIME, &t0);

	Event *eventBuffer = new Event[READ_BUFFER_SIZE];
	int r;
	lseek(linearityDataFile, 0, SEEK_SET);
	while((r = read(linearityDataFile, eventBuffer, sizeof(Event)*READ_BUFFER_SIZE)) > 0) {
		int nEvents = r / sizeof(Event);
		for(int i = 0; i < nEvents; i++) {
			Event &event = eventBuffer[i];
			assert(hA_Fine2List[event.gid-gidStart] != NULL);
			if(event.fine < 0.5 * nominalM || event.fine > 4 * nominalM) continue;
			hA_Fine2List[event.gid-gidStart]->Fill(event.stage, event.fine);
			unsigned asic = event.gid >> 9;
			asicPresent[asic-asicStart] = true;
		}
	}
	
	lseek(leakageDataFile, 0, SEEK_SET);
	while((r = read(leakageDataFile, eventBuffer, sizeof(Event)*READ_BUFFER_SIZE)) > 0) {
		int nEvents = r / sizeof(Event);
		for(int i = 0; i < nEvents; i++) {
			Event &event = eventBuffer[i];
			assert(event.gid >= gidStart);
			assert(event.gid < gidEnd);
			assert(pB_FineList[event.gid-gidStart] != NULL);
			if(event.fine < 0.5 * nominalM || event.fine > 4 * nominalM) continue;
			pB_FineList[event.gid-gidStart]->Fill(event.stage, event.fine);
			unsigned asic = event.gid >> 9;
			asicPresent[asic-asicStart] = true;
		}
	}
	delete [] eventBuffer;
	struct timespec t1;
	clock_gettime(CLOCK_REALTIME, &t1);
	
	boost::mt19937 generator;

	int hasData = 0;
	for(unsigned gid = gidStart; gid < gidEnd; gid++) {
		unsigned tac = gid & 0x3;
		bool isT = ((gid >> 2) & 0x1) == 0;
		unsigned channel = (gid >> 3) & 63;
		unsigned asic = gid >> 9;
		TacInfo &ti = tacInfo[gid];
		//if(asic != 0 || channel != 8) continue;

		
		// We had _no_ data for this ASIC, we assume it's not present in the system
		// Let's just move on without further ado
		if(!asicPresent[asic-asicStart])
			continue;
		
		// Code below came from two loops
		// and have conflitcting variable names
		{
		
			TH2S *hA_Fine2 = hA_Fine2List[gid-gidStart];
			if(hA_Fine2 == NULL) continue;
			if(hA_Fine2->GetEntries() < 1000) {
				fprintf(stderr, "WARNING: Not enough data to calibrate. Skipping TAC. (A: %4d %2d %d %c)\n",
					asic, channel, tac, isT  ? 'T' : 'E'
				);
				continue;
			}
			hasData=1;
			char hName[128];
		
			// Obtain a rough estimate of the TDC range
			sprintf(hName, isT ? "C%03d_%02d_%d_A_T_hFine" : "C%03d_%02d_%d_A_E_hFine",
				asic, channel, tac);
			TH1D *hA_Fine = hA_Fine2->ProjectionY(hName);
			hA_Fine->Rebin(8);
			hA_Fine->Smooth(4);
			float adcMean = hA_Fine->GetMean();
			int adcMeanBin = hA_Fine->FindBin(adcMean);
			int adcMeanCount = hA_Fine->GetBinContent(adcMeanBin);
			
			int adcMinBin = adcMeanBin;
			while(hA_Fine->GetBinContent(adcMinBin) > 0.20 * adcMeanCount)
				adcMinBin--;
			
			int adcMaxBin = adcMeanBin;
			while(hA_Fine->GetBinContent(adcMaxBin) > 0.20 * adcMeanCount)
				adcMaxBin++;
			
			float adcMin = hA_Fine->GetBinCenter(adcMinBin);
			float adcMax = hA_Fine->GetBinCenter(adcMaxBin);
			
			
			// Set limits on ADC range to exclude spurious things.
			hA_Fine2->GetYaxis()->SetRangeUser(
				adcMin - 32 > 0.5 * nominalM ? adcMin - 32 : 0.5 * nominalM,
				adcMax + 32 < 4.0 * nominalM ? adcMax + 32 : 4.0 * nominalM
				);
				
			sprintf(hName, isT ? "C%03d_%02d_%d_A_T_pFine_X" : "C%03d_%02d_%d_A_E_pFine_X",
				asic, channel, tac);
			TProfile *pA_Fine = ti.pA_Fine = hA_Fine2->ProfileX(hName, 1, -1, "s");
			
			
			int nBinsX = pA_Fine->GetXaxis()->GetNbins();
			float xMin = pA_Fine->GetXaxis()->GetXmin();
			float xMax = pA_Fine->GetXaxis()->GetXmax();
			
			
			// Obtain a rough estimate of the edge position
			float tEdge = 0;
			float lowerT0 = 0;
			float upperT0 = 0;
			
			adcMin = 1024.0;
			for(int n = 5; n >= 1; n--) {
				for(int j = 1; j < (nBinsX - 1 - n); j++) {
					float v1 = pA_Fine->GetBinContent(j);
					float v2 = pA_Fine->GetBinContent(j+n);
					float e1 =  pA_Fine->GetBinError(j);
					float e2 =  pA_Fine->GetBinError(j+n);
					int c1 = pA_Fine->GetBinEntries(j);
					int c2 = pA_Fine->GetBinEntries(j+n);
					float t1 = pA_Fine->GetBinCenter(j);
					float t2 = pA_Fine->GetBinCenter(j+n);
					
					if(c1 == 0 || c2 == 0) continue;
					if(e1 > 5.0 || e2 > 5.0) continue;

					float slope = (v2 - v1)/(t2 - t1);
					// Slope is usually -nominalM
					// But at edge, it's 10 x nominalM
					if(slope > 5 * nominalM) {
						tEdge = (t2 + t1)/2.0;
						lowerT0 = t1;// - 0.5 * pA_Fine->GetXaxis()->GetBinWidth(0);
						upperT0 = t2;// + 0.5 * pA_Fine->GetXaxis()->GetBinWidth(0);
						adcMin = fminf (adcMin, pA_Fine->GetBinContent(j));
					}
				}
			}
			while(lowerT0 > 2.0 && tEdge > 2.0 && upperT0 > 2.0) {
				lowerT0 -= 2.0;
				tEdge -= 2.0;
				upperT0 -= 2.0;
			}
			
			float tEdgeTolerance = upperT0 - lowerT0;
			// Fit a line to a TDC period to determine the interpolation factor
			pA_Fine->Fit("pol1", "Q", "", tEdge + tEdgeTolerance, tEdge + 2.0  - tEdgeTolerance);
			TF1 *fPol = pA_Fine->GetFunction("pol1");
			if(fPol == NULL) {
				fprintf(stderr, "WARNING: Could not make a linear fit. Skipping TAC. (A: %4d %2d %d %c)\n",
					asic, channel, tac, isT  ? 'T' : 'E'
				);
				continue;
				
			}
			float estimatedM = - fPol->GetParameter(1);
			
			
			boost::uniform_real<> range(lowerT0, upperT0);
			boost::variate_generator<boost::mt19937&, boost::uniform_real<> > nextRandomTEdge(generator, range);
		
			TF1 *pf = new TF1("periodicF1", periodicF1, xMin, xMax, nPar1);
			for(int p = 0; p < nPar1; p++) pf->SetParName(p, paramNames1[p]);
			pf->SetNpx(2 * nBinsX);
			
			float b;
			float m;
			float tB;
			float p2;
			float currChi2 = INFINITY;
			float prevChi2 = INFINITY;
			int nTry = 0;
			float maxChi2 = 2E6;
			do {
				pf->SetParameter(0, tEdge);		pf->SetParLimits(0, lowerT0, upperT0);
				pf->SetParameter(1, adcMin);		pf->SetParLimits(1, adcMin - estimatedM * tEdgeTolerance, adcMin);
				pf->SetParameter(2, estimatedM);	pf->SetParLimits(2, 0.99 * estimatedM, 1.01 * estimatedM),
				pA_Fine->Fit("periodicF1", "Q", "", 0.5, xMax);
				
				TF1 *pf_ = pA_Fine->GetFunction("periodicF1");
				if(pf_ != NULL) {
					prevChi2 = currChi2;
					currChi2 = pf_->GetChisquare() / pf_->GetNDF();	
					
					if((currChi2 < prevChi2)) {
						tEdge = pf->GetParameter(0);
						b  = pf->GetParameter(1);
						m  = pf->GetParameter(2);		
						tB = - (b/m - 1.0);
						p2 = 0;
					}
				}
				else {
					//	tEdge = nextRandomTEdge();
				}
				nTry += 1;
				
			} while((currChi2 <= 0.95*prevChi2) && (nTry < 10));
			
			if(prevChi2 > maxChi2 && currChi2 > maxChi2) {
				fprintf(stderr, "WARNING: NO FIT OR VERY BAD FIT (1). Skipping TAC. (A: %4d %2d %d %c)\n",
					asic, channel, tac, isT  ? 'T' : 'E'
				);
				delete pf;
				continue;
			}
			
			TF1 *pf2 = new TF1("periodicF2", periodicF2, xMin, xMax,  nPar2);		
			pf2->SetNpx(2*nBinsX);
			for(int p = 0; p < nPar2; p++) pf2->SetParName(p, paramNames2[p]);
			
			currChi2 = INFINITY;
			prevChi2 = INFINITY;
			nTry = 0;
			do {
				//pf2->SetParameter(0, tEdge);		pf2->SetParLimits(0, tEdge-0.1, tEdge+0.1);
				pf2->FixParameter(0, tEdge);
				pf2->SetParameter(1, tB);		pf2->SetParLimits(1, 1.10*tB, 0);
				//pf2->FixParameter(1, tB);
				pf2->SetParameter(2, m);		pf2->SetParLimits(2, 1.00 * m, 1.15 * m);
				pf2->SetParameter(3, -1.0);		pf2->SetParLimits(3, -5.0, 0);
				pA_Fine->Fit("periodicF2", "Q", "", 0.5, xMax);

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
			
			if(prevChi2 > maxChi2 && currChi2 > maxChi2) {
				fprintf(stderr, "WARNING: NO FIT OR VERY BAD FIT (2). Skipping TAC. (A: %4d %2d %d %c)\n",
					asic, channel, tac, isT  ? 'T' : 'E'
				);
				delete pf;
				delete pf2;
				continue;
			}
				
			ti.shape.tEdge = tEdge = pf2->GetParameter(0);
			ti.shape.tB = tB = pf2->GetParameter(1);
			ti.shape.m  = m  = pf2->GetParameter(2);
			ti.shape.p2 = p2 = pf2->GetParameter(3);
			
		
			myP2.setShapeParameters((64*asic+channel)-channelStart, tac, isT, tB, m, p2);
			myP2.setT0((64*asic+channel)-channelStart, tac, isT, tEdge);

			// Allocate the control histograms
			sprintf(hName, isT ? "C%03d_%02d_%d_A_T_control_T" : "C%03d_%02d_%d_A_E_control_T", 
					asic, channel, tac);
			ti.pA_ControlT = new TProfile(hName, hName, linearityNbins, linearityRangeMinimum, linearityRangeMaximum, "s");

			sprintf(hName, isT ? "C%03d_%02d_%d_A_T_control_E" : "C%03d_%02d_%d_A_E_control_E", 
					asic, channel, tac);
			ti.pA_ControlE = new TH1F(hName, hName, 512, -ErrorHistogramRange, ErrorHistogramRange);
			
			delete pf;
			delete pf2;
		}
		
		{
			char hName[128];
				
			float x = leakagePhasePoint[gid];
			float tQ = 3 - fmod(1024.0 + x - ti.shape.tEdge, 2.0);
			
			TProfile *pB_Fine = pB_FineList[gid-gidStart];
			assert(pB_Fine != NULL);
			if(pB_Fine->GetEntries() < 200) {
				continue;
			}
				
			int nBinsX = pB_Fine->GetXaxis()->GetNbins();
			float xMin = pB_Fine->GetXaxis()->GetXmin() + pB_Fine->GetXaxis()->GetBinWidth(1);
			float xMax = pB_Fine->GetXaxis()->GetXmax();
			
			TF1 *pl1 = new TF1("pl1", "[0]+[1]*x", xMin, xMax);		
			pl1->SetParameter(0, 200);
			pl1->SetParameter(1, 0.01);
			pl1->SetParLimits(0, 0, 1024);
			pl1->SetParLimits(1, -1.0, 1.0);
			pl1->SetNpx(nBinsX);
			
			int nTry = 0;
			while(nTry < 10){
				pB_Fine->Fit("pl1", "Q", "", xMin, xMax);
				TF1 *fit = pB_Fine->GetFunction("pl1");
				if(fit == NULL) break;
				float chi2 = fit->GetChisquare();
				float ndf = fit->GetNDF();
				if(chi2/ndf < 2.0) break;
				nTry += 1;
			}
			
			
			TF1 *fit = pB_Fine->GetFunction("pl1");
			if(fit == NULL) {
				fprintf(stderr, "WARNING: NO FIT! Skipping TAC. (B: %4d %2d %d %c)\n",
					asic, channel, tac, isT  ? 'T' : 'E'
				);
				continue;
			}
			float chi2 = fit->GetChisquare();
			float ndf = fit->GetNDF();
			ti.leakage.tQ = tQ;
			ti.leakage.a0 = fit->GetParameter(0);
			ti.leakage.a1 = fit->GetParameter(1) / (1024 * 4);
			ti.leakage.a2 = 0;//fit->GetParameter(2) / ((1024 * 4)*(1024*4));
			
			myP2.setLeakageParameters((64 * asic + channel)-channelStart, tac, isT, ti.leakage.tQ, ti.leakage.a0, ti.leakage.a1, ti.leakage.a2);
			

			sprintf(hName, isT ? "C%03d_%02d_%d_B_T_control_T" : "C%03d_%02d_%d_B_E_control_T", 
					asic, channel, tac);
			ti.pB_ControlT = new TProfile(hName, hName, leakageNbins, leakageRangeMinimum, leakageRangeMaximum, "s");
		
			sprintf(hName, isT ? "C%03d_%02d_%d_B_T_control_E" : "C%03d_%02d_%d_B_E_control_E", 
					asic, channel, tac);
			ti.pB_ControlE = new TH1F(hName, hName, 256, -ErrorHistogramRange, ErrorHistogramRange);
		}
	
	}
	return hasData;

}

void qualityControl(
	int asicStart, int asicEnd, 
	int linearityDataFile,
	int leakageDataFile, float *leakagePhasePoint,
	TacInfo *tacInfo, DAQ::TOFPET::P2 &myP2,
	char *plotFilePrefix
)
{
	Event *eventBuffer = new Event[READ_BUFFER_SIZE];
	// Correct t0 and build shape quality control histograms
	const int nIterations = 3;

	unsigned gidStart = asicStart * 64 * 2 * 4;
	unsigned gidEnd = asicEnd * 64 * 4 * 2;
	unsigned nTAC = gidEnd - gidStart;
	unsigned channelStart = asicStart * 64;
	
	unsigned nChannels = (asicEnd - asicStart) * 64;
	unsigned nBranches = 2*nChannels;
	unsigned bidStart = asicStart*64;

	
	TH1F *hBranchErrorA[nBranches];
	TH1F *hBranchErrorB[nBranches];
	for(int bid = 0; bid < nBranches; bid++) {
		bool tOrE = (bid & 0x1) == 0;
		unsigned channel = (bid >> 1) & 63;
		unsigned asic = bid >> 7;

		char hName[128];

		sprintf(hName, "C%03u_%02u_A_%c_control_E", asic+asicStart, channel, tOrE ? 'T' : 'E');
		hBranchErrorA[bid] = new TH1F(hName, hName, 256, -ErrorHistogramRange, ErrorHistogramRange);

		sprintf(hName, "C%03u_%02u_B_%c_control_E", asic+asicStart, channel, tOrE ? 'T' : 'E');
		hBranchErrorB[bid] = new TH1F(hName, hName, 256, -ErrorHistogramRange, ErrorHistogramRange);
		
	}

	// Need two iterations to correct t0, then one more to build final histograms
	for(int iter = 0; iter <= nIterations; iter++) {
		for(int gid = gidStart; gid < gidEnd; gid++) {
			if(tacInfo[gid].pA_ControlT == NULL) continue;
			assert(tacInfo[gid].pA_ControlE != NULL);
			tacInfo[gid].pA_ControlT->Reset();
			tacInfo[gid].pA_ControlE->Reset();
		}

		for(int bid = 0; bid < nBranches; bid++) {
			hBranchErrorA[bid]->Reset();
		}
	
		lseek(linearityDataFile, 0, SEEK_SET);
		int r;
		while((r = read(linearityDataFile, eventBuffer, sizeof(Event)*READ_BUFFER_SIZE)) > 0) {
			int nEvents = r / sizeof(Event);
			for (int i = 0; i < nEvents; i++) {
				Event &event = eventBuffer[i];
				assert(event.gid >= gidStart);
				assert(event.gid < gidEnd);

				TacInfo &ti = tacInfo[event.gid];
				
				unsigned tac = event.gid & 0x3;
				bool isT = ((event.gid >> 2) & 0x1) == 0;
				unsigned channel = (event.gid >> 3);
				
				unsigned bid = ((channel - bidStart) << 1) | (isT ? 0 : 1);

				if (ti.pA_ControlT == NULL) continue;
				assert(ti.pA_ControlE != NULL);

				float t = event.stage;
				float t_ = fmod(1024.0 + t - ti.shape.tEdge, 2.0);
				float adcEstimate = aperiodicF2(t_, 0, ti.shape.tB, ti.shape.m, ti.shape.p2);
				float adcError = event.fine - adcEstimate;

				float tEstimate = myP2.getT(channel-channelStart, tac, isT, event.fine, event.coarse, event.tacIdleTime, 0);
				float qEstimate = myP2.getQ(channel-channelStart, tac, isT, event.fine, event.tacIdleTime,0);
				bool isNormal = myP2.isNormal(channel-channelStart, tac, isT, event.fine, event.coarse, event.tacIdleTime, 0);
				float tError = tEstimate - event.stage;

				if(!isNormal) continue;
				ti.pA_ControlT->Fill(event.stage, tError);
				// Don't fill if out of histogram's range
				if(fabs(tError) < ErrorHistogramRange) {
					ti.pA_ControlE->Fill(tError);
					hBranchErrorA[bid]->Fill(tError);
				}
			}
		}
		
		if(iter >= nIterations) continue;

		for(unsigned gid = gidStart; gid < gidEnd; gid++) {
			unsigned tac = gid & 0x3;
			bool isT = ((gid >> 2) & 0x1) == 0;
			unsigned channel = gid >> 3;

			TacInfo &ti = tacInfo[gid];
			if(ti.pA_ControlT == NULL) continue;
			assert(ti.pA_ControlE != NULL);
			
			float offset = ti.pA_ControlT->GetMean(2);
			
			// Try and get a better offset by fitting the error histogram
			int nEntries = ti.pA_ControlE->GetEntries();
			if (nEntries > 1000) {
				TF1 *f = NULL;
				ti.pA_ControlE->Fit("gaus", "Q");
				f = ti.pA_ControlE->GetFunction("gaus");
				if (f != NULL) {
					offset = f->GetParameter(1);
				}
			}
	
			float t0 = myP2.getT0(channel-channelStart, tac, isT);
			float newt0 = t0 - offset;
			myP2.setT0(channel-channelStart, tac, isT, newt0);
		}
	}
	
	// Build leakage quality control
	// Some channels have a large offset during this
	// We will cut remove it, just for the purpose of making the error histogram
	float *localT0 = new float[nTAC];
	for(unsigned i = 0; i < nTAC; i++) localT0[i] = 0.0;
	for(int iter = 0; iter < 2; iter++) {
		
		
		int r;
		lseek(leakageDataFile, 0, SEEK_SET);
		while((r = read(leakageDataFile, eventBuffer, sizeof(Event)*READ_BUFFER_SIZE)) > 0) {
			int nEvents = r / sizeof(Event);
			for(int i = 0; i < nEvents; i++) {
				Event &event = eventBuffer[i];
				assert(event.gid >= gidStart);
				assert(event.gid < gidEnd);
		
				unsigned tac = event.gid & 0x3;
				bool isT = ((event.gid >> 2) & 0x1) == 0;
				unsigned channel = (event.gid >> 3);
				
				unsigned bid = ((channel - bidStart) << 1) | (isT ? 0 : 1);
				
				TacInfo &ti = tacInfo[event.gid];
				if (ti.pB_ControlT == NULL) continue;
				assert(ti.pB_ControlE != NULL);
				
				float adcEstimate = ti.leakage.a0 + ti.leakage.a1 * event.tacIdleTime + ti.leakage.a2 * event.tacIdleTime * event.tacIdleTime;
				float adcError = event.fine - adcEstimate;
		
				float tEstimate = myP2.getT(channel-channelStart, tac, isT, event.fine, event.coarse, event.tacIdleTime, 0);
				float qEstimate = myP2.getQ(channel-channelStart, tac, isT, event.fine, event.tacIdleTime,0);
				bool isNormal = myP2.isNormal(channel-channelStart, tac, isT, event.fine, event.coarse, event.tacIdleTime,0);
				float tError = tEstimate - leakagePhasePoint[event.gid];
				tError += localT0[event.gid - gidStart];

				if(!isNormal) continue;
				ti.pB_ControlT->Fill(event.stage, tError);
				if(fabs(tError) < ErrorHistogramRange) {
					ti.pB_ControlE->Fill(tError);
					hBranchErrorB[bid]->Fill(tError);
				}
			}
		}
		
		for(int gid = gidStart; gid < gidEnd; gid++) {
			TacInfo &ti = tacInfo[gid];
			if (ti.pB_ControlT == NULL) continue;
			assert(ti.pB_ControlE != NULL);
			
			float offset = ti.pB_ControlT->GetMean(2);
			int nEntries = ti.pB_ControlE->GetEntries();
			if (nEntries > 1000) {
				TF1 *f = NULL;
				ti.pB_ControlE->Fit("gaus", "Q");
				f = ti.pB_ControlE->GetFunction("gaus");
				if (f != NULL) {
					offset = f->GetParameter(1);
				}
			}
			localT0[gid - gidStart] -= offset;
		}
	}
	delete [] localT0;
	
	
	TCanvas *c = new TCanvas();
	for(unsigned aOrB = 0; aOrB < 2; aOrB++) {
		TH1F **hBranchError = (aOrB == 0) ? hBranchErrorA : hBranchErrorB;

		for(unsigned tOrE = 0; tOrE < 2; tOrE++) {
			TH1F *hCounts, *hResolution;
			const char *modeString;
			
			unsigned s = aOrB << 1 | tOrE;
			switch(aOrB << 1 | tOrE) {
				case 0	:	modeString = "11";
						hCounts = new TH1F("hCounts_T_A", "Counts", nChannels, 0, nChannels);
						hResolution = new TH1F("hError_T_A", "TDC resolution histogram", 1000, 0, 500E-12);
						break;
				case 1	:	modeString = "21";
						hCounts = new TH1F("hCounts_E_A", "Counts", nChannels, 0, nChannels);
						hResolution = new TH1F("hError_E_A", "TDC resolution histogram", 1000, 0, 500E-12);
						break;
				case 2	:	modeString = "12";
						hCounts = new TH1F("hCounts_T_B", "Counts", nChannels, 0, nChannels);
						hResolution = new TH1F("hError_T_B", "TDC resolution histogram", 1000, 0, 500E-12);
						break;
				default	:	modeString = "22";
						hCounts = new TH1F("hCounts_E_B", "Counts", nChannels, 0, nChannels);
						hResolution = new TH1F("hError_E_B", "TDC resolution histogram", 1000, 0, 500E-12);
						break;
			}
			TGraphErrors *gResolution = new TGraphErrors(nChannels);
			
			
			int nPoints = 0;
			for(unsigned channel = 0; channel < nChannels; channel++) {
				TH1F *hError = hBranchError[channel << 1 | tOrE ];
				
				hCounts->Fill(channel, hError->GetEntries());
				if(hError->GetEntries() < 1000) continue;
				hError->Fit("gaus", "Q");
				TF1 *fit = hError->GetFunction("gaus");
				if(fit == NULL) continue;
				float sigma = fit->GetParameter(2) * DAQ::Common::SYSTEM_PERIOD;
				float sigmaError = fit->GetParError(2) * DAQ::Common::SYSTEM_PERIOD;
				gResolution->SetPoint(nPoints, channel, sigma);
				gResolution->SetPointError(nPoints, 0, sigmaError);
				hResolution->Fill(sigma);
				
				nPoints += 1;
			}
			gResolution->Set(nPoints);
			
			char plotFileName[1024];
			sprintf(plotFileName, "%s_%s.pdf", plotFilePrefix, modeString);
			c->Clear();
			c->Divide(2,2);
			
			c->cd(1);
			gResolution->Draw("AP");
			gResolution->SetTitle("TDC resolution");
			gResolution->GetXaxis()->SetTitle("Channel");
			gResolution->GetXaxis()->SetRangeUser(0, nChannels);
			gResolution->GetYaxis()->SetTitle("Resolution (s RMS)");
			gResolution->GetYaxis()->SetRangeUser(0, 500E-12);
			gResolution->Draw("AP");
			
			c->cd(2);
			hResolution->SetTitle("TDC resolution histogram");
			hResolution->GetXaxis()->SetTitle("TDC resolution (s RMS)");
			hResolution->Draw("HIST");
			
			c->cd(3);
			hCounts->GetYaxis()->SetRangeUser(0, hCounts->GetMaximum() * 1.10);
			hCounts->GetXaxis()->SetTitle("Channel");
			hCounts->Draw("HIST");	

			sprintf(plotFileName, "%s_%s.pdf", plotFilePrefix, modeString);
			c->SaveAs(plotFileName);
			sprintf(plotFileName, "%s_%s.png", plotFilePrefix, modeString);
			c->SaveAs(plotFileName);
			
		}
	}

	delete [] eventBuffer;
}

void sortData(char *tDataFileName, char *eDataFileName, char *tableFileNamePrefix, int nAsicsPerFile)
{
	printf("Sorting data into temporary files...\n");
	
	int nOutputFiles = int(ceil(float(MAX_N_ASIC)/nAsicsPerFile));
	int nTAC = MAX_N_ASIC * 64 * 2 * 4;

	char **linearityDataFilesNames = new char *[nOutputFiles];
	char **leakageDataFilesNames = new char *[nOutputFiles];
	FILE **linearityDataFiles = new FILE *[nOutputFiles];
	FILE **leakageDataFiles = new FILE *[nOutputFiles];
	for(int n = 0; n < nOutputFiles; n++)
	{
		linearityDataFilesNames[n] = NULL;
		leakageDataFilesNames[n] = NULL;
		
		linearityDataFiles[n] = NULL;
		leakageDataFiles[n] = NULL;
	}
	

	// Histogram parameters which we need to get from the histograms in the ROOT files
	int linearityNbins = 0;
	float linearityRangeMinimum = INFINITY;
	float linearityRangeMaximum = -INFINITY;
	
	int leakageNbins = 0;
	float leakageRangeMinimum = INFINITY;
	float leakageRangeMaximum = -INFINITY;
	
	// We also need to get the phase point for TAC leakage from the data
	float *leakagePhasePoint = new float[nTAC];
	for(int n = 0; n < nTAC; n++) {
		leakagePhasePoint[n] = 0;
	}
	
	int asicEnd = 0;
	for(int tOrE = 0; tOrE < 2; tOrE++) {
		bool isT = (tOrE == 0);

		TFile * hFile = new TFile(isT ? tDataFileName : eDataFileName, "READ");

		for(int linearityOrLeakage = 0; linearityOrLeakage < 2; linearityOrLeakage++) {
			bool isLinearity = (linearityOrLeakage == 0);
			printf("Stage: %d of %d %s branch, %s\n",
				2*tOrE + linearityOrLeakage + 1, 4,
				isT ? "T" : "E", isLinearity ? "linearity" : "leakage");
			
			TTree *data = (TTree *)hFile->Get(isLinearity ? "data3" : "data3B");

			Float_t fStep1;		data->SetBranchAddress("step1", &fStep1);
			Float_t fStep2;		data->SetBranchAddress("step2", &fStep2);
			Int_t asic;		data->SetBranchAddress("asic", &asic);
			Int_t channel;		data->SetBranchAddress("channel", &channel);
			Int_t tac;		data->SetBranchAddress("tac", &tac);
			Int_t tCoarse;		data->SetBranchAddress("tcoarse", &tCoarse);
			Int_t eCoarse;		data->SetBranchAddress("ecoarse", &eCoarse);
			Int_t tFine;		data->SetBranchAddress("tfine", &tFine);
			Int_t eFine;		data->SetBranchAddress("efine", &eFine);
			Long64_t tacIdleTime;	data->SetBranchAddress("tacIdleTime", &tacIdleTime);
			int nEvents = data->GetEntries();

			for(int i = 0; i < nEvents; i+=1) {
				if(i % 10000 == 0) {
					float fraction = float(i) / nEvents * 100;
					printf("%8d of %8d (%5.1f%%) done\r", i, nEvents, fraction); fflush(stdout);
				}
				data->GetEntry(i);
				
				if(isT && (tCoarse == 0) && (eCoarse == 0)) continue; // Patch for bad events
				int fCoarse = isT ? tCoarse : eCoarse;
				int fFine = isT ? tFine : eFine;
				
				unsigned gid = ((64*asic + channel) << 3) | (tOrE << 2) | (tac & 0x3);
				
				Event event;
				event.gid = gid;
				event.coarse = fCoarse;
				event.fine = fFine;
				event.tacIdleTime = tacIdleTime;
				event.stage = isLinearity ? fStep2 : fStep1;

				asicEnd = asicEnd > asic ? asicEnd : asic;
				
				if(!isLinearity) leakagePhasePoint[gid] = fStep2;
	
				int fileID = asic / nAsicsPerFile;

				FILE **ff = isLinearity ? linearityDataFiles : leakageDataFiles;
				char **nn = isLinearity ? linearityDataFilesNames : leakageDataFilesNames;
				FILE *f = ff[fileID];
				if(f == NULL) {

					char *fName = new char[1024];
					sprintf(fName, "%s_%d_%s.tmp", tableFileNamePrefix, fileID, isLinearity ? "linearity" : "leakage");
					f = fopen(fName, "wb");
					if(f == NULL) {
						int e = errno;
						fprintf(stderr, "Could not open '%s' for writing : %d %s\n", fName, e, strerror(e));
						exit(1);
					}
					
					nn[fileID] = fName;
					ff[fileID] = f;
				}
				int r = fwrite(&event, sizeof(event), 1, f);
				if( r != 1) {
					int e = errno;
					fprintf(stderr, "Error writing to '%s' : %d %s\n", 
						nn[fileID], e, strerror(e));
					exit(1);
				}
			}
			printf("\n");
			
			
			// Find histogram bin size from histograms in ROOT file
			// We use the E branch histograms for the simple reason that due to Python's referernce
			// counting and do_tdc_phase_scan.py's organization, the T branch histograms may not be 
			// in the ROOT file.
			if(isLinearity) {
				char hName[128];
				sprintf(hName, "heFine_%03d_%02d_%d", asic, channel, tac);
				TH2 *h = (TH2 *)hFile->Get(hName);
				if(h != NULL) {
					linearityNbins = h->GetXaxis()->GetNbins();
					linearityRangeMinimum = h->GetXaxis()->GetXmin();
					linearityRangeMaximum = h->GetXaxis()->GetXmax();
				}
			}
			else {
				char hName[128];
				sprintf(hName, "heFineB_%03d_%02d_%d", asic, channel, tac);
				TH2 *h = (TH2 *)hFile->Get(hName);
				if(h != NULL) {
					leakageNbins = h->GetXaxis()->GetNbins();
					leakageRangeMinimum = h->GetXaxis()->GetXmin();
					leakageRangeMaximum = h->GetXaxis()->GetXmax();
				}
			}
		}		
		hFile->Close();
	}
	
	assert(linearityNbins != 0);
	assert(leakageNbins != 0);

	for(int n = 0; n < nOutputFiles; n++)
	{
		if(linearityDataFiles[n] != NULL) {
			fclose(linearityDataFiles[n]);
		}
		linearityDataFiles[n] = NULL;
		
		if(linearityDataFilesNames[n] != NULL) {
			delete [] linearityDataFilesNames[n];
		}
		linearityDataFilesNames[n]= NULL;
	
		if(leakageDataFiles[n] != NULL) {
			fclose(leakageDataFiles[n]);
		}
		leakageDataFiles[n] = NULL;

		if(leakageDataFilesNames[n] != NULL) {
			delete [] leakageDataFilesNames[n];
		}		
		leakageDataFilesNames[n] = NULL;
	}
	
	delete [] leakageDataFilesNames;
	delete [] leakageDataFiles;
	delete [] linearityDataFilesNames;
	delete [] linearityDataFiles;
	
	nOutputFiles = ceil(float(asicEnd) / nAsicsPerFile);
	
	
        char fName[1024];
	sprintf(fName, "%s_ranges.tmp", tableFileNamePrefix);
	FILE *rangeFileSize = fopen(fName, "w");
	if(rangeFileSize == NULL) {
		int e = errno;
		fprintf(stderr, "Could not open '%s' for writing : %d %s\n", fName, e, strerror(e));
		exit(1);
	}
	fprintf(rangeFileSize, "%d %f %f\n", linearityNbins, linearityRangeMinimum, linearityRangeMaximum);
	fprintf(rangeFileSize, "%d %f %f\n", leakageNbins, leakageRangeMinimum, leakageRangeMaximum);
	fclose(rangeFileSize);

	sprintf(fName, "%s_list.tmp", tableFileNamePrefix);
	FILE *listFile = fopen(fName, "w");
	if(listFile == NULL) {
		int e = errno;
		fprintf(stderr, "Could not open '%s' for writing : %d %s\n", fName, e, strerror(e));
		exit(1);
	}
        for(int n = 0; n < nOutputFiles; n++) {
		sprintf(fName, "%s_%d_leakage_phase.tmp", tableFileNamePrefix, n);
		FILE *f = fopen(fName, "w");
		if(f == NULL) {
			int e = errno;
			fprintf(stderr, "Could not open '%s' for writing : %d %s\n", fName, e, strerror(e));
			exit(1);
		}
		
		for(int i = n; i < n+nAsicsPerFile; i++) {
			for(int j = 0; j < 64*4*2; j++) {
				fprintf(f, "%f\n", leakagePhasePoint[i * nAsicsPerFile * 64 * 4 * 2 + j]);
			}
		}
		fclose(f);
		
		fprintf(listFile, "%d %d %d\n", n, n*nAsicsPerFile, (n+1)*nAsicsPerFile);
        }
        fclose(listFile);
}
