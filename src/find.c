//
// Created by Jack Matheson on 9/21/21.
//
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "find.h"
#include "ngram.h"
#include "sds/sds.h"
#include "rax/rax.h"
#include "base64/base64.h"

#define SELECT_LUT_HDR      "select distinct(cmdlut.hash), count(cmdlut.hash) as rank, cmd, ts " \
                            "from cmdlut " \
                            "inner join cmdraw on cmdlut.hash = cmdraw.hash " \
                            "where "

#define SELECT_LUT_FTR      "group by cmdlut.hash order by rank desc, ts desc limit 5;"

bool concat_handler(uint32_t ngram, void *data) {
    sds *work = (sds *)data;
    *work = sdscatprintf(*work, "ngram = %u or ", ngram);
    return true;
}

static int find_handler(void *data, int argc, char **argv, char **col) {
    for(size_t f = 0; f < argc; f++) {
        char *c = *col++;
        char *v = *argv++;
        if(strcmp(c, "cmd") == 0) {
            size_t len;
            char *orig = (char *) base64_decode((unsigned char *)v, strlen(v), &len);
            if(orig != NULL && orig[len - 1] == '\n') {
                orig[len - 1] = 0;
            }
            printf("%s\n", orig);
        }
    }
    printf("\n");
    return 0;
}

bool find_cmd(sqlite3 *db, char **keywords) {
    char **iter = keywords;
    sds c = sdscatprintf(sdsempty(), "%s ", SELECT_LUT_HDR);
    while(*iter) {
        gen_ngrams(*iter++, 3, concat_handler, &c);
    }
    sdsrange(c, 0, -4);
    c = sdscatprintf(c, "%s", SELECT_LUT_FTR);

    char *err;
    int r = sqlite3_exec(db, c, find_handler, NULL, &err);
    if(r != SQLITE_OK) {
        fprintf(stderr, "Unable to query database: %s\n", err);
    }
    return true;
}
