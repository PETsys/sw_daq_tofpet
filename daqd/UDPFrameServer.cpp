#include "UDPFrameServer.hpp"
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

static const char *feAddr = "192.168.1.25";
static const unsigned short fePort = 2000;

//static const int N_ASIC=4;



UDPFrameServer::UDPFrameServer(int debugLevel)
	: FrameServer(debugLevel)
{
	udpSocket = socket(AF_INET,SOCK_DGRAM,0);
	assert(udpSocket != -1);
	
	// "Connect" our socket to the front end service
	struct sockaddr_in localAddress;	
	memset(&localAddress, 0, sizeof(struct sockaddr_in));
	localAddress.sin_family = AF_INET;
	localAddress.sin_addr.s_addr=inet_addr(feAddr);  
	localAddress.sin_port=htons(fePort);
	int r = connect(udpSocket, (struct sockaddr *)&localAddress, sizeof(struct sockaddr_in));
	assert(r == 0);
	
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 100000;
	r = setsockopt(udpSocket, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv));
	assert(r == 0);
	
	char buffer[32];
	memset(buffer, 0xFF, sizeof(buffer));
	send(udpSocket, buffer, sizeof(buffer), 0);
		
	startWorker();
}


UDPFrameServer::~UDPFrameServer()
{

}



void *UDPFrameServer::doWork(void *arg)
{	
	printf("UDPFrameServer::runWorker starting...\n");
	UDPFrameServer *m = (UDPFrameServer *)arg;
	
	for(int i = 0; i < N_ASIC * 64 * 4; i++) {
		m->tacLastEventTime[i] = 0;
	}
	for(int i = 0; i < N_ASIC * 64; i++) {
		m->channelLastEventTime[i] = 0;
	}
	
	long nFramesFound = 0;
	long nFramesPushed = 0;
	
	
	unsigned char rxBuffer[2048];
	while(!m->die) {
		int r = recv(m->udpSocket, rxBuffer, sizeof(rxBuffer), 0);

		if (r == -1 && (errno = EAGAIN || errno == EWOULDBLOCK)) {
			pthread_cond_signal(&m->condReplyQueue);
			
			pthread_mutex_lock(&m->lock);
			if(m->dirtyDataFrameQueue.empty()) {
				m->dirtyDataFrameQueue.push(NULL);
			}
			pthread_mutex_unlock(&m->lock);
			pthread_cond_signal(&m->condDirtyDataFrame);
			
			continue;
		}
		
		
		assert(r >= 0);
		
		
		if(rxBuffer[0] == 0x5A) {
			if(m->debugLevel > 2) printf("Worker:: Found a reply frame with %d bytes\n", r);
			for(int i = 0; i < r; i++) {
				printf("%02x ", unsigned(rxBuffer[i]) & 0xFF);
			}
			printf("\n");
			reply_t *reply = new reply_t;
			memcpy(reply->buffer, rxBuffer + 1, r - 1);
			reply->size = r - 1;
			pthread_mutex_lock(&m->lock);
			m->replyQueue.push(reply);
			pthread_mutex_unlock(&m->lock);
		}
		else if(rxBuffer[0] == 0xA5) {
			if(m->debugLevel > 2) printf("Worker:: Found a data frame with %d bytes\n", r);
			if (m->acquisitionMode != 0) {			
				unsigned char *tmp = rxBuffer + 1;
				do {
					
					int  dfLength = (tmp[0] << 8) + tmp[1];
					tmp += 2;
					
					if((tmp + dfLength) > (rxBuffer + r))
						break;
					
					bool decodeOK = decodeDataFrame(m, tmp, dfLength);
					if(!decodeOK) break;
					
					tmp += dfLength;
				}while (tmp < rxBuffer + r);
			}
			pthread_mutex_lock(&m->lock);
			if(m->dirtyDataFrameQueue.empty()) {
				m->dirtyDataFrameQueue.push(NULL);
			}
			pthread_mutex_unlock(&m->lock);
			pthread_cond_signal(&m->condDirtyDataFrame);	
		}
		else {
			printf("Worker: found an unknown frame (0x%02X) with %d bytes\n", unsigned(rxBuffer[0]), r);
			
		}
		pthread_cond_signal(&m->condReplyQueue);
			
		
		
	}
	

// 	printf("UDPFrameServer::runWorker stats\n");
// 	printf("\t%10ld frames found\n", nFramesFound);
// 	printf("\t%10ld frames pushed\n", nFramesPushed);
	printf("UDPFrameServer::runWorker exiting...\n");
	return NULL;
}


int UDPFrameServer::sendCommand(char *buffer, int bufferSize, int commandLength)
{
	send(udpSocket, buffer, commandLength, 0);
	
	
	
	boost::posix_time::ptime start = boost::posix_time::microsec_clock::local_time();
	
	int replyLength = 0;
	do {
		pthread_mutex_lock(&lock);
		reply_t *reply = NULL;
		
		if(replyQueue.empty()) {
			pthread_cond_wait(&condReplyQueue, &lock);
		}
		
		if (!replyQueue.empty()) {
			reply = replyQueue.front();
			replyQueue.pop();
		}
		pthread_mutex_unlock(&lock);
		
		if(reply == NULL) {
			boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
			if ((now - start).total_milliseconds() > 200) 
				break;
			else
				continue;
			
		}
		
		// Check that command and this reply have the same SN
		if(reply->size >= 2 && reply->buffer[0] == buffer[0] && reply->buffer[1] == buffer[1]) {
			memcpy(buffer, reply->buffer, reply->size);
			replyLength = reply->size;
		}
		delete reply;
	} while(replyLength == 0);
		
	return replyLength;
}
