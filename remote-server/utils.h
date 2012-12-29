#ifndef UTILS_H__
#define UTILS_H__

#ifdef DEBUG
  #define debug(...) printf(__VA_ARGS__)
#else
  #define debug(...)
#endif

#define MAX_CMD_SIZE 64
#define MAX_DBUS_BUFF 1024

#define DEFAULT_PORT 52000

#endif
