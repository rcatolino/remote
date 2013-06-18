#include <arpa/inet.h>
#include <asm/types.h>
#include <gio/gio.h>
#include <glib.h>
#include <ifaddrs.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "smdp.h"
#include "utils.h"

#define ID "remote-server"
#define PROTO "tcp"
#define PORT_SZ 6
#define AD_SZ 16

int netlink_init(struct sockaddr_nl * addr) {
  int ret;
  int nl_sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
  if (!nl_sock) {
    perror("netlink socket error ");
    return -1;
  }

  memset(addr, 0, sizeof(struct sockaddr_nl));
  addr->nl_family = AF_NETLINK;
  addr->nl_groups = /*RTMGRP_LINK | */RTMGRP_IPV4_IFADDR/* | RTMGRP_IPV4_ROUTE*/;
  // see linux/rtnetlink.h
  ret = bind(nl_sock, (struct sockaddr *)addr, sizeof(struct sockaddr_nl));
  if (ret == -1) {
    perror("netlink bind failed ");
    return -1;
  }

  return nl_sock;
}

int addr_change(int nl_sock, struct sockaddr_nl * nladdr) {
  int ret = 0;
  struct nlmsghdr * nlhdr; // See linux/netlink.h
  int length;
  char buff[4096];
  struct iovec msg_iov = {buff, sizeof(buff)}; // See bits/uio.h
  struct msghdr hdr = {nladdr, sizeof(struct sockaddr_nl),
                       &msg_iov, sizeof(msg_iov),
                       NULL, 0, // No ancillary data
                       0}; // No flags
                      // See bits/socket.h

  debug("Netlink, waiting for next message\n");
  // The following loop receive a potentially multipart message
  // See man 3 netlink for the macros
  length = recvmsg(nl_sock, &hdr, sizeof(hdr));
  debug("Message received!\n");
  for (nlhdr = (struct nlmsghdr *)buff; NLMSG_OK(nlhdr, length); nlhdr = NLMSG_NEXT(nlhdr, length)) {
    debug("Parsing message\n");
    if (nlhdr->nlmsg_type == NLMSG_DONE) {
      debug("Netlink, end of multipart message\n");
      break;
    }

    if (nlhdr->nlmsg_type == NLMSG_ERROR) {
      struct nlmsgerr * err;
      // parse the nlmsgerr in the payload
      err = (struct nlmsgerr *) NLMSG_DATA(nlhdr);
      debug("Netlink error : %s\n", strerror(err->error));
      continue;
    }

    if (nlhdr->nlmsg_type == NLMSG_NOOP) {
      debug("Netlink noop\n");
      continue;
    }

    if (nlhdr->nlmsg_type == RTM_NEWADDR) {
      debug("RTM_NEWADDR\n");
      ret = 1;
    }

    if (nlhdr->nlmsg_type == RTM_DELADDR) {
      debug("RTM_DELADDR\n");
      ret = 1;
    }
  }

  return ret;
}

int wait_event(struct pollfd fds[2], struct sockaddr_nl * nladdr, const struct service_t * service) {
  switch (poll(fds, 2, -1)) {
      case -1 :
        derror("wait_for_answer error, poll ");
        return -1;
      case 0 :
        debug("wait_for_answer timeout\n");
        return 0;
      default :
        if (fds[0].revents & POLLIN) {
          debug("New netlink event!\n");
          if (addr_change(fds[0].fd, nladdr)) {
            debug("We have to change the service address\n");
            return 1;
          }
        } else if (fds[1].revents & POLLIN) {
          return wait_for_query(fds[1].fd, service);
        }
        break;
  }

  return 0;
}

int init_service(struct service_t * service, const char * port) {
  char ad[AD_SZ];
  struct ifaddrs * iflist;
  struct ifaddrs * interface;

	if (getifaddrs(&iflist) == -1) {
		debug("serviceBroadcast, failed to get iflist\n");
    return -1;
	}

	debug("Available interfaces :\n");
	for (interface = iflist; interface != NULL; interface = interface->ifa_next){
		struct sockaddr_in* addr;
		char loopback[] = "127.0.0.1";
    char localA[] = "10.";
    char localB[] = "172."; // Does anyone even use class B?
    char localC[] = "192.168.";
		if (interface->ifa_addr == NULL || interface->ifa_addr->sa_family != AF_INET) {
			continue;
		}

		addr = (struct sockaddr_in*)interface->ifa_addr;
    inet_ntop(AF_INET, &addr->sin_addr, ad, AD_SZ);
		debug("\t%s : %s\n", interface->ifa_name, ad);
		if (strcmp(ad, loopback) != 0 &&
        (strncmp(ad, localA, sizeof(localA)-1) == 0 ||
         strncmp(ad, localB, sizeof(localB)-1) == 0 ||
         strncmp(ad, localC, sizeof(localC)-1) == 0)) {
			//this should be a good address, stop looking any further
      debug("Address found\n");
			break;
		}
	}

	freeifaddrs(iflist);
  debug("Creating service : %s://%s:%s\n", PROTO, ad, port);
  if (create_service(service, ID, PROTO, ad, port) == -1) {
    debug("serviceBroadcast, error in create_service\n");
    return -1;
  }

  return 0;
}

void *serviceBroadcast(void *args) {
  struct service_t service;
  char port[PORT_SZ];
  int socket;
  int stop = 0;
  int reinit_needed = 1;

  // Netlink attributes :
  int nl_sock;
  struct sockaddr_nl nladdr;

  // poll attribute :
  struct pollfd fds[2];

  if (!args) {
    debug("serviceBroadcast, error: no args\n");
    return NULL;
  }

  nl_sock = netlink_init(&nladdr);
  debug("serviceBroadcast, start broadcasting\n");
  socket = start_broadcast_server();
  if (socket == -1) {
    debug("Couldn't start broadcast server");
    return NULL;
  }

  if (snprintf(port, PORT_SZ, "%d", *(int*)args) == -1) {
    debug("serviceBroadcast, error, bad port\n");
    return NULL;
  }

  fds[0].fd = nl_sock;
  fds[1].fd = socket;
  fds[0].events = POLLIN;
  fds[1].events = POLLIN;

  while (reinit_needed) {
    if (init_service(&service, port) == -1) {
      debug("Couldn't initialize service\n");
      return NULL;
    }
    reinit_needed = 0;
    stop = 0;

    while (!stop) {
      switch (wait_event(fds, &nladdr, &service)) {
        case -1 :
          debug("serviceBroadcast, failed waiting for query\n");
          stop = 1;
          break;
        case 1 :
          reinit_needed = 1;
          stop = 1;
          break;
      }

      if (stop) continue;

      if (send_service_broadcast(socket, &service) == -1) {
        debug("serviceBroadcast, failed sending broadcasting service\n");
        stop = 1;
        break;
      }
    }

    delete_service(&service);
  }
  return args;
}

