#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <netinet/in.h>
#include <string.h>
#include <time.h>

#include "btkad.h"
#include "knode.h"

knode::knode()
{
    b_birthday = time(NULL);
    genkadid(b_ident);
}

knode::knode(const char id[20], in_addr_t addr, in_port_t port)
    :b_address(addr), b_port(port), 
    b_last_seen(0), b_failed(0)
{
    b_birthday = time(NULL);
    memcpy(b_ident, id, 20);
}

int
knode::touch()
{
    b_failed = 0;
    b_last_seen = time(NULL);
}

bool
knode::_isgood()
{
    if (!_isvalidate()){
        return false;
    }
    return (b_last_seen+900<time(NULL));
}


int
knode::set(const kitem_t *in)
{
    if (cmpid(in->kadid) != 0){
        b_birthday = time(NULL);
        memcpy(b_ident, in->kadid, 20);
    }
    b_address = in->host;
    b_port = in->port;
    return 0;
}

int
knode::failed()
{
    b_failed++;
    return 0;
}

int
knode::get(kitem_t *out)
{
    memcpy(out->kadid, b_ident, 20);
    out->port = b_port;
    out->host = b_address;
    out->atime = b_last_seen;
    out->failed = b_failed; 
    return 0;
}

int
knode::cmpid(const char id[20])
{
    return memcmp(id, b_ident, 20);
}

int
knode::cmphost(in_addr_t host)
{
    return host - b_address;
}

int
knode::cmpport(in_port_t port)
{
    return port - b_port;
}
