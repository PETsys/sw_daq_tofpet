#include "LUT.hpp"
#include <Common/Constants.hpp>
#include <stdio.h>
#include <assert.h>

using namespace DAQ::Common;
using namespace DAQ::TOFPET;

// Number of TACs per channel
static const int nTAC = 4*2;

LUT::LUT(int nChannels)
	: nChannels(nChannels)
{
	tableSize = nChannels * nTAC;
	table = new TAC[tableSize];
	for(int i = 0; i < tableSize; i++) {
		table[i].t0 = 0;
		for(int j = 0; j < nEntries; j++) {
			table[i].table[j] = 0;
		}
	}
}

LUT::~LUT()
{
	delete [] table;
}


void LUT::setAll(float v)
{
	for(int i = 0; i < tableSize; i++) {
		table[i].t0 = 0;
		for(int j = 0; j < nEntries; j++) {
			table[i].table[j] = v;
		}
	}
		
}

int LUT::getIndex(int channel, int tac, bool isT)
{
	int index = channel;
	index = index*4 + tac;
	index = index*2 + (isT ? 0 : 1);
	
	assert(index >= 0);
	assert(index < tableSize);
	
	
	return index;
}

void LUT::setQ(int channel, int tac, bool isT, int adc, float Q)
{
	if(adc < 0) adc = 0;
	if(adc >= nEntries) adc = nEntries - 1;
	
	int index = getIndex(channel, tac, isT);
	TAC &te = table[index];
	te.table[adc] = Q;
}

float LUT::getQ(int channel, int tac, bool isT, int adc)
{
	if(adc < 0) adc = 0;
	if(adc >= nEntries) adc = nEntries - 1;
		
	int index = getIndex(channel, tac, isT);
	TAC &te = table[index];
	return te.table[adc];
}

void LUT::setT0(int channel, int tac, bool isT, float t0)
{
	int index = getIndex(channel, tac, isT);
	TAC &te = table[index];
	te.t0 = t0;
}

float LUT::getT0(int channel, int tac, bool isT)
{
	int index = getIndex(channel, tac, isT);
	TAC &te = table[index];
	return te.t0;
}


float LUT::getT(int channel, int tac, bool isT, int adc, int coarse)
{
	float q = getQ(channel, tac, isT, adc);
	float f = (coarse % 2 == 0) ?  
		// WARNING: Q range based corrections require that calibration puts Q in [1..3] range
		// WARNING: forcing Q to [1..3] range adds a bias, which will need an extra field to be corrected
		  (q < 2.5 ? 2.0 - q : 4.0 - q) :
		  (3.0 - q);	

	float t = coarse + f;
//	printf("C%4d %c T%d TCOARSE %4d TFINE%4d => %4.3f => %4.3f => %6.3f\n", channel, isT ? 'T' : 'E', tac, coarse, adc, q, f, t); 
		  

	return t + getT0(channel, tac, isT);
}

bool LUT::isNormal(int channel, int tac, bool isT, int adc, int coarse)
{
	float q = getQ(channel, tac, isT, adc);
	if(q >= 1.00 && q <= 3.00) return true;
	else return false;
	
/*	bool isEven = coarse % 2 == 0;
	
	if(isEven && q >= 1.0 && q <= 2.0) return true;
	else if (!isEven && q >= 2.0 && q <= 3.0) return true;
	else return false;*/
}

void LUT::storeFile(int start, int end, const char *fName)
{
	FILE *f = fopen(fName, "w");
	fwrite(table+start*nTAC, sizeof(TAC), (end-start)*nTAC, f);
	fclose(f);
	
}

void LUT::loadFile(int start, int end, const char *fName)
{
	FILE *f = fopen(fName, "r");
	fread(table+start*nTAC, sizeof(TAC), (end-start)*nTAC, f);
	fclose(f);
	
}

void LUT::loadFiles(const char *mapFileName)
{
	int N = tableSize / (64*nTAC);
	FILE * mapFile = fopen(mapFileName, "r");
	char lutFileName[1024];
	for(int n = 0; n < N; n++) {
		fscanf(mapFile, "%[^\n]\n", lutFileName);
		printf("LUT:: loading %s into [%d..%d[\n", lutFileName, 64*n, 64*(n+1));
		loadFile(64*n, 64*(n+1), lutFileName);
	}

	fclose(mapFile);
}
