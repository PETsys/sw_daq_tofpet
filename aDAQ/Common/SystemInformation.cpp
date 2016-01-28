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
}

SystemInformation::~SystemInformation()
{
}

void SystemInformation::loadMapFile(const char *fname) 
{
	fprintf(stderr, "Loading map file: '%s' ... ", fname); fflush(stderr);
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
		
		channelInformation[channel] = { region, xi, yi, x, y, z };
		nLoaded++;
	}
	
	
	fclose(mapFile);
	fprintf(stderr, "%d valid channel coordinates found\n", nLoaded);
}