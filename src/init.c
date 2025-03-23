#include <sqlite3.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>
#include "sds/sds.h"
#include "init.h"

#define CREATE_TABLE_LUT    "create table if not exists cmdlut (" \
                                "host text," \
                                "ngram integer," \
                                "hash text" \
                            ");"

#define LUT_INDEX           "create unique index if not exists ngramindex on cmdlut(ngram, hash);"
#define LUT_HASH_INDEX      "create index if not exists ngramhashindex on cmdlut(hash);"

#define CREATE_TABLE_RAW    "create table if not exists cmdraw (" \
                                "hash text," \
                                "ts integer," \
                                "cmd text," \
                                "cwd text" \
                            ");"

#define RAW_INDEX           "create unique index if not exists hashindex on cmdraw(hash);"
#define TS_INDEX            "create index if not exists tsindex on cmdraw(ts);"

#define CREATE_TABLE_AN     "create table if not exists cmdan (" \
                                "hash text primary key,"         \
                                "type integer,"                  \
                                "desc text"                      \
                            ");"

#define ANNOTATION_INDEX    "create unique index if not exists anindex on cmdan(hash, type);"

#define CREATE_TABLE_VERSION "create table if not exists histxversion (" \
                                "version integer primary key asc," \
                                "whence integer" \
                              ");"

#define M1_ADD_CWD "alter table cmdraw add column cwd text;"

#define FINISH_MIGRATION "insert into histxversion(version, whence) values(%d, unixepoch());"

#define GET_VERSION "select max(version) from histxversion;"
#define THIS_VERSION 1


int getdbversion(sqlite3 *db) {

    const char *sql = GET_VERSION;
    sqlite3_stmt *stmt;
    int r = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    if (r != SQLITE_OK) {
        fprintf(stderr, "Unable to prepare statement: %s\n", sqlite3_errmsg(db));
        return 0;
    }

    int value;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        value = sqlite3_column_int(stmt, 0);
    } else {
        fprintf(stderr, "Unable to get db version: %s\n", sqlite3_errmsg(db));
        value = 0;
    }

    sqlite3_finalize(stmt);

    return value;
}

bool mark_version(sqlite3 *db, int version) {
    sds sql = sdsempty();
    sql = sdscatprintf(sql, FINISH_MIGRATION, version);
    int r = sqlite3_exec(db, sql, NULL, NULL, NULL);
    sdsfree(sql);
    if (r != SQLITE_OK) {
        fprintf(stderr, "Unable to mark version: %s\n", sqlite3_errmsg(db));

        return false;
    }

    return true;
}

bool migrate_1(sqlite3 *db) {
    int r = sqlite3_exec(db, M1_ADD_CWD, NULL, NULL, NULL);
    if (r != SQLITE_OK) {
        fprintf(stderr, "Unable to execute command: %s\n", sqlite3_errmsg(db));
        return false;
    }

    mark_version(db, THIS_VERSION);
    return true;
}

bool init(char *dbn, sqlite3 **db) {
    sqlite3 *d;
    char *err;
    char *seq[] = {
            CREATE_TABLE_LUT,
            LUT_INDEX,
            LUT_HASH_INDEX,
            CREATE_TABLE_RAW,
            RAW_INDEX,
            TS_INDEX,
            CREATE_TABLE_AN,
            ANNOTATION_INDEX,
            CREATE_TABLE_VERSION,
            NULL
    };
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
            fprintf(stderr, "Unable to initialize database [%s]: %s\n", *s, err);
            sqlite3_free(err);
            return false;
        }
    }

    *db = d;

    int dbv = getdbversion(d);

    if (dbv < THIS_VERSION) {
        bool m = migrate_1(d);
    }

    return true;
}
