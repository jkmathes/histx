#if defined(__linux__)
    #define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
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

#define K_ESC 0x1b

#define K_ARROW_VI 0x4f
#define K_ARROW 0x5b
#define K_UP    0x41
#define K_DOWN  0x42
#define K_RIGHT 0x43
#define K_LEFT  0x44

#define K_BACK 0x7f

#define K_ENTER 0xa

#define CURSOR_UP   "\e[%dA"
#define CLEAR_LINE  "\e[K"

#define PRETTY_GREY   "\e[0;37m"
#define PRETTY_SELECT "\e[46m"

#define PRETTY_CYAN "\e[0;36m"
#define PRETTY_NORM "\e[0m"

#define CURSOR_DISABLE "\e[?25l"
#define CURSOR_ENABLE  "\e[?25h"

static int get_term_width() {
    struct winsize w;
    ioctl(fileno(stdout), TIOCGWINSZ, &w);
    return (int)(w.ws_col);
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

bool explore_handler(struct hit_context *hit) {
    if(hits[explore_total] != NULL) {
        sdsfree(hits[explore_total]);
        sdsfree(hitsraw[explore_total]);
    }
    hits[explore_total] = sdsnew(hit->cmd);
    hitsraw[explore_total] = sdsnew(hits[explore_total]);
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

bool explore_cmd(sqlite3 *db, FILE *output) {
    int c;

    hits = (char **)malloc(sizeof(char *) * SEARCH_LIMIT);
    hitsraw = (char **)malloc(sizeof(char *) * SEARCH_LIMIT);

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
            else {
                explore_manipulate(&current_line, c);
                dump_state(db, &current_line, &current_selection);
            }
        }
    }

    dump_final();
    printf(CURSOR_ENABLE);
    if(selection != NULL) {
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
        sdsfree(selection);
    }
    reset_non_blocking();
    for(int f = 0; f < SEARCH_LIMIT; f++) {
        if(hits[f] != NULL) {
            sdsfree(hits[f]);
            sdsfree(hitsraw[f]);
        }
    }
    if(current_line != NULL) {
        sdsfree(current_line);
    }
    return true;
}


