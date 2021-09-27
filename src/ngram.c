#include "ngram.h"

bool gen_ngrams(char *s, size_t n, bool (*handler)(uint32_t, void *), void *data) {
    uint32_t ngram = 0;
    size_t size = 0;
    while(*s) {
        ngram = ((ngram << 8) & 0xffffff) | *s++;
        size++;
        if(size >= n) {
            if(handler(ngram, data) == false) {
                return false;
            }
        }
    }
    return true;
}
