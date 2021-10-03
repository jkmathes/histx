#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <sys/time.h>
#include "find.h"
#include "ngram.h"
#include "acs.h"
#include "sds/sds.h"
#include "base64/base64.h"

#define EXPAND_STR(v)       #v
#define TO_STR(v)           EXPAND_STR(v)

#define SELECT_LUT_HDR      "select distinct(cmdlut.hash), count(cmdlut.hash) as rank, cmd, ts " \
                            "from cmdlut " \
                            "inner join cmdraw on cmdlut.hash = cmdraw.hash " \
                            "where "

#define SELECT_LUT_FTR      "group by cmdlut.hash order by rank desc, ts desc limit " \
                            TO_STR(SEARCH_LIMIT) \
                            ";"
                                                                                      \
#define SELECT_ALL          "select distinct(cmdlut.hash), cmd, ts from cmdlut " \
                            "inner join cmdraw on cmdlut.hash = cmdraw.hash " \
                            "group by cmdlut.hash order by ts desc;"

#define PRETTY_CYAN "\e[0;36m"
#define PRETTY_NORM "\e[0m"

bool concat_handler(uint32_t ngram, void *data) {
    sds *work = (sds *)data;
    *work = sdscatprintf(*work, "ngram = %u or ", ngram);
    return true;
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

struct find_context {
    size_t matches;
    char **keywords;
    bool universe;
    struct universal_matcher *m;
};

static int find_handler(void *data, int argc, char **argv, char **col) {
    char *cmd = NULL;
    char *ts = NULL;
    struct find_context *context = (struct find_context *)data;

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

    if(context->universe) {
        if(context->matches >= SEARCH_LIMIT) {
            return 1;
        }
        if(string_matches(cmd, context->m) == false) {
            return 0;
        }
        context->matches++;
    }

    uint64_t w = strtoll(ts, NULL, 10);
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t now = (uint64_t )(tv.tv_sec) * 1000 + (uint64_t )(tv.tv_usec) / 1000;
    uint64_t delta = now - w;
    char *when_pretty = format_when(delta);

    printf(PRETTY_CYAN "[%s]" PRETTY_NORM " %s\n", when_pretty, cmd);
    free(cmd);
    sdsfree(when_pretty);
    return 0;
}

bool find_cmd(sqlite3 *db, char **keywords) {
    char **iter = keywords;
    bool universe = true;
    struct universal_matcher *machine = NULL;

    sds c = sdscatprintf(sdsempty(), "%s ", SELECT_LUT_HDR);
    while(*iter) {
        if(strlen(*iter) > 2) {
            universe = false;
        }
        gen_ngrams(*iter++, 3, concat_handler, &c);
    }
    sdsrange(c, 0, -4);
    c = sdscatprintf(c, "%s", SELECT_LUT_FTR);

    if(universe) {
        sdsfree(c);
        machine = create_goto();
        if(build_goto(keywords, machine) == false) {
            free_goto(machine);
            return false;
        }
        c = sdscatprintf(sdsempty(), "%s", SELECT_ALL);
    }

    char *err;
    struct find_context context = {.universe = universe, .matches = 0, .keywords = keywords, .m = machine};
    int r = sqlite3_exec(db, c, find_handler, &context, &err);
    if(r != SQLITE_OK) {
        if(!(universe && r == SQLITE_ABORT)) {
            fprintf(stderr, "Unable to query database: %s\n", err);
        }
    }
    free_goto(machine);
    sdsfree(c);
    return true;
}
