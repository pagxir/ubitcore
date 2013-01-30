#ifndef __KUTILS_H__
#define __KUTILS_H__

struct kitem_t{
    char kadid[20];
    in_addr_t host;
    in_port_t port;
    time_t    age;
    time_t    atime;
    int       failed;
};

struct kaddist_t{
    uint8_t kaddist[20];
    kaddist_t();
    kaddist_t(const uint8_t kadid[20]);
    kaddist_t(const char kadid1[20], const char kadid2[20]);
    bool operator>(const kaddist_t &op2)const;
    bool operator<(const kaddist_t &op2)const;
    bool operator==(const kaddist_t &op2)const;
    kaddist_t &operator=(const kaddist_t &op);
};

struct kadid_t{
    char kadid[20];
    kadid_t();
    kadid_t(const char kadid[20]);
    bool operator==(const kadid_t &op)const;
    kaddist_t operator^(const kadid_t &op2)const;
    kadid_t &operator=(const kadid_t &op);
};

const char *idstr(const char id[20]);

#endif
