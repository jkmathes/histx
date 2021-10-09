#include <stdio.h>
#include <string.h>
#include <inttypes.h>
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

bool concat_handler(uint32_t ngram, void *data) {
    sds *work = (sds *)data;
    *work = sdscatprintf(*work, "ngram = %u or ", ngram);
    return true;
}

struct find_context {
    size_t matches;
    char **keywords;
    bool universe;
    struct universal_matcher *m;
    bool (*hit_handler)(struct hit_context *);
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
            free(cmd);
            return 1;
        }
        if(string_matches(cmd, context->m) == false) {
            free(cmd);
            return 0;
        }
        context->matches++;
    }

    uint64_t w = strtoll(ts, NULL, 10);
    struct hit_context hit = { .cmd = cmd, .ts = w};
    context->hit_handler(&hit);
    free(cmd);
    return 0;
}

bool find_cmd(sqlite3 *db, char **keywords, bool (*hit_handler)(struct hit_context *)) {
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
    struct find_context context = {
        .universe = universe,
        .matches = 0,
        .keywords = keywords,
        .m = machine,
        .hit_handler = hit_handler
    };

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
