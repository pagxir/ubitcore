/* $Id$ */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "butils.h"

static unsigned char __info_hash[20];
static unsigned char __peer_ident[20];

const unsigned char *
get_info_hash()
{
    return __info_hash;
}

int
getclientid(char clientid[20])
{
    memcpy(clientid, __peer_ident, 20);
    return 0;
}

int
set_info_hash(unsigned char hash[20])
{
    memcpy(__info_hash, hash, 20);
    return 0;
}

int
setclientid(char ident[20])
{
    memcpy(__peer_ident, ident, 20);
    return 0;
}
