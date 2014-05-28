#include <stdio.h>
#include <assert.h>
#include <Core/SharedMemory.hpp>
#include <boost/lexical_cast.hpp>
#include <TFile.h>
#include <TH2F.h>
#include <TTree.h>


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>

static const int nTAC = 128*2*4;

int main(int argc, char *argv[])
{
	assert(argc == 6);

	const char *shmName = argv[1];
	unsigned long shmSize =boost::lexical_cast<unsigned long>(argv[2]);
	const char *rootFileName = argv[3];
	float nClocks = boost::lexical_cast<float>(argv[4]);
	int nBins = boost::lexical_cast<int>(argv[5]);
	
	
	int dataFrameSharedMemory_fd = shm_open(shmName, O_RDONLY, 0660);
	assert(dataFrameSharedMemory_fd >= 0);
	
	unsigned long dataFrameSharedMemorySize = shmSize * sizeof(DataFrame);
	ftruncate(dataFrameSharedMemory_fd, dataFrameSharedMemorySize);
	
	DataFrame *dataFrameSharedMemory = (DataFrame *)mmap(NULL, 
						  dataFrameSharedMemorySize, 
						  PROT_READ, 
						  MAP_SHARED, 
						  dataFrameSharedMemory_fd, 
						  0);
						 
	assert(dataFrameSharedMemory != NULL);
	
	
	TFile *rootFile = new TFile(rootFileName, "RECREATE");
	TTree *data = new TTree("data2", "data2");
	
	Float_t fStep2;		data->Branch("step2", &fStep2);
	Int_t fAsic;		data->Branch("asic", &fAsic);
	Int_t fChannel;		data->Branch("channel", &fChannel);
	Int_t fTac;		data->Branch("tac", &fTac);
	Int_t ftCoarse;		data->Branch("tcoarse", &ftCoarse);
	Int_t ftFine;		data->Branch("tfine", &ftFine);
	Int_t feCoarse;		data->Branch("ecoarse", &feCoarse);
	Int_t feFine;		data->Branch("efine", &feFine);
	
	TH2F *h[nTAC];
	for(int n = 0; n < nTAC; n += 1) {
		Int_t channel = (n/2) / 4;
		Int_t tac = (n/2) % 4;
		Bool_t isT = n % 2 == 0;
		
		char hName[128];			
		sprintf(hName, isT ? "htFine_%03d_%02d_%1d" : "heFine_%03d_%02d_%1d" , 
			channel/64, channel%64,	tac);
		h[n] = new TH2F(hName,  hName, nBins, 0, nClocks, 1024, 0, 1024);
	}

	
	struct {
		float step1;
		float step2;
		float step3;
		float step4;
		int index;
	} message;
	
	while(fread(&message, sizeof(message), 1, stdin) == 1) {
		fStep2 = message.step2;	
		
		DataFrame *dataFrame = dataFrameSharedMemory+message.index;
		int nEvents = dataFrame->nEvents;
		for (int i = 0; i < nEvents; i++) {
			EventPlate &e = dataFrame->events[i].d.plate;
			fAsic = e.asicID;
			fChannel = e.channelID;
			fTac = e.tacID;
			ftCoarse = e.tCoarse;
			ftFine = e.tFine;
			feCoarse = e.eCoarse;
			feFine = e.eFine;
			
			data->Fill();
			
			int n = ((64 * e.asicID) + e.channelID) * 4;
			h[2*n + 0]->Fill(fStep2, e.tFine);
			h[2*n + 1]->Fill(fStep2, e.eFine);
		}
		
		fwrite(&message.index, sizeof(int), 1, stdout);
		fflush(stdout);
	}
	
	rootFile->Write();
	rootFile->Close();
	
	return 0;
}
