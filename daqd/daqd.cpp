#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <assert.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>
#include <map>

#include "UDPFrameServer.hpp"
#include "Protocol.hpp"
#include "Client.hpp"
#include <boost/lexical_cast.hpp>

/*
 * Used to stop on CTRL-C or kill
 */
static bool globalUserStop = 0;
static FrameServer *globalFrameServer = NULL;

static void catchUserStop(int signal) {
	fprintf(stderr, "Caught signal %d\n", signal);
	globalUserStop = 1;
}

static int createListeningSocket(char *socketName);
static void pollSocket(int listeningSocket, FrameServer *frameServer);



int main(int argc, char *argv[])
{
	if(argc < 2) {
		fprintf(stderr, "Usage: %s </path/to/socket>\n", argv[0]);
		return 1;
	}
	char *socketName = argv[1];
	
	int debugLevel = 0;
	if(argc >= 3)
		debugLevel = boost::lexical_cast<int>(argv[2]);
		

 	globalUserStop = false;					
	signal(SIGTERM, catchUserStop);
	signal(SIGINT, catchUserStop);

	int listeningSocket = createListeningSocket(socketName);
	if(listeningSocket < 0)
		return -1;

 	globalFrameServer = new UDPFrameServer(debugLevel);
	
	pollSocket(listeningSocket, globalFrameServer);	
	close(listeningSocket);	
	unlink(socketName);

	delete globalFrameServer;

	return 0;
}

int createListeningSocket(char *socketName)
{
	struct sockaddr_un address;
	int socket_fd = -1;
	socklen_t address_length;
	
	if((socket_fd = socket(PF_UNIX, SOCK_STREAM, 0)) == -1) {
		fprintf(stderr, "Could not allocate socket (%d)\n", errno);
		return -1;
	}

	
	memset(&address, 0, sizeof(struct sockaddr_un));
	address.sun_family = AF_UNIX;
	snprintf(address.sun_path, PATH_MAX, socketName);


	if(bind(socket_fd, (struct sockaddr *) &address, sizeof(struct sockaddr_un)) != 0) {
		fprintf(stderr, "Could not bind() socket (%d)\n", errno);
		fprintf(stderr, "Check that no other instance is running and remove %s\n", socketName);
		return -1;
	}
	
	if(chmod(socketName, 0660) != 0) {
		perror("Could not not set socket permissions\n");
		return -1;
	}
	
	if(listen(socket_fd, 5) != 0) {
		fprintf(stderr, "Could not listen() socket (%d)\n", errno);
		return -1;
	}
	
	return socket_fd;
}

void pollSocket(int listeningSocket, FrameServer *frameServer)
{
	sigset_t mask, omask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGTERM);
	
	struct epoll_event event;	
	int epoll_fd = epoll_create(10);
	if(epoll_fd == -1) {
	  fprintf(stderr, "Error %d on epoll_create()\n", errno);
	  return;
	}
	
	// Add the listening socket
	memset(&event, 0, sizeof(event));
	event.data.fd = listeningSocket;
	event.events = EPOLLIN;
	if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listeningSocket, &event) == -1) {
	  fprintf(stderr, "Error %d on epoll_ctl()\n", errno);
	  return;
	  
	}
	
	std::map<int, Client *> clientList;
	
	while (true) {
		if (sigprocmask(SIG_BLOCK, &mask, &omask)) {
			fprintf(stderr, "Could not set sigprockmask() (%d)\n", errno);
			break;
		}
		  
		if(globalUserStop == 1) {
			sigprocmask(SIG_SETMASK, &omask, NULL);
			break;
		}
	  
		// Poll for one event
		memset(&event, 0, sizeof(event));
		int nReady = epoll_pwait(epoll_fd, &event, 1, 100, &omask);		
		sigprocmask(SIG_SETMASK, &omask, NULL);
	  
		if (nReady == -1) {
		  fprintf(stderr, "Error %d on epoll_pwait()\n", errno);
		  break;
		  
		}
		else if (nReady < 1)
		  continue;
		
		if(event.data.fd == listeningSocket) {
			int client = accept(listeningSocket, NULL, NULL);
			fprintf(stderr, "Got a new client: %d\n", client);
			
			// Add the event to the list
			memset(&event, 0, sizeof(event));
			event.data.fd = client;
			event.events = EPOLLIN | EPOLLERR | EPOLLHUP;
			epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client, &event);
			
			clientList.insert(std::pair<int, Client *>(client, new Client(client, frameServer)));
		}		
		else {
//		  fprintf(stderr, "Got a client (%d) event %08llX\n", event.data.fd, event.events);
		  if ((event.events & EPOLLHUP) || (event.events & EPOLLERR)) {
			fprintf(stderr, "Client hung up or error\n");
			epoll_ctl(epoll_fd, EPOLL_CTL_DEL, event.data.fd, NULL);
			delete clientList[event.data.fd]; clientList.erase(event.data.fd);
		  }
		  else if (event.events & EPOLLIN) {			
			Client * client = clientList[event.data.fd];
			int actionStatus = client->handleRequest();
			
			if(actionStatus == -1) {
				fprintf(stderr, "Error handling client %d\n", event.data.fd);
				epoll_ctl(epoll_fd, EPOLL_CTL_DEL, event.data.fd, NULL);
				delete clientList[event.data.fd]; clientList.erase(event.data.fd);
				continue;
			}
			
		  }
		  else {
			fprintf(stderr, "Event was WTF\n");
			epoll_ctl(epoll_fd, EPOLL_CTL_DEL, event.data.fd, NULL);
			delete clientList[event.data.fd]; clientList.erase(event.data.fd);
		  }
		  
		  
		  
		}
		  
		
		
	}
	
	
  
}
