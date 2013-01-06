#ifndef UTILS_H__
#define UTILS_H__

#include <gio/gio.h>
#include <stdio.h>
#include "mpris.h"

#ifdef DEBUG
  #define debug(...) printf(__VA_ARGS__)
#else
  #define debug(...)
#endif

#define MAX_CMD_SIZE 255
#define MAX_DBUS_BUFF 1024

#define DEFAULT_PORT 52000

#define DEFAULT_CONFIG "remote-config.json"

struct proxyParams {
  const char * name;
  const char * path;
  const char * interface;
  GDBusProxy * proxy;
  //GHashTable * feedback_table;

  struct proxyParams * prev;
};

struct callParams {
  const struct proxyParams * proxy;
  const char * method;
};

#endif
