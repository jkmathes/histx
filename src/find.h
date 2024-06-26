#ifndef HISTX_FIND_H
#define HISTX_FIND_H

#include <stdbool.h>
#include <sqlite3.h>
#include <inttypes.h>

extern int SEARCH_LIMIT;

struct hit_context {
    char *cmd;
    uint64_t ts;
    uint8_t annotation_type;
};

bool find_cmd(sqlite3 *db, char **keywords, bool (*hit_handler)(struct hit_context *));

#endif //HISTX_FIND_H
