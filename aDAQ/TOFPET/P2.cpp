#include "P2.hpp"
#include <Common/Constants.hpp>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <libgen.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <boost/regex.hpp>

using namespace std;
using namespace DAQ::Common;
using namespace DAQ::TOFPET;

// Number of TACs per channel
static const int nTAC = 4*2;

P2::P2(int nChannels)
	: nChannels(nChannels)
{
	tableSize = nChannels * nTAC;
	TQtableSize = nChannels * 2 * 6; 
	totTableSize = nChannels * 1024; 


	do_TQcorr = false;
	useEnergyCal= false;

	table = new TAC[tableSize];
	TQtable = new TQ[TQtableSize];
	ToTtable = new ToTcal[totTableSize];
	timeOffset = new float[nChannels];

	nBins_tqT= new float[nChannels];
	nBins_tqE= new float[nChannels];
	for(int i = 0; i < nChannels; i++){
		nBins_tqT[i]=0;
		nBins_tqE[i]=0;	
	}

	
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
	
	for(int i = 0; i < nChannels; i++) {
	 	for(int j = 0; j < 1024; j++) {
			ToTtable[i*1024+j].channel=0;
	 		ToTtable[i*1024+j].tot=0;
	 		ToTtable[i*1024+j].energy=0;
	 	}
	}
	
	for(int i = 0; i < nChannels; i++) timeOffset[i]=0;
}

P2::~P2()
{
	delete [] table;
	delete [] TQtable;
	delete [] ToTtable;
	delete [] timeOffset;
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
	
	if (index <0)index=0;
	if (index >= tableSize){
		printf("index=%d, channel=%d, tac=%d\n", index, channel, tac);
		index=tableSize-1;
	}
	assert(index >= 0);
	assert(index < tableSize);
	
	
	return index;
}

int P2::getIndexTQ(int channel, bool isT, int totbin)
{
	int index = channel*12 + 6*(isT ? 0 : 1) + totbin;
	if(index>=TQtableSize)index=TQtableSize-1;
	if(index<0)index=0;
	//assert(index >= 0);
	//assert(index < TQtableSize);
	
	
	return index;
}

long P2::getIndexTOT(int channel, float tot)
{
	if(tot<0) return long(channel*1024); 
	long index = channel*1024 + int(tot/0.5);
	
	if(index>=totTableSize)index=totTableSize-1;
	if(index<0)index=0;
	
	//	assert(index >= 0);
	//assert(index < totTableSize);
	
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
	


	if(te.shape.m == 0) return 2; 
	
	
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
	
	// If we don't have leakage calibration for this channel, proceed without it
	float tQ3 = te.leakage.a0 > 0 ? tQ2 : tQ1;
	
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
	int index = getIndex(channel, tac, isT);
	TAC &te = table[index];

	float q = getQ(channel, tac, isT, adc, tacIdleTime, coarseToT);	
	float f = (coarse % 2 == 0) ?  
		// WARNING: Q range based corrections require that calibration puts Q in [1..3] range
		// WARNING: forcing Q to [1..3] range adds a bias, which will need an extra field to be corrected
		  (q < 2.5 ? 2.0 - q : 4.0 - q) :
		  (3.0 - q);	
	if(te.shape.m == 0)f = (coarse % 2 == 0) ? 1.5 : 2.5;
	float t = coarse + f;
	return t + getT0(channel, tac, isT);
}

float P2::getEnergy(int channel, float tot)
{
	float energy;
	long index = getIndexTOT(channel, tot);
	ToTcal &ToTe = ToTtable[index];
	ToTcal &ToTe1 = ToTtable[index+1];
	if(useEnergyCal){
		if(ToTe1.channel==channel) energy= ToTe.energy+(ToTe1.energy-ToTe.energy)/0.5*(tot-ToTe.tot);
		else energy= ToTe.energy;
		//		if(energy>1000)printf("ToTe.energy=%f tot=%f energy=%f\n",ToTe.energy, tot, energy );
	}
	else energy=tot;

	return energy;
}

bool P2::isNormal(int channel, int tac, bool isT, int adc, int coarse, long long tacIdleTime, float coarseToT)
{
	int index = getIndex(channel, tac, isT);
	TAC &te = table[index];
	float q = getQ(channel, tac, isT, adc, tacIdleTime, coarseToT);
	if((q >= 1.00 && q <= 3.00) || te.shape.m == 0) return true;
	else return false;
}

