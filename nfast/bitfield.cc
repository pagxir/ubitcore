#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "bitfield.h"

bitfield::bitfield()
    :b_size(0)
{
}

size_t bitfield::resize(size_t size)
{
    size_t osize = b_size;
    b_size = size;
    b_field.resize(byte_size());
    return osize;
}

static unsigned char 
__use_bit(int idx)
{
    return 1<<(0x7^(idx&0x7));
}

bool bitfield::bitget(int idx)
{
    assert(idx<b_size);
    bool bval = b_field[idx>>3]&__use_bit(idx);
    return bval;
}

bool bitfield::bitset(int idx)
{
    assert(idx<b_size);
    unsigned char ubyte = b_field[idx>>3];
    ubyte |= __use_bit(idx);
    b_field[idx>>3] = ubyte;
    return true;
}

bool bitfield::bitclr(int idx)
{
    assert(idx<b_size);
    unsigned char ubyte = b_field[idx>>3];
    ubyte &= ~__use_bit(idx);
    b_field[idx>>3] = ubyte;
    return true;
}

size_t bitfield::bitfill(unsigned char *ubytes, size_t count)
{
    assert(count==byte_size());
    memcpy(&b_field[0], ubytes, count);
    return b_size;
}
