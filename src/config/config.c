#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <limits.h>
#include <string.h>
#include <inttypes.h>
#include "sds/sds.h"
#include "config/config.h"

struct trie_node {
    char c;
    struct trie_node **children;
    char *value;
};

struct trie_node *create_trie_node(char c) {
    struct trie_node *r = (struct trie_node *)malloc(sizeof(struct trie_node));
    r->c = c;
    size_t max_children = (((uint64_t)1 << (sizeof(char) * CHAR_BIT)) - 1);
    r->children = (struct trie_node **)malloc(sizeof(struct trie_node *) * max_children);
    memset(r->children, 0, sizeof(struct trie_node *) * max_children);
    r->value = NULL;
    return r;
}

bool trie_add(struct trie_node *t, char *key, char *value) {
    if(key == NULL) {
        return false;
    }
    while(*key) {
        if(t->children[*key] == NULL) {
            t->children[*key] = create_trie_node(*key);
        }
        t = t->children[*key++];
    }
    if(t->value != NULL) {
        sdsfree(t->value);
    }
    t->value = sdscatprintf(sdsempty(), "%s", value);
    return true;
}

char *trie_get(struct trie_node *t, char *key) {
    if(key == NULL) {
        return NULL;
    }
    while(*key) {
        if(t->children[*key] == NULL) {
            return NULL;
        }
        t = t->children[*key++];
    }
    return t->value;
}

static struct trie_node *root = NULL;

char *get_setting(char *key) {
    if(root == NULL) {
        return NULL;
    }
    return trie_get(root, key);
}

bool add_setting(char *key, char *value) {
    if(root == NULL) {
        root = create_trie_node(-1);
    }
    return trie_add(root, key, value);
}

bool load_config(FILE *fp) {
    yyin = fp;
    if(yyin == NULL) {
        return false;
    }
    if(yyparse() != 0) {
        return false;
    }
    fclose(yyin);
    return true;
}

static void destroy_config_r(struct trie_node *iter) {
    if(iter == NULL) {
        return;
    }
    size_t max_children = (((uint64_t)1 << (sizeof(char) * CHAR_BIT)) - 1);
    for(size_t f = 0; f < max_children; f++) {
        if(iter->children[f] != NULL) {
            destroy_config_r(iter->children[f]);
        }
    }
    if(iter->value != NULL) {
        sdsfree(iter->value);
    }
    free(iter->children);
    free(iter);
}

void destroy_config() {
    destroy_config_r(root);
}
