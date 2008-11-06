/* $Id$ */
#include <time.h>
#include <string.h>

#include "bclock.h"
#include "btimerd.h"

bclock::bclock(const char *text, int second):
    ident_text(text)
{
    b_ident = "bclock";
    b_second = second;
    last_time = now_time();
}

int
bclock::bdocall()
{
    time_t now;
    time(&now);
#if 1
    printf("bcall(%s): %s\n", ident_text.c_str(), ctime(&now));
#endif
    while (b_runable){
        if(0 == btime_wait(last_time+b_second)){
            last_time = now_time(); 
        }
    }
    return -1;
}
