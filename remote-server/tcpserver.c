// Listen for commands
#include "tcpserver.h"
#include "utils.h"

#include <arpa/inet.h>
#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static struct sockaddr_in client_address;
static int client_socket;
static GMutex mutex;

static int checkRet(int ret) {
  if (ret==0){
    // Client is shutting down.
    return -1;
  }else if(ret==-1){
    perror("recv on socket failed");
    return -1;
  }else{
    //debug("Received %d bytes.\n", ret);
  }
  return 0;
}

int initServer(int listen_port){
  int listen_socketd;
  uint16_t port;
  struct sockaddr_in own_address;
  int options = 1;

  port = htons(listen_port);
  listen_socketd = socket(AF_INET, SOCK_STREAM, 0);
  if(listen_socketd == -1) {
    perror("tcpserver: socket creation ");
    return -1; //fail to create new socket
  }

  if (setsockopt(listen_socketd,SOL_SOCKET,SO_REUSEADDR,&options,4)) {
    perror("setsockopt ");
  }

  memset(&own_address,0,sizeof(struct sockaddr_in)); //clear struct
  own_address.sin_family = AF_INET; //ipv4
  own_address.sin_port = port; //listening port
  own_address.sin_addr.s_addr = INADDR_ANY; //own address
  if (bind(listen_socketd, (struct sockaddr *) &own_address, sizeof(own_address)) < 0) {
    perror("tcpserver: bind ");
    return -1;
  }

  if (listen(listen_socketd,1) < 0) {
    perror("tcpserver: listen ");
    return -1;
  }

  debug("TCP Server listenning on port %d\n",ntohs(port));
  return listen_socketd;
}

int waitClient(int listen_socket){
  int request_socketd;
  socklen_t client_size = sizeof(client_address);

  request_socketd = accept(listen_socket, (struct sockaddr *) &client_address,
                           &client_size);
  if (request_socketd == -1) {
    perror("tcpserver: accept ");
    return -1;
  }

  debug("tcpserver: new connection\n");
  debug("\tremote address : %s\n", inet_ntoa(client_address.sin_addr));
  debug("\tremote port : %d\n", ntohs(client_address.sin_port));
  client_socket = request_socketd;
  return request_socketd;
}

void closeClient(int client_sock){

  printf("Client deconected\n");
  shutdown(client_sock, SHUT_RDWR);
  close(client_sock);
}

int receive(int socketd, char * buff, size_t buff_size){
  size_t ret = 0;
  size_t cmd_size = 0;
  size_t bytes_rcv = 0;
  // Receive the size of the command.
  ret = recv(socketd, (void*)(&cmd_size), sizeof(cmd_size), 0);
  if (checkRet(ret) == -1) {
    return -1;
  }

  cmd_size = be64toh(cmd_size);
  if (cmd_size > buff_size) {
    debug("Invalid command size, host endian : %ld, network endian : %ld\n",
          cmd_size, htobe64(cmd_size));
    return -1;
  }

  for (bytes_rcv=0; bytes_rcv<cmd_size; ){
    ret=recv(socketd, (void*)(buff+bytes_rcv), cmd_size-bytes_rcv, 0);
    bytes_rcv+=ret;
    if (checkRet(ret) == -1 || bytes_rcv > buff_size) {
      return -1;
    }
  }

  return ret;
}

int transmitMsg(int socketd, char * buff, uint32_t size, char * head,
                uint32_t head_size){
  uint32_t whole_size = size + head_size;
  if (socketd == 0) {
    debug("Socket closed\n");
    return -1;
  }

  if (whole_size > MAX_CMD_SIZE) {
    size = whole_size-MAX_CMD_SIZE;
    whole_size = size + head_size;
  }

  whole_size = htonl(whole_size);
  g_mutex_lock(&mutex);
  if (send(socketd, &whole_size, sizeof(uint32_t), MSG_MORE)==-1) {
    g_mutex_unlock(&mutex);
    perror("send size on socket failed");
    return -1;
  }
  if (head) {
    if (send(socketd, head, head_size, MSG_MORE)==-1) {
      g_mutex_unlock(&mutex);
      perror("send head on socket failed");
      return -1;
    }
  }

  if (send(socketd, buff, size, 0)==-1) {
    g_mutex_unlock(&mutex);
    perror("send on socket failed");
    return -1;
  }

  g_mutex_unlock(&mutex);
  //debug("data sent\n");
  return 0;
}

int transmitFile(int socketd, const char * path) {
  int image;
  int ret;
  struct stat info;
  ssize_t size = 0;
  ssize_t nsize = 0;
  char * buff;

  if (socketd == 0) {
    return -1;
    debug("Socket closed\n");
  }

  debug("Opening %s\n", path);
  image = open(path, O_RDONLY | O_CLOEXEC | O_NOCTTY);
  if (image == -1) {
    perror("open failed ");
    return -1;
  }

  fstat(image, &info);
  if (!S_ISREG(info.st_mode)) {
    debug("Tried to transmit a non regular file\n");
    goto out_fd;
  }

  size = info.st_size;
  if (size > SSIZE_MAX) {
    debug("cover too big, size max : %ld\n", SSIZE_MAX);
    goto out_fd;
  }

  buff = malloc(size);
  ret = read(image, buff, size);
  if (ret != size) {
    debug("could not read whole file : ret = %d, size = %zd\n", ret, size);
    if (ret == -1) {
      perror("read ");
    }
    goto out_buff;
  }

  nsize = htonl(size);
  g_mutex_lock(&mutex);
  if (send(socketd, &nsize, sizeof(uint32_t), MSG_MORE) == -1) {
    g_mutex_unlock(&mutex);
    perror("send file size ");
    goto out_buff;
  }

  debug("Sending file : %zd bytes\n", size);
  if (send(socketd, buff, size, 0) == -1) {
    g_mutex_unlock(&mutex);
    perror("send file ");
    goto out_fd;
  }

  g_mutex_unlock(&mutex);
  close(image);
  free(buff);
  debug("data sent\n");
  return 0;

out_buff:
  free(buff);
out_fd:
  close(image);
  return -1;
}

int transmit(int socketd, char * buff, size_t size) {
  return transmitMsg(socketd, buff, size, NULL, 0);
}

const char * getClientAddress() {
  return inet_ntoa(client_address.sin_addr);
}
