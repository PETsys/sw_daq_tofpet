#include "SystemInformation.hpp"
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

using namespace DAQ::Common;

SystemInformation::SystemInformation()
{
	for(int channel = 0; channel < SYSTEM_NCHANNELS; channel++) {
		channelInformation[channel] = { -1, -1, -1, NAN, NAN, NAN };
	}
	
	for(int n1 = 0; n1 < DAQ::Common::MAX_TRIGGER_REGIONS; n1++) {
		for(int n2 = 0; n2 < DAQ::Common::MAX_TRIGGER_REGIONS; n2++) {
			regionMap[n1 * DAQ::Common::MAX_TRIGGER_REGIONS + n2] = n1 != n2;
		}
	}
}

SystemInformation::~SystemInformation()
{
}

void SystemInformation::loadMapFile(const char *fname) 
{
	fprintf(stderr, "Loading channel map file: '%s' ... ", fname); fflush(stderr);
	FILE *mapFile = fopen(fname, "r");
	if(mapFile == NULL) {
		int e = errno;
		fprintf(stderr, "Could not open '%' for reading : %d %s\n",
			fname, e, strerror(e));
		exit(1);
	}
	
	
	int channel;
	int region;
	int xi;
	int yi;
	float x;
	float y;
	float z;
	int hv;
	
	int nLoaded = 0;
	while(fscanf(mapFile, "%d\t%d\t%d\t%d\t%f\t%f\t%f\t%d\n", &channel, &region, &xi, &yi, &x, &y, &z, &hv) == 8) {
		if(channel < 0 || channel >= SYSTEM_NCHANNELS) {
			fprintf(stderr, "Bad channel ID %d on line %d\n", channel, nLoaded+1);
			exit(1);
		}
		if(region < 0 || region >= MAX_TRIGGER_REGIONS) {
			fprintf(stderr, "Bad region ID %d on line %d\n", region, nLoaded+1);
			exit(1);
		}
		
		channelInformation[channel] = { region, xi, yi, x, y, z };
		nLoaded++;
	}
	
	
	fclose(mapFile);
	fprintf(stderr, "%d valid channel coordinates found\n", nLoaded);
}

void SystemInformation::loadTriggerMapFile(const char *fname) 
{
	fprintf(stderr, "Loading trigger map file: '%s' ...\n", fname); fflush(stderr);
	FILE *mapFile = fopen(fname, "r");
	if(mapFile == NULL) {
		int e = errno;
		fprintf(stderr, "Could not open '%' for reading : %d %s\n",
			fname, e, strerror(e));
		exit(1);
	}
	
	for(int n = 0; n < MAX_TRIGGER_REGIONS*MAX_TRIGGER_REGIONS; n++)
		regionMap[n] = false;
	
	int region1, region2;
	int nLoaded = 0;
	while(fscanf(mapFile, "%d\t%d\n", &region1, &region2) == 2) {
		if(region1 < 0 || region1 >= MAX_TRIGGER_REGIONS) {
			fprintf(stderr, "Bad region ID %d on line %d\n", region1, nLoaded+1);
			exit(1);
		}
		if(region2 < 0 || region2 >= MAX_TRIGGER_REGIONS) {
			fprintf(stderr, "Bad region ID %d on line %d\n", region2, nLoaded+1);
			exit(1);
		}
		regionMap[region1 * DAQ::Common::MAX_TRIGGER_REGIONS + region2] = true;
		regionMap[region2 * DAQ::Common::MAX_TRIGGER_REGIONS + region1] = true;
		
		nLoaded++;
	}
	
	
	fclose(mapFile);
}