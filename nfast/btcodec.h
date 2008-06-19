#ifndef __BTCODEC_H__
#define __BTCODEC_H__
class bentity
{
    public:
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

    private:
        int b_len;
        char *b_text;
};

extern bentity __INFINITE;
#endif
