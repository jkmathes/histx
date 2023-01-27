#include "ngram.h"

bool gen_ngrams(char *s, size_t n, bool (*handler)(uint32_t, void *), void *data) {
    uint32_t ngram = 0;
    size_t size = 0;
    uint32_t masks[] = {0x0, 0xff, 0xffff, 0xffffff, 0xffffffff};

    if(n < 1 || n > 3) {
        return false;
    }
    uint32_t mask = masks[n];
    while(*s) {
        ngram = ((ngram << 8) & mask) | *s++;
        size++;
        if(size >= n) {
            if(handler(ngram, data) == false) {
                return false;
            }
        }
    }
    return true;
}
