#ifndef __BPEERMGR_H__
#define __BPEERMGR_H__

class bpeermgr
{
public:
    bpeermgr();
    int bload(const char *buffer, int count);
};

int bload_peer(const char *buffer, size_t count);

#endif
