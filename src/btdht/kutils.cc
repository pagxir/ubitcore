#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <netinet/in.h>

#include "btkad.h"
#include "kutils.h"

kaddist_t::kaddist_t()
{
    memset(kaddist, 0, 20);
}

kaddist_t::kaddist_t(const uint8_t kadid[20])
{
    memcpy(kaddist, kadid, 20);
}

kaddist_t::kaddist_t(const char kadid1[20], const char kadid2[20])
{
    int i;
    for (i=0; i<20; i++){
        kaddist[i] = kadid1[i]^kadid2[i];
    }
}

bool kaddist_t::operator<(const kaddist_t &op2)const
{
    return memcmp(kaddist, op2.kaddist, 20)<0;
}

bool kaddist_t::operator>(const kaddist_t &op2)const
{
    return memcmp(kaddist, op2.kaddist, 20)>0;
}

bool kaddist_t::operator==(const kaddist_t &op2)const
{
    return memcmp(kaddist, op2.kaddist, 20)==0;
}

kaddist_t &kaddist_t::operator=(const kaddist_t &op)
{
    memcpy(kaddist, op.kaddist, 20);
    return *this;
}

kadid_t::kadid_t()
{
    memset(kadid, 0, 20);
}

kadid_t::kadid_t(const char _kadid[20])
{
    memcpy(kadid, _kadid, 20);
}

bool kadid_t::operator==(const kadid_t &op)const
{
    return memcmp(kadid, op.kadid, 20)==0;
}

kaddist_t kadid_t::operator^(const kadid_t &op2)const
{
    return kaddist_t(kadid, op2.kadid);
}

kadid_t & kadid_t::operator=(const kadid_t &op)
{
    memcpy(kadid, op.kadid, 20);
    return *this;
}
