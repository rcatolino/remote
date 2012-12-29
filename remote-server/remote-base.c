#include <stdio.h>

#include "config.h"
#include "tcpserver.h"
#include "utils.h"

int main(int argc, char *argv[]) {
  int socketd = initServer(DEFAULT_PORT);
  char buff[MAX_CMD_SIZE+1];
  char dbus_buff[MAX_DBUS_BUFF+1];

  if(loadConfig("remote-config.json") == -1) {
    // Bad config file
    return -1;
  }

  while (1) {
    int clients = waitClient(socketd);
    if (clients == -1) {
      // We can't connect to a client.
      return -1;
    }

    int connected = 1;
    while (connected) {
      int ret=receive(clients, buff, MAX_CMD_SIZE);
      if (ret == -1) {
        closeClient(clients);
        connected = 0;
        continue;
      }

      buff[ret]='\0';
      debug("received : %s\n", buff);
      translateCmd(buff, dbus_buff);
    }
  }
}
