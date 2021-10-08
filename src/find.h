#ifndef HISTX_FIND_H
#define HISTX_FIND_H

#include <stdbool.h>
#include <sqlite3.h>

#define SEARCH_LIMIT 5

struct hit_context {
    char *cmd;
    uint64_t ts;
};

bool find_cmd(sqlite3 *db, char **keywords, bool (*hit_handler)(struct hit_context *));

#endif //HISTX_FIND_H
