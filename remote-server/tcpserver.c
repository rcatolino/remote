// Listen for commands
#include "tcpserver.h"
#include "utils.h"

#include <arpa/inet.h>
#include <errno.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static struct sockaddr_in client_address;
static int client_socket;

static int checkRet(int ret, int socketd) {
  if (ret==0){
    // Client is shutting down.
    return -1;
  }else if(ret==-1){
    perror("recv on socket failed");
    return -1;
  }else{
    debug("Received %d bytes.\n", ret);
  }
  return 0;
}

void handler(int sigNb) {
  debug("WATDAFUCK???\n");
  closeClient(client_socket);
}

int initServer(int listen_port){

	int listen_socketd;
	uint16_t port;
	struct sockaddr_in own_address;
	int options = 1;

	port = htons(listen_port);
	listen_socketd = socket(AF_INET, SOCK_STREAM, 0);

	if(listen_socketd == -1) {
		perror("socket creation ");
		return -1; //fail to create new socket
	}
	if (setsockopt(listen_socketd,SOL_SOCKET,SO_REUSEADDR,&options,4)) {
		perror("setsockopt ");
	}
	memset(&own_address,0,sizeof(struct sockaddr_in)); //clear struct
	own_address.sin_family = AF_INET; //ipv4
	own_address.sin_port = port; //listening port
	own_address.sin_addr.s_addr = INADDR_ANY; //own address... 

	if (bind(listen_socketd, (struct sockaddr *) &own_address, sizeof(own_address)) < 0) {
		//fail to name socket;
		perror("bind ");
		return -1;
	}

	if (listen(listen_socketd,1) < 0)
	{
		perror("listen ");
		return -1;
	}
	debug("Server listenning on port %d\n",ntohs(port));
  return listen_socketd;
}

int waitClient(int listenSocket){

	int request_socketd;

	socklen_t client_size = sizeof(client_address);

  request_socketd = accept(listenSocket,\
    (struct sockaddr *) &client_address, &client_size);
  if (request_socketd == -1)
  {
    perror("accept ");
    return -1;
  }
  debug("new connection\n");
  debug("\tremote address : %s\n", inet_ntoa(client_address.sin_addr));
  debug("\tremote port : %d\n", ntohs(client_address.sin_port));

  client_socket = request_socketd;
	return request_socketd;
}

void closeClient(int clientSock){

  debug("Client deconected\n");
  shutdown(clientSock, SHUT_RDWR);
	close(clientSock);
}

int receive(int socketd, char * buff, int size){
  size_t ret = 0;
  size_t cmdSize = 0;
  size_t bytesRcv = 0;
  // Receive the size of the command.
  ret=recv(socketd, (void*)(&cmdSize), sizeof(cmdSize), 0);
  if (checkRet(ret, socketd) == -1 || cmdSize > MAX_CMD_SIZE) {
    if (cmdSize > MAX_CMD_SIZE) debug("Invalid command size\n");
    return -1;
  }

  debug("New command to be received\n");
  for (bytesRcv=0; bytesRcv<cmdSize; ){
    ret=recv(socketd, (void*)(buff+bytesRcv), cmdSize-bytesRcv, 0);
    bytesRcv+=ret;
    if (checkRet(ret, socketd) == -1 || bytesRcv > MAX_CMD_SIZE) {
      return -1;
    }
  }

  debug("New command received\n");
  return ret;
}

int transmit(int socketd, char * buff, int size){
  if (size > MAX_CMD_SIZE) {
    return -1;
  }

  if (send(socketd, &size, 1, MSG_MORE)==-1) {
    perror("send size on socket failed");
    return -1;
  }

  if (send(socketd, buff, size, 0)==-1) {
    perror("send on socket failed");
    return -1;
  }

  debug("data sent\n");
  return 0;
}

