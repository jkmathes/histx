#if defined(__linux__)
    #define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <pwd.h>
#include <sys/time.h>
#include <time.h>
#include "sds/sds.h"
#include "index.h"
#include "find.h"
#include "cat.h"
#include "init.h"

#define PRETTY_CYAN "\e[0;36m"
#define PRETTY_NORM "\e[0m"

static char *reduce_cmd(char **iter) {
    sds cmd = sdsempty();
    while (*iter) {
        cmd = sdscat(cmd, *iter++);
        if (*iter) {
            cmd = sdscat(cmd, " ");
        }
    }
    cmd[strcspn(cmd, "\n")] = 0;
    return cmd;
}

static char *get_cmd_from_stdin() {
    char *line = NULL;
    size_t len = 0;
    ssize_t linelen = 0;
    linelen = getline(&line, &len, stdin);
    if (linelen > 0) {
        sds cmd = sdsnew(line);
        cmd[strcspn(cmd, "\n")] = 0;
        free(line);
        return cmd;
    } else {
        return NULL;
    }
}

static char *get_home() {
    char *home = getenv("HOME");
    if(home == NULL) {
        home = getpwuid(getuid())->pw_dir;
    }
    return home;
}

static char *format_when(uint64_t delta) {
    uint32_t secs = delta / 1000;
    uint32_t mins = secs / 60;
    uint32_t hours = mins / 60;
    uint32_t days = hours / 24;
    if(days > 0) {
        return sdscatprintf(sdsempty(), "%u day%s ago", days, days > 1 ? "s" : "");
    }
    else if(hours > 0) {
        return sdscatprintf(sdsempty(), "%u hour%s ago", hours, hours > 1 ? "s" : "");
    }
    else if(mins > 0) {
        return sdscatprintf(sdsempty(), "%u minute%s ago", mins, mins > 1 ? "s" : "");
    }
    return sdscatprintf(sdsempty(), "A few moments ago");
}

bool cat_printer(struct hit_context *hit) {
    struct timeval tv;
    tv.tv_sec = hit->ts / 1000;
    tv.tv_usec = hit->ts - tv.tv_sec * 1000; // this is basically pointless

    char timestamp[24];
    strftime(timestamp, sizeof(timestamp) - 1, "%F %T", localtime(&tv.tv_sec));

    printf(PRETTY_CYAN "[%s]" PRETTY_NORM " %s\n", timestamp, hit->cmd);
    return true;
}

bool find_printer(struct hit_context *hit) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t now = (uint64_t )(tv.tv_sec) * 1000 + (uint64_t )(tv.tv_usec) / 1000;
    uint64_t delta = now - hit->ts;
    char *when_pretty = format_when(delta);
    printf(PRETTY_CYAN "[%s]" PRETTY_NORM " %s\n", when_pretty, hit->cmd);
    sdsfree(when_pretty);
    return true;
}

int main(int argc, char **argv) {
    char *dbn = sdscatprintf(sdsempty(), "%s/.histx.db", get_home());
    sqlite3 *db;

    if(access(dbn, F_OK) != 0) {
        if(init(dbn, &db) == false) {
            fprintf(stderr, "Unable to initialize histx.db\n");
        }
    }
    else {
        int r = sqlite3_open(dbn, &db);
        if (r) {
            fprintf(stderr, "Unable to open %s [%s]\n", dbn, sqlite3_errmsg(db));
            sqlite3_close(db);
            return -1;
        }
    }

    sdsfree(dbn);

    char **iter = argv + 1;
    if(*iter && strcmp(*iter, "index") == 0) {
        iter++;
        sds cmd = reduce_cmd(iter);
        index_cmd(db, cmd);
        sdsfree(cmd);
    }
    else if (*iter && strcmp(*iter, "index-in") == 0) {
        sds cmd;
        while ((cmd = get_cmd_from_stdin()) != NULL) {
            if(strlen(cmd) > 0) {
                index_cmd(db, cmd);
            }
            sdsfree(cmd);
        }
    }
    else if(*iter && strcmp(*iter, "find") == 0) {
        iter++;
        find_cmd(db, iter, find_printer);
    }
    else if(*iter && strcmp(*iter, "cat") == 0) {
        cat_cmd(db, cat_printer);
    }
    sqlite3_close(db);
    return 0;
}
