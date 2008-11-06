#ifndef __BTIMERD_H__
#define __BTIMERD_H__
int btimercheck();
time_t now_time();
time_t comming_timer();
int benqueue(time_t timeout);
int btime_wait(time_t timeout);
#endif
