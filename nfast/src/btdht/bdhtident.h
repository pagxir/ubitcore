#ifndef __BDHTIDENT_H__
#define __BDHTIDENT_H__

class netpt
{
    public:
        netpt(uint32_t host, uint16_t port);
        bool operator==(const netpt &ident)const;
        bool operator<(const netpt &ident)const;

    private:
        uint32_t b_host;
        uint16_t b_port;

};

class bdhtident
{
    public:
        bdhtident(uint8_t ident[20]);
        bool operator==(const bdhtident &ident)const;
        bool operator<(const bdhtident &ident)const;
        bool operator>(const bdhtident &ident)const;
        const bdhtident operator^(const bdhtident &ident)const;
        size_t lg();

    private:
        uint8_t b_ident[20];
};
#endif
