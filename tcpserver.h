#ifndef TCPSERVER_H__
#define TCPSERVER_H__

int initServer(int listen_port);
// Creates a server listening on 'listen_port'.

int waitClient(int listenSocket);
// Waits for a client to connect on the server. Blocking.

void closeClient(int clientSock);
// Terminates a connection.

int receive(int socketd, char * buff, int size);
// Waits for data on the connection. Blocking.

int transmit(int socketd, char * buff, int size);
// Sends data on the connection. Blocking.

#endif
