#ifndef STREAMING_SERVER_H__
#define STREAMING_SERVER_H__

#include <glib.h>

#ifdef AUDIO_FEEDBACK

int createStreamingServer(GMainLoop * loop, const char * address);

int deleteStreamingServer();

int startStreaming();

int pauseStreaming();

#endif

#endif
