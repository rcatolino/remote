#include "dbusif.h"
#include "mpris.h"
#include "tcpserver.h"
#include "utils.h"

#include <arpa/inet.h>
#include <endian.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <string.h>

static struct mprisInstance * mpris_data = NULL;
static int client_socket;

char * pathFromUrl(char * url) {
  if (url[0]=='/') {
    return url;
  }

  if (strncmp(url, URL_FILE_HEAD, URL_FILE_HEAD_SZ) == 0) {
    return url+URL_FILE_HEAD_SZ;
  }

  return url;
}

void printMprisData() {
  if (mpris_data == NULL) {
    return;
  }
  printf("Playback : %s, Loop : %s, Shuffle : %d.\n\tTitle : %s, Artist : %s,\
Album : %s, Album cover location : %s, Position : %ld, Track Length : %ld\n", mpris_data->playback,
mpris_data->loop, mpris_data->shuffle, mpris_data->title, mpris_data->artist,
mpris_data->album, mpris_data->artUrl, mpris_data->position, mpris_data->length);
}

static void sendPlaybackStatus() {
  if (mpris_data->playback == NULL) {
    return;
  }
  debug("client socket : %d\n", client_socket);
  transmitMsg(client_socket, mpris_data->playback, strlen(mpris_data->playback),
              PLAYBACK_HEAD, PLAYBACK_HEAD_SZ);
}

static void sendLoopStatus() {
  if (mpris_data->loop == NULL) {
    return;
  }
  transmitMsg(client_socket, mpris_data->loop, strlen(mpris_data->loop),
              LOOP_HEAD, LOOP_HEAD_SZ);
}

static void sendShuffleStatus() {
  if (mpris_data->shuffle == 0) {
    transmit(client_socket, SHUFFLE_OFF, SHUFFLE_OFF_SZ);
  } else {
    transmit(client_socket, SHUFFLE_ON, SHUFFLE_ON_SZ);
  }
}

void sendPosition() {
  int64_t pos = htobe64(mpris_data->position);
  transmitMsg(client_socket, (char*)&pos, sizeof(int64_t),
              POSITION, POSITION_SZ);
}

static void sendTrackStatus(int property_list) {
  if (HAS_TITLE(property_list) && mpris_data->title != NULL) {
    debug("sending title\n");
    transmitMsg(client_socket, pathFromUrl(mpris_data->title),
                strlen(pathFromUrl(mpris_data->title)),
                TRACK_TITLE, TRACK_TITLE_SZ);
  }
  if (HAS_ARTIST(property_list) && mpris_data->artist != NULL) {
    debug("sending artist\n");
    transmitMsg(client_socket, mpris_data->artist, strlen(mpris_data->artist),
                TRACK_ARTIST, TRACK_ARTIST_SZ);
  }
  if (HAS_ALBUM(property_list) && mpris_data->album != NULL) {
    debug("sending album\n");
    transmitMsg(client_socket, mpris_data->album, strlen(mpris_data->album),
                TRACK_ALBUM, TRACK_ALBUM_SZ);
  }
  if (HAS_LENGTH(property_list)) {
    int64_t length = htobe64(mpris_data->length);
    debug("sending length : %ld\n", mpris_data->length);
    transmitMsg(client_socket, (char *)&length, sizeof(int64_t),
                TRACK_LENGTH, TRACK_LENGTH_SZ);
  }
  if (HAS_ARTURL(property_list) && mpris_data->artUrl != NULL) {
    debug("sending arturl\n");
    transmit(client_socket, TRACK_ARTURL, TRACK_ARTURL_SZ);
    transmitFile(client_socket, pathFromUrl(mpris_data->artUrl));
  }
}

void sendCachedData() {
  if (mpris_data == NULL) {
    return;
  }
  printMprisData();
  sendPlaybackStatus();
  sendLoopStatus();
  sendShuffleStatus();
  sendPosition();
  sendTrackStatus(TITLE | ARTIST | ALBUM | LENGTH | ARTURL);
}

