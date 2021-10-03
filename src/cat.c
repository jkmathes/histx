//
// Created by Matt Wirges on 10/3/21.
//
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <sys/time.h>
#include "cat.h"
#include "ngram.h"
#include "sds/sds.h"
#include "base64/base64.h"

#define SELECT_RAW_CMD "select hash, ts, cmd " \
                       "from cmdraw "          \
                       "order by ts asc"

#define PRETTY_CYAN "\e[0;36m"
#define PRETTY_NORM "\e[0m"

#pragma clang diagnostic push
#pragma ide diagnostic ignored "UnusedParameter"
static int cat_handler(void *data, int argc, char **argv, char **col) {
#pragma clang diagnostic pop

    char *cmd = NULL;
    char *ts = NULL;

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
    }

    uint64_t epoch = strtoll(ts, NULL, 10);
    struct timeval tv;
    tv.tv_sec = epoch / 1000;
    tv.tv_usec = epoch - tv.tv_sec * 1000; // this is basically pointless

    char timestamp[24];
    strftime(timestamp, sizeof(timestamp), "%F %T", localtime(&tv.tv_sec));

    printf(PRETTY_CYAN "[%s]" PRETTY_NORM "%s\n", timestamp, cmd);
    free(cmd);
    return 0;
}

bool cat_cmd(sqlite3 *db) {

    sds q = sdsnew(SELECT_RAW_CMD);
    char *err;
    int r = sqlite3_exec(db, q, cat_handler, NULL, &err);
    if (r != SQLITE_OK) {
        fprintf(stderr, "Unable to query database: %s\n", err);
        return false;
    }

    return true;
}