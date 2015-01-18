#include "mpris.h"
#include "dbusif.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Seeking forward or backward moves the position by .5% (100/200) of the total track length.
#define SEEK_FACTOR 200

struct mprisCmd {
  const char *client_cmd;
  const char *mpris_method;
  enum argumentType arg_type;
  void (*custom_call)(struct callParams *, const char *);
};

void seek_call(struct callParams *cp, const char *argument_buff);
void setpos_call(struct callParams *cp, const char *argument_buff);
void shuffle_call(struct callParams *cp, const char *argument_buff);
void loop_call(struct callParams *cp, const char *argument_buff);
void volume_up(struct callParams *cp, const char *argument_buff);
void volume_down(struct callParams *cp, const char *argument_buff);

static const struct mprisCmd mpris_cmds[] = {
  { .client_cmd = "PLAY", .mpris_method = "Play", .arg_type = NO_TYPE, .custom_call = NULL },
  { .client_cmd = "PAUSE", .mpris_method = "Pause", .arg_type = NO_TYPE, .custom_call = NULL },
  { .client_cmd = "STOP", .mpris_method = "Stop", .arg_type = NO_TYPE, .custom_call = NULL },
  { .client_cmd = "PREV", .mpris_method = "Previous", .arg_type = NO_TYPE, .custom_call = NULL },
  { .client_cmd = "NEXT", .mpris_method = "Next", .arg_type = NO_TYPE, .custom_call = NULL },
  { .client_cmd = "VOLUMEUP", .mpris_method = "Volume", .arg_type = NO_TYPE, .custom_call = volume_up },
  { .client_cmd = "VOLUMEDOWN", .mpris_method = "Volume", .arg_type = NO_TYPE, .custom_call = volume_down },
  { .client_cmd = "SETPOS", .mpris_method = "SetPosition", .arg_type = I64, .custom_call = setpos_call},
  { .client_cmd = "SEEK", .mpris_method = "Seek", .arg_type = BOOL, .custom_call = seek_call },
  { .client_cmd = "SHUFFLE", .mpris_method = "Shuffle", .arg_type = BOOL, .custom_call = shuffle_call },
  { .client_cmd = "LOOP", .mpris_method = "LoopStatus", .arg_type = STR, .custom_call = loop_call }
};

static GVariant *strToI64GVariant(const char *argument_buff) {
  int64_t position = 0;
  if (sscanf(argument_buff, "%" SCNd64, &position) == EOF) {
    perror("sscanf ");
    return NULL;
  }

  GVariant *gposition = g_variant_new_int64(position);
  return gposition;
}

void seek_call(struct callParams *cp, const char *argument_buff) {
  GVariant *parameter = NULL;

  debug("Custom dbus call for method %s, argument %s.\n", cp->method, argument_buff);
  if (argument_buff == NULL || cp->arg_type != BOOL) {
    debug("Error, bad position argument for call to %s.\n", cp->method);
    return;
  }

  if (strcmp(argument_buff, "BACKWARD") == 0) {
    parameter = g_variant_new_int64(-1*getTrackLength()/SEEK_FACTOR);
  } else if (strcmp(argument_buff, "FORWARD") == 0) {
    parameter = g_variant_new_int64(getTrackLength()/SEEK_FACTOR);
  } else {
    debug("Error, valid values for %s property are : BACKWARD and FORWARD.\n", cp->method);
  }

  call(cp, g_variant_new_tuple(&parameter, 1));
}

void setpos_call(struct callParams *cp, const char *argument_buff) {
  GVariant *parameters[2] = {NULL, NULL};

  debug("Custom dbus call for method %s, argument %s.\n", cp->method, argument_buff);
  if (argument_buff == NULL || cp->arg_type != I64) {
    debug("Error, bad position argument for call to %s.\n", cp->method);
    return;
  }

  parameters[1] = strToI64GVariant(argument_buff);
  if (!parameters[1]) {
    debug("Error, couldn't convert %s to int64.\n", argument_buff);
    return;
  }

  parameters[0] = getTrackId();
  if (!parameters[0]) {
    debug("Error, no trackid available.\n");
    g_variant_unref(parameters[1]);
    return;
  }

  call(cp, g_variant_new_tuple(parameters, 2));
}

void shuffle_call(struct callParams *cp, const char *argument_buff) {
  GVariant *value = NULL;

  debug("Custom dbus call for property %s, argument %s.\n", cp->method, argument_buff);
  if (argument_buff == NULL || cp->arg_type != BOOL) {
    debug("Error, bad status argument for call to %s.\n", cp->method);
    return;
  }

  if (strcmp(argument_buff, "ON") == 0) {
    value = g_variant_new_boolean(TRUE);
  } else if (strcmp(argument_buff, "OFF") == 0) {
    value = g_variant_new_boolean(FALSE);
  } else {
    debug("Error, valid values for %s property are : ON and OFF.\n", cp->method);
  }

  setProperty(cp->proxy, cp->method, value);
}

void loop_call(struct callParams *cp, const char *argument_buff) {
  GVariant *value = NULL;

  debug("Custom dbus call for property %s, argument %s.\n", cp->method, argument_buff);
  if (argument_buff == NULL || cp->arg_type != STR) {
    debug("Error, bad status argument for call to %s.\n", cp->method);
    return;
  }

  if (strcasecmp(argument_buff, "None") == 0) {
    value = g_variant_new_string("None");
  } else if (strcasecmp(argument_buff, "Track") == 0) {
    value = g_variant_new_string("Track");
  } else if (strcasecmp(argument_buff, "Playlist") == 0) {
    value = g_variant_new_string("Playlist");
  } else {
    debug("Error, valid values for %s property are : None, Track and Playlist.\n", cp->method);
  }

  setProperty(cp->proxy, cp->method, value);
}

void volume_up(struct callParams *cp, const char *argument_buff) {
  debug("Custom dbus call volume_up for property %s.\n", cp->method);
  setProperty(cp->proxy, cp->method, g_variant_new_double(getVolume()+0.1));
}

void volume_down(struct callParams *cp, const char *argument_buff) {
  debug("Custom dbus call volume_down for property %s.\n", cp->method);
  setProperty(cp->proxy, cp->method, g_variant_new_double(getVolume()-0.1));
}

int fillMprisCallTable(GHashTable *call_table, const struct proxyParams *proxy) {
  char *key;
  struct callParams *tmp;
  unsigned int i;

  debug("Creating standard mpris command config\n");
  for (i = 0; i < sizeof(mpris_cmds)/sizeof(mpris_cmds[0]); i++) {
    if (g_hash_table_contains(call_table, mpris_cmds[i].client_cmd)) {
      // This command has already been assigned a method. Manual command
      // specifications have priority over mpris ones, so that one can override
      // the mpris defaults for a specific command and keep the mpris defaults
      // for the rest.
      debug("Call params for %s were already specified in call table. Call params ignored.\n", mpris_cmds[i].client_cmd);
      continue;
    }

    // Memory allocated here is freed by glib when the hash table is emptied.
    key = malloc(sizeof(mpris_cmds[i].client_cmd));
    strcpy(key, mpris_cmds[i].client_cmd);
    tmp = malloc(sizeof(struct callParams));
    tmp->proxy = proxy;
    tmp->method = mpris_cmds[i].mpris_method;
    tmp->arg_type = mpris_cmds[i].arg_type;
    tmp->custom_call = mpris_cmds[i].custom_call;
    g_hash_table_insert(call_table, key, tmp);
    debug("Call params for %s inserted into call table\n", mpris_cmds[i].client_cmd);
  }

  return 0;
}
