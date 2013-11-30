#include "mpris.h"
#include "dbusif.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *mpris_cmds[][2] = {
  { "PLAY", "Play" },
  { "PAUSE", "Pause" },
  { "STOP", "Stop" },
  { "PREV", "Previous" },
  { "NEXT", "Next" }
};

static GVariant *strToGVariant(const char *argument_buff) {
  int64_t position = 0;
  if (sscanf(argument_buff, "%" SCNd64, &position) == EOF) {
    perror("sscanf ");
    return NULL;
  }

  GVariant *gposition = g_variant_new_int64(position);
  return gposition;
}

void seek_call(struct callParams *cp, const char *argument_buff) {
  GVariant *parameters[2] = {NULL, NULL};

  debug("Custom dbus call for method %s, argument %s.\n", cp->method, argument_buff);
  if (argument_buff == NULL || cp->arg_type != I64) {
    debug("Error, bad position argument for call to %s.\n", cp->method);
    return;
  }

  parameters[1] = strToGVariant(argument_buff);
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

int fillMprisCallTable(GHashTable *call_table, const struct proxyParams *proxy) {
  char *key;
  struct callParams *tmp;
  int i;

  debug("Creating standard mpris command config\n");
  for (i = 0; i < sizeof(mpris_cmds)/sizeof(mpris_cmds[0]); i++) {
    // Memory allocated here is freed by glib when the hash table is emptied.
    key = malloc(sizeof(mpris_cmds[i][0]));
    strcpy(key, mpris_cmds[i][0]);
    tmp = malloc(sizeof(struct callParams));
    tmp->proxy = proxy;
    tmp->method = mpris_cmds[i][1];
    tmp->arg_type = NO_TYPE;
    tmp->custom_call = NULL;
    g_hash_table_insert(call_table, key, tmp);
    debug("Call params for %s inserted into call table\n", mpris_cmds[i][0]);
  }

  // Add the seek command.
  key = malloc(sizeof("SEEK"));
  strcpy(key, "SEEK");
  tmp = malloc(sizeof(struct callParams));
  tmp->proxy = proxy;
  tmp->method = "SetPosition";
  tmp->arg_type = I64;
  tmp->custom_call = seek_call;
  g_hash_table_insert(call_table, key, tmp);

  return 0;
}
