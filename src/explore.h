#ifndef HISTX_EXPLORE_H
#define HISTX_EXPLORE_H

#include <stdbool.h>
#include "find.h"

bool explore_cmd(sqlite3 *db, FILE *output);
bool explore_debug(sqlite3 *db);

#endif //HISTX_EXPLORE_H
