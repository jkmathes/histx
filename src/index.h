//
// Created by Jack Matheson on 9/19/21.
//

#ifndef HISTX_INDEX_H
#define HISTX_INDEX_H

#include <stdbool.h>
#include <sqlite3.h>

char *create_hash(char *src, size_t len);
bool index_cmd(sqlite3 *db, char *cmd);

#endif //HISTX_INDEX_H
