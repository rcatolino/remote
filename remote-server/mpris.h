#ifndef MPRIS_H__
#define MPRIS_H__

#include "utils.h"

#define MPRIS_PLAYER_IF "org.mpris.MediaPlayer2.Player"
#define MPRIS_TRACKLIST_IF "org.mpris.MediaPlayer2.TrackList"

#define SHUFFLE_OFF "SHUFFLE 0"
#define SHUFFLE_ON "SHUFFLE 1"
#define LOOP_HEAD "LOOP "
#define PLAYBACK_HEAD "PLAYBACK "
#define LOOP_HEAD_SZ sizeof(LOOP_HEAD)-1
#define PLAYBACK_HEAD_SZ sizeof(PLAYBACK_HEAD)-1
#define SHUFFLE_SZ sizeof(SHUFFLE_OFF)-1

#define TRACK_TITLE "TRACK TITLE "
#define TRACK_TITLE_SZ sizeof(TRACK_TITLE)-1
#define TRACK_ARTIST "TRACK ARTIST "
#define TRACK_ARTIST_SZ sizeof(TRACK_ARTIST)-1
#define TRACK_ALBUM "TRACK ALBUM "
#define TRACK_ALBUM_SZ sizeof(TRACK_ALBUM)-1
#define TRACK_ARTURL "TRACK ARTURL "
#define TRACK_ARTURL_SZ sizeof(TRACK_ARTURL)-1
#define TRACK_LENGTH "TRACK LENGTH "
#define TRACK_LENGTH_SZ sizeof(TRACK_LENGTH)-1

#define TITLE   0x01
#define ARTIST  0x02
#define ALBUM   0x04
#define LENGTH  0x08
#define ARTURL  0x10

#define HAS_TITLE(a)  (a & TITLE)
#define HAS_ARTIST(a) (a & ARTIST)
#define HAS_ALBUM(a)  (a & ALBUM)
#define HAS_LENGTH(a) (a & LENGTH)
#define HAS_ARTURL(a) (a & ARTURL)

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

void printMprisData();
void sendCachedData();
int createMprisInstance(struct proxyParams * pp);
void updateMprisClientSocket(int socketd);

#endif