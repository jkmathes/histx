#ifndef HISTX_UTIL_H
#define HISTX_UTIL_H

#include <stdint.h>
#include "sds/sds.h"

sds format_when(uint64_t delta);
sds when_pretty(uint64_t ts);

#endif //HISTX_UTIL_H
