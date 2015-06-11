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

using namespace DAQd;

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
	assert(shmSize = MaxDataFrameQueueSize * sizeof(DataFrame));
	
	shm = (DataFrame *)mmap(NULL, 
				shmSize,
				PROT_READ, 
				MAP_SHARED, 
				shmfd,
				0);
				
				
	m_lut[ 0x7FFF ] = -1; // invalid state
	uint16_t lfsr = 0x0000;
	for ( int16_t n = 0; n < ( 1 << 15 ) - 1; ++n )
	{
		m_lut[ lfsr ] = n;
		const uint8_t bits13_14 = lfsr >> 13;
		uint8_t new_bit;
		switch ( bits13_14 )
		{ // new_bit = !(bit13 ^ bit14)
			case 0x00:
			case 0x03:
				new_bit = 0x01;
				break;
			case 0x01:
			case 0x02:
				new_bit = 0x00;
				break;
		}// switch
		lfsr = ( lfsr << 1 ) | new_bit; // add the new bit to the right
		lfsr &= 0x7FFF; // throw away the 16th bit from the shift
	}// for
	assert ( lfsr == 0x0000 ); // after 2^15-1 steps we're back at 0				
  
}

SHM::~SHM()
{
	munmap(shm, shmSize);
	close(shmfd);
}

unsigned long long SHM::getSizeInBytes()
{
	return shmSize;
}

unsigned long long SHM::getSizeInFrames()
{
	return MaxDataFrameQueueSize;
}

unsigned SHM::decodeSticCoarse(unsigned coarse, unsigned long long frameID)
{
	coarse = m_lut[coarse];
	long long clocksElapsed = (frameID % 256) *1024*4ULL;	// Periodic reset every 256 frames
	long long wrapNumber	= clocksElapsed / 32767;
	long long wrapRemainder	= clocksElapsed % 32767;
	if(coarse < wrapRemainder) coarse += 32767;
	coarse -= wrapRemainder;
	return coarse;
}