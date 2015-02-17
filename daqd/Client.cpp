#include "Client.hpp"
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
using namespace std;

Client::Client(int socket, FrameServer *frameServer)
: socket(socket), frameServer(frameServer)
{
}

Client::~Client()
{
	close(socket);
	for(map<int, FrameServer::DataFramePtr *>::iterator it = ownedDataFramePtrs.begin(); it != ownedDataFramePtrs.end(); it++) {
		frameServer->returnDataFramePtr(it->second);
	}
}

struct CmdHeader_t { uint16_t type; uint16_t length; };

int Client::handleRequest()
{	
	CmdHeader_t cmdHeader;
	
	int nBytesRead = recv(socket, socketBuffer, sizeof(cmdHeader), 0);
	if(nBytesRead !=  sizeof(cmdHeader)) {
		fprintf(stderr, "Could not read() %u bytes from client %d\n", sizeof(CmdHeader_t), socket);
		return -1;
	}
	
	memcpy(&cmdHeader, socketBuffer, sizeof(CmdHeader_t));
	
	if(cmdHeader.length > sizeof(socketBuffer))
		return -1;
	
	int nBytesNext = cmdHeader.length - sizeof(CmdHeader_t);
	if(nBytesNext > 0) {
		if(recv(socket, socketBuffer+sizeof(CmdHeader_t), nBytesNext, 0) != nBytesNext) {
			fprintf(stderr, "Could not read() %d bytes from client %d\n", nBytesNext, socket);
			return -1;
		}
	}
	
	int actionStatus = 0;
//	fprintf(stderr, "commandType = %d, commandLength = %d\n", int(cmdHeader.type), int(cmdHeader.length));
	if (cmdHeader.type == commandAcqOnOff)
		doAcqOnOff();
	else if(cmdHeader.type == commandGetDataFrameSharedMemoryName) 
		actionStatus = doGetDataFrameSharedMemoryName();
	else if(cmdHeader.type == commandGetDataFrameBuffers)
		actionStatus = doGetEventFrameBuffers();
	else if(cmdHeader.type == commandReturnDataFrameBuffers)
		actionStatus = doReturnEventFrameBuffers();
	else if(cmdHeader.type == commandToFrontEnd)
		actionStatus = doCommandToFrontEnd(nBytesNext);
	if(cmdHeader.type == commandGetTOFPETDataFRame)
		actionStatus = doGetTOFPETDataFrame();
	
	
	if(actionStatus == -1) {
		fprintf(stderr, "Error handling client %d, command was %u\n", socket, unsigned(cmdHeader.type));
		return -1;
	}
	
	return 0;
}

int Client::doCommandToFrontEnd(int commandLength)
{
	char buffer[256];
	memcpy(buffer, socketBuffer + sizeof(CmdHeader_t), commandLength);
	int portID = buffer[0];
	int slaveID = buffer[1];
	int replyLength = frameServer->sendCommand(portID, slaveID, buffer+2, sizeof(buffer), commandLength-2);
	
	uint16_t trl = replyLength;

	send(socket, &trl, sizeof(trl), MSG_NOSIGNAL);
	send(socket, buffer+2, replyLength, MSG_NOSIGNAL);
	return 0;
}

int Client::doAcqOnOff()
{

	uint16_t acqMode = 0;
	memcpy(&acqMode, socketBuffer + sizeof(CmdHeader_t), sizeof(uint16_t));
	printf("Client::doAcqOnOff() called with acqMode = %hu\n", acqMode);
	if (acqMode != 0) 
		frameServer->startAcquisition(acqMode);
	else
		frameServer->stopAcquisition();
	printf("Client::doAcqOnOff() exiting...\n");
	return 0;
}

int Client::doGetEventFrameBuffers()
{
	uint16_t nFramesRequested = 0;
	uint16_t nonEmpty = 0;
	memcpy(&nFramesRequested, socketBuffer + sizeof(CmdHeader_t), sizeof(uint16_t));
	memcpy(&nonEmpty, socketBuffer + sizeof(CmdHeader_t) + sizeof(uint16_t), sizeof(uint16_t));
	
//	printf("Client requested %u frames\n", nFramesRequested);

	unsigned maxFramesPerRequest = 1024;
	nFramesRequested = nFramesRequested < maxFramesPerRequest ? nFramesRequested : maxFramesPerRequest;
	int32_t frameBufferIndexes[maxFramesPerRequest];
	
	uint16_t nFramesForClient = 0;
	
	for(unsigned n = 0; n < nFramesRequested; n++) {		
		FrameServer::DataFramePtr *dataFramePtr = frameServer->getDataFrameByPtr(nonEmpty == 0 ? false : true);
		if (dataFramePtr == NULL) {
			fprintf(stderr, "Client asked for %d frames, but will only get %d\n", nFramesRequested, nFramesForClient);
			fprintf(stderr, "Client owns %u frame buffers\n", ownedDataFramePtrs.size());
			break;
		}
		
		int index  = dataFramePtr->index;		
		ownedDataFramePtrs.insert(pair<int, FrameServer::DataFramePtr *>(index, dataFramePtr));
		frameBufferIndexes[n] = index;
		nFramesForClient += 1;

	}

	struct { uint16_t length; uint16_t nFrames; int32_t indexes[]; } header;
	header.length = sizeof(header) + nFramesForClient * sizeof(int32_t);
	header.nFrames = nFramesForClient;

//	fprintf(stderr, "About to send %hu frames to client\n", nFramesForClient);
	int status = send(socket, &header, sizeof(header), MSG_NOSIGNAL);
	if(status < sizeof(header)) return -1;
//	fprintf(stderr, "... header sent\n");
	status = send(socket, frameBufferIndexes, nFramesForClient*sizeof(int32_t), MSG_NOSIGNAL);
	if(status < nFramesForClient*sizeof(int32_t)) return -1;
//	fprintf(stderr, "... list sent\n");
	
	
	return 0;
}

