#ifndef __BTCODEC_H__
#define __BTCODEC_H__
#include <stdint.h>

class bentity
{
    public:
        virtual ~bentity();
        virtual int bget(int64_t *vval);
        virtual bentity &bget(int index);
        virtual bentity &bget(const char *name);
        virtual const char *b_str(size_t *len);
        virtual const char *c_str(size_t *len);
};

class btcodec
{
    public:
        btcodec();
        ~btcodec();
        bentity &bget();
        int bload(const char *buffer, int len);
        const char *b_str(int off, size_t *len);

    private:
        int b_len;
        char *b_text;
        bentity *b_entity;

    private:
        int b_off;
        int getbyte();

    public:
        bentity *bobject(int offset);
        bentity *bobject();
};

#endif
