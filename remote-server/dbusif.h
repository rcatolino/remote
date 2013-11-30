#ifndef DBUSIF_H__
#define DBUSIF_H__

#include "utils.h"

GVariant *updateProperty(const struct proxyParams *pp,
                          const char *prop_name);

int createConnection(struct proxyParams *pp, GCallback on_property_changed,
                     GCallback on_name_owner_changed);

void closeConnection(struct proxyParams *pp);

void call(struct callParams *cp, const char *argument_buff);

void updateClientSocket(int socketd);

void printProxy(struct proxyParams *pp);
#endif
