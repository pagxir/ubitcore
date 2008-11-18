#ifndef __BTIMERD_H__
#define __BTIMERD_H__
time_t now_time();
time_t comming_time();
int btimerdrun();
void delay_resume(time_t timeout);
void btime_wait(time_t timeout);
#endif
