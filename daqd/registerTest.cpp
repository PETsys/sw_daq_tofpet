#include <stdio.h>
#include <assert.h>

typedef unsigned long DWORD;
typedef unsigned long long UINT64;

extern "C" {
#include "dtfly_p_etpu_defs.h"
#include "dtfly_p_etpu_api.h"
}

static const unsigned DMA_TRANS_BYTE_SIZE = 262144;
static const unsigned rawWordBufferSize = DMA_TRANS_BYTE_SIZE / sizeof(uint64_t);

static void userIntHandler(SIntInfoData * IntInfoData)
{
}

int main(int argc, char *argv[])
{
	//assert(argc == 2);
	//char * outFileName = argv[1];
	//FILE *outFile = fopen(outFileName, "w");
	
	
	int dtFlyPinitStatus = InitDevice(userIntHandler);
	assert(dtFlyPinitStatus == 0);	
	int status;
	
	uint32_t regData[1];
	uint32_t regAddress = 513;
	
	// // Read, write, then read again register 0
	// // ReadWriteConfigRegs(direction, registerNumber, registerDataBuffer, numberOfRegisters)
	// status = ReadWriteConfigRegs(READ, regAddress, regData, 1);
	// printf("Read status is %d, value is %08lx\n", status, regData[0]);

	// regData[0] = 0x12345678;
	// status = ReadWriteConfigRegs(WRITE, regAddress, regData, 1);
	// printf("Write status is %d\n", status);

	// status = ReadWriteConfigRegs(READ, regAddress, regData, 1);
	// printf("Read status is %d, value is %08lx\n", status, regData[0]);

	
	for(uint32_t i=0; i<1024*1e6;i++){
		regData[0]= i;
		regAddress= i%1024;
		//if(i<=2048)printf("i=%ld regAddress=%ld\n", i, regAddress);
		status = ReadWriteUserRegs(WRITE, regAddress,regData , 1);	
		
	}



	
	/*
	 * Read and dump to file
	 */ 
	// SBufferInit buffer;
	// unsigned bufferIndex = 0;
	// AllocateBuffer(bufferIndex, &buffer);
	
	// for(int nReads = 0; nReads < 10; nReads += 1) {
	// 	status = ReceiveDMAbyBufIndex(DMA1, bufferIndex, 1);
	// 	if(status > 0) {
	// 		uint64_t *wordBuffer = (uint64_t *)buffer.UserAddr;
	// 		int nWords = status / sizeof(uint64_t);
			
	// 		for(int i = 0; i < nWords; i++) {
	// 			fprintf(outFile, "%016llx\n", wordBuffer[i]);
	// 		}
	// 	}
	// }	
	// ReleaseBuffer(bufferIndex);	
	
	
	
	ReleaseDevice();
	//fclose(outFile);
	return 0;
}
