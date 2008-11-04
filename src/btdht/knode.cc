#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <time.h>

#include "btkad.h"
#include "knode.h"

knode::knode()
    :b_destroy(true)
{
}

knode::knode(char id[20], in_addr_t addr, in_port_t port)
    :b_address(addr), b_port(port), b_destroy(false),
    b_valid(false), b_last_seen(0)
{
    memcpy(b_ident, id, 20);
}

void
knode::touch()
{
    b_last_seen = time(NULL);
}
