#include <inttypes.h>
#include "acutest.h"

void test_sanity(void) {
    TEST_CHECK(sizeof(uint8_t) == sizeof(int8_t));
}

TEST_LIST = {
        { "sanity", test_sanity },
        { NULL, NULL }
};
