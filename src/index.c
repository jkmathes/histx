//
// Created by Jack Matheson on 9/19/21.
//
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "sds/sds.h"
#include "index.h"
#include "ngram.h"

#if defined(__APPLE__)
    #define COMMON_DIGEST_FOR_OPENSSL
    #include <CommonCrypto/CommonDigest.h>
#else
    #include <openssl/md5.h>
#endif

#define INSERT_HASH_HDR     "insert or ignore into cmdraw(hash, cmd) values("
#define INSERT_HASH_FTR     ");"

#define INSERT_LUT_HDR      "insert or ignore into cmdlut(host, ngram, hash) values("
#define INSERT_LUT_FTR      ");"

static bool insert_hash(sqlite3 *db, char *hash, char *cmd) {
    char *err;
    sds c = sdscatprintf(sdsempty(), "%s \"%s\", \"%s\" %s",
                INSERT_HASH_HDR,
                hash, cmd,
                INSERT_HASH_FTR
    );
    int r = sqlite3_exec(db, c, NULL, NULL, &err);
    if(r != SQLITE_OK) {
        fprintf(stderr, "Unable to write to database: %s\n", err);
        fprintf(stderr, "query[%s]\n", c);
        return false;
    }
    return true;
}

static bool insert_lut(sqlite3 *db, uint32_t ngram, char *hash) {
    char *err;
    sds c = sdscatprintf(sdsempty(), "%s \"%s\", %u, \"%s\" %s",
                INSERT_LUT_HDR,
                "", ngram, hash,
                INSERT_LUT_FTR
    );
    int r = sqlite3_exec(db, c, NULL, NULL, &err);
    if(r != SQLITE_OK) {
        fprintf(stderr, "Unable to write to database: %s\n", err);
        fprintf(stderr, "query[%s]\n", c);
        return false;
    }
    return true;
}

char *create_hash(char *src) {
    uint8_t digest[SHA256_DIGEST_LENGTH];
    sds r = sdsempty();
    size_t len = strlen(src);
    SHA256_CTX c;
    SHA256_Init(&c);
    SHA256_Update(&c, src, len);
    SHA256_Final(digest, &c);
    for(size_t f = 0; f < SHA256_DIGEST_LENGTH; f++) {
        r = sdscatprintf(r, "%02x", digest[f]);
    }
    return r;
}

struct ngram_context {
    sqlite3 *db;
    char *hash;
};

bool ngram_handler_lut(uint32_t ngram, void *data) {
    struct ngram_context *context = (struct ngram_context *)data;
    return insert_lut(context->db, ngram, context->hash);
}

bool index_cmd(sqlite3 *db, char *cmd) {
    uint32_t ngram = 0;
    char *c = cmd;
    int size = 0;
    char *hash = create_hash(cmd);
    bool r = insert_hash(db, hash, cmd);
    if(r == false) {
        return r;
    }

    struct ngram_context context = { .db = db, .hash = hash };
    gen_ngrams(cmd, 3, ngram_handler_lut, &context);
    while(*c) {
        ngram = ((ngram << 8) & 0xffffff) | *c++;
        size++;
        if(size >= 3) {
            r = insert_lut(db, ngram, hash);
            if(r == false) {
                return r;
            }
        }
    }
    return true;
}
