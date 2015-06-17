#include "DAQFrameServer.hpp"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <boost/crc.hpp>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>  
#include <assert.h>
#include <errno.h>

const uint64_t IDLE_WORD = 0xFFFFFFFFFFFFFFFFULL;
const uint64_t HEADER_WORD = 0xFFFFFFFFFFFFFFF5ULL;
const uint64_t TRAILER_WORD = 0xFFFFFFFFFFFFFFFAULL;


typedef boost::crc_optimal<32, 0x04C11DB7, 0x0A1CB27F, 0, false, false> crcResult_t;

bool checkCRC(uint8_t *word, int size, uint32_t CRC){
	crcResult_t crcResult;
	crcResult.reset();  
	crcResult.process_bytes(word, size);
	unsigned int crc32 = crcResult.checksum();
	// 	printf("CRC checking ");
	// for (int i = 0; i < size; i++) 
	// 	printf("%02X ", word[i]);
	// printf("\n");
	// printf("CRC32 is %08x %08x\n", crc32, CRC);
	if(crc32==CRC) return true;
	else return false;
	
}


DAQFrameServer::DAQFrameServer(int nFEB, int *feTypeMap, int debugLevel)
  : FrameServer(nFEB, feTypeMap, debugLevel)
{
	DP = new DtFlyP();
	printf("allocated DP object = %p\n", DP);
	//	P = new DtFlyP;

	startWorker();
}


DAQFrameServer::~DAQFrameServer()
{
	delete DP;	
}

void DAQFrameServer::startAcquisition(int mode)
{
	FrameServer::startAcquisition(mode);
	DP->setAcquistionOnOff(true);
}

void DAQFrameServer::stopAcquisition()
{
	DP->setAcquistionOnOff(false);
 	FrameServer::stopAcquisition();	
}

int DAQFrameServer::sendCommand(int portID, int slaveID, char *buffer, int bufferSize, int commandLength)
{

	uint16_t sentSN = (unsigned(buffer[0]) << 8) + unsigned(buffer[1]);	

	boost::posix_time::ptime t1 = boost::posix_time::microsec_clock::local_time();
	DP->sendCommand(portID, slaveID, buffer,bufferSize, commandLength);
	

	boost::posix_time::ptime t2 = boost::posix_time::microsec_clock::local_time();

	int replyLength = 0;
	int nLoops = 0;
	char replyBuffer[12*4];
	do {
		boost::posix_time::ptime tl = boost::posix_time::microsec_clock::local_time();
		if((tl - t2).total_milliseconds() > 10) break;

		int status = DP->recvReply(replyBuffer, 12*4);
		if (status < 0) {
			continue; // Timed out and did not receive a reply
		}
		
		if(status < 2) { // Received something weird
			fprintf(stderr, "Received very short reply: %d bytes\n", status);
			continue;
		}
		
		uint16_t recvSN = (unsigned(replyBuffer[0]) << 8) + replyBuffer[1];
		if(sentSN != recvSN) {
			fprintf(stderr, "Mismatched SN: sent %6hx, got %6hx\n", sentSN, recvSN);
			continue;
		}
	
		// Got what looks like a valid reply with the correct SN
		replyLength = status;
		memcpy(buffer, replyBuffer, replyLength);		
	} while(replyLength == 0);	
	
	boost::posix_time::ptime t3 = boost::posix_time::microsec_clock::local_time();
	
	if (replyLength == 0 ) {
		printf("Command reply timing: (t2 - t1) => %ld us, (t3 - t2) => %ld us, i = %d, status = %s\n", 
			(t2 - t1).total_microseconds(), 
			(t3 - t2).total_microseconds(),
			nLoops,
			replyLength > 0 ? "OK" : "FAIL"
		);
	}
	
	return replyLength;
}

