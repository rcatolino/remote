#include <gio/gio.h>
#include <string.h>

#include "dbusif.h"
#include "tcpserver.h"
#include "utils.h"

static int client_socket;
GVariant * updateProperty(const struct proxyParams * pp,
                          const char * prop_name) {
  GError * error;
  GVariant * ret;
  GVariant * property;
  error = NULL;
  if (!pp->proxy) {
    debug("updateProperty : Cannot update property %s since dbus proxy for %s is inexistant\n",
          prop_name,
          pp->name);
    return NULL;
  }

  ret = g_dbus_proxy_call_sync(pp->proxy,
                               "org.freedesktop.DBus.Properties.Get",
                               g_variant_new("(ss)", pp->interface, prop_name),
                               G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
  if (ret == NULL) {
    debug("updateProperty : Error getting property %s : %s\n", prop_name, error->message);
    return NULL;
  }

  g_variant_get(ret, "(v)", &property);
  g_variant_unref(ret);
  return property;
}

int setProperty(const struct proxyParams * pp, const char *prop_name,
                GVariant *value) {
  GError *error;
  GVariant *ret;
  error = NULL;
  if (!pp->proxy) {
    debug("updateProperty : Cannot set property %s since dbus proxy for %s is inexistant\n",
          prop_name,
          pp->name);
    return -1;
  }

  ret = g_dbus_proxy_call_sync(pp->proxy,
                               "org.freedesktop.DBus.Properties.Set",
                               g_variant_new("(ssv)", pp->interface, prop_name, value),
                               G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
  if (ret == NULL) {
    debug("updateProperty : Error setting property %s : %s\n", prop_name, error->message);
    return -1;
  }

  g_variant_unref(ret);
  return 0;
}

static void printProperties (GDBusProxy *proxy)
{
  gchar **property_names;
  guint n;

  debug("    properties:\n");

  property_names = g_dbus_proxy_get_cached_property_names (proxy);
  for (n = 0; property_names != NULL && property_names[n] != NULL; n++) {
    const gchar *key = property_names[n];
    GVariant *value;
    gchar *value_str;
    value = g_dbus_proxy_get_cached_property (proxy, key);
    value_str = g_variant_print (value, TRUE);
    debug("      %s -> %s\n", key, value_str);
    g_variant_unref (value);
    g_free (value_str);
  }
  g_strfreev (property_names);
}

void printProxy(struct proxyParams * pp)
{
  char *name_owner;
  name_owner = g_dbus_proxy_get_name_owner(pp->proxy);
  if (name_owner != NULL) {
    debug("+++ Proxy object points to remote object owned by %s\n"
          "    name:         %s\n"
          "    object path:  %s\n"
          "    interface:    %s\n",
          name_owner,
          pp->name,
          pp->path,
          pp->interface);
    printProperties (pp->proxy);
  } else {
    debug("--- Proxy object is inert - there is no name owner for the name\n"
          "    name:         %s\n"
          "    object path:  %s\n"
          "    interface:    %s\n",
          pp->name,
          pp->path,
          pp->interface);
  }
  g_free (name_owner);
}

void onSignal(GDBusProxy *proxy,
              gchar      *sender_name,
              gchar      *signal_name,
              GVariant   *parameters,
              gpointer    pp) {
  gchar *parameters_str;

  parameters_str = g_variant_print(parameters, TRUE);
  debug(" *** Received Signal: %s: %s\n", signal_name, parameters_str);
  g_free(parameters_str);
}

static void onNameOwnerNotify(GObject *object,
                              GParamSpec *pspec,
                              struct proxyParams *pp) {
  char * name_owner = g_dbus_proxy_get_name_owner(pp->proxy);
  debug("onNameOwnerNotify : change!%d\n", pp->active);
  if (name_owner) {
    pp->active = 1;
  } else if (pp->active == 1) {
    pp->active = 0;
    closeConnection(pp);
    if (createConnection(pp, NULL, NULL) == -1) {
      debug("dbusif.c : Could not re-create dbus proxy connection.\n");
    }
  }

  printProxy((struct proxyParams *)pp);
}

void closeConnection(struct proxyParams * pp) {
  if (!pp->proxy) {
    return;
  }

  debug("closeConnection : closing %s, %p\n", pp->name, pp->proxy);
  g_object_unref(pp->proxy);
  pp->proxy = NULL;
}

int createConnection(struct proxyParams * pp, GCallback on_property_changed,
                     GCallback on_name_owner_changed) {
  return createConnectionOnBus(pp, on_property_changed, on_name_owner_changed,
                               G_BUS_TYPE_SESSION);
}

int createConnectionOnBus(struct proxyParams * pp, GCallback on_property_changed,
                          GCallback on_name_owner_changed, GBusType bus) {
  GError *error;
  GDBusProxyFlags flags;
  GDBusProxy *proxy;
  char *name_owner;

  proxy = NULL;
  error = NULL;

  pp->proxy = NULL;
  flags = G_DBUS_PROXY_FLAGS_NONE;
  proxy = g_dbus_proxy_new_for_bus_sync(bus,
                                         flags,
                                         NULL, /* GDBusInterfaceInfo */
                                         pp->name,
                                         pp->path,
                                         pp->interface,
                                         NULL, /* GCancellable */
                                         &error);
  if (proxy == NULL) {
    g_printerr ("dbugif.c : Error creating proxy: %s\n", error->message);
    g_error_free (error);
    return -1;
  }

  pp->proxy = proxy;
  debug("proxy adress : %p, object type : %s\n", pp->proxy, G_OBJECT_TYPE_NAME(pp->proxy));
  if (on_property_changed != NULL) {
    // Only ask to be signaled on this proxy if we need it for feedback
    g_signal_connect(proxy, "g-properties-changed",
                     G_CALLBACK(on_property_changed),
                     pp);
  }
  /*
  g_signal_connect(proxy, "g-signal",
                   G_CALLBACK(onSignal),
                   pp);
                   */
  if (on_name_owner_changed != NULL) {
    g_signal_connect(proxy, "notify::g-name-owner",
                     G_CALLBACK(on_name_owner_changed),
                     pp);
  } else {
    g_signal_connect(proxy, "notify::g-name-owner",
                     G_CALLBACK(onNameOwnerNotify),
                     pp);
  }

  name_owner = g_dbus_proxy_get_name_owner(pp->proxy);
  if (name_owner) {
    pp->active = 1;
  } else {
    pp->active = 0;
  }

  g_free(name_owner);
  printProxy(pp);
  return 0;
}

void call_dbus(const struct proxyParams *pp, const char *method, GVariant *parameter) {
  GError *error;
  GVariant *ret;

  if (!pp->proxy) {
    debug("dbusif.c : Error, unconnected proxy %s.\n", pp->name);
    return;
  }

  if (parameter) {
    debug("proxy adress : %p, object type : %s, argument %s\n", pp->proxy,
          G_OBJECT_TYPE_NAME(pp->proxy), g_variant_print(parameter, TRUE));
  } else {
    debug("proxy adress : %p, object type : %s\n", pp->proxy,
          G_OBJECT_TYPE_NAME(pp->proxy));
  }

  error = NULL;
  ret = g_dbus_proxy_call_sync(G_DBUS_PROXY(pp->proxy),
                               method,
                               parameter,
                               G_DBUS_CALL_FLAGS_NONE,
                               -1,
                               NULL,
                               &error);
  if (ret == NULL) {
    debug("Error calling method : %s\n", error->message);
  } else {
    g_variant_unref(ret);
  }
}

void call(struct callParams *cp, GVariant *parameter) {
  if (!cp || !cp->proxy->active) {
    debug("dbusif.c : Error, bad call paramters for method %s.\n", cp->method);
    return;
  }

  call_dbus(cp->proxy, cp->method, parameter);
}

void updateClientSocket(int socketd) {
  client_socket = socketd;
}
