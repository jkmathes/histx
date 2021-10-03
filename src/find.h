#ifndef HISTX_FIND_H
#define HISTX_FIND_H

#include <stdbool.h>
#include <sqlite3.h>

#define SEARCH_LIMIT 5
bool find_cmd(sqlite3 *db, char **keywords);

#endif //HISTX_FIND_H