void *DAQFrameServer::doWork()
{	

	printf("DAQFrameServer::runWorker starting...\n");
	printf("DP object is %p\n", DP);
	DAQFrameServer *m = this;
	
	for(int i = 0; i < N_ASIC * 64 * 4; i++) {
		tacLastEventTime[i] = 0;
	}
	for(int i = 0; i < N_ASIC * 64; i++) {
		channelLastEventTime[i] = 0;
	}
	
	long nFramesFound = 0;
	long nFramesPushed = 0;
	
	
	const int MAX_FRAME_SIZE = 0x8000;
	uint64_t *frameBuffer = new uint64_t[MAX_FRAME_SIZE];
	
	const int maxSkippedLoops = 32*1024;
	int skippedLoops = 0;
	int nonidle=0;
	bool lastFrameWasBad = true;
	while(!die){
		if(skippedLoops >= maxSkippedLoops) {
			//printf("skippedLoops = %d\n", skippedLoops);
			skippedLoops = 0;
			
			// Trigger the conditions, to ensure clients don't get locked for ever
			pthread_cond_signal(&m->condReplyQueue);
			
			pthread_mutex_lock(&m->lock);
			if(m->dirtyDataFrameQueue.empty()) {
				m->dirtyDataFrameQueue.push(NULL);
			}
			pthread_mutex_unlock(&m->lock);
			pthread_cond_signal(&m->condDirtyDataFrame);
		}
		
		uint64_t firstWord;
		int nWords;

		// If we are comming back from a bad frame, let's dump until we read an idle
		if(lastFrameWasBad) {
			nWords = DP->getWords(&firstWord, 1);
			//printf("DBG1 %016llx\n", firstWord);
			if(nWords != 1) { skippedLoops = 1000001; continue; }			
			if(firstWord != IDLE_WORD) { skippedLoops++; continue; }
		}
		lastFrameWasBad = false;

		// Read something, which may be a filler or a header
		nWords = DP->getWords(&firstWord, 1);
		if (nWords != 1) { skippedLoops = 1000002; continue; }		
		if(firstWord == IDLE_WORD) { skippedLoops++; continue; }
/*		printf("DBG2 %016llx\n", firstWord);		*/
		if(firstWord != HEADER_WORD) { lastFrameWasBad = true; skippedLoops = 1000003; continue; }
		skippedLoops = 0;
		
		// Read the frame's first word
		nWords = DP->getWords(&firstWord, 1);
		if (nWords != 1) { skippedLoops = 1000003; continue; }		
/*		printf("DBG3 %016llx\n", firstWord);		*/
		
		unsigned frameSource = (firstWord >> 54) & 0x400;
		unsigned frameType = (firstWord >> 51) & 0x7;
		unsigned frameSize = (firstWord >> 36) & 0x7FFF;
		unsigned frameID = (firstWord ) & 0xFFFFFFFFF;
		
		
		frameBuffer[0] = firstWord; //one word was already read
		nWords = DP->getWords(frameBuffer+1,frameSize-1);
		if(nWords < 0) { lastFrameWasBad = true; skippedLoops = 1000004; continue; }
		
		// After a frame, we always have a tailerword word
		uint64_t trailerWord;
		nWords = DP->getWords(&trailerWord, 1);
		if(nWords < 0) { lastFrameWasBad = true; skippedLoops = 1000005; continue; }
// 		printf("DBG4 %016llx \n", trailerWord);
		if(trailerWord != TRAILER_WORD) { 
			for(unsigned i = 0; i < frameSize; i++) { 
				printf("%4d %016llx \n", i, frameBuffer[i]);
			}
			printf("TAIL %016llx \n", trailerWord);
				
				lastFrameWasBad = true; skippedLoops = 1000006; continue; 
		}
		
/*		printf("64 frame received ");
		for(unsigned i = 0; i < frameSize; i++) { 
			printf("%016llx ", frameBuffer[i]);
		}
		printf("\n");
		printf("frame .source = %lu, .type = %lu, .size = %lu\n", frameSource, frameType, frameSize);*/
		
		
// 		if(frameType == 0) {	
// 			unsigned replySize = frameBuffer[1];
// 			uint8_t * replyPayload = (uint8_t *)(frameBuffer+2);
// 	
// 			/*		
//  			printf("8 reply received ");
//  			for(int i = 0; i < replySize; i++)
//  				printf("%02x ", unsigned(replyPayload[i]));
//  			printf("\n");
// 			*/
// 			
// 			uint16_t recvSN = (unsigned(replyPayload[0]) << 8) + replyPayload[1];			
// 
// 			if(frameSize > 32) {
// 				lastFrameWasBad = true;
// 				continue;
// 			}
// 			
// 			if(replySize > (frameSize - 2) * sizeof(uint64_t)) {
// 				lastFrameWasBad = true;
// 				continue;
// 			}
// 			
// 			reply_t *reply = new reply_t;
// 			memcpy(reply->buffer,replyPayload, replySize);
// 			reply->size=replySize;
// 			pthread_mutex_lock(&m->lock);
// 			m->replyQueue.push(reply);
// 			pthread_mutex_unlock(&m->lock);
// 			pthread_cond_signal(&condReplyQueue);
// 		
// 		}
// 		else 
		if(frameType == 1) {
			uint8_t * framePayload = (uint8_t *)frameBuffer;
			
			if (acquisitionMode != 0) {
				bool decodeOK = decodeDataFrame(m, framePayload, frameSize * sizeof(uint64_t), true);
			}
		}
		else {
			printf("Worker: found an unknown frame type!\n");
	
			for(int i=0; i < 2; i++){
				printf("firstnon-idle %016llx\n", (unsigned long long)firstWord);
			}
			printf("\n");
		}
		
		pthread_mutex_lock(&m->lock);
		if(m->dirtyDataFrameQueue.empty()) {
			m->dirtyDataFrameQueue.push(NULL);
		}
		pthread_mutex_unlock(&m->lock);
		pthread_cond_signal(&m->condDirtyDataFrame);		
		pthread_cond_signal(&m->condReplyQueue);
		
		
	}
	delete [] frameBuffer;
	// 	printf("DAQFrameServer::runWorker stats\n");
	// 	printf("\t%10ld frames found\n", nFramesFound);
	// 	printf("\t%10ld frames pushed\n", nFramesPushed);
	printf("DAQFrameServer::runWorker exiting...\n");
}

uint64_t DAQFrameServer::getPortUp()
{
	return DP->getPortUp();
}

uint64_t DAQFrameServer::getPortCounts(int port, int whichCount)
{
	return DP->getPortCounts(port, whichCount);
}
