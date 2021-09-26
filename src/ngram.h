//
// Created by Jack Matheson on 9/23/21.
//

#ifndef HISTX_NGRAM_H
#define HISTX_NGRAM_H

#include <stdbool.h>
#include <stdlib.h>

bool gen_ngrams(char *s, size_t n, bool (*handler)(uint32_t ngram, void *), void *data);

#endif //HISTX_NGRAM_H
