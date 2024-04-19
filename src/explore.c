#if defined(__linux__)
    #define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <termios.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include "find.h"
#include "explore.h"
#include "sds/sds.h"
#include "config/config.h"
#include "index.h"
#include "base64.h"
#include "util.h"

#define K_ESC 0x1b

#define K_ARROW_VI 0x4f
#define K_ARROW 0x5b
#define K_UP    0x41
#define K_DOWN  0x42
#define K_RIGHT 0x43
#define K_LEFT  0x44
#define K_PRUNE 0x10 // ctl+p

#define K_BACK 0x7f

#define K_ENTER 0xa

#define CURSOR_UP   "\e[%dA"
#define CLEAR_LINE  "\e[K"

#define PRETTY_GREY   "\e[0;37m"
#define PRETTY_SELECT "\e[46m"

#define PRETTY_PRUNE  "\e[45m"
#define PRETTY_PRUNEHOV "\e[4;45m"
#define PRETTY_CYAN "\e[0;36m"
#define PRETTY_NORM "\e[0m"

#define CURSOR_DISABLE "\e[?25l"
#define CURSOR_ENABLE  "\e[?25h"

#define MARK_PRUNE  "insert into cmdan(hash, type, desc) "       \
                    "values (\"%s\", %d, \"%s\") "               \
                    "on conflict(hash) do update "               \
                    "set type=excluded.type, desc=excluded.desc" \
                    ";"

#define SELECT_PRUNE "select r.hash, r.cmd, r.ts, a.type, a.desc " \
                     "from cmdraw as r "                           \
                     "inner join cmdan as a "                      \
                     "where r.hash = a.hash and a.type = 1 "       \
                     "group by r.hash"                             \
                     ";"

#define DELETE_PRUNE "delete from cmdan "              \
                     "where hash = '%s' and type = 1 " \
                     ";"

#define PURGE_PRUNES "begin; "                                     \
                     "   delete from cmdraw where hash in ( "      \
                     "      select hash from cmdan where type = 1" \
                     "   );"                                       \
                     "   delete from cmdlut where hash in ( "      \
                     "      select hash from cmdan where type = 1" \
                     "   );"                                       \
                     "   delete from cmdan where type = 1; "       \
                     "commit;"

static int get_term_width() {
    struct winsize w;
    ioctl(fileno(stdout), TIOCGWINSZ, &w);
    return (int)(w.ws_col);
}

static void prune_current(sqlite3 *db, sds selection, bool unmark) {
    char *err;
    sds hash = create_hash(selection, sdslen(selection));
    sds c = sdsempty();
    if(unmark) {
        c = sdscatprintf(c, DELETE_PRUNE, hash);
    }
    else {
        c = sdscatprintf(c, MARK_PRUNE, hash, 1, "prune");
    }
    int r = sqlite3_exec(db, c, NULL, NULL, &err);
    if(r != SQLITE_OK) {
        fprintf(stderr, "Unable to mark for pruning: %s\n", err);
        return;
    }
    sdsfree(hash);
    sdsfree(c);
}

static void enable_non_blocking() {
    struct termios current;
    tcgetattr(STDIN_FILENO, &current);
    current.c_lflag &= ~ICANON;
    current.c_lflag &= ~ECHO;
    current.c_cc[VMIN] = 1;
    tcsetattr(STDIN_FILENO, TCSANOW, &current);
}

static void reset_non_blocking() {
    struct termios current;
    tcgetattr(STDIN_FILENO, &current);
    current.c_lflag |= ICANON;
    current.c_lflag |= ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &current);
}

static bool has_input() {
    struct timeval tv;
    fd_set fds;
    tv.tv_sec = tv.tv_usec = 0;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
    return FD_ISSET(STDIN_FILENO, &fds);
}

static volatile bool explore_done = false;

static void done_handler(int dummy) {
    explore_done = true;
}

static volatile int explore_total;
static int term_width;
static int max_length;
static char **hits;
static char **hitsraw;
static uint8_t *hitsannotations;

bool explore_handler(struct hit_context *hit) {
    if(hits[explore_total] != NULL) {
        sdsfree(hits[explore_total]);
        sdsfree(hitsraw[explore_total]);
    }
    hits[explore_total] = sdsnew(hit->cmd);
    hitsraw[explore_total] = sdsnew(hits[explore_total]);
    hitsannotations[explore_total] = hit->annotation_type;
    sdsrange(hits[explore_total++], 0, term_width - 2);
    return true;
}

