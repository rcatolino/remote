#ifndef DBUSIF_H__
#define DBUSIF_H__

#include "utils.h"

GVariant *updateProperty(const struct proxyParams *pp,
                          const char *prop_name);

int setProperty(const struct proxyParams * pp, const char *prop_name,
                GVariant *value);

int createConnectionOnBus(struct proxyParams * pp, GCallback on_property_changed,
                          GCallback on_name_owner_changed, GBusType bus);

int createConnection(struct proxyParams *pp, GCallback on_property_changed,
                     GCallback on_name_owner_changed);

void closeConnection(struct proxyParams *pp);

void call(struct callParams *cp, GVariant *parameter);

void call_dbus(const struct proxyParams *pp, const char *method, GVariant *parameter);

void updateClientSocket(int socketd);

void printProxy(struct proxyParams *pp);
#endif
