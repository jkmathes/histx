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

#define SELECT_LUT_HDR      "select distinct(cmdlut.hash), count(cmdlut.hash) as rank, cmd, ts, type " \
                            "from cmdlut "                                                             \
                            "inner join cmdraw on cmdlut.hash = cmdraw.hash "                          \
                            "left outer join cmdan on cmdraw.hash = cmdan.hash "                       \
                            "where "

#define SELECT_LUT_T        "group by cmdlut.hash order by rank desc, ts desc limit " \
                            "%d"                                                      \
                            ";"

#define SELECT_ALL          "select cmdraw.hash, ts, cmd, type "                 \
                            "from cmdraw "                                       \
                            "left outer join cmdan on cmdraw.hash = cmdan.hash " \
                            "order by ts desc"                                   \
                            ";"

char *SELECT_LUT_FTR = NULL;
int SEARCH_LIMIT = 5;

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
    char *type = NULL;
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
        else if(strcmp(c, "type") == 0) {
            type = v;
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
    uint8_t t = 0;
    if(type != NULL) {
        t = strtol(type, NULL, 10);
    }
    struct hit_context hit = { .cmd = cmd, .ts = w, .annotation_type = t};
    context->hit_handler(&hit);
    free(cmd);
    return 0;
}

bool find_cmd(sqlite3 *db, char **keywords, bool (*hit_handler)(struct hit_context *)) {
    char **iter = keywords;
    bool universe = true;
    bool all_empty = true;
    struct universal_matcher *machine = NULL;

    SELECT_LUT_FTR = sdscatprintf(sdsempty(), SELECT_LUT_T, SEARCH_LIMIT);
    sds c = sdscatprintf(sdsempty(), "%s ", SELECT_LUT_HDR);
    while(*iter) {
        size_t kw_len = strlen(*iter);
        if(kw_len > 2) {
            universe = false;
        }
        if(kw_len > 0) {
            all_empty = false;
        }
        gen_ngrams(*iter++, 3, concat_handler, &c);
    }
    // remove the trailing ' or ' that the concat_handler added
    // (only meaningful if we were ngramming)
    sdsrange(c, 0, -4);
    c = sdscatprintf(c, "%s", SELECT_LUT_FTR);
    sdsfree(SELECT_LUT_FTR);

    if(all_empty) {
        sdsfree(c);
        return false;
    }

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
            sqlite3_free(err);
            return false;
        }
    }
    if(err != NULL) {
        sqlite3_free(err);
    }
    free_goto(machine);
    sdsfree(c);
    return true;
}
