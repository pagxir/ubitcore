#ifndef __BPEERMGR_H__
#define __BPEERMGR_H__
#include <memory>
#include "bsocket.h"

class bpeermgr
{
public:
    bpeermgr();
    int bload(const char *buffer, int count);
};

struct ep1_t
{
    uint32_t b_host;
    unsigned short b_port;
    unsigned short b_flag;
};

struct ep_t: public ep1_t
{
    ep_t();
    ep_t(ep1_t&ep);
    bsocket b_socket;
    int bclear();
};

inline ep_t::ep_t()
{
}

inline ep_t::ep_t(ep1_t &ep):ep1_t(ep)
{
}

inline int
ep_t::bclear()
{
    bsocket *so = new bsocket();
    *so = b_socket;
    delete so;
    return 0;
}

int bready_pop(ep_t *ep);
int bready_push(ep_t *ep);
int bdequeue(ep_t **ep);
int bload_peer(const char *buffer, size_t count);

#endif
