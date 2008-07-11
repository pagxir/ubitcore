#ifndef __BITFIELD_H__
#define __BITFIELD_H__
#include <vector>

class bitfield
{
    public:
        bitfield();
        bool bitget(int index);
        bool bitset(int index);
        bool bitclr(int index);
        size_t bits_size(){ return b_size; }
        size_t byte_size(){ return (b_size+7)/8; }
        size_t resize(size_t size);
        size_t bitfill(unsigned char *buf, size_t count);

    private:
        size_t b_size;
        std::vector<unsigned char> b_field;
};
#endif
