#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>
#include <sys/time.h>
#include "sds/sds.h"
#include "index.h"
#include "ngram.h"
#include "base64/base64.h"

#if defined(__APPLE__)
    #define COMMON_DIGEST_FOR_OPENSSL
    #include <CommonCrypto/CommonDigest.h>
#else
    #include <openssl/sha.h>
#endif

#define INSERT_HASH_HDR     "insert or replace into cmdraw(hash, ts, cmd, cwd) values("
#define INSERT_HASH_FTR     ");"

#define INSERT_LUT_HDR      "insert or ignore into cmdlut(host, ngram, hash) values("
#define INSERT_LUT_FTR      ");"

static bool insert_hash(sqlite3 *db, char *hash, char *cmd, char *cwd) {
    char *err;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t ts = (uint64_t )(tv.tv_sec) * 1000 + (uint64_t )(tv.tv_usec) / 1000;
    sds c = sdscatprintf(sdsempty(), "%s \"%s\",%" PRIu64 ",\"%s\", \"%s\" %s",
                INSERT_HASH_HDR,
                hash, ts, cmd, cwd,
                INSERT_HASH_FTR
    );
    int r = sqlite3_exec(db, c, NULL, NULL, &err);
    if(r != SQLITE_OK) {
        fprintf(stderr, "Unable to write to database: %s\n", err);
        fprintf(stderr, "query[%s]\n", c);
        return false;
    }
    sdsfree(c);
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
    sdsfree(c);
    return true;
}

sds create_hash(char *src, size_t len) {
    uint8_t digest[SHA256_DIGEST_LENGTH];
    sds r = sdsempty();

#if defined(__APPLE__)
    SHA256_CTX c;
    SHA256_Init(&c);
    SHA256_Update(&c, src, len);
    SHA256_Final(digest, &c);
#else
    SHA256(src, len, digest);
#endif

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

bool index_cmd(sqlite3 *db, char *cmd, char *cwd) {
    size_t b64len;
    size_t len = strlen(cmd);
    sds hash = create_hash(cmd, len);
    char *b64 = (char *)base64_encode((unsigned char *)cmd, len, &b64len);
    char *cwdb64 = (char *)base64_encode((unsigned char *)cwd, strlen(cwd), &b64len);
    bool r = insert_hash(db, hash, b64, cwdb64);
    free(b64);
    if(r == false) {
        return r;
    }

    struct ngram_context context = { .db = db, .hash = hash };
    gen_ngrams(cmd, 3, ngram_handler_lut, &context);
    sdsfree(hash);
    return true;
}
