/* $Id: bdhtnet.cc 94 2008-07-19 04:12:35Z pagxir $ */
#ifndef __BDHTNET_H__
#define __BDHTNET_H__

#include <map>

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

    private:
        bool b_polling;
};

class bdhtransfer{
    public:
        bdhtransfer(void *bdhtnet, uint32_t id);
        void binput(bdhtcodec *codec, uint32_t host, uint16_t port);
        int bdopolling(bdhtpoller *poller);

    private:
        std::queue<int> b_queue;
        bdhtpoller *b_poller;
};

class bdhtnet: public bthread
{
    public:
        bdhtnet();
        virtual int bdocall(time_t timeout);

    public:
        bdhtransfer *get_transfer();
        int ping_node(uint32_t tid, uint32_t host, uint16_t port);

    private:
        void binput(bdhtcodec *codec, uint32_t host, uint16_t port);
        std::map<int, bdhtransfer*> b_requests;
        bsocket b_socket;
        time_t b_last;
        uint32_t b_tid;
};

class bdhtbucket: public bdhtpoller
{
    public:
        void add_node(uint32_t host, uint16_t port);
        virtual int bdocall(time_t timeout);

    private:
        int b_count;
        int b_index;
        uint16_t b_port_list[8];
        uint32_t b_host_list[8];
        bdhtransfer *b_transfer;
};
#endif
