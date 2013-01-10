#include <gio/gio.h>
#include <jansson.h>
#include <stdio.h>
#include <string.h>

#include "mpris.h"
#include "utils.h"

static json_t * data;

static int fillProxyParams(struct proxyParams * tmp, json_t * obj) {
  json_t * app_name;
  json_t * dbus_name;
  json_t * path;
  json_t * interface;
  json_t * commands;
  json_t * feedback;

  app_name = json_object_get(obj, "app");
  if (app_name == NULL || !json_is_string(app_name)) {
    debug("proxy object does not have an app name\n");
    return -1;
  }

  dbus_name = json_object_get(obj, "dbus-name");
  if (dbus_name == NULL || !json_is_string(dbus_name)) {
    debug("proxy object does not have a dbus-name\n");
    return -1;
  }

  path = json_object_get(obj, "path");
  if (path == NULL || !json_is_string(path)) {
    debug("proxy object does not have an object path\n");
    return -1;
  }

  interface = json_object_get(obj, "interface");
  if (interface == NULL || !json_is_string(interface)) {
    debug("proxy object does not have an interface\n");
    return -1;
  }

  tmp->name = json_string_value(dbus_name);
  tmp->path = json_string_value(path);
  tmp->interface = json_string_value(interface);
  feedback = json_object_get(obj, "feedback");
  if (feedback != NULL && json_is_string(feedback) &&
      strncmp(json_string_value(feedback), "mpris", 5) == 0) {
    debug("create an mpris instance to listen for feedback for app %s\n",
          json_string_value(app_name));
    createMprisInstance(tmp);
  }

  commands = json_object_get(obj, "cmds");
  if (commands == NULL || !json_is_object(commands)) {
    debug("proxy object does not have a command section\n");
    return -1;
  }

  debug("proxy params filled for app %s\n", json_string_value(app_name));
  return 0;
}

static int fillCallTable(GHashTable * call_table, const struct proxyParams * proxy, json_t * cmd_obj) {
  char * commandBuff;
  const char * commandName;
  void * iter;
  json_t * methodName;
  struct callParams * tmp;

  if (!json_is_object(cmd_obj)) {
    return -1;
  }

  iter = json_object_iter(cmd_obj);
  if (iter == NULL) {
    debug("No command for proxy %s\n", proxy->name);
    return 0;
  }

  while (iter) {
    commandName = json_object_iter_key(iter);
    methodName = json_object_iter_value(iter);
    if (methodName == NULL || !json_is_string(methodName) ||
        strlen(commandName) >= MAX_CMD_SIZE) {
      debug("Incorrect command/method association, value should be a string \
and key should be shorter than %d char, for %s\n", MAX_CMD_SIZE, commandName);
      iter = json_object_iter_next(cmd_obj, iter);
      continue;
    }

    commandBuff = malloc(strlen(commandName)+1);
    strncpy(commandBuff, commandName, strlen(commandName)+1);
    tmp = malloc(sizeof(struct callParams));
    tmp->proxy = proxy;
    tmp->method = json_string_value(methodName);
    g_hash_table_insert(call_table, commandBuff, tmp);
    debug("Call params for %s inserted into call table\n", commandName);
    iter = json_object_iter_next(cmd_obj, iter);
  }

  return 0;
}

int loadConfig(const char * path) {
  json_error_t error;

  data = json_load_file(path, 0, &error);
  if (data == NULL) {
    printf("Could not parse config file : %s\nAt position %d,%d :\n\t%s\n",
        error.text, error.line, error.column, error.source);
    return -1;
  }

  return 0;
}

static int isValidProfileObject(const json_t * data, json_t ** out_name,
                                json_t ** out_data, const char * name) {

  json_t * profile_name = json_object_get(data, "profile-name");
  json_t * profile_data = json_object_get(data, "profile-data");
  *out_name = NULL;
  *out_data = NULL;
  if (!profile_name || !json_is_string(profile_name) || !profile_data ||
      (!json_is_object(profile_data) && !json_is_array(profile_data))) {
    // Not a valid profile object
    return -1;
  }

  if (!name || strcmp(name, json_string_value(profile_name)) == 0) {
    // We're not looking for a specific profile, this one will do, or
    // we are looking for a specific profile and this one matches.
    *out_name = profile_name;
    *out_data = profile_data;
    return 0;
  }

  // This is a valid profile object, but doesn't match the desired profile name.
  return 1;
}

