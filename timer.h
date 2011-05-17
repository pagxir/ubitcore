#ifndef _TIMER_H_
#define _TIMER_H_
struct waitcb;

void callout_invoke(void * upp);
void callout_reset(struct waitcb * waitcbp, size_t millisec);
#endif

