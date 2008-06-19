#ifndef __BTCODEC_H__
#define __BTCODEC_H__
class bentity
{
    public:
        virtual int b_len();
        virtual const char *b_str();
        virtual int c_len();
        virtual const char *c_str();
        virtual bentity &bget(int index);
        virtual bentity &bget(const char *name);
};

class btcodec
{
    public:
		btcodec();
		bentity &bget();
        int bload(char *buffer, int len);

	private:
		int b_len;
		char *b_text;
};

extern bentity __INFINITE;
#endif
