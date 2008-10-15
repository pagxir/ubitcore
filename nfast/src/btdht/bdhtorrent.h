/* $Id: bdhtnet.cc 94 2008-07-19 04:12:35Z pagxir $ */
#ifndef __BDHTORRENT_H__
#define __BDHTORRENT_H__

class bdhtnet;

class bdhtorrent: public bdhtpoller
{
    public:
        bdhtorrent(bdhtnet *dhtnet);
        void set_infohash(uint8_t infohash[20]);
        void add_node(uint32_t host, uint16_t port);
        void get_peers(const void *ibuf, size_t len);
        virtual int bdocall(time_t timeout);

    private:
        void get_peers_next(const void *ibuf, size_t len);

    private:
        int b_index;
        int b_state;
        time_t b_touch;
        bdhtnet *b_dhtnet;
        uint8_t  b_infohash[20];
        std::queue<bootstraper*> b_tryagain;
        std::queue<bootstraper*> b_tryfinal;
        std::map<netpt,bootstraper*> b_trapmap;
        std::map<bdhtident,bootstraper*> b_findmap;
        std::map<bdhtident,bootstraper*> b_bootmap;
};

#endif
