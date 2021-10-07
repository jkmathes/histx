#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <pwd.h>
#include "sds/sds.h"
#include "index.h"
#include "find.h"
#include "cat.h"
#include "init.h"

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
        find_cmd(db, iter);
    }
    else if(*iter && strcmp(*iter, "cat") == 0) {
        cat_cmd(db);
    }
    sqlite3_close(db);
    return 0;
}
