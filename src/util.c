#include <sys/time.h>
#include <time.h>
#include "util.h"
#include "sds/sds.h"

sds when_pretty(uint64_t ts) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t now = (uint64_t )(tv.tv_sec) * 1000 + (uint64_t )(tv.tv_usec) / 1000;
    uint64_t delta = now - ts;
    return format_when(delta);
}

sds format_when(uint64_t delta) {
    uint32_t secs = delta / 1000;
    uint32_t mins = secs / 60;
    uint32_t hours = mins / 60;
    uint32_t days = hours / 24;
    if(days > 0) {
        return sdscatprintf(sdsempty(), "%u day%s ago", days, days > 1 ? "s" : "");
    }
    else if(hours > 0) {
        return sdscatprintf(sdsempty(), "%u hour%s ago", hours, hours > 1 ? "s" : "");
    }
    else if(mins > 0) {
        return sdscatprintf(sdsempty(), "%u minute%s ago", mins, mins > 1 ? "s" : "");
    }
    return sdscatprintf(sdsempty(), "A few moments ago");
}