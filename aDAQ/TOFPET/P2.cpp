#include "P2.hpp"
#include <Common/Constants.hpp>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <libgen.h>
#include <errno.h>
#include <string.h>
#include <iostream>
using namespace std;
using namespace DAQ::Common;
using namespace DAQ::TOFPET;

// Number of TACs per channel
static const int nTAC = 4*2;

P2::P2(int nChannels)
	: nChannels(nChannels)
{
	tableSize = nChannels * nTAC;
	TQtableSize = nChannels * 2 * 6; //tot binning
	
	do_TQcorr = false;
	table = new TAC[tableSize];
	TQtable = new TQ[TQtableSize];
	
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
	for(int i = 0; i < TQtableSize; i++) {
		TQtable[i].nbins=0;
		TQtable[i].channel=0;
		TQtable[i].isT=true;
		for(int j = 0; j < 320; j++) {
			TQtable[i].tcorr[j] = 0;
		}
	}
}

P2::~P2()
{
	delete [] table;
	delete [] TQtable;
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

int P2::getIndexTQ(int channel, bool isT, int totbin)
{
	int index = channel*12 + 6*(isT ? 0 : 1) + totbin;
	if(index>=TQtableSize)index=TQtableSize-1;
	if(index<0)index=0;
	assert(index >= 0);
	assert(index < TQtableSize);
	
	
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



float P2::getQtac(int channel, int tac, bool isT, int adc, long long tacIdleTime)
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

float P2::getQmodulationCorrected(float tQ, int channel, bool isT, float coarseToT)
{

	int totbin = coarseToT / 50;

	if (coarseToT < 0) totbin = 0;
	if (coarseToT > 300)totbin = 5;
	int TQindex = getIndexTQ(channel, isT, totbin);
	TQ &TQe = TQtable[TQindex];
	
	float start = isT ? 1.0 : 1.0;
	float end = isT ? 3.0 : 3.0;
	float delta_t= (end - start) / TQe.nbins;
	
	float tQ3 = tQ;
	//Correct for clock modulations of tQ, using long time aquisitions with a source 
	//Need to check if we have this data for a given channel

	int tQ_bin = int((tQ - start)/delta_t);
	
	if((tQ_bin >= 0) && (tQ_bin < (TQe.nbins-1))){
		tQ3= start + TQe.tcorr[tQ_bin] + (TQe.tcorr[tQ_bin+1]-TQe.tcorr[tQ_bin])*(tQ-(start+tQ_bin*delta_t))/delta_t;
	}
	else if(tQ_bin == TQe.nbins-1){
		tQ3= start + TQe.tcorr[tQ_bin]+(end-TQe.tcorr[tQ_bin])*(tQ-(start+tQ_bin*delta_t))/delta_t;
	}
	else {
		// AWWW! Bad tQ! Bad tQ!
		// Let's just return it without touching
		return tQ;
	}
	//	printf("tqdac= %f %d %d %f %f %d\n", tQ, channel, isT, coarseToT, tQ3,TQe.nbins );
	return  tQ3; 
	  
	
}

float P2::getQ(int channel, int tac, bool isT, int adc, long long tacIdleTime, float coarseToT)
{
  
	float tQ = getQtac(channel, tac, isT, adc, tacIdleTime);
	if (do_TQcorr){
		
	 	float tQ_corrected = getQmodulationCorrected(tQ, channel, isT, coarseToT);
		tQ = tQ_corrected;
	}
	return tQ;
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


float P2::getT(int channel, int tac, bool isT, int adc, int coarse, long long tacIdleTime, float coarseToT)
{

	float q = getQ(channel, tac, isT, adc, tacIdleTime, coarseToT);	
	float f = (coarse % 2 == 0) ?  
		// WARNING: Q range based corrections require that calibration puts Q in [1..3] range
		// WARNING: forcing Q to [1..3] range adds a bias, which will need an extra field to be corrected
		  (q < 2.5 ? 2.0 - q : 4.0 - q) :
		  (3.0 - q);	
	
	float t = coarse + f;
	return t + getT0(channel, tac, isT);
}

bool P2::isNormal(int channel, int tac, bool isT, int adc, int coarse, long long tacIdleTime, float coarseToT)
{
	float q = getQ(channel, tac, isT, adc, tacIdleTime, coarseToT);
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

void P2::loadTQFile(int start, int end, const char *fTQName)
{
	FILE *f = fopen(fTQName, "r");
	int  col1, col3, col4,  line_elems=5;
	char col2[8];
	float col6;
	bool isT;
	int channel;
	while(line_elems > 2){
		line_elems=fscanf(f, "%5d\t%s\t%d\t%d\t%*f\t%f",&col1, col2, &col3, &col4, &col6);
	  isT = (col2[0] == 'T') ? true : false;
	  channel=col1+start;
	  // printf("channel=%d %d %d\n", channel, start, col1);
	  int TQindex = getIndexTQ(channel, isT, col3);
	  TQ &TQe = TQtable[TQindex];

	  TQe.channel=col1;
	  // printf("channel=%d %d",channel,col1 );
	  TQe.isT= isT;
	  TQe.nbins=col4;
	  TQe.tcorr[col4]=col6; 

	  // printf("%s %d %d %d %f\n", col2, TQindex, TQtable[TQindex].channel, TQtable[TQindex].nbins ,TQe.tcorr[col3]);
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
	
	char tqFileName[1024];
	char tqFilePath[1024];
	
	int start, end;
	while(fscanf(mapFile, "%d %d %s %s\n", &start, &end, lutFileName, tqFileName) == 4) {
	  if(!(tqFileName[0] == 'n' && tqFileName[1] == 'o' && tqFileName[2] == 'n')){
	    if(tqFileName[0] != '/') {
	      sprintf(tqFilePath, "%s/%s", baseName, tqFileName );
	    }
	    else {
	      sprintf(tqFilePath, "%s", tqFileName);
	    }
	  }
	  if(lutFileName[0] != '/') {
	    sprintf(lutFilePath, "%s/%s", baseName, lutFileName );
	  }
	  else {
	    sprintf(lutFilePath, "%s", lutFileName);
	  }
	  
	  if(!(tqFileName[0] == 'n' && tqFileName[1] == 'o' && tqFileName[2] == 'n')){
	    printf("P2:: loading '%s' and '%s' into [%d..%d[\n", lutFilePath, tqFilePath, start, end);
	    loadTQFile(start, end, tqFilePath);
	    do_TQcorr = true;
	  }
	  else{
	    printf("P2:: loading '%s' into [%d..%d[\n", lutFilePath, start, end);
	    do_TQcorr = false;
	  }
	  
	  loadFile(start, end, lutFilePath);	
		// string lutFile;
		//printf("P2:: loading '%s'", lutFile);
		//if (lutFile.find('tdc.cal') != string::npos){
	      
	
	       
	}

	fclose(mapFile);
}
