#ifndef HISTX_EXPLORE_H
#define HISTX_EXPLORE_H

#include <stdbool.h>
#include "find.h"

#define MODE_EXPLORE 0x1
#define MODE_PRUNE 0x2

bool explore_cmd(sqlite3 *db, FILE *output, uint8_t mode);
bool explore_debug(sqlite3 *db);

#endif //HISTX_EXPLORE_H
