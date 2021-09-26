//
// Created by Jack Matheson on 9/21/21.
//

#ifndef HISTX_FIND_H
#define HISTX_FIND_H

#include <stdbool.h>
#include <sqlite3.h>

bool find_cmd(sqlite3 *db, char **keywords);

#endif //HISTX_FIND_H
