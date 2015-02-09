#include "SHM.hpp"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <assert.h>
#include <errno.h>

SHM::SHM(std::string shmPath)
{
	shmfd = shm_open(shmPath.c_str(), 
			O_RDONLY, 
			S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
	if (shmfd < 0) {
		fprintf(stderr, "Opening '%s' returned %d (errno = %d)\n", shmPath.c_str(), shmfd, errno );		
		exit(1);
	}
	shmSize = lseek(shmfd, 0, SEEK_END);
	shm = (DataFrame *)mmap(NULL, 
				shmSize,
				PROT_READ, 
				MAP_SHARED, 
				shmfd,
				0);
  
}

SHM::~SHM()
{
	munmap(shm, shmSize);
	close(shmfd);
}

unsigned long long SHM::getSize()
{
	return shmSize;
}

unsigned long long SHM::getFrameID(int index)
{
	DataFrame &df = shm[index];
	unsigned long long r = df.frameID;
	return r;
}

bool SHM::getFrameLost(int index)
{
	DataFrame &df = shm[index];
	return df.frameLost;
}

int SHM::getNEvents(int index)
{
	DataFrame &df = shm[index];
	return df.nEvents;
}

int SHM::getEventType(int index, int event)
{
	DataFrame &df = shm[index];
	Event &e = df.events[event];
	return e.type;
}

int SHM::getTCoarse(int index, int event)
{
	DataFrame &df = shm[index];
	Event &e = df.events[event];
	return e.tCoarse;
}

int SHM::getECoarse(int index, int event)
{
	DataFrame &df = shm[index];
	Event &e = df.events[event];
	return e.d.tofpet.eCoarse;
}

int SHM::getTFine(int index, int event)
{
	DataFrame &df = shm[index];
	Event &e = df.events[event];
	return e.d.tofpet.tFine;
}

int SHM::getEFine(int index, int event)
{
	DataFrame &df = shm[index];
	Event &e = df.events[event];
	return e.d.tofpet.eFine;
}

int SHM::getAsicID(int index, int event)
{ 
	DataFrame &df = shm[index];
	Event &e = df.events[event];
	return e.asicID;
}

int SHM::getChannelID(int index, int event)
{
	DataFrame &df = shm[index];
	Event &e = df.events[event];
	return e.channelID;  
	}

int SHM::getTACID(int index, int event)
{
	DataFrame &df = shm[index];
	Event &e = df.events[event];
	return e.d.tofpet.tacID;
}

long long SHM::getTACIdleTime(int index, int event)
{
	DataFrame &df = shm[index];
	Event &e = df.events[event];
	return e.d.tofpet.tacIdleTime;
}

long long SHM::getChannelIdleTime(int index, int event)
{
	DataFrame &df = shm[index];
	Event &e = df.events[event];
	return e.d.tofpet.channelIdleTime;
}