static int isValidProfileArray(const json_t * data, json_t ** out_name,
                               json_t ** out_data, const char * name) {
  int i;
  int ret = 0;
  json_t * obj;
  *out_name = NULL;
  *out_data = NULL;

  for (i = 0; i < json_array_size(data); i++) {
    obj = json_array_get(data, i);
    ret = isValidProfileObject(obj, out_name, out_data, name);
    if (ret == -1) {
      // Invalid profile object ==> invalid profile array :
      return ret;
    } else if (ret == 0) {
      // Valid profile object, matching 'name'
      return ret;
    }
    // Valid profile object, but not matching 'name'. Keep looking
  }

  // No profile matched name.
  return 1;
}

const char * chooseProfile(const char * name) {
  json_t * profile_name;
  json_t * profile_data;

  if (data == NULL) {
    debug("Config file not loaded\n");
    return NULL;
  }

  // There is only one object. Is it a profile, or a proxy?
  if (json_is_array(data)) {
    if (isValidProfileArray(data, &profile_name, &profile_data, name) == 0) {
      data = profile_data;
      return json_string_value(profile_name);
    }

    if (!name) {
      // This is not a profile array, but we are not looking for one.
      // Let's assume it's a proxy array.
      return "default";
    }

    // We are looking for a profile, but there aren't any!
    return NULL;
  }

  // There is only one object. Is it a profile, or a proxy?
  if (json_is_object(data)) {
    if (isValidProfileObject(data, &profile_name, &profile_data, name) == -1) {
      data = profile_data;
      return json_string_value(profile_name);
    }

    if (!name) {
      // This is not a profile, but we are not looking for one. Let's assume it's a proxy.
      return "default";
    }

    // We are looking for a profile, but there aren't any!
    return NULL;
  }

  return NULL;
}

int parseConfig(struct proxyParams ** pp, GHashTable * hash_table) {
// Create the proxyParams structures, holding the information to create the various proxies
// Fills in the hash_table containing the call ref for each command.
  struct proxyParams * params;
  struct proxyParams * tmp;
  json_t * obj;
  int ret;
  int i = 0;

  if (data == NULL) {
    debug("Config file not loaded\n");
    return -1;
  }

  if (hash_table == NULL) {
    debug("Invalid hash_table\n");
  }

  tmp = NULL;
  (*pp) = NULL;

  if (!json_is_array(data)) {
    // There is only one proxy
    debug("data is not an array\n");
    if (!json_is_object(data)) {
      debug("data is not an array or an object\n");
      return -1;
    }

    params = malloc(sizeof(struct proxyParams));
    params->prev = tmp;
    ret = fillProxyParams(params, data);
    if (ret == -1) {
      debug("Incorect proxy specifications for top-level object in config file!\n");
      free(params);
      return -1;
    }

    fillCallTable(hash_table, params, json_object_get(data, "cmds"));
    (*pp) = params;
    return 0;
  }

  // There are several proxies
  debug("Config : %ld top level item\n", json_array_size(data));
  for (i = 0; i < json_array_size(data); i++) {
    obj = json_array_get(data, i);
    if (!json_is_object(obj)) {
      debug("non-object top-level item number %d in config file!\n", i);
      continue;
    }

    params = malloc(sizeof(struct proxyParams));
    params->prev = tmp;
    ret = fillProxyParams(params, obj);
    if (ret == -1) {
      debug("Incorect proxy specifications for top-level object %d in config file!\n", i);
      free(params);
      continue;
    }

    tmp = params;
    fillCallTable(hash_table, params, json_object_get(obj, "cmds"));
  }

  (*pp) = tmp;
  if (tmp == NULL) return -1;
  return 0;
}

