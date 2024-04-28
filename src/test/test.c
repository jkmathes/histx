#if defined(__linux__)
    #define _GNU_SOURCE
#endif
#include <inttypes.h>
#include <unistd.h>
#include <sqlite3.h>
#include <stdbool.h>
#include <stdlib.h>
#include "acutest.h"
#include "init.h"
#include "index.h"
#include "sds.h"
#include "find.h"
#include "config/config.h"
#include "base64.h"
#include "ngram.h"

void test_sanity(void) {
    TEST_CHECK(sizeof(uint8_t) == sizeof(int8_t));
}

sqlite3 *create_temp_db() {
    sqlite3 **db = (sqlite3 **)malloc(sizeof(sqlite3 **));
    if(init(":memory:", db) == false) {
        return NULL;
    }
    sqlite3 *r = *db;
    free(db);
    return r;
}

bool destroy_temp_db(sqlite3 *db) {
    if(db != NULL) {
        sqlite3_close(db);
        return true;
    }
    return false;
}

static int index_check_handler(void *data, int argc, char **argv, char **col) {
    char *input = (char *)data;

    for(size_t f = 0; f < argc; f++) {
        char *c = *col++;
        char *v = *argv++;
        if(strcmp(c, "hash") == 0) {
            sds hash = create_hash(input, strlen(input));
            TEST_CHECK(strcmp(v, hash) == 0);
            TEST_MSG("Hash mismatch: expected[%s], found[%s]", hash, v);
            sdsfree(hash);
        }
        else if(strcmp(c, "cmd") == 0) {
            size_t len;
            char *cmd = (char *)base64_decode((unsigned char *)v, strlen(v), &len);
            TEST_CHECK(strlen(cmd) == strlen(input));
            TEST_MSG("Length mismatch: expected[%s], found[%s]", input, cmd);
            TEST_CHECK(strcmp(cmd, input) == 0);
            TEST_MSG("String mismatch: expected[%s], found[%s]", input, cmd);
            free(cmd);
        }
    }

    return 0;
}

void test_index(void) {
    sqlite3 *db = create_temp_db();
    if(!TEST_CHECK(db != NULL)) {
        TEST_MSG("Unable to create temporary database");
        return;
    }

    sds input = sdscatprintf(sdsempty(), "%s", "");
    char *input_set = "abcdefghijklmnopqrstuvwxyz 0123456789";
    size_t len = strlen(input_set);
    char *query = "select * from cmdraw order by ts desc limit 1;";

    for(size_t f = 0; f < 100; f++) {
        char r = input_set[random() % len];
        input = sdscatprintf(input, "%c", r);
        TEST_CHECK(index_cmd(db, input));
        char *err;
        int sr = sqlite3_exec(db, query, index_check_handler, input, &err);
        TEST_CHECK(sr == SQLITE_OK);
    }
    sdsfree(input);
    destroy_temp_db(db);
}

static char *current_input = NULL;
static int current_hit = 0;

static bool find_check_handler(struct hit_context *hit) {
    if(current_hit == 0) {
        TEST_CHECK(strcmp(hit->cmd, current_input) == 0);
    }
    else {
        TEST_CHECK(strlen(hit->cmd) < strlen(current_input));
    }
    current_hit++;
    return true;
}

void test_find(void) {
    sqlite3 *db = create_temp_db();
    if(!TEST_CHECK(db != NULL)) {
        TEST_MSG("Unable to create temporary database");
        return;
    }

    sds input = sdscatprintf(sdsempty(), "%s", "");
    char *input_set = "abcdefghijklmnopqrstuvwxyz 0123456789";
    size_t len = strlen(input_set);
    char *keywords[2];
    keywords[1] = NULL;

    for(size_t f = 0; f < 100; f++) {
        char r = input_set[random() % len];
        input = sdscatprintf(input, "%c", r);
        keywords[0] = input;
        current_input = input;
        current_hit = 0;
        TEST_CHECK(index_cmd(db, input));
        TEST_CHECK(find_cmd(db, keywords, find_check_handler));
    }

    sdsfree(input);
    destroy_temp_db(db);
}

void test_config(void) {
    TEST_CHECK(get_setting("none") == NULL);
    add_setting("key1", "value1");
    add_setting("key2", "value2");
    TEST_CHECK(get_setting("key1") != NULL);
    TEST_CHECK(strcmp(get_setting("key2"), "value2") == 0);
    TEST_CHECK(get_setting("key3") == NULL);
    TEST_CHECK(strcmp(get_setting("key1"), "value1") == 0);

    FILE *fp = tmpfile();
    fprintf(fp, "key1 = true\n");
    fprintf(fp, "key2 = True\n");
    fprintf(fp, "key3 = TRUE\n");
    fprintf(fp, "key4 = false\n");
    fprintf(fp, "key5 = False\n");
    fprintf(fp, "key6 = FALSE\n");
    rewind(fp);
    load_config(fp);

    char *truths[] = { "key1", "key2", "key3", NULL };
    for(int f = 0; truths[f] != NULL; f++) {
        TEST_CHECK(strcmp(get_setting(truths[f]), "true") == 0);
    }
    char *falses[] = { "key4", "key5", "key6", NULL };
    for(int f = 0; falses[f] != NULL; f++) {
        TEST_CHECK(strcmp(get_setting(falses[f]), "false") == 0);
    }
    destroy_config();
}

bool test_ngram_count_handler(uint32_t ngram, void *data) {
    int *c = (int *)data;
    *c = *c + 1;
    return true;
}

bool test_ngram_sum_handler(uint32_t ngram, void *data) {
    uint32_t *c = (uint32_t *)data;
    *c = *c + ngram;
    return true;
}

void test_ngrams(void) {
    int *c = (int *)malloc(sizeof(int));
    *c = 0;
    TEST_CHECK(gen_ngrams("testing", 3, test_ngram_count_handler, (void *)c));
    TEST_CHECK(*c == 5);
    *c = 0;
    TEST_CHECK(gen_ngrams("testing", 2, test_ngram_count_handler, (void *)c));
    TEST_CHECK(*c == 6);
    free(c);

    uint32_t *sum = (uint32_t *)malloc(sizeof(uint32_t));
    *sum = 0;
    TEST_CHECK(gen_ngrams("testing", 3, test_ngram_sum_handler, (void *)sum));
    TEST_CHECK(*sum == 0x22B2525);
    free(sum);
}

void test_hash(void) {
    sds input = sdscatprintf(sdsempty(), "%s", "histx test hash");
    sds hash = create_hash(input, sdslen(input));
    TEST_CHECK(strncmp(hash, "06afb70aa2b22ddc874af3881454dca9d6cfd4fedc81b36f85928f0ac3c752d1", 64) == 0);
    sdsfree(input);
    sdsfree(hash);
}

TEST_LIST = {
        { "sanity", test_sanity },
        { "index command", test_index },
        { "find command", test_find },
        { "configuration", test_config },
        { "ngram generation", test_ngrams },
	    { "hash testing", test_hash },
        { NULL, NULL }
};
