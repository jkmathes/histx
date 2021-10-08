//
// Created by Matt Wirges on 10/3/21.
//

#ifndef HISTX_CAT_H
#define HISTX_CAT_H

#include <stdbool.h>
#include <sqlite3.h>
#include "find.h"

bool cat_cmd(sqlite3 *db, bool (*cat_handler)(struct hit_context *));


#endif //HISTX_CAT_H
