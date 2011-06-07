#ifndef _BTCODEC_H_
#define _BTCODEC_H_

class btstr;
class btint;
class btlist;
class btdict;
class btcodec;
class btentity;
class btoctal_in;
class btoctal_out;

enum {BTT_NONE, BTT_CODEC, BTT_INT, BTT_STR, BTT_LIST, BTT_DICT, BTT_WRAP};

class btfastvisit
{
	public:
		btfastvisit();
		~btfastvisit();
		btfastvisit & operator() (btcodec *codecp);
		void replace(btentity *entity);

	public:
        int bget(int *ival);
		btentity *bget(void);
        btfastvisit &bget(int index);
        btfastvisit &bget(const char *name);
        const char *b_str(size_t *len);
        const char *c_str(size_t *len);

	private:
		int m_type;
		btint *m_intp;
		btstr *m_strp;
		btlist *m_listp;
		btdict *m_dictp;
		btcodec *m_codecp;

	private:
		int set_entity(btentity *entity);
};

class btcodec
{
    public:
        btcodec();
        ~btcodec();

	public:
		btentity *str(const char *str, size_t len);
        btentity *root(void);

	public:
        int load(const char *buf, size_t len);
		int encode(void *buf, size_t count);

	private:
		int parse(btoctal_in &octal);
		int output(btoctal_out &octal, btentity *entity);

	private:
		btentity *m_elink;
		btentity *m_header;
		btentity **m_tailer;
};

#endif

