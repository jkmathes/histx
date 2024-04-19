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
#include "config/config.h"
#include "sds/sds.h"
#include "index.h"
#include "find.h"
#include "cat.h"
#include "init.h"
#include "explore.h"
#include "util.h"

#define PRETTY_CYAN "\e[0;36m"
#define PRETTY_NORM "\e[0m"

#define HISTX_USAGE "usage: histx [-d dbfile] <command>\n" \
                    "\toptions:\n" \
                    "\t\t-d path/to/db/file.db -- defaults to $HOME/.histx.db or the value of $HISTX_DB_FILE\n" \
                    "\t\t-h this usage information\n" \
                    "\tcommands:\n" \
                    "\t\tindex             - index all arguments after this command - \n" \
                    "\t\t                    if the only argument after index is `-` read from stdin\n" \
                    "\t\tfind              - find matching commands using the the passed keywords\n" \
                    "\t\tcat               - dump the indexed commands\n" \
                    "\t\texplore [tmpfile] - interactive searching of the index\n"                              \
                    "\t\t                    If [tmpfile] is provided, will write the selection (if any)\n" \
                    "\t\t                    to the tmp file.\n" \
                    "\t\tprune             - allows you to mark entries to be pruned from the index via the explore interface\n"



void print_usage_and_exit() {
    fprintf(stderr, HISTX_USAGE);
    exit(1);
}

void check_legal_command(char * cmd) {
    if (cmd == NULL) print_usage_and_exit();

    char *available_commands[] = { "index", "cat", "find", "explore", "prune" };
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
    char *when = when_pretty(hit->ts);
    printf(PRETTY_CYAN "[%s]" PRETTY_NORM " %s\n", when, hit->cmd);
    sdsfree(when);
    return true;
}

int main(int argc, char **argv) {
    int opt;
    char *dbn = NULL;
    char *dbe = getenv("HISTX_DB_FILE");
    if (dbe != NULL) {
        dbn = sdsnew(dbe);
    }
    while ((opt = getopt(argc, argv, "hd:")) != -1) {
        switch (opt) {
            case 'd':
                // always overrides
                if(dbn != NULL) {
                    sdsfree(dbn);
                }
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
    sds config = sdscatprintf(sdsempty(), "%s/.histx", get_home());
    FILE *config_fp = fopen(config, "r+");
    if(load_config(config_fp) == false) {
        fprintf(stderr, "Syntax error in [%s]\n", config);
        return -1;
    }
    sdsfree(config);

    sqlite3 *db;

    if(init(dbn, &db) == false) {
        fprintf(stderr, "Unable to initialize histx.db\n");
        return -1;
    }

    sdsfree(dbn);

    char *search_limit = get_setting("search-limit");
    if(search_limit != NULL) {
        const char *estr = NULL;
        // the maximum search hits can be between 5 and 20
        SEARCH_LIMIT = strtonum(search_limit, 5, 20, &estr);
        if (estr != NULL) {
            SEARCH_LIMIT = 5;
        }
    }
    else {
        SEARCH_LIMIT = 5;
    }

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
        iter++;
        FILE *output = NULL;
        if (*iter != NULL) {
            output = fopen(*iter, "w");
        }
        explore_cmd(db, output, MODE_EXPLORE);
        //explore_debug(db);
        if (output != NULL) {
            fclose(output);
        }
    }
    else if(*iter && strcmp(*iter, "prune") == 0) {
        explore_cmd(db, NULL, MODE_PRUNE);
    }
    destroy_config();
    sqlite3_close(db);
    return 0;
}
