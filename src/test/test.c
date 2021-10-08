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
#include "base64.h"

void test_sanity(void) {
    TEST_CHECK(sizeof(uint8_t) == sizeof(int8_t));
}

sqlite3 *create_temp_db() {
    sqlite3 **db = (sqlite3 **)malloc(sizeof(sqlite3 **));
    if(init(":memory:", db) == false) {
        return NULL;
    }
    return *db;
}

bool destroy_temp_db(sqlite3 *db) {
    if(db != NULL) {
        sqlite3_close(db);
        sqlite3_free(db);
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
            char *hash = create_hash(input, strlen(input));
            TEST_CHECK(strcmp(v, hash) == 0);
            TEST_MSG("Hash mismatch: expected[%s], found[%s]", hash, v);
        }
        else if(strcmp(c, "cmd") == 0) {
            size_t len;
            char *cmd = (char *)base64_decode((unsigned char *)v, strlen(v), &len);
            TEST_CHECK(strlen(cmd) == strlen(input));
            TEST_MSG("Length mismatch: expected[%s], found[%s]", input, cmd);
            TEST_CHECK(strcmp(cmd, input) == 0);
            TEST_MSG("String mismatch: expected[%s], found[%s]", input, cmd);
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
}

TEST_LIST = {
        { "sanity", test_sanity },
        { "index command", test_index },
        { NULL, NULL }
};
