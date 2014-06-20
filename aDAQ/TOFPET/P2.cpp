#include "P2.hpp"
#include <Common/Constants.hpp>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <libgen.h>
#include <errno.h>
#include <string.h>

using namespace DAQ::Common;
using namespace DAQ::TOFPET;

// Number of TACs per channel
static const int nTAC = 4*2;

P2::P2(int nChannels)
	: nChannels(nChannels)
{
	tableSize = nChannels * nTAC;
	table = new TAC[tableSize];
	for(int i = 0; i < tableSize; i++) {
		table[i].t0 = 0;
		table[i].shape.tB = 0;
		table[i].shape.m = 0;
		table[i].shape.p2 = 0;
		table[i].leakage.tQ = 0;
		table[i].leakage.a0 = 0;
		table[i].leakage.a1 = 0;
		table[i].leakage.a2 = 0;
	}
	defaultQ = 0;
}

P2::~P2()
{
	delete [] table;
}


void P2::setAll(float v)
{
	defaultQ = v;
		
}

int P2::getIndex(int channel, int tac, bool isT)
{
	int index = channel;
	index = index*4 + tac;
	index = index*2 + (isT ? 0 : 1);
	
	assert(index >= 0);
	assert(index < tableSize);
	
	
	return index;
}

void P2::setShapeParameters(int channel, int tac, bool isT, float tB, float m, float p2)
{
	int index = getIndex(channel, tac, isT);
	TAC &te = table[index];
	te.shape.tB = tB;
	te.shape.m = m;
	te.shape.p2 = p2;
}

void P2::setLeakageParameters(int channel, int tac, bool isT, float tQ, float a0, float a1, float a2)
{
	int index = getIndex(channel, tac, isT);
	TAC &te = table[index];
	te.leakage.tQ = tQ;
	te.leakage.a0 = a0;
	te.leakage.a1 = a1;
	te.leakage.a2 = a2;
}


float P2::getQ(int channel, int tac, bool isT, int adc, long long tacIdleTime)
{
	int index = getIndex(channel, tac, isT);
	TAC &te = table[index];
	
	if(te.shape.p2 == 0) return defaultQ; // We don't have such thing as a linear TAC!
	
	
	float p2 = te.shape.p2;
	float m = te.shape.m;
	float tB = te.shape.tB;
	
	// tQ without accounting for leakage
	float tQ1 = +( 2 * p2 * tB + sqrtf(4 * adc * p2 + m*m) - m)/(2 * p2);
	
	
	// tQ with leakage estimation
	// Estimate tB based on the tacIdleTime
	float adcEstimate = te.leakage.a0 + te.leakage.a1 * tacIdleTime  + te.leakage.a2 * tacIdleTime * tacIdleTime;
	tB = -(- 2*p2*te.leakage.tQ + sqrtf(4*adcEstimate*p2 + m*m ) - m)/(2*p2);
	float tQ2 = +( 2 * p2 * tB + sqrtf(4 * adc * p2 + m*m) - m)/(2 * p2);
	
/*	if(tac == 0 && isT && tacIdleTime == (5*1024)*4)
		fprintf(stderr, "P2:getQ (%d, %d, %c, %d, %lld) : tB = %f\n", channel, tac, isT ? 'T' : 'E', adc, tacIdleTime, tB);*/
	
/*	printf("%f %f %f %lld => %f => %f (vs %f : %f)\n", te.leakage.tQ, te.leakage.b, te.leakage.m, tacIdleTime, adcEstimate, tB, te.shape.tB, (tB - te.shape.tB));
	printf("tQ : %f vs %f : %f\n", tQ1, tQ2, tQ1 - tQ2);*/
	return tQ2 + defaultQ;
}

void P2::setT0(int channel, int tac, bool isT, float t0)
{
	int index = getIndex(channel, tac, isT);
	TAC &te = table[index];
	te.t0 = t0;
}

float P2::getT0(int channel, int tac, bool isT)
{
	int index = getIndex(channel, tac, isT);
	TAC &te = table[index];
	return te.t0;
}


float P2::getT(int channel, int tac, bool isT, int adc, int coarse, long long tacIdleTime)
{
	float q = getQ(channel, tac, isT, adc, tacIdleTime);
	float f = (coarse % 2 == 0) ?  
		// WARNING: Q range based corrections require that calibration puts Q in [1..3] range
		// WARNING: forcing Q to [1..3] range adds a bias, which will need an extra field to be corrected
		  (q < 2.5 ? 2.0 - q : 4.0 - q) :
		  (3.0 - q);	

	float t = coarse + f;
//	printf("C%4d %c T%d TCOARSE %4d TFINE%4d => %4.3f => %4.3f => %6.3f\n", channel, isT ? 'T' : 'E', tac, coarse, adc, q, f, t); 
		  

	return t + getT0(channel, tac, isT);
}

bool P2::isNormal(int channel, int tac, bool isT, int adc, int coarse, long long tacIdleTime)
{
	float q = getQ(channel, tac, isT, adc, tacIdleTime);
	if(q >= 1.00 && q <= 3.00) return true;
	else return false;
}

void P2::storeFile(int start, int end, const char *fName)
{
	FILE *f = fopen(fName, "w");
	for(int channel = start; channel < end; channel++)
		for(int isT = 0; isT < 2; isT++)
			for(int tac = 0; tac < 4; tac++) {
				int index = getIndex(channel, tac, isT == 0);
				TAC &te = table[index];
				fprintf(f, "%5d\t%c\t%d\t%10.6e\t%10.6e\t%10.6e\t%10.6e\t%10.6e\t%10.6e\t%10.6e\t%10.6e\n", 
					channel - start, isT == 0 ? 'T' : 'E', tac,
					te.t0, 
					te.shape.tB, te.shape.m, te.shape.p2,
					te.leakage.tQ, te.leakage.a0, te.leakage.a1, te.leakage.a2
				);
				
			}
	fclose(f);
	
}

void P2::loadFile(int start, int end, const char *fName)
{
	FILE *f = fopen(fName, "r");
	for(int channel = start; channel < end; channel++)
		for(int isT = 0; isT < 2; isT++)
			for(int tac = 0; tac < 4; tac++) {
				int index = getIndex(channel, tac, isT == 0);
				TAC &te = table[index];
				fscanf(f, "%*5d\t%*c\t%*d\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\n", 
					&te.t0, 
					&te.shape.tB, &te.shape.m, &te.shape.p2,
					&te.leakage.tQ, &te.leakage.a0, &te.leakage.a1, &te.leakage.a2
				);
				
			}
	
	fclose(f);
	
}

void P2::loadFiles(const char *mapFileName)
{
	printf("P2:: loading TDC calibrations...\n");
	FILE * mapFile = fopen(mapFileName, "r");
	char baseName[1024];
	strncat(baseName, dirname((char *)mapFileName), 1024);
	char lutFileName[1024];
	char lutFilePath[1024];
	int start, end;
	while(fscanf(mapFile, "%d %d %[^\n]\n", &start, &end, lutFileName) == 3) {
		if(lutFileName[0] != '/') {
			sprintf(lutFilePath, "%s/%s", baseName, lutFileName);
		}
		else {
			sprintf(lutFilePath, "%s", lutFileName);
		}
		
		
		printf("P2:: loading '%s' into [%d..%d[\n", lutFilePath, start, end);
		loadFile(start, end, lutFilePath);
	}

	fclose(mapFile);
}
