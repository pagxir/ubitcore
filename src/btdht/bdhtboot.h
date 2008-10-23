#ifndef __BDHTBOOT_H__
#define __BDHTBOOT_H__

class bdhtboot: public bdhtpoller
{
    public:
        bdhtboot(bdhtnet *dhtnet);
        void set_target(uint8_t target[20]);
        void add_dhtnode(uint32_t host, uint16_t port);
        void find_node_next(const void *ibuf, size_t len);
        virtual int bdocall(time_t timeout);

    private:
        int b_count;
        uint32_t b_hosts[8];
        uint16_t b_ports[8];

    private:
        int b_state;
        time_t b_touch;
        bdhtnet *b_dhtnet;
        uint8_t  b_target[20];
        std::queue<bootstraper*> b_tryfinal;
        std::map<bdhtident, bdhtident> b_filter;
        std::map<netpt, bootstraper*> b_trapmap;
        std::map<bdhtident, bootstraper*> b_findmap;
        std::map<bdhtident, bootstraper*> b_bootmap;
};

#endif
