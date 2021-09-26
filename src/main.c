#include <stdio.h>
#include <sqlite3.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include "sds/sds.h"
#include "index.h"
#include "find.h"

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

#define CREATE_FTS          "create virtual table commands using fts5(cmd)"

bool init(char *dbn) {
    sqlite3 *db;
    char *err;
    char *seq[] = {CREATE_TABLE_LUT, LUT_INDEX, CREATE_TABLE_RAW, RAW_INDEX, TS_INDEX, NULL};
    char **s = &seq[0];
    int r = sqlite3_open(dbn, &db);
    if(r) {
        fprintf(stderr, "Unable to open %s [%s]\n", dbn, sqlite3_errmsg(db));
        sqlite3_close(db);
        return false;
    }

    while(*s) {
        r = sqlite3_exec(db, *s++, NULL, NULL, &err);
        if(r != SQLITE_OK) {
            fprintf(stderr, "Unable to initialize database: %s\n", err);
            sqlite3_free(err);
            return false;
        }
    }

    sqlite3_close(db);
    return true;
}

bool test_histx();

char *reduce_cmd(char **iter) {
    sds cmd = sdsempty();
    while (*iter) {
        cmd = sdscat(cmd, *iter++);
        if (*iter) {
            cmd = sdscat(cmd, " ");
        }
    }
    return cmd;
}

int main(int argc, char **argv) {
    //test_histx();

    char *dbn = "histx.db";
    sqlite3 *db;

    if(access("histx.db", F_OK) != 0) {
        if(init("histx.db") == false) {
            fprintf(stderr, "Unable to initialize histx.db\n");
        }
    }

    int r = sqlite3_open(dbn, &db);
    if(r) {
        fprintf(stderr, "Unable to open %s [%s]\n", dbn, sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }

    char **iter = argv + 1;
    if(*iter && strcmp(*iter, "index") == 0) {
        iter++;
        sds cmd = reduce_cmd(iter);
        index_cmd(db, cmd);
    }
    else if(*iter && strcmp(*iter, "find") == 0) {
        iter++;
        find_cmd(db, iter);
    }
    sqlite3_close(db);
    return 0;
}
