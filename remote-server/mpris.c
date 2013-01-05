#include "dbusif.h"
#include "mpris.h"
#include "tcpserver.h"
#include "utils.h"

#include <gio/gio.h>
#include <stdlib.h>
#include <string.h>

static struct mprisInstance * mpris_data = NULL;
static int client_socket;

void printMprisData() {
  printf("Playback : %s, Loop : %s, Shuffle : %d.\n\tTitle : %s, Artist : %s,\
Album : %s, Album cover location : %s\n", mpris_data->playback,
mpris_data->loop, mpris_data->shuffle, mpris_data->title, mpris_data->artist,
mpris_data->album, mpris_data->artUrl);
}

static void playbackChanged(char * value, unsigned long length) {
  debug("MADAFUCKIN LENGTH YO! %lu\n", length);
  if (mpris_data->playback) {
    if (strcmp(value, mpris_data->playback) != 0) {
      transmit(client_socket, value, length);
      g_free(mpris_data->playback);
      mpris_data->playback = value;
    } else {
      g_free(value);
    }
  } else {
    transmit(client_socket, value, length);
    mpris_data->playback = value;
  }
}

static void loopChanged(char * value, unsigned long length) {
  if (mpris_data->loop) {
    if (strcmp(value, mpris_data->loop) != 0) {
      transmit(client_socket, value, length);
      g_free(mpris_data->loop);
      mpris_data->loop = value;
    } else {
      g_free(value);
    }
  } else {
    transmit(client_socket, value, length);
    mpris_data->loop = value;
  }
}

static void shuffleChanged(int value) {
  if (value != mpris_data->shuffle) {
    mpris_data->shuffle = value;
    if (value == 0) {
      transmit(client_socket, SHUFFLE_OFF, sizeof(SHUFFLE_OFF));
    } else {
      transmit(client_socket, SHUFFLE_ON, sizeof(SHUFFLE_ON));
    }
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

  value = g_dbus_proxy_get_cached_property(instance->player->proxy, "Metadata");
  if (value == NULL) {
    debug("The player %s does not implement the Metadata property\n", instance->player->name);
    return;
  }

  if (instance->title) g_free(instance->title);
  if (!g_variant_lookup(value, "xesam:title", "s", &instance->title)) {
    debug("No metadata on title!\n");
  }

  if (instance->artist) g_free(instance->artist);
  array = g_variant_lookup_value(value, "xesam:artist", G_VARIANT_TYPE_STRING_ARRAY);
  if (!array) {
    debug("No metadata on artist!\n");
  } else {
    g_variant_get_child(array, 0, "s", &instance->artist);
  }

  if (instance->album) g_free(instance->album);
  if (!g_variant_lookup(value, "xesam:album", "s", &instance->album)) {
    debug("No metadata on album!\n");
  }
  if (!g_variant_lookup(value, "mpris:length", "i", &instance->length)) {
    debug("No metadata on length!\n");
  }
  if (instance->artUrl) g_free(instance->artUrl);
  if (!g_variant_lookup(value, "mpris:artUrl", "s", &instance->artUrl)) {
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
      char buff[MAX_CMD_SIZE];
      unsigned long length;
      gchar *value_str;
      GVariant * value_variant;
      value_str = g_variant_print(value, TRUE);
      g_print("      %s -> %s\n", key, value_str);
      g_free(value_str);
      if (strncmp(key, "Metadata",8) == 0) {
        debug("property '%s' present in changed properties\n", key);
        value_variant = g_variant_lookup_value(value, "xesam:title",
                                                      G_VARIANT_TYPE_STRING);
        if (value_variant != NULL) {
          value_str = g_variant_print(value_variant, FALSE);
          if (strlen(value_str) < MAX_CMD_SIZE - 10) {
            snprintf(buff, MAX_CMD_SIZE, "PLAYING %s", value_str);
            debug("Need to send feedback from %s, with value %s\n",
                  ((struct proxyParams*)pp)->name, buff);
            transmit(client_socket, buff, strlen(buff));
          }
          g_free(value_str);
        }
      } else if (strcmp(key, "PlaybackStatus")) {
        playbackChanged(g_variant_dup_string(value, &length), length);
      } else if (strcmp(key, "LoopStatus")) {
        loopChanged(g_variant_dup_string(value, &length), length);
      } else if (strcmp(key, "Shuffle")) {
        shuffleChanged(g_variant_get_boolean(value));
      }
    }

    g_variant_iter_free (iter);
  }
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
  // Create proxy for Player :
  if (createConnection(instance->player, G_CALLBACK(onPropertiesChanged)) == -1) {
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
  return 0;
}

void updateMprisClientSocket(int socketd) {
  client_socket = socketd;
}
