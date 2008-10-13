#ifndef __BDHTIDENT_H__
#define __BDHTIDENT_H__
class bdhtident
{
    bdhtident(uint8_t ident[20]);
    bool operator==(const bdhtident &ident)const;
    bool operator<(const bdhtident &ident)const;
    bool operator>(const bdhtident &ident)const;
    const bdhtident operator^(const bdhtident &ident)const;
    private:
        uint8_t b_ident[20];
};
#endif