static int propertyMaybeChanged(char ** old_value, char * new_value) {
  if (*old_value) {
    if (strcmp(new_value, *old_value) != 0) {
      g_free(*old_value);
      (*old_value) = new_value;
      return 1;
    } else {
      g_free(new_value);
      return 0;
    }
  } else {
    (*old_value) = new_value;
    return 1;
  }
}

static void playbackChanged(char * value) {
  if (propertyMaybeChanged(&mpris_data->playback, value) == 1) {
    sendPlaybackStatus();
  }
}

static void loopChanged(char * value) {
  if (propertyMaybeChanged(&mpris_data->loop, value) == 1) {
    sendLoopStatus();
  }
}

static void shuffleChanged(int value) {
  if (value != mpris_data->shuffle) {
    mpris_data->shuffle = value;
    sendShuffleStatus();
  }
}

static void positionChanged(int64_t value) {
  if (value != mpris_data->position) {
    mpris_data->position = value;
    sendPosition();
  }
}

static void trackChanged(GVariant * data_map) {
  GVariant * array;
  char * value_str = NULL;
  int ret = 0;
  int64_t length = 0;

  value_str = g_variant_print (data_map, TRUE);
  debug("      metadata -> %s\n", value_str);
  g_free(value_str);

  if (!g_variant_lookup(data_map, "xesam:title", "s", &value_str) &&
      !g_variant_lookup(data_map, "xesam:url", "s", &value_str)) {
      debug("No metadata on title/url!\n");
  } else {
    ret |= TITLE*propertyMaybeChanged(&mpris_data->title, value_str);
  }

  array = g_variant_lookup_value(data_map, "xesam:artist", G_VARIANT_TYPE_STRING_ARRAY);
  if (!array) {
    debug("No metadata on artist!\n");
  } else {
    g_variant_get_child(array, 0, "s", &value_str);
    if(!value_str) {
      debug("Invalid metadata on artist!\n");
    } else {
      ret |= ARTIST*propertyMaybeChanged(&mpris_data->artist, value_str);
    }
  }

  if (!g_variant_lookup(data_map, "xesam:album", "s", &value_str)) {
    debug("No metadata on album!\n");
  } else {
    ret |= ALBUM*propertyMaybeChanged(&mpris_data->album, value_str);
  }

  if (!g_variant_lookup(data_map, "mpris:length", "i", &length) &&
      !g_variant_lookup(data_map, "mpris:length", "x", &length)) {
    debug("No metadata on length!\n");
  } else {
    if (length != mpris_data->length) {
      mpris_data->length = length;
      ret |= LENGTH;
    }
  }

  if (!g_variant_lookup(data_map, "mpris:artUrl", "s", &value_str)) {
    debug("No metadata on art uri!\n");
  } else {
    ret |= ARTURL*propertyMaybeChanged(&mpris_data->artUrl, value_str);
  }

  if (ret != 0) {
    sendTrackStatus(ret);
  }
}

