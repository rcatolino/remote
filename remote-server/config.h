#ifndef CONFIG_H__
#define CONFIG_H__

#include "utils.h"

const char * chooseProfile(const char * name);
// Set the active profile to 'name', or the first valid one if 'name' is NULL.

char * getProfiles();
// Returns a null terminated string containing all the profiles names,
// separated by spaces. Should be freed.
int loadConfig(const char * path);
// Loads the config file and parse the json values;

int parseConfig(struct proxyParams ** pp, GHashTable * hash_table);
// Parse the config set-up

#endif
