#include <jansson.h>
#include <stdio.h>

#include "utils.h"

static json_t * data;

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


