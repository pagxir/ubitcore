#ifndef __BPEERMGR_H__
#define __BPEERMGR_H__

class bpeermgr
{
public:
    bpeermgr();
    int bload(const char *buffer, int count);
};

struct ep_t
{
    unsigned long b_host;
    unsigned short b_port;
    unsigned short b_flag;
};

int bdequeue(ep_t *ep);
int bload_peer(const char *buffer, size_t count);

#endif
