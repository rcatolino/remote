#include <stdio.h>
#include "tcpserver.h"
#include "utils.h"

int main(int argc, char *argv[]) {
  int socketd = initServer(5000);
  char buff[100];
  int i = 0;
  int j = 0;
  char * board[150];
  size_t rowsizes[150];
  memset(board, 0, 150*sizeof(char*));
  memset(rowsizes, 0, 150*sizeof(char*));

  while (1) {
    int clients = waitClient(socketd);
    int ret=receive(clients, buff, 99);
    buff[ret-1]='\0';
    printf("received : %s\n", buff);
    FILE * file = fopen(buff,"r");
    if (file==NULL) {
      printf("no such file\n");
      perror("fopen ");
      close(clients);
      continue;
    }

    size_t n = 0;
    i=0;
    while (getline(&board[i], &rowsizes[i], file)!= -1 && i<150) {
      if (i==0) {
        i++;
        continue;
      }
      n=rowsizes[i];
      snprintf(buff,n<100?n:100,"%s",board[i]);
      buff[n<100?n:100]='\0';
      printf("%d\t%s",n,buff);
      i++;
    }

    perror("getline ");
    snprintf(buff,100,"%d\n",i-1);
    buff[100]='\0';
    transmit(clients, buff, strlen(buff));
    printf("%d\n",i);
    for (j=1; j<=i; ++j) {
      transmit(clients, board[j], strlen(board[j])<rowsizes[j]?strlen(board[j]):rowsizes[j]);
    }
    fclose(file);
    close(clients);
  }
}
