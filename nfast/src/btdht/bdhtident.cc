#include <string.h>
#include <stdint.h>

#include "bdhtident.h"

netpt::netpt(uint32_t host, uint16_t port)
{
    b_host = host;
    b_port = port;
}

bool
netpt::operator==(const netpt &pt)const
{
    if (b_host != pt.b_host){
        return false;
    }
    return b_port == pt.b_port;
}

bool
netpt::operator<(const netpt &pt)const
{
    if (b_host != pt.b_host){
        return b_host < pt.b_host;
    }
    return b_port < pt.b_port;
}

bdhtident::bdhtident(uint8_t ident[20])
{
    memcpy(b_ident, ident, 20);
} 

bool
bdhtident::operator==(const bdhtident &ident)const
{
    int i;
    for (i=0; i<20; i++){
        if (b_ident[i]!=ident.b_ident[i]){
            return false;
        }
    }
    return true;
}

bool
bdhtident::operator<(const bdhtident &ident)const
{
    int i;
    for (i=0; i<20; i++){
        if (b_ident[i]<ident.b_ident[i]){
            return true;
        }
    }
    return false;
}

bool
bdhtident::operator>(const bdhtident &ident)const
{
    int i;
    for (i=0; i<20; i++){
        if (b_ident[i]>ident.b_ident[i]){
            return true;
        }
    }
    return false;
}

const bdhtident
bdhtident::operator^(const bdhtident &ident)const
{
    int i;
    uint8_t ibuf[20];

    for (i=0; i<20; i++){
        ibuf[i] = ident.b_ident[i]^b_ident[i];
    }

    return bdhtident(ibuf);
}
