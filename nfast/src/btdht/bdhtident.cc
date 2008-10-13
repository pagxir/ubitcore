#include <string.h>
#include <stdint.h>

#include "bdhtident.h"


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
