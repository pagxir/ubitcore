#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <netinet/in.h>
#include <string.h>
#include <time.h>

#include "btkad.h"
#include "knode.h"

knode::knode()
    :b_destroy(true)
{
    genkadid(b_ident);
}

knode::knode(const char id[20], in_addr_t addr, in_port_t port)
    :b_address(addr), b_port(port), b_destroy(false),
    b_valid(false), b_last_seen(0), b_failed(0)
{
    memcpy(b_ident, id, 20);
}

void
knode::touch()
{
    b_last_seen = time(NULL);
}

int
knode::set(const kitem_t *in)
{
    assert(b_valid == false);
    b_valid = true;
    memcpy(b_ident, in->kadid, 20);
    b_address = in->host;
    b_port = in->port;
    touch();
    return 0;
}

int
knode::replace(const kitem_t *in, kitem_t *out)
{
    if (cmpid(in->kadid)==0){
        if (cmphost(in->host)!=0
                || cmpport(in->port)!=0){
            //printf("undef\n");
        }
        touch();
        return 0;
    }
    getnode(out);
    return b_last_seen;
}

int
knode::failed_contact(const kitem_t *in)
{
    if (cmpid(in->kadid)){
        return 0;
    }
    b_failed++;
    if (b_failed > 3){
        b_valid = false;
    }
    return 0;
}

int
knode::getnode(kitem_t *out)
{
    memcpy(out->kadid, b_ident, 20);
    out->port = b_port;
    out->host = b_address;
    out->atime = b_last_seen;
    out->failed = b_valid; 
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

int
knode::XOR(char target[20])
{
    int i;
    for (i=0; i<20; i++){
        target[i] ^= b_ident[i];
    }
    return 0;
}
