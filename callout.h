#ifndef _CALLOUT_H_
#define _CALLOUT_H_
struct waitcb;

void callout_invoke(void *upp);
void callout_reset(struct waitcb *waitcbp, size_t millisec);
#endif

