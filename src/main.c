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
#include "explore.h"

#define PRETTY_CYAN "\e[0;36m"
#define PRETTY_NORM "\e[0m"

#define HISTX_USAGE "usage: histx [-d dbfile] <command>\n" \
                    "\toptions:\n" \
                    "\t\t-d path/to/db/file.db -- defaults to $HOME/.histx.db or the value of $HISTX_DB_FILE\n" \
                    "\t\t-h this usage information\n" \
                    "\tcommands:\n" \
                    "\t\tindex - index all arguments after this command - if the only argument after index is `-` read from stdin\n" \
                    "\t\tfind - find matching strings matching the passed arg\n"                                \
                    "\t\tcat - dump the indexed strings\n" \
                    "\t\texplore - interactive searching of the index\n"

void print_usage_and_exit() {
    fprintf(stderr, HISTX_USAGE);
    exit(1);
}

void check_legal_command(char * cmd) {
    if (cmd == NULL) print_usage_and_exit();

    char *available_commands[] = { "index", "index-in", "cat", "find", "explore" };
    size_t cc = sizeof(available_commands) / sizeof(char*);
    for (int i = 0; i < cc; i++) {
        if (strcmp(cmd, available_commands[i]) == 0) {
            return;
        }
    }

    print_usage_and_exit();
}


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
        sdsupdatelen(cmd);
        free(line);
        return cmd;
    }
    else {
        free(line);
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
    int opt;
    char *dbn;
    char *dbe = getenv("HISTX_DB_FILE");
    if (dbe != NULL) {
        dbn = sdsnew(dbe);
    }
    while ((opt = getopt(argc, argv, "hd:")) != -1) {
        printf("optind=%i, opt=%c, opterr=%i, optarg=%s", optind, opt, opterr, optarg);
        switch (opt) {
            case 'd':
                // always overrides
                dbn = sdsnew(optarg);
                break;
            case 'h':
            case '?':
            default:
                print_usage_and_exit();
        }
    }

    check_legal_command(*(argv + optind));

    if (dbn == NULL) {
        dbn = sdscatprintf(sdsempty(), "%s/.histx.db", get_home());
    }
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

    char **iter = argv + optind;

    if(*iter && strcmp(*iter, "index") == 0) {
        iter++;
        if (strcmp(*iter, "-") == 0 && argc - optind + 1) {
            sds cmd;
            while ((cmd = get_cmd_from_stdin()) != NULL) {
                if(strlen(cmd) > 0) {
                    index_cmd(db, cmd);
                }
                sdsfree(cmd);
            }
        }
        else {
            sds cmd = reduce_cmd(iter);
            index_cmd(db, cmd);
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
    else if(*iter && strcmp(*iter, "explore") == 0) {
        explore_cmd(db);
    }
    sqlite3_close(db);
    return 0;
}
