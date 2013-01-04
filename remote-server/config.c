#include <gio/gio.h>
#include <jansson.h>
#include <stdio.h>
#include <string.h>

#include "utils.h"

static json_t * data;

static int fillProxyParams(struct proxyParams * tmp, json_t * obj) {
  json_t * app_name;
  json_t * dbus_name;
  json_t * path;
  json_t * interface;
  json_t * commands;

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

  commands = json_object_get(obj, "cmds");
  if (commands == NULL || !json_is_object(commands)) {
    debug("proxy object does not have a command section\n");
    return -1;
  }

  tmp->name = json_string_value(dbus_name);
  tmp->path = json_string_value(path);
  tmp->interface = json_string_value(interface);
  debug("proxy params filled for app %s\n", json_string_value(app_name));
  return 0;
}

static int fillFeedbackTable(struct proxyParams * proxy, json_t * fb_obj) {
  char * fbBuff;
  char * propertyBuff;
  const char * fbName;
  const char * propertyName;
  void * iter;
  json_t * value;

  if (fb_obj == NULL || !json_is_object(fb_obj)) {
    return -1;
  }

  iter = json_object_iter(fb_obj);
  if (iter == NULL) {
    debug("No feedback for proxy %s\n", proxy->name);
    return 0;
  }

  while (iter) {
    fbName = json_object_iter_key(iter);
    value = json_object_iter_value(iter);
    if (value == NULL || !json_is_string(value)) {
      debug("Incorrect feedback/property association, value should be a string, for %s\n", fbName);
      iter = json_object_iter_next(fb_obj, iter);
      continue;
    }

    // TODO: check for the validity of the key as a feedback.
    propertyName = json_string_value(value);
    if (proxy->feedback_table == NULL) {
      proxy->feedback_table = g_hash_table_new(g_str_hash, g_str_equal);
    }

    propertyBuff = malloc(strlen(propertyName)+1);
    strncpy(propertyBuff, propertyName, strlen(propertyName)+1);
    fbBuff = malloc(strlen(fbName)+1);
    strncpy(fbBuff, fbName, strlen(fbName)+1);
    g_hash_table_insert(proxy->feedback_table, propertyBuff, fbBuff);
    debug("Feedback params for %s inserted into feedback table\n", fbName);
    iter = json_object_iter_next(fb_obj, iter);
  }

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
    params->feedback_table = NULL;
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
    fillFeedbackTable(params, json_object_get(obj, "feedback"));
  }

  (*pp) = tmp;
  if (tmp == NULL) return -1;
  return 0;
}

int lookupCmd(const char * cmd, char * buff, json_t * obj) {
  // TODO: move these checks at parse time
  int ret = -1;
  json_t * app_name;
  json_t * dbus_name;
  json_t * command;
  json_t * path;
  json_t * method;

  app_name = json_object_get(obj, "app");
  if (app_name == NULL || !json_is_string(app_name)) {
    debug("app object does not have an app name\n");
    return -1;
  }

  dbus_name = json_object_get(obj, "dbus-name");
  if (dbus_name == NULL || !json_is_string(dbus_name)) {
    debug("app object does not have a dbus-name\n");
    return -1;
  }

  command = json_object_get(obj, cmd);
  if (command == NULL || !json_is_object(command)) {
    return 0;
  }

  path = json_object_get(command, "path");
  if (path == NULL || !json_is_string(path)) {
    debug("command %s exists in %s, but has no dbus path\n", cmd, json_string_value(app_name));
    return 0;
  }

  method = json_object_get(command, "method");
  if (method == NULL || !json_is_string(method)) {
    debug("command %s exists in %s, but has no dbus method\n", cmd, json_string_value(app_name));
    return 0;
  }

  ret = snprintf(buff, MAX_DBUS_BUFF, "%s %s %s", json_string_value(dbus_name), json_string_value(path), json_string_value(method));
  if (ret < 0) {
    perror("snprintf ");
    return 0;
  } else if (ret >= MAX_DBUS_BUFF) {
    printf("Dbus command too long, rebuild with a bigger MAX_DBUS_BUFF\n");
    return 0;
  }

  debug("cmd translated to : %s\n", buff);
  ret = 1;

  return ret;
}

int translateCmd(const char * cmd, char * buff) {

  int ret;
  json_t * obj;
  int i = 0;
  if (!json_is_array(data)) {
    debug("data is not an array\n");
    if (!json_is_object(data)) {
      debug("data is not an array or an object\n");
      return -1;
    }
  }

  debug("Config : %ld top level item\n", json_array_size(data));
  for (i = 0; i < json_array_size(data); i++) {
    obj = json_array_get(data, i);
    //debug("obj type : %d\n", json_typeof(obj));
    if (!json_is_object(obj)) {
      debug("non-object top-level item number %d in config file!\n", i);
      json_array_remove(data, i);
      continue;
    }

    ret = lookupCmd(cmd, buff, obj);
    if (ret == -1) {
      json_array_remove(data, i);
      continue;
    } else if (ret == 1) {
      // We found a dbus command associated
      return 1;
    }
  }

  debug("Could not find any dbus call associated to %s\n", cmd);
  return 0;
}


