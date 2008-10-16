/* $Id: bdhtnet.cc 94 2008-07-19 04:12:35Z pagxir $ */
#ifndef __BDHTNET_H__
#define __BDHTNET_H__
#include <map>

class bdhtnet;
class bdhtransfer;

class bdhtcodec
{
    public:
        int bload(const char *buffer, size_t length);
        int type() { return b_type; }
        int transid() { return b_transid; }

    private:
        int b_type;
        int b_transid;
        std::string b_ident;
        btcodec b_codec;
};

class bdhtpoller: public bthread
{
    public:
        bool polling() { return b_polling; }

    protected:
        bool b_polling;
};

class bdhtnet: public bthread
{
    public:
        bdhtnet();
        virtual int bdocall(time_t timeout);

    public:
        bdhtransfer *get_transfer();
        int detach(uint32_t id);
        int ping_node(uint32_t tid, uint32_t host, uint16_t port);
        int find_node(uint32_t tid, uint32_t host,
                uint16_t port, uint8_t ident[20]);
        int get_peers(uint32_t tid, uint32_t host,
                uint16_t port, uint8_t ident[20]);

    private:
        void binput(bdhtcodec *codec, const void *ibuf, size_t len,
                uint32_t host, uint16_t port);
        std::map<int, bdhtransfer*> b_requests;
        bsocket b_socket;
        time_t b_last;
        uint32_t b_tid;
};

struct bootstraper
{
    uint16_t b_flag;
    uint16_t b_port;
    uint32_t b_host;
    bdhtransfer *b_transfer;
};

int bdhtnet_node(const char *host, int port);
int bdhtnet_start();
#endif
