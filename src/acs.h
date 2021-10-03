#ifndef HISTX_ACS_H
#define HISTX_ACS_H

#include <inttypes.h>

struct universal_matcher {
    size_t max_states;
    int16_t *g;
    uint8_t *out;
    int8_t *fail;
};

bool string_matches(char *input, struct universal_matcher *machine);
bool build_goto(char **keywords, struct universal_matcher *machine);
struct universal_matcher *create_goto();
void free_goto(struct universal_matcher *machine);

#endif //HISTX_ACS_H
