#ifndef __BDHTBOOT_H__
#define __BDHTBOOT_H__

class bdhtboot: public bdhtpoller
{
    public:
        bdhtboot(bdhtnet *dhtnet);
        void set_target(uint8_t target[20]);
        void add_node(uint32_t host, uint16_t port);
        void find_next(const void *ibuf, size_t len);
        virtual int bdocall(time_t timeout);

    private:
        int b_index;
        int b_state;
        time_t b_touch;
        bdhtnet *b_dhtnet;
        uint8_t  b_findtarget[20];
        std::queue<bootstraper*> b_tryagain;
        std::queue<bootstraper*> b_tryfinal;
        std::map<netpt, bootstraper*> b_trapmap;
        std::map<bdhtident, bootstraper*> b_findmap;
        std::map<bdhtident, bootstraper*> b_bootmap;
};

#endif
