#ifndef TCPSERVER_H__
#define TCPSERVER_H__

#include <stdint.h>

#define FILE_BUF_SIZE 10000

const char * getClientAddress();
// The address string is stored in a static buffer allocated by inet_ntoa
// thus each call to inet_ntoa/get_address will override it.

int initServer(int listen_port);
// Creates a server listening on 'listen_port'.

int waitClient(int listen_socket);
// Waits for a client to connect on the server. Blocking.

void closeClient(int client_sock);
// Terminates a connection.

int receive(int socketd, char * buff, int size);
// Waits for data on the connection. Blocking.

int transmit(int socketd, char * buff, int size);
// Sends data on the connection. Blocking.

int transmitMsg(int socketd, char * buff, uint32_t size, char * head, uint32_t head_size);
// Sends head, then data. Blocking.

int transmitFile(int socketd, const char * path);
// Sends file. Blocking.
#endif
