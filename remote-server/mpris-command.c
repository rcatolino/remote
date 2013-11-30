#include "mpris.h"

#include <stdlib.h>
#include <string.h>

static const char *mpris_cmds[][2] = {
  { "PLAY", "Play" },
  { "PAUSE", "Pause" },
  { "STOP", "Stop" },
  { "PREV", "Previous" },
  { "NEXT", "Next" }
};

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
  g_hash_table_insert(call_table, key, tmp);

  return 0;
}
