#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include "acs.h"

/**
 * If 100% of the input keywords are smaller than our ngram size,
 * we have no choice but to start searching the universe. This is done
 * via a lightweight version of aho-corasick; pre-compile the search
 * terms once, and then start putting each stored command through
 * the built machine looking for hits. Once we get some number (5),
 * bail out
 */
struct queue_node {
    size_t state;
    struct queue_node *next;
};

struct queue {
    struct queue_node *head;
    struct queue_node *tail;
};

struct queue *create_queue() {
    struct queue *r = (struct queue *)malloc(sizeof(struct queue));
    r->head = NULL;
    r->tail = NULL;
    return r;
}

struct queue_node *create_node(size_t state) {
    struct queue_node *r = (struct queue_node *)malloc(sizeof(struct queue_node));
    if(r == NULL) {
        return NULL;
    }
    r->state = state;
    r->next = NULL;
    return r;
}

bool enqueue(struct queue *q, size_t state) {
    struct queue_node *n = create_node(state);
    if(n == NULL) {
        return false;
    }
    if(q->head == NULL) {
        q->head = n;
        q->tail = n;
    }
    else {
        q->tail->next = n;
        q->tail = n;
    }
    return true;
}

bool dequeue(struct queue *q, size_t *state) {
    if(q->head == NULL) {
        *state = SIZE_T_MAX;
        return false;
    }
    struct queue_node *old = q->head;
    size_t r = old->state;
    q->head = old->next;
    free(old);
    *state = r;
    return true;
}

static void dump_goto(int16_t *g, size_t max_states) {
    for(int ff = 0; ff < max_states; ff++) {
        int16_t *c = g + (ff * CHAR_MAX);
        for(int gg = 0; gg < CHAR_MAX; gg++) {
            if(c[gg] == -1) {
                printf("-");
            }
            else {
                printf("%d", c[gg]);
            }
        }
        printf("\n");
    }
}

static void dump_fails(int8_t *fail, size_t max_states) {
    for(size_t f = 0; f < max_states; f++) {
        printf("%d ", fail[f]);
    }
    printf("\n");
}

bool build_goto(char **keywords, struct universal_matcher *machine) {
    size_t max_states = 1;
    size_t max_keywords = 0;
    char **iter = keywords;
    while(*iter) {
        max_states += strlen(*iter++);
        max_keywords++;
    }

    if(max_states > INT16_MAX || max_keywords > UINT8_MAX) {
        return NULL;
    }

    size_t buffer_size = sizeof(int16_t) * CHAR_MAX * max_states;
    int16_t *r = (int16_t *)malloc(buffer_size);
    uint8_t *out = (uint8_t *)malloc(sizeof(uint8_t) * max_states);
    int8_t *fail = (int8_t *)malloc(sizeof(int8_t) * max_states);

    memset(r, -1, buffer_size);
    memset(out, 0, sizeof(uint8_t) * max_states);
    memset(fail, -1, sizeof(int8_t) * max_states);

    iter = keywords;
    size_t j = 1;
    size_t word = 0;

    while(*iter) {
        uint16_t state = 0;
        char *s = *iter++;
        while(*s) {
            char c = *s++;
            int16_t *current = r + (state * CHAR_MAX);
            size_t i = (size_t)c;
            if(current[i] == -1) {
                current[i] = j++;
            }
            state = current[i];
        }

        out[state] |= (1 << word);
        word++;
    }

    struct queue *q = create_queue();

    for(size_t i = 0; i < CHAR_MAX; i++) {
        if(r[i] == -1) {
            r[i] = 0;
        }
        if(r[i] != 0) {
            fail[r[i]] = 0;
            enqueue(q, r[i]);
        }
    }

    while(q->head != NULL) {
        size_t state;
        dequeue(q, &state);
        int16_t *current = r + (state * CHAR_MAX);
        for(size_t i = 0; i < CHAR_MAX; i++) {
            if(current[i] != -1) {
                int8_t f = fail[state];
                int16_t *fail_state = r + (f * CHAR_MAX);
                while(fail_state[i] == -1) {
                    f = fail[f];
                    fail_state = r + (f * CHAR_MAX);
                }

                f = (int8_t)(fail_state[i] & 0xff);
                fail[current[i]] = f;

                out[current[i]] |= out[f];

                enqueue(q, current[i]);
            }
        }
    }

    machine->max_states = max_states;
    machine->out = out;
    machine->fail = fail;
    machine->g = r;
    return true;
}

bool string_matches(char *input, struct universal_matcher *machine) {
    int16_t *g = machine->g;
    int8_t *fail = machine->fail;
    uint8_t *out = machine->out;
    size_t max_states = machine->max_states;

    size_t state = 0;
    while(*input) {
        char c = *input++;
        int16_t *current = g + (state * CHAR_MAX);
        while(current[c] == -1) {
            state = fail[state];
            current = g + (state * CHAR_MAX);
        }
        state = current[c];
        if(out[state] == 0) {
            continue;
        }

        for(int f = 0; f < max_states; f++) {
            if((out[state] & (1 << f)) > 0) {
                return true;
            }
        }
    }
    return false;
}

struct universal_matcher *create_goto() {
    struct universal_matcher *r = (struct universal_matcher *)malloc(sizeof(struct universal_matcher));
    r->out = NULL;
    r->max_states = 0;
    r->g = NULL;
    r->fail = NULL;
    return r;
}

void free_goto(struct universal_matcher *m) {
    if(!m) {
        return;
    }
    if(m->fail) free(m->fail);
    if(m->out) free(m->out);
    if(m->g) free(m->g);
    free(m);
}
