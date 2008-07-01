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
    unsigned long b_host;
    unsigned short b_port;
    unsigned short b_flag;
};

struct ep_t: public ep1_t
{
    ep_t(ep1_t&ep);
    bsocket b_socket;
};

inline ep_t::ep_t(ep1_t &ep):ep1_t(ep)
{

}

int bready_pop(ep_t **ep);
int bready_push(ep_t *ep);
int bdequeue(ep_t **ep);
int bload_peer(const char *buffer, size_t count);

#endif
