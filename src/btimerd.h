#ifndef __BTIMERD_H__
#define __BTIMERD_H__
time_t now_time();
time_t comming_time();
int btimerdrun();
int benqueue(void *ident, time_t timeout);
int btime_wait(time_t timeout);
#endif
