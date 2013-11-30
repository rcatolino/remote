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

#define PROFILE_HEAD "PROFILE "
#define PROFILE_HEAD_SZ sizeof(PROFILE_HEAD)-1

#define PROFILES_HEAD "PROFILES "
#define PROFILES_HEAD_SZ sizeof(PROFILES_HEAD)-1

#define POSITION_REQ "POSITION"
#define POSITION_REQ_SZ sizeof(POSITION_REQ)-1

#ifdef AUDIO_FEEDBACK
#define STREAMING_ON_REQ "STREAM_ON"
#define STREAMING_ON_REQ_SZ sizeof(STREAMING_ON_REQ)-1

#define STREAMING_OFF_REQ "STREAM_OFF"
#define STREAMING_OFF_REQ_SZ sizeof(STREAMING_OFF_REQ)-1

#define STREAMING_STOP "STREAM_STOP"
#define STREAMING_STOP_SZ sizeof(STREAMING_STOP)-1
#endif

struct proxyParams {
  const char * name;
  const char * path;
  const char * interface;
  GDBusProxy * proxy;
  int active;

  struct proxyParams * prev;
};

enum argument_type {
  NO_TYPE,
  I64
};

struct callParams;
struct callParams {
  const struct proxyParams * proxy;
  const char * method;
  enum argument_type arg_type;
  void (*custom_call)(struct callParams *, const char *);
};

#endif
