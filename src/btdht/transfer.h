#ifndef __BDHTRANSFER_H__
#define __BDHTRANSFER_H__
#include <queue>

class bthread;
class bdhtnet;
class bdhtcodec;

struct bdhtpack
{
    void *b_ibuf;
    size_t b_len;
    uint32_t b_host;
    uint16_t b_port;
    uint16_t b_align;
};

class kship{
    public:
        kship(bdhtnet *bdhtnet, uint32_t id);
        ~kship();
        int ping_node(uint32_t host, uint16_t port);
        int find_node(uint32_t host, uint16_t port, uint8_t ident[20]);
        int get_peers(uint32_t host, uint16_t port, uint8_t ident[20]);
        void binput(bdhtcodec *codec, const void *buf, size_t len,
                uint32_t host, uint16_t port);
        int get_response(void *buf, size_t size,
                uint32_t *phost, uint16_t *pport);

    private:
        uint32_t b_ident;
        bdhtnet *b_dhtnet;
        std::queue<bdhtpack*> b_queue;
        bthread *b_thread;
};
#endif
