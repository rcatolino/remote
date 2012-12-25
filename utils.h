#ifndef UTILS_H__
#define UTILS_H__

#ifdef DEBUG
  #define debug(...) printf(__VA_ARGS__)
#else
  #define debug(...)
#endif

#endif