void explore_manipulate(sds *current, int c) {
    if(c == K_BACK) {
        if(sdslen(*current) == 1) {
            sdsfree(*current);
            *current = sdsempty();
        }
        else {
            sdsrange(*current, 0, -3);
        }
    }
    else {
        *current = sdscatprintf(*current, "%c", c);
    }
}

static int select_prune_handler(void *data, int argc, char **argv, char **col) {
    char *cmd = NULL;
    char *ts = NULL;
    uint32_t *counter = (uint32_t *)data;
    *counter = *counter + 1;
    
    if (*counter == 1) {
        printf(PRETTY_PRUNE "### TO BE PRUNED ######################" PRETTY_NORM "\n");
    }

    for(size_t f = 0; f < argc; f++) {
        char *c = *col++;
        char *v = *argv++;
        if(strcmp(c, "cmd") == 0) {
            size_t len;
            char *orig = (char *)base64_decode((unsigned char *)v, strlen(v), &len);
            cmd = orig;
        }
        else if(strcmp(c, "ts") == 0) {
            ts = v;
        }
    }

    if(cmd != NULL && ts != NULL) {
        uint64_t w = strtoll(ts, NULL, 10);
        sds when = when_pretty(w);
        printf(PRETTY_CYAN "[%s]" PRETTY_NORM " %s\n", when, cmd);
        free(cmd);
        sdsfree(when);
    }
    return 0;
}

static void confirm_prune(sqlite3 *db) {
    char *err;
    uint32_t counter = 0;
    int r = sqlite3_exec(db, SELECT_PRUNE, select_prune_handler, &counter, &err);
    if(r != SQLITE_OK) {
        fprintf(stderr, "Unable to fetch prune candidates: %s\n", err);
        return;
    }
    if (counter == 0) {
        printf(PRETTY_PRUNE "Nothing to prune" PRETTY_NORM "\n");
        return;
    } else {
        printf(PRETTY_PRUNE "#######################################" PRETTY_NORM "\n");
    }

    bool confirmed = false;
    char c;
    while(!confirmed) {
        printf("Confirm prune? [y/n] ");
        scanf(" %c", &c);
        c = (char)tolower(c);
        if(c != 'y' && c != 'n') {
            continue;
        }
        confirmed = true;
    }
    if(c == 'y') {
        r = sqlite3_exec(db, PURGE_PRUNES, NULL, NULL, &err);
        if(r != SQLITE_OK) {
            fprintf(stderr, "Unable to prune: %s\n", err);
            return;
        }
        printf("%d items pruned\n", counter);
    }
    else {
        printf("Aborting prune\n");
    }
}

void dump_state(sqlite3 *db, sds *current_line, int *current_selection) {
    explore_total = 0;
    max_length = 0;
    printf("Search: %s\xe2\x96\x88" CLEAR_LINE "\n", *current_line);
    int argc;
    char **split = sdssplitargs(*current_line, &argc);
    if(split != NULL) {
        char **argv = (char **) malloc(sizeof(char **) * (argc + 1));
        char **iter = argv;
        char **split_iter = split;
        for(int counter = 0; counter < argc; counter++) {
            *iter++ = *split_iter++;
        }
        *iter = NULL;
        find_cmd(db, argv, explore_handler);
        if(explore_total > 0 && *current_selection >= explore_total) {
            *current_selection = (explore_total - 1);
        }
        else if(*current_selection < 0) {
            *current_selection = 0;
        }
        for(size_t f = 0; f < SEARCH_LIMIT; f++) {
            if(explore_total > 0 && hitsannotations[f] == 1) {
                if (f == *current_selection) {
                    printf(PRETTY_PRUNEHOV "%s" PRETTY_NORM CLEAR_LINE "\n", hits[f]);
                } else {
                    printf(PRETTY_PRUNE "%s" PRETTY_NORM CLEAR_LINE "\n", hits[f]);
                }
            }
            else if(explore_total > 0 && f == *current_selection) {
                printf(PRETTY_SELECT "%s" PRETTY_NORM CLEAR_LINE "\n", hits[f]);
            }
            else if(f < explore_total) {
                printf(PRETTY_GREY "%s" PRETTY_NORM CLEAR_LINE "\n", hits[f]);
            }
            else {
                printf(CLEAR_LINE "\n");
                if(hits[f] != NULL) {
                    sdsfree(hits[f]);
                    hits[f] = NULL;
                    sdsfree(hitsraw[f]);
                    hitsraw[f] = NULL;
                }
            }
        }
        sdsfreesplitres(split, argc);
        free(argv);
        fflush(stdout);
        printf(CURSOR_UP, SEARCH_LIMIT + 1);
    }
}

