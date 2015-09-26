#include <arpa/inet.h>
#include <asm/types.h>
#include <gio/gio.h>
#include <glib.h>
#include <ifaddrs.h>
#include <libmnl/libmnl.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "smdp.h"
#include "utils.h"

#define DEBUG 1
#define ID "remote-server"
#define PROTO "tcp"
#define PORT_SZ 6
#define AD_SZ 16

/*
int nlmsg_cb(struct nlattr* attr, void *data) {
  if (attr->nlmsg_type == NLMSG_ERROR) {
    struct nlmsgerr * err;
    // parse the nlmsgerr in the payload
    err = (struct nlmsgerr *) NLMSG_DATA(nlhdr);
    debug("Netlink error : %s\n", strerror(err->error));
    continue;
  } else if (nlhdr->nlmsg_type == NLMSG_NOOP) {
    debug("Netlink noop\n");
    continue;
  } else if (nlhdr->nlmsg_type == RTM_NEWADDR) {
    debug("RTM_NEWADDR\n");
    ret = 1;
  } else if (nlhdr->nlmsg_type == RTM_DELADDR) {
    debug("RTM_DELADDR\n");
    ret = 1;
  } else if (ret == 0) debug("nlmsg_type : %d\n", nlhdr->nlmsg_type);

  return MNL_CB_OK;
}
*/

int addr_change(struct mnl_socket* nls) {
  int ret = 0;
  ssize_t length;
  int len = 0;
  char buff[MNL_SOCKET_BUFFER_SIZE];

  debug("Netlink, waiting for next message\n");
  // The following loop receive a potentially multipart message
  // See man 3 netlink for the macros
  // length = recvmsg(nl_sock, &hdr, sizeof(hdr));
  if((length = mnl_socket_recvfrom(nls, buff, sizeof(buff))) == -1) {
      perror("netlink recvfrom error ");
      return -1;
  }

  debug("Message received!\n");

  for (struct nlmsghdr* nlhdr = (struct nlmsghdr *)buff;
       mnl_nlmsg_ok(nlhdr, length);
       nlhdr = mnl_nlmsg_next(nlhdr, &len)) {
    debug("Parsing message %d\n", len);
    mnl_nlmsg_fprintf(stdout, nlhdr, length, 0);
    // Assume its an adress change :
    ret = 1;

    /*
    switch (mnl_attr_pars(nlhdr, 0, nlmsg_cb, NULL)) {
      case MNL_CB_ERROR:
        return -1;
      case MNL_CB_OK:
        return 1;
      case MNL_CB_STOP:
        return 0;
    }
    */
  }

  return ret;
}

int wait_event(struct pollfd fds[2], struct mnl_socket* nls,
               const struct service_t * service) {
  switch (poll(fds, 2, -1)) {
      case -1 :
        derror("wait_for_query error, poll ");
        return -1;
      case 0 :
        debug("wait_for_query timeout\n");
        return 0;
      default :
        if (fds[0].revents & POLLIN) {
          debug("New netlink event!\n");
          if (addr_change(nls)) {
            debug("We have to change the service address\n");
            return 1;
          }
        } else if (fds[1].revents & POLLIN) {
          int ret;
          debug("\nNew event on multicast socket\n\n");
          ret = wait_for_query(fds[1].fd, service, 1);
          return (ret == 1) ? -1 : 2;
        }
        break;
  }

  debug("Uninteresting event\n");
  return 0;
}

int init_service(struct service_t * service, const char * port) {
  char ad[AD_SZ];
  struct in_addr local_addr = {.s_addr = INADDR_ANY};
  int ifindex = 0;
  struct ifaddrs * iflist;
  struct ifaddrs * interface;
  int socket = 0;

	if (getifaddrs(&iflist) == -1) {
		debug("serviceBroadcast, failed to get iflist\n");
    return -1;
	}

	debug("Available interfaces :\n");
	for (interface = iflist; interface != NULL; interface = interface->ifa_next) {
		struct sockaddr_in* addr;
		char loopback[] = "127.0.0.1";
    char localA[] = "10.";
    char localB[] = "172."; // Does anyone even use class B?
    char localC[] = "192.168.";
		if (interface->ifa_addr == NULL || interface->ifa_addr->sa_family != AF_INET) {
			continue;
		}

		addr = (struct sockaddr_in*)interface->ifa_addr;
    local_addr = addr->sin_addr;
    ifindex = if_nametoindex(interface->ifa_name);
    inet_ntop(AF_INET, &addr->sin_addr, ad, AD_SZ);
		debug("\t%s[%d] : %s\n", interface->ifa_name, ifindex, ad);
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
  debug("serviceBroadcast, start broadcasting\n");
  socket = start_broadcast_server(local_addr, ifindex);
  if (socket == -1) {
    debug("Couldn't start broadcast server");
    return -1;
  }

  debug("Creating service : %s://%s:%s\n", PROTO, ad, port);
  if (create_service(service, ID, PROTO, ad, port) == -1) {
    debug("serviceBroadcast, error in create_service\n");
    return -1;
  }

  return socket;
}

void *serviceBroadcast(void *args) {
  struct service_t service;
  char port[PORT_SZ];
  int stop = 0;
  int reinit_needed = 1;

  // Netlink attributes :
  struct mnl_socket *nls;

  // poll attribute :
  struct pollfd fds[2];

  if (!args) {
    debug("serviceBroadcast, error: no args\n");
    return NULL;
  }

  if (snprintf(port, PORT_SZ, "%d", *(int*)args) == -1) {
    debug("serviceBroadcast, error, bad port\n");
    return NULL;
  }

  if ((nls = mnl_socket_open(NETLINK_ROUTE)) == (void*)-1) {
    perror("netlink handle open error ");
    return NULL;
  }

  if (mnl_socket_bind(nls, RTMGRP_IPV4_IFADDR, 0)) {
    perror("netlink socket bind error ");
    return NULL;
  }

  fds[0].fd = mnl_socket_get_fd(nls);
  fds[0].events = POLLIN;

  while (reinit_needed) {
    fds[1].fd = init_service(&service, port);
    fds[1].events = POLLIN;
    if ( fds[1].fd == -1) {
      debug("Couldn't initialize service\n");
      return NULL;
    }
    reinit_needed = 0;
    stop = 0;

    while (!stop) {
      switch (wait_event(fds, nls, &service)) {
        case -1 :
          debug("serviceBroadcast, failed waiting for query\n");
          // stop = 1; Don't stop because a message is invalid
          break;
        case 1 :
          reinit_needed = 1;
          stop = 1;
          break;
        case 0 :
          stop = 0;
          break;
        case 2 :
          if (send_service_broadcast(fds[1].fd, &service) == -1) {
            debug("serviceBroadcast, failed sending broadcasting service\n");
            stop = 1;
            break;
          }
          break;
      }
    }

    delete_service(&service);
    stop_broadcast_server(fds[1].fd);
  }
  return args;
}

