#include <gio/gio.h>
#include <string.h>

#include "tcpserver.h"
#include "utils.h"

static int client_socket;

static void print_properties (GDBusProxy *proxy)
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

static void printProxy(struct proxyParams * pp)
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
    print_properties (pp->proxy);
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

static void on_signal (GDBusProxy *proxy,
                       gchar      *sender_name,
                       gchar      *signal_name,
                       GVariant   *parameters,
                       gpointer    pp)
{
  gchar *parameters_str;

  parameters_str = g_variant_print (parameters, TRUE);
  g_print (" *** Received Signal: %s: %s\n",
           signal_name,
           parameters_str);
  g_free (parameters_str);
}

static void on_name_owner_notify (GObject    *object,
                                  GParamSpec *pspec,
                                  gpointer    pp)
{
  printProxy((struct proxyParams *)pp);
}

/*
static void onPropertiesChanged(GDBusProxy          *proxy,
                                GVariant            *changed_properties,
                                const gchar* const  *invalidated_properties,
                                gpointer             pp)
{
  if (g_variant_n_children(changed_properties) > 0) {
    GVariantIter *iter;
    const gchar *key;
    GVariant *value;

    debug(" *** Properties Changed:\n");
    g_variant_get(changed_properties, "a{sv}", &iter);
    while (g_variant_iter_loop (iter, "{&sv}", &key, &value)) {
      gchar *value_str;
      value_str = g_variant_print(value, TRUE);
      g_print("      %s -> %s\n", key, value_str);
      g_free(value_str);
    }

    g_variant_iter_free (iter);
  }

  if (g_strv_length ((GStrv) invalidated_properties) > 0) {
    guint n;
    g_print (" *** Properties Invalidated:\n");
    for (n = 0; invalidated_properties[n] != NULL; n++) {
      const gchar *key = invalidated_properties[n];
      g_print ("      %s\n", key);
    }
  }
}
*/

int createConnection(struct proxyParams * pp, GCallback onPropertyChanged)
{
  GError *error;
  GDBusProxyFlags flags;
  GDBusProxy *proxy;

  proxy = NULL;
  error = NULL;

  flags = G_DBUS_PROXY_FLAGS_NONE;
  proxy = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION,
                                         flags,
                                         NULL, /* GDBusInterfaceInfo */
                                         pp->name,
                                         pp->path,
                                         pp->interface,
                                         NULL, /* GCancellable */
                                         &error);
  if (proxy == NULL) {
    g_printerr ("Error creating proxy: %s\n", error->message);
    g_error_free (error);
    return -1;
  }

  pp->proxy = proxy;
  debug("proxy adress : %p, object type : %s\n", pp->proxy, G_OBJECT_TYPE_NAME(pp->proxy));
  if (onPropertyChanged != NULL) {
    // Only ask to be signaled on this proxy if we need it for feedback
    g_signal_connect(proxy, "g-properties-changed",
                     G_CALLBACK(onPropertyChanged),
                     pp);
  }
  g_signal_connect(proxy, "g-signal",
                   G_CALLBACK(on_signal),
                   pp);
  g_signal_connect(proxy, "notify::g-name-owner",
                   G_CALLBACK(on_name_owner_notify),
                   pp);
  printProxy(pp);

  return 0;
}

void call(struct callParams * cp) {

  GError * error;
  GVariant * ret;
  debug("proxy adress : %p, object type : %s\n", cp->proxy->proxy, G_OBJECT_TYPE_NAME(cp->proxy->proxy));
  error = NULL;
  ret = g_dbus_proxy_call_sync(G_DBUS_PROXY(cp->proxy->proxy),
                    cp->method,
                    NULL,
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

void updateClientSocket(int socketd) {
  client_socket = socketd;
}
