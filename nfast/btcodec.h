#ifndef __BTCODEC_H__
#define __BTCODEC_H__
class bentity
{
    public:
        virtual ~bentity();
        virtual int bget(int *ival);
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
        bentity *balloc(int off=0);
        int bload(const char *buffer, int len);
        const char *b_str(int off, size_t *len);

    private:
        int b_len;
        char *b_text;
        bentity *b_entity;
};

extern bentity __INFINITE;
#endif
