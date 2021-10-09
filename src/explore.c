#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include "find.h"
#include "explore.h"
#include "sds/sds.h"

#define K_ESC 0x1b

#define K_ARROW 0x5b
#define K_UP    0x41
#define K_DOWN  0x42
#define K_RIGHT 0x43
#define K_LEFT  0x44

#define K_BACK 0x7f

#define K_ENTER 0xa

#define CURSOR_UP   "\e[6A"
#define CLEAR_LINE  "\e[K"

#define PRETTY_GREY   "\e[0;37m"
#define PRETTY_SELECT "\e[46m"

#define PRETTY_CYAN "\e[0;36m"
#define PRETTY_NORM "\e[0m"

#define CURSOR_DISABLE "\e[?25l"
#define CURSOR_ENABLE  "\e[?25h"

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

static int explore_total;
static int max_length;
static char *hits[SEARCH_LIMIT];

bool explore_handler(struct hit_context *hit) {
    if(hits[explore_total] != NULL) {
        sdsfree(hits[explore_total]);
    }
    hits[explore_total++] = sdsnew(hit->cmd);
    return true;
}

sds explore_manipulate(sds current, int c) {
    sds r = current;
    if(c == K_BACK) {
        if(sdslen(current) == 1) {
            sdsfree(current);
            r = sdsempty();
        }
        else {
            sdsrange(current, 0, -3);
        }
    }
    else {
        r = sdscatprintf(current, "%c", c);
    }
    return r;
}

void dump_state(sqlite3 *db, sds current_line, int *current_selection) {
    explore_total = 0;
    max_length = 0;
    printf("Search: %s\xe2\x96\x88" CLEAR_LINE "\n", current_line);
    int argc;
    char **split = sdssplitargs(current_line, &argc);
    if(split != NULL) {
        char **argv = (char **) malloc(sizeof(char **) * argc + 1);
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
            if(explore_total > 0 && f == *current_selection) {
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
                }
            }
        }
        sdsfreesplitres(split, argc);
        free(argv);
        fflush(stdout);
        printf(CURSOR_UP);
    }
}

void dump_final() {
    for(size_t f = 0; f <= SEARCH_LIMIT; f++) {
        printf(CLEAR_LINE "\n");
    }
    fflush(stdout);
    printf(CURSOR_UP);
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

bool explore_cmd(sqlite3 *db) {
    int c;

    signal(SIGINT, done_handler);
    signal(SIGTERM, done_handler);

    enable_non_blocking();
    sds current_line = sdsempty();

    printf(CURSOR_DISABLE);
    int current_selection = 0;
    memset(hits, 0, sizeof(char *) * SEARCH_LIMIT);
    dump_state(db, current_line, &current_selection);
    sds selection = NULL;

    while(!explore_done) {
        if(has_input()) {
            c = fgetc(stdin);
            if(c == K_ESC) {
                c = fgetc(stdin);
                if(c == K_ARROW) {
                    c = fgetc(stdin);
                }
            }

            if(c == K_ENTER) {
                explore_done = true;
                if(hits[current_selection] != NULL) {
                    selection = sdsnew(hits[current_selection]);
                }
            }
            else if(c == K_DOWN) {
                current_selection++;
                dump_state(db, current_line, &current_selection);
            }
            else if(c == K_UP) {
                current_selection--;
                dump_state(db, current_line, &current_selection);
            }
            else {
                current_line = explore_manipulate(current_line, c);
                dump_state(db, current_line, &current_selection);
            }
        }
    }

    dump_final();
    printf(CURSOR_ENABLE);
    reset_non_blocking();
    if(selection != NULL) {
        printf("%s\n", selection);
        sdsfree(selection);
    }
    return true;
}