static void updateStatus(struct mprisInstance * instance) {
  // Should really only be used at initialization, since it updates every
  // property. On property changed, only the changed properties should be
  // updated.
  GVariant * value;
  GVariant * array;

  // Update fields :
  value = g_dbus_proxy_get_cached_property(instance->player->proxy, "PlaybackStatus");
  if (value == NULL) {
    debug("The player %s does not implement the PlaybackStatus property\n", instance->player->name);
  } else {
    if (instance->playback) g_free(instance->playback);
    instance->playback = g_variant_dup_string(value, NULL);
    g_variant_unref(value);
  }

  value = g_dbus_proxy_get_cached_property(instance->player->proxy, "LoopStatus");
  if (value == NULL) {
    debug("The player %s does not implement the LoopStatus property\n", instance->player->name);
  } else {
    if (instance->loop) g_free(instance->loop);
    instance->loop = g_variant_dup_string(value, NULL);
    g_variant_unref(value);
  }

  value = g_dbus_proxy_get_cached_property(instance->player->proxy, "Shuffle");
  if (value == NULL) {
    debug("The player %s does not implement the Shuffle property\n", instance->player->name);
  } else {
    instance->shuffle = g_variant_get_boolean(value);
    g_variant_unref(value);
  }

  value = g_dbus_proxy_get_cached_property(instance->player->proxy, "Position");
  if (value == NULL) {
    debug("The player %s does not implement the Position property\n", instance->player->name);
  } else {
    instance->position = g_variant_get_int64(value);
    g_variant_unref(value);
  }

  value = g_dbus_proxy_get_cached_property(instance->player->proxy, "Metadata");
  if (value == NULL) {
    debug("The player %s does not implement the Metadata property\n", instance->player->name);
    return;
  }

  if (instance->title) g_free(instance->title);
  if (!g_variant_lookup(value, "xesam:title", "s", &instance->title)) {
    debug("No metadata on title!\n");
    instance->title = NULL;
  }

  if (instance->artist) g_free(instance->artist);
  array = g_variant_lookup_value(value, "xesam:artist", G_VARIANT_TYPE_STRING_ARRAY);
  if (!array) {
    debug("No metadata on artist!\n");
    instance->artist = NULL;
  } else {
    g_variant_get_child(array, 0, "s", &instance->artist);
  }

  if (instance->album) g_free(instance->album);
  if (!g_variant_lookup(value, "xesam:album", "s", &instance->album)) {
    instance->album = NULL;
    debug("No metadata on album!\n");
  }
  if (!g_variant_lookup(value, "mpris:length", "i", &instance->length) &&
      !g_variant_lookup(value, "mpris:length", "x", &instance->length)) {
    instance->length = 0;
    debug("No metadata on length!\n");
  }
  if (instance->artUrl) g_free(instance->artUrl);
  if (!g_variant_lookup(value, "mpris:artUrl", "s", &instance->artUrl)) {
    instance->artUrl = NULL;
    debug("No metadata on art uri!\n");
  }

  g_variant_unref(value);
  debug("Properties updated\n");
}

static void onPropertiesChanged(GDBusProxy          *proxy,
                                GVariant            *changed_properties,
                                const gchar* const  *invalidated_properties,
                                gpointer             pp)
{
  if (g_variant_n_children(changed_properties) > 0) {
    GVariantIter *iter;
    gchar *key;
    GVariant *value;

    debug(" *** Properties Changed:\n");
    g_variant_get(changed_properties, "a{sv}", &iter);
    while (g_variant_iter_loop (iter, "{&sv}", &key, &value)) {
      gchar *value_str;
      value_str = g_variant_print(value, TRUE);
      g_print("      %s -> %s\n", key, value_str);
      g_free(value_str);
      if (strncmp(key, "Metadata",8) == 0) {
        trackChanged(value);
      } else if (strcmp(key, "PlaybackStatus") == 0) {
        playbackChanged(g_variant_dup_string(value, NULL));
      } else if (strcmp(key, "LoopStatus") == 0) {
        loopChanged(g_variant_dup_string(value, NULL));
      } else if (strcmp(key, "Shuffle") == 0) {
        shuffleChanged(g_variant_get_boolean(value));
      } else if (strcmp(key, "Position") == 0) {
        positionChanged(g_variant_get_int64(value));
      }
    }

    g_variant_iter_free (iter);
  }
}

