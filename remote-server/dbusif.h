#ifndef DBUSIF_H__
#define DBUSIF_H__

#include "utils.h"

int createConnection(struct proxyParams * pp, GCallback onPropertyChanged);

int call(struct callParams * cp);

void updateClientSocket(int socketd);

#endif
