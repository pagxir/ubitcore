#ifndef __BTIMERD_H__
#define __BTIMERD_H__
int btimercheck();
time_t now_time();
int benqueue(time_t timeout);
int btime_wait(time_t timeout);
#endif
