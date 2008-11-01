#include <string.h>
#include <stdint.h>

#include "ident.h"

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
    return 0==memcmp(b_ident, ident.b_ident, 20);;
}

bool
bdhtident::operator<(const bdhtident &ident)const
{
    return memcmp(b_ident, ident.b_ident, 20)<0;
}

bool
bdhtident::operator>(const bdhtident &ident)const
{
    return memcmp(b_ident, ident.b_ident, 20)>0;
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

static uint8_t mask[256] = {
    8,7,6,6,5,5,5,5,4,4,4,4,4,4,4,4,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

size_t
bdhtident::lg()
{
    int i;
    uint8_t flag = 0xFF;

    for (i=0; i<20; i++){
        if (b_ident[i] != 0){
            flag = b_ident[i];
            break;
        }
    }

    return (i<<3)+mask[flag];
}