void dump_final() {
    for(size_t f = 0; f <= SEARCH_LIMIT; f++) {
        printf(CLEAR_LINE "\n");
    }
    fflush(stdout);
    printf(CURSOR_UP, SEARCH_LIMIT + 1);
}

bool explore_debug(sqlite3 *db) {
    int c;
    signal(SIGINT, done_handler);
    signal(SIGTERM, done_handler);

    enable_non_blocking();
    while(!explore_done) {
        if (has_input()) {
            c = fgetc(stdin);
            if(c == K_ENTER) {
                explore_done = true;
            }
            else {
                printf("%x ", c);
                fflush(stdout);
            }
        }
    }
    reset_non_blocking();
    return true;
}

bool explore_cmd(sqlite3 *db, FILE *output, uint8_t mode) {
    int c;

    hits = (char **)malloc(sizeof(char *) * SEARCH_LIMIT);
    hitsraw = (char **)malloc(sizeof(char *) * SEARCH_LIMIT);
    hitsannotations = (uint8_t *)malloc(sizeof(uint8_t) * SEARCH_LIMIT);

    signal(SIGINT, done_handler);
    signal(SIGTERM, done_handler);

    enable_non_blocking();
    sds current_line = sdsempty();

    printf(CURSOR_DISABLE);
    int current_selection = 0;
    memset(hits, 0, sizeof(char *) * SEARCH_LIMIT);
    dump_state(db, &current_line, &current_selection);
    sds selection = NULL;
    term_width = get_term_width();

    /**
     * If the 'explore-basic' option was set in histx config,
     * simply display the explore result, and don't stuff
     * it into the tty stdin buffer
     */
    bool dont_stuff = false;
    char *explore_basic = get_setting("explore-basic");
    if(explore_basic != NULL && strcmp(explore_basic, "true") == 0) {
        dont_stuff = true;
    }

    while(!explore_done) {
        if(has_input()) {
            bool is_arrow = false;
            c = fgetc(stdin);
            if(c == K_ESC) {
                c = fgetc(stdin);
                // I noticed, at least in zsh, if in vi mode you get ^O for arrow instead of ^[
                // We probably need a more "portable" way to deal with variant termcaps
                if(c == K_ARROW || c == K_ARROW_VI) {
                    is_arrow = true;
                    c = fgetc(stdin);
                }
            }

            if(c == K_ENTER) {
                explore_done = true;
                if(hits[current_selection] != NULL) {
                    selection = sdsnew(hitsraw[current_selection]);
                }
            }
            else if(is_arrow && c == K_DOWN) {
                current_selection++;
                dump_state(db, &current_line, &current_selection);
            }
            else if(is_arrow && c == K_UP) {
                current_selection--;
                dump_state(db, &current_line, &current_selection);
            }
            else if (is_arrow && mode == MODE_PRUNE) {
                bool unmark = hitsannotations[current_selection] == 1;
                if (unmark && c == K_LEFT || !unmark && c == K_RIGHT) {
                    prune_current(db, hitsraw[current_selection], unmark);
                    dump_state(db, &current_line, &current_selection);
                }
            }
            else if(c == K_PRUNE && mode == MODE_PRUNE) {
                bool unmark = hitsannotations[current_selection] == 1;
                prune_current(db, hitsraw[current_selection], unmark);
                dump_state(db, &current_line, &current_selection);
            }
            else {
                explore_manipulate(&current_line, c);
                dump_state(db, &current_line, &current_selection);
            }
        }
    }

    dump_final();
    printf(CURSOR_ENABLE);
    if(selection != NULL && mode == MODE_EXPLORE) {
        if (output != NULL) {
            fprintf(output, "%s\n", selection);
        }
        else {
            char *tty = ttyname(fileno(stdin));
            int term = open(tty, O_RDONLY);
            if(term < 0 || dont_stuff) {
                // Fallback to stdout print
                printf("%s\n", selection);
            }
            else {
                char *iter = selection;
                while(*iter) {
                    ioctl(term, TIOCSTI, iter++);
                }
                close(term);
            }
        }
    }

    reset_non_blocking();

    if(mode == MODE_PRUNE) {
        confirm_prune(db);
    }

    for(int f = 0; f < SEARCH_LIMIT; f++) {
        if(hits[f] != NULL) {
            sdsfree(hits[f]);
            sdsfree(hitsraw[f]);
        }
    }
    if(current_line != NULL) {
        sdsfree(current_line);
    }
    if(selection != NULL) {
        sdsfree(selection);
    }
    free(hits);
    free(hitsraw);
    free(hitsannotations);
    return true;
}


