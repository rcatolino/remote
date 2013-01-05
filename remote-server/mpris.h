#ifndef MPRIS_H__
#define MPRIS_H__

#include "utils.h"

#define MPRIS_PLAYER_IF "org.mpris.MediaPlayer2.Player"
#define MPRIS_TRACKLIST_IF "org.mpris.MediaPlayer2.TrackList"

#define SHUFFLE_OFF "SHUFFLE 0"
#define SHUFFLE_ON "SHUFFLE 1"

struct mprisInstance {
  struct proxyParams * player;
  struct proxyParams * tracklist;
  char * title;
  char * artist;
  char * album;
  int length;
  char * next;
  char * previous;
  char * artUrl;
  int shuffle;
  char * loop;
  char * playback;
};

int createMprisInstance(struct proxyParams * pp);
void updateMprisClientSocket(int socketd);

#endif
