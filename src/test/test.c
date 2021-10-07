#include <inttypes.h>
#include <unistd.h>
#include <sqlite3.h>
#include <stdbool.h>
#include "acutest.h"
#include "init.h"
#include "index.h"
#include "sds.h"
#include "find.h"

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

void test_index(void) {
    sqlite3 *db = create_temp_db();
    if(!TEST_CHECK(db != NULL)) {
        return;
    }
    TEST_CHECK(index_cmd(db, "t"));
    TEST_CHECK(index_cmd(db, "t2"));
}

TEST_LIST = {
        { "sanity", test_sanity },
        { "index command", test_index },
        { NULL, NULL }
};
