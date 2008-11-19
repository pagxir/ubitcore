#ifndef __DHTLOADER_H__
#define __DHTLOADER_H__
int dht_fd();
int dht_poll(bool nonblock);
int dht_load(int argc, char *argv[]);
#endif