void P2::storeFile(int start, int end, const char *fName)
{
	FILE *f=NULL;
	for(int channel = start; channel < end; channel++)
		for(int isT = 0; isT < 2; isT++)
			for(int tac = 0; tac < 4; tac++) {
				int index = getIndex(channel, tac, isT == 0);
				TAC &te = table[index];
				if(te.shape.m==0) continue;
				else if(f==NULL) {
					f = fopen(fName, "w");
					if(f == NULL) {
						int e = errno;
						fprintf(stderr, "Could not open '%s' for writing : %d %s\n", 
							fName, e, strerror(e));
							exit(1);
					}
				}
				fprintf(f, "%5d\t%c\t%d\t%10.6e\t%10.6e\t%10.6e\t%10.6e\t%10.6e\t%10.6e\t%10.6e\t%10.6e\n", 
					channel - start, isT == 0 ? 'T' : 'E', tac,
					te.t0, 
					te.shape.tB, te.shape.m, te.shape.p2,
					te.leakage.tQ, te.leakage.a0, te.leakage.a1, te.leakage.a2
				);
				
			}
	if(f!=NULL)fclose(f);
	
}

void P2::loadTDCFile(int start, int end, const char *fName)
{
	FILE *f = fopen(fName, "r");
	if(f == NULL) {
		int e = errno;
		fprintf(stderr, "Could not open '%s' for reading : %d %s\n", fName, e, strerror(e));
		exit(0);
	}
	int channel;
	char tOrE;
	int tac;
	TAC te;
	while(fscanf(f, "%5d\t%c\t%d\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\n", 
				 &channel, &tOrE, &tac,
				 &te.t0, 
				 &te.shape.tB, &te.shape.m, &te.shape.p2,
				 &te.leakage.tQ, &te.leakage.a0, &te.leakage.a1, &te.leakage.a2
				 ) == 11)
		{
			tOrE == 'T' ? nBins_tqT[start+channel]+=te.shape.m/2.0 :  nBins_tqE[start+channel]+=te.shape.m/2.0;
			int index = getIndex(start+channel, tac, tOrE == 'T');
			table[index] = te;
		}
	

	fclose(f);

}
void P2::loadTQFile(int start, int end, const char *fName)
{
	FILE *f = fopen(fName, "r");
	if(f == NULL) {
		int e = errno;
		fprintf(stderr, "Could not open '%s' for reading : %d %s\n", fName, e, strerror(e));
		exit(0);
	}
	int  col1, col3, col4,  line_elems=5;
	char col2[8];
	float col6;
	bool isT;
	int channel;
	while(line_elems > 2){
		line_elems=fscanf(f, "%5d\t%s\t%d\t%d\t%*f\t%f",&col1, col2, &col3, &col4, &col6);
	  isT = (col2[0] == 'T') ? true : false;
	  channel= col1;   ///col1+start;
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

void P2::loadTOTFile(int start, int end, const char *fName)
{
	FILE *f = fopen(fName, "r");
	if(f == NULL) {
		int e = errno;
		fprintf(stderr, "Could not open '%s' for reading : %d %s\n", fName, e, strerror(e));
		exit(0);
	}
	int col1;
	float col2;
	float col3;
	while(fscanf(f, "%d\t%f\t%f\n",&col1, &col2, &col3) == 3 ){
		long totIndex=getIndexTOT(col1,col2);
		ToTcal &totCale = ToTtable[totIndex];
		totCale.channel=col1;
		totCale.tot=col2;
		totCale.energy=col3;
	}
	fclose(f);
	
}

void P2::loadOffsetFile(int start, int end, const char *fName)
{
	FILE *f = fopen(fName, "r");
	if(f == NULL) {
		int e = errno;
		fprintf(stderr, "Could not open '%s' for reading : %d %s\n", fName, e, strerror(e));
		exit(0);
	}
	int col1;
	float col2;
	while(fscanf(f, "%d\t%f\n",&col1, &col2) == 2){
		timeOffset[start+col1]=col2;
	}
	fclose(f);
	
}


static void copySubMatch(char *dst, const char *start, const char *end) 
{
	while(start != end) {
		*dst = *start;
		start++;
		dst++;
	}
	*dst = '\0';
}

void P2::loadFiles(const char *mapFileName, bool loadTQ, bool multistep, float step1, float step2)
{
	FILE * mapFile = fopen(mapFileName, "r");
	if(mapFile == NULL) {
		int e = errno;
		fprintf(stderr, "Could not open '%s' for reading : %d %s\n", mapFileName, e, strerror(e));
		exit(0);
	}

	char fileNameCopy[1024];
	strncpy(fileNameCopy, mapFileName, 1024);
	
	char baseName[1024]; baseName[0] = 0;	
	strncpy(baseName, dirname((char *)fileNameCopy), 1024);

	char lutFileName[1024];
	char lutFilePath[1024];
	
	char tqFileName[1024];
	char tqFilePath[1024];

	char totFileName[1024];
	char totFilePath[1024];

	char offsetFileName[1024];
	char offsetFilePath[1024];

	char suffix[1024];
	int start, end;
	char line[1024];
	
	const boost::regex e("([0-9]+)\\s+([0-9]+)\\s+(\\S+)(\\s+)?(\\S+)?(\\s+)?(\\S+)?(\\s+)?(\\S+)?");
	int lineNumber = 0;
	while(fscanf(mapFile, "%[^\n]\n", line) == 1) {
		lineNumber += 1;
		boost::match_results<const char *> what;
		if(!boost::regex_match((const char *)line, what, e, boost::match_default)) {
			fprintf(stderr, "Bad syntax in '%s' (line %d) (1)\n", mapFileName, lineNumber);
			continue;
		}
		
		if(!what[1].matched || !what[2].matched || !what[3].matched) {
			fprintf(stderr, "Bad syntax in '%s' (line %d) (2)\n", mapFileName, lineNumber);
		continue;
		}
		
		char temporary[128];

		copySubMatch(temporary, what[1].first, what[1].second);
		assert(sscanf(temporary, "%d", &start) == 1);

		copySubMatch(temporary, what[2].first, what[2].second);
		assert(sscanf(temporary, "%d", &end) == 1);

		copySubMatch(lutFileName, what[3].first, what[3].second);
		

		if(what[5].matched) {
			copySubMatch(tqFileName, what[5].first, what[5].second);
		}
		else
			strcpy(tqFileName, "none");
		
		if(what[7].matched) {
			copySubMatch(totFileName, what[7].first, what[7].second);
		}
		else
			strcpy(totFileName, "none");

		if(what[9].matched) {
			copySubMatch(offsetFileName, what[9].first, what[9].second);
		}
		else
			strcpy(offsetFileName, "none");
		
	
		if(strcmp(tqFileName, "none")!=0){
			if(multistep==true){
				sprintf(suffix,".stp1_%f_stp2_%f",step1, step2);
				strcat(tqFileName,suffix); 
			}
				
			if(tqFileName[0] != '/') {
				sprintf(tqFilePath, "%s/%s", baseName, tqFileName );
			}
			else {
				sprintf(tqFilePath, "%s", tqFileName);
			}
		}

		if(strcmp(totFileName, "none")!=0){
			if(totFileName[0] != '/') {
				sprintf(totFilePath, "%s/%s", baseName, totFileName );
			}
			else {
				sprintf(totFilePath, "%s", totFileName);
			}
		}
		if(strcmp(offsetFileName, "none")!=0){			
			if(offsetFileName[0] != '/') {
				sprintf(offsetFilePath, "%s/%s", baseName, offsetFileName );
			}
			else {
				sprintf(offsetFilePath, "%s", offsetFileName);
			}
		}
		

		if(lutFileName[0] != '/') {
			sprintf(lutFilePath, "%s/%s", baseName, lutFileName );
		}
		else {
			sprintf(lutFilePath, "%s", lutFileName);
		}
		
		if(strcmp(lutFileName, "none")!=0){
			printf("P2:: loading TDC calibrations...\n");
			printf("P2:: loading '%s' into [%d..%d[\n", lutFilePath, start, end);
			loadTDCFile(start, end, lutFilePath);
		}
		if(strcmp(tqFileName, "none")!=0 and loadTQ==true){
			printf("P2:: loading TQ calibrations...\n");
			printf("P2:: loading '%s' into [%d..%d[\n", tqFilePath, start, end);
			loadTQFile(start, end, tqFilePath);
			do_TQcorr = true;
		}
		if(strcmp(totFileName, "none")!=0){
			printf("P2:: loading Energy calibrations...\n");
			printf("P2:: loading '%s' into [%d..%d[\n", totFilePath, start, end);
			loadTOTFile(start, end, totFilePath);
			useEnergyCal=true;
		}
		if(strcmp(offsetFileName, "none")!=0){
			printf("P2:: loading timeOffset calibrations...\n");
			printf("P2:: loading '%s' into [%d..%d[\n",  offsetFilePath, start, end);
			loadOffsetFile(start, end, offsetFilePath);
		}	       
	}

	fclose(mapFile);
}