static void on_name_owner_changed(GObject    *object,
                                  GParamSpec *pspec,
                                  struct proxyParams * pp)
{
  char * name_owner = g_dbus_proxy_get_name_owner(pp->proxy);
  debug("on_name_owner_changed : change %d!\n", pp->active);
  printProxy(pp);
  if (name_owner && pp->active == 0) {
    updateStatus(mpris_data);
    pp->active = 1;
  } else if (!name_owner && pp->active == 1) {
    updateStatus(mpris_data);
    pp->active = 0;
    // Free the connection used, try to get a new one :
    closeConnection(pp);
    if (createConnection(pp, G_CALLBACK(onPropertiesChanged),
                         G_CALLBACK(on_name_owner_changed)) == -1) {
      debug("mpris.c : Could not re-create mpris proxy.\n");
    }
  }
  printMprisData();
}

void updatePositionProperty() {
  const GVariantType * type;
  int64_t pos = 0;
  if (!mpris_data || !mpris_data->player) {
    return;
  }

  GVariant * position = updateProperty(mpris_data->player, "Position");
  if (position == NULL) {
    return;
  }

  type = g_variant_get_type(position);
  if (g_variant_is_of_type(position, G_VARIANT_TYPE_INT32) ||
      g_variant_is_of_type(position, G_VARIANT_TYPE_UINT32) ||
      g_variant_is_of_type(position, G_VARIANT_TYPE_INT64) ||
      g_variant_is_of_type(position, G_VARIANT_TYPE_UINT64)) {
    g_variant_get(position, (char *)type, &pos);
    positionChanged(pos);
  }

  g_variant_unref(position);
}

void updatePlaybackProperty() {
  GVariant * playback = updateProperty(mpris_data->player, "PlaybackStatus");
  if (playback == NULL) {
    return;
  }

  playbackChanged(g_variant_dup_string(playback, NULL));
  g_variant_unref(playback);
}

void updateTrackProperty() {
  GVariant * metadata = updateProperty(mpris_data->player, "Metadata");
  if (metadata == NULL) {
    return;
  }

  trackChanged(metadata);
  g_variant_unref(metadata);
}

int createMprisInstance(struct proxyParams * pp) {
  struct mprisInstance * instance;
  if (mpris_data != NULL) {
    debug("There is already an mpris instance\n");
  }

  debug("Creating an mpris instance for app %s on path %s.\n", pp->name,
                                                               pp->path);
  instance = malloc(sizeof(struct mprisInstance));
  memset(instance, 0, sizeof(struct mprisInstance));
  instance->player = malloc(sizeof(struct proxyParams));
  instance->player->name = pp->name;
  instance->player->path = pp->path;
  instance->player->interface = MPRIS_PLAYER_IF;
  instance->player->active = 0;
  // Create proxy for Player :
  if (createConnection(instance->player, G_CALLBACK(onPropertiesChanged),
                       G_CALLBACK(on_name_owner_changed)) == -1) {
    debug("Could not create mpris proxy, does this application implement the mrpis protocol?\n");
    free(instance);
    return -1;
  }

  instance->tracklist = NULL;
  /*
  // Create proxy for tracklist :
  createConnection(instance->player, onPropertiesChanged);
  */
  updateStatus(instance);
  mpris_data = instance;
  printMprisData();
  // Some players (ie vlc) don't send the signal properties changed when a new client
  // connects. In this case we won't have any info, then force the update of some properties :
  if (instance->playback == NULL) {
    updatePlaybackProperty();
  }

  if (instance->title == NULL) {
    updateTrackProperty();
  }

  client_socket = 0;
  return 0;
}

void deleteMprisInstance() {
  if (mpris_data == NULL) {
    debug("There is no mpris instance to delete");
    return;
  }

  debug("Deleting mpris instance for %s\n", mpris_data->player->name);
  closeConnection(mpris_data->player);
  free(mpris_data->player);
  g_free(mpris_data->title);
  g_free(mpris_data->artist);
  g_free(mpris_data->album);
  g_free(mpris_data->artUrl);
  g_free(mpris_data->loop);
  g_free(mpris_data->playback);
  free(mpris_data);
  mpris_data = NULL;

}

void updateMprisClientSocket(int socketd) {
  client_socket = socketd;
}