int Client::doGetTOFPETDataFrame()
{
	struct { uint64_t id; uint16_t valid; uint16_t nEvents; uint16_t lost; uint16_t vetoed; } header =  { 0, 0, 0, 0, 0 };	
	FrameServer::DataFramePtr *dataFramePtr = frameServer->getDataFrameByPtr(false);
	if (dataFramePtr == NULL) {
		int status = send(socket,  &header, sizeof(header), MSG_NOSIGNAL);
		return status;
	}
	DataFrame &df = *(dataFramePtr->p);
	
	header.valid = 1;
	header.id = df.frameID;
	header.lost = df.frameLost ? 1 : 0;
	header.vetoed = 0;
	unsigned tofpetEvents = 0;
	for(unsigned i = 0; i  < df.nEvents; i++)
		if(df.events[i].type == 0) tofpetEvents ++;
	header.nEvents = tofpetEvents;
	int status = send(socket,  &header, sizeof(header), MSG_NOSIGNAL);
	if(status < sizeof(header)) return -1;
	
	for(unsigned i = 0; i  < df.nEvents; i++) {
		Event &e = df.events[i];
		if(e.type != 0) continue;
		
		struct {uint64_t tacIdleTime; 
			uint64_t channelIdleTime; 
			uint16_t asicID; uint16_t channelID; uint16_t tacID;
			uint16_t tCoarse; uint16_t eCoarse;
			uint16_t tFine; uint16_t eFine;
		} event = {
			e.d.tofpet.tacIdleTime,
			e.d.tofpet.channelIdleTime,
			e.asicID, e.channelID, e.d.tofpet.tacID,
			e.tCoarse, e.d.tofpet.eCoarse,
			e.d.tofpet.tFine, e.d.tofpet.eFine
		};
		
		status = status = send(socket,  &event, sizeof(event), MSG_NOSIGNAL);
		if(status < 0) break;	
	}
	frameServer->returnDataFramePtr(dataFramePtr);
	
	return status;
}

int Client::doReturnEventFrameBuffers()
{
	uint16_t nFramesRequested;
	memcpy(&nFramesRequested, socketBuffer + sizeof(CmdHeader_t), sizeof(uint16_t));
//	fprintf(stderr, "Returning %hu frames\n", nFramesRequested);
	for(unsigned n = 0;  n < nFramesRequested; n++) {		
		int32_t index = -1;
		unsigned char *p = socketBuffer +  sizeof(CmdHeader_t) + sizeof(uint16_t) + n*sizeof(int32_t);
		memcpy(&index, p, sizeof(int32_t));
//		fprintf(stderr, "... returning index %d\n", index);	
		if(ownedDataFramePtrs.count(index) > 0) {
			FrameServer::DataFramePtr *dataFramePtr = ownedDataFramePtrs[index];
			ownedDataFramePtrs.erase(index);
			frameServer->returnDataFramePtr(dataFramePtr);
		}
	}
	return 0;
	
}

int Client::doGetDataFrameSharedMemoryName()
{
	const char *name = frameServer->getDataFrameSharedMemoryName();
	DataFrame *d1;
	char *p1 = (char *)d1;
	char *p2 = (char *)&(d1->events[0]);
	char *p3 = (char *)&(d1->events[1]);
	
	struct { uint16_t length;  uint64_t sizes[3]; } header;
	header.length = sizeof(header) + strlen(name);
	header.sizes[0] = sizeof(DataFrame);
	header.sizes[1] = p2 - p1;
	header.sizes[2] = p3 - p2;
	
	int status = 0;
	status = send(socket, &header, sizeof(header), MSG_NOSIGNAL);
	if(status < sizeof(header)) return -1;
	
	status = send(socket, name, strlen(name), MSG_NOSIGNAL);
	if(status < strlen(name)) return -1;
	
	return 0;
}
