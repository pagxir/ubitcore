#ifndef __BDHTRANSFER_H__
#define __BDHTRANSFER_H__

struct bdhtpack
{
    void *b_ibuf;
    size_t b_len;
    uint32_t b_host;
    uint16_t b_port;
    uint16_t b_align;
};

class bdhtransfer{
    public:
        bdhtransfer(bdhtnet *bdhtnet, uint32_t id);
        int ping_node(uint32_t host, uint16_t port);
        int find_node(uint32_t host, uint16_t port, uint8_t ident[20]);
        void binput(bdhtcodec *codec, const void *buf, size_t len,
                uint32_t host, uint16_t port);
        int get_response(void *buf, size_t size,
                uint32_t *phost, uint16_t *pport);
        int bdopolling(bdhtpoller *poller);

    private:
        uint32_t b_ident;
        bdhtnet *b_dhtnet;
        std::queue<bdhtpack*> b_queue;
        bdhtpoller *b_poller;
};
#endif
