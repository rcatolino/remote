#ifndef DBUSIF_H__
#define DBUSIF_H__

#include "utils.h"

int createConnection(struct proxyParams * pp, GCallback on_property_changed,
                     GCallback on_name_owner_changed);

void closeConnection(struct proxyParams * pp);

int call(struct callParams * cp);

void updateClientSocket(int socketd);

void printProxy(struct proxyParams * pp);
#endif
