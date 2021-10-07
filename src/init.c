#include <sqlite3.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>
#include "init.h"

#define CREATE_TABLE_LUT    "create table if not exists cmdlut (" \
                                "host text," \
                                "ngram integer," \
                                "hash text" \
                            ");"

#define LUT_INDEX           "create unique index ngramindex on cmdlut(ngram, hash);"

#define CREATE_TABLE_RAW    "create table if not exists cmdraw (" \
                                "hash text," \
                                "ts integer," \
                                "cmd text" \
                            ");"

#define RAW_INDEX           "create unique index hashindex on cmdraw(hash);"
#define TS_INDEX            "create index tsindex on cmdraw(ts);"

bool init(char *dbn, sqlite3 **db) {
    sqlite3 *d;
    char *err;
    char *seq[] = {CREATE_TABLE_LUT, LUT_INDEX, CREATE_TABLE_RAW, RAW_INDEX, TS_INDEX, NULL};
    char **s = &seq[0];
    int r = sqlite3_open(dbn, &d);
    if(r) {
        fprintf(stderr, "Unable to open %s [%s]\n", dbn, sqlite3_errmsg(d));
        sqlite3_close(d);
        return false;
    }

    while(*s) {
        r = sqlite3_exec(d, *s++, NULL, NULL, &err);
        if(r != SQLITE_OK) {
            fprintf(stderr, "Unable to initialize database: %s\n", err);
            sqlite3_free(err);
            return false;
        }
    }

    *db = d;
    return true;
}