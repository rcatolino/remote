#include <gio/gio.h>
#include <jansson.h>
#include <stdio.h>
#include <string.h>

#include "mpris.h"
#include "utils.h"

static json_t * data;
static json_t * data_root;

static int fillProxyParams(struct proxyParams * tmp, json_t * obj) {
  json_t * app_name;
  json_t * dbus_name;
  json_t * path;
  json_t * interface;
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

  debug("proxy params filled for app %s\n", json_string_value(app_name));
  return 0;
}

static int fillCallTable(GHashTable *call_table, const struct proxyParams *proxy, json_t *cmd_obj) {
  char *command_buff;
  const char *command_name;
  void *iter;
  json_t *method_name = NULL;
  json_t *method_arg = NULL;
  struct callParams *tmp;

  if (!cmd_obj) {
    debug("Config error : no command specification. for proxy %s\n", proxy->name);
    return -1;
  }

  if (json_is_string(cmd_obj) && strncmp("mpris", json_string_value(cmd_obj), 5) == 0) {
    // Use the standart mpris protocol.
    fillMprisCallTable(call_table, proxy);
    return 0;
  }

  if (!json_is_object(cmd_obj)) {
    return -1;
  }

  iter = json_object_iter(cmd_obj);
  if (iter == NULL) {
    debug("No command for proxy %s\n", proxy->name);
    return 0;
  }

  while (iter) {
    enum argumentType arg_type = NO_TYPE;
    command_name = json_object_iter_key(iter);
    method_name = json_object_iter_value(iter);
    if (method_name == NULL || strlen(command_name) >= MAX_CMD_SIZE) {
      debug("Incorrect command/method association. A method needs to be associated to the command, and key should be shorter than %d char, for %s\n",
            MAX_CMD_SIZE, command_name);
      iter = json_object_iter_next(cmd_obj, iter);
      continue;
    }

    if (json_is_array(method_name)) {
      // The name of the method is actually the first element in the array. The second is the
      // argument type.
      switch (json_array_size(method_name)) {
        case 0:
          // Empty array, error.
          debug("Incorrect method specification for command %s.\n", command_name);
          continue;
        case 1:
          method_name = json_array_get(method_name, 0);
          break;
        default:
          method_arg = json_array_get(method_name, 1);
          method_name = json_array_get(method_name, 0);
      }
    }

    if (!json_is_string(method_name)) {
      debug("Invalid method name specification for command %s.\n", command_name);
      continue;
    }

    if (method_arg != NULL) {
      // Case where the method takes an argument.
      if (!json_is_string(method_arg)) {
        debug("Invalid argument type specification for command %s.\n", command_name);
        continue;
      }

      if (strcasecmp(json_string_value(method_arg), "int64") == 0) {
        arg_type = I64;
      } else {
        debug("Unsuported argument type %s.\n", json_string_value(method_arg));
        continue;
      }
    }

    command_buff = malloc(strlen(command_name)+1);
    strncpy(command_buff, command_name, strlen(command_name)+1);
    tmp = malloc(sizeof(struct callParams));
    tmp->proxy = proxy;
    tmp->method = json_string_value(method_name);
    tmp->arg_type = arg_type;
    tmp->custom_call = NULL; // Command specified manually can't use custom_call.
    g_hash_table_insert(call_table, command_buff, tmp);
    debug("Call params for %s inserted into call table\n", command_name);
    iter = json_object_iter_next(cmd_obj, iter);
  }

  return 0;
}

int loadConfig(const char * path) {
  json_error_t error;

  data = NULL;
  data_root = json_load_file(path, 0, &error);
  if (data_root == NULL) {
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

char * getProfiles() {
  json_t * obj;
  json_t * name;
  json_t * data;
  unsigned int i = 0;
  int count = 0;
  const char ** profiles_name;
  char * buff;
  int profiles_length = 0;

  if (data_root == NULL) {
    return NULL;
  }

  if (json_is_array(data_root)) {
    profiles_name = malloc(sizeof(char *));
    for (i = 0; i < json_array_size(data_root); i++) {
      obj = json_array_get(data_root, i);
      if (isValidProfileObject(obj, &name, &data, NULL) == -1) {
        debug("Invalid profile array! Object %i is invalid.\n", i);
        free(profiles_name);
        return NULL;
      }

      profiles_name[i] = json_string_value(name);
      profiles_length += strlen(profiles_name[i]) + 1;
    }

    buff = malloc(profiles_length);
    memset(buff,' ', profiles_length);
    for (i = 0; i<json_array_size(data_root); i++) {
      memcpy(buff+count, profiles_name[i], strlen(profiles_name[i]));
      count += strlen(profiles_name[i])+1;
    }

    buff[profiles_length-1]='\0';
    debug("profile size : %d, count : %d\n", profiles_length, count);
    debug("profiles_name : %s\n", buff);
    free(profiles_name);
    return buff;
  }

  return NULL;
}

static int isValidProfileArray(const json_t * data, json_t ** out_name,
                               json_t ** out_data, const char * name) {
  unsigned int i;
  int ret = 0;
  json_t * obj;
  *out_name = NULL;
  *out_data = NULL;

  for (i = 0; i < json_array_size(data); i++) {
    obj = json_array_get(data, i);
    ret = isValidProfileObject(obj, out_name, out_data, name);
    if (ret == -1) {
      // Invalid profile object ==> invalid profile array :
      debug("Invalid profile array! Object %i is invalid.\n", i);
      return ret;
    } else if (ret == 0) {
      // Valid profile object, matching 'name'
      debug("Valid profile %s found\n", json_string_value(*out_name));
      return ret;
    }
    // Valid profile object, but not matching 'name'. Keep looking
  }

  // No profile matched name.
  debug("No profile matching %s found in array\n", name);
  return 1;
}

const char * chooseProfile(const char * name) {
  json_t * profile_name;
  json_t * profile_data;

  if (data_root == NULL) {
    debug("Config file not loaded\n");
    return NULL;
  }

  // There is only one object. Is it a profile, or a proxy?
  if (json_is_array(data_root)) {
    if (isValidProfileArray(data_root, &profile_name, &profile_data, name) == 0) {
      data = profile_data;
      return json_string_value(profile_name);
    }

    if (!name) {
      // This is not a profile array, but we are not looking for one.
      // Let's assume it's a proxy array.
      data = data_root;
      return "default";
    }

    // We are looking for a profile, but there aren't any!
    debug("No profile available");
    if (name) {
      debug(" for name %s\n", name);
    } else {
      debug(".\n");
    }
    return NULL;
  }

  // There is only one object. Is it a profile, or a proxy?
  if (json_is_object(data_root)) {
    if (isValidProfileObject(data_root, &profile_name, &profile_data, name) == -1) {
      data = profile_data;
      return json_string_value(profile_name);
    }

    if (!name) {
      // This is not a profile, but we are not looking for one. Let's assume it's a proxy.
      data = data_root;
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
  unsigned int i = 0;

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
    params->active = 0;
    ret = fillProxyParams(params, data);
    if (ret == -1) {
      debug("Incorect proxy specifications for top-level object in config file!\n");
      free(params);
      return -1;
    }

    if (fillCallTable(hash_table, params, json_object_get(data, "cmds")) == -1) {
      free(params);
      return -1;
    }

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

    if (fillCallTable(hash_table, params, json_object_get(obj, "cmds")) == -1) {
      free(params);
      continue;
    }

    tmp = params;
  }

  (*pp) = tmp;
  if (tmp == NULL) return -1;
  return 0;
}

