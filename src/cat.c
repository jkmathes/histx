//
// Created by Matt Wirges on 10/3/21.
//
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "find.h"
#include "cat.h"
#include "ngram.h"
#include "sds/sds.h"
#include "base64/base64.h"

#define SELECT_RAW_CMD "select hash, ts, cmd, cwd " \
                       "from cmdraw "          \
                       "order by ts asc"

static int cat_handler(void *data, int argc, char **argv, char **col) {
    bool (*handler)(struct hit_context *) = (bool (*)(struct hit_context *))data;
    char *cmd = NULL;
    char *ts = NULL;
    char *cwd = NULL;

    for (size_t f = 0; f < argc; f++) {
        char *c = *col++;
        char *v = *argv++;
        if (strcmp(c, "cmd") == 0) {
            size_t len;
            char *orig = (char *) base64_decode((unsigned char *)v, strlen(v), &len);
            cmd = orig;
        }
        else if (strcmp(c, "ts") == 0) {
            ts = v;
        }
        else if (strcmp(c, "cwd") == 0) {
            if (v != NULL) {
                size_t len;
                char *orig = (char *) base64_decode((unsigned char *)v, strlen(v), &len);
                cwd = orig;
            } else {
                cwd = v;
            }
        }
    }

    uint64_t epoch = strtoll(ts, NULL, 10);
    struct hit_context hit = { .ts = epoch, .cmd = cmd, .cwd = cwd};
    handler(&hit);
    free(cmd);
    return 0;
}

bool cat_cmd(sqlite3 *db, bool (*handler)(struct hit_context *)) {
    sds q = sdsnew(SELECT_RAW_CMD);
    char *err;
    int r = sqlite3_exec(db, q, cat_handler, handler, &err);
    if (r != SQLITE_OK) {
        fprintf(stderr, "Unable to query database: %s\n", err);
        return false;
    }
    sdsfree(q);
    return true;
}
