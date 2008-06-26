/* $Id$ */
#include <time.h>
#include <string.h>

#include "bclock.h"

bclock::bclock(const char *text, int second):
    ident_text(text)
{
    b_ident = "bclock";
    b_second = second;
    last_time = time(NULL);
}

bclock::~bclock()
{
}

int
bclock::bdocall(time_t timeout)
{
    time_t now;
    time(&now);
#if 0
    fprintf(stderr, "\rbcall(%s): %s", ident_text, ctime(&now));
#endif
    while(-1 != btime_wait(last_time+b_second)){
        last_time = time(NULL);
    }
    return -1;
}

