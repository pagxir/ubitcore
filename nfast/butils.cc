/* $Id:$ */
#include <stdlib.h>
#include <string.h>
#include "butils.h"

static unsigned char __info_hash[20];
static unsigned char __peer_ident[20];

const unsigned char *
get_info_hash()
{
    return __info_hash;
}

const unsigned char *
get_peer_ident()
{
    return __peer_ident;
}

void
set_info_hash(unsigned char hash[20])
{
    memcpy(__info_hash, hash, 20);
}

void
set_peer_ident(unsigned char ident[20])
{
    memcpy(__peer_ident, ident, 20);
}
