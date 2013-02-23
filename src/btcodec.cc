/* $Id$ */
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include <cmath>
#include <string>
#include "btcodec.h"

#define TRY_KILL(en) \
    if (en != &__INFINITE){  \
        if (en != &__FINISH) \
            delete en;       \
        en = &__INFINITE;    \
    }

bentity __INFINITE, __FINISH;

class btlist: public bentity
{
    public:
        btlist(btcodec *codec, int off, int len);
        virtual bentity& bget(int index);
        ~btlist();

    private:
        bentity *b_entity;
        btcodec *b_codec;
        int b_off;
        int b_len;
};

btlist::btlist(btcodec *codec, int off, int len)
    :b_codec(codec), b_off(off), b_len(len)
{
    b_entity = &__INFINITE;
}

btlist::~btlist()
{
    TRY_KILL(b_entity);
}

bentity& btlist::bget(int index)
{
    TRY_KILL(b_entity);
    bentity *obj = NULL;
    for (obj=b_codec->bobject(b_off+1);
            obj!=&__INFINITE; obj=b_codec->bobject()){
        if (obj == &__FINISH){
            break;
        }
        if (index == 0){
            b_entity = obj;
            break;
        }
        TRY_KILL(obj);
        index--;
    }
    return *b_entity;
}

class btdict: public bentity
{
    public:
        btdict(btcodec *codec, int off, int len);
        virtual bentity& bget(const char *name);
        virtual const char *b_str(size_t *len);
        ~btdict();

    private:
        bentity *b_entity;
        btcodec *b_codec;
        int b_off;
        int b_len;
};

btdict::btdict(btcodec *codec, int off, int len)
    :b_codec(codec), b_off(off), b_len(len)
{
    b_entity = &__INFINITE;
}

btdict::~btdict()
{
    TRY_KILL(b_entity);
}

const char *btdict::b_str(size_t *len)
{
    size_t local = 0;
    const char *retval
        = b_codec->b_str(b_off, &local);
    if (b_len > local)
        return NULL;
    *len = b_len;
    return retval;
}

bentity& btdict::bget(const char *name)
{
    TRY_KILL(b_entity);
    bentity *obj = NULL;
    size_t len = 0;
    size_t namelen = strlen(name);
    for (obj=b_codec->bobject(b_off+1);
            obj!=&__INFINITE; obj=b_codec->bobject()){
        if (obj == &__FINISH){
            break;
        }
        bool keyFound = false;
        const char *title = obj->c_str(&len);
        if (namelen==len && !memcmp(name, title, len))
            keyFound = true;
        TRY_KILL(obj);
        obj = b_codec->bobject();
        if (obj == &__FINISH){
            break;
        }
        if (keyFound == true){
            b_entity = obj;
            break;
        }
        TRY_KILL(obj);
    }
    return *b_entity;
}

class btint: public bentity
{
    public:
        btint(int64_t lval);
        virtual int bget(int64_t *ival);

    private:
        int64_t b_value;
};

int btint::bget(int64_t *ival)
{
    *ival = b_value;
    return 0;
}

btint::btint(int64_t lval)
    :b_value(lval)
{
}

class btstr: public bentity
{
    public:
        btstr(const char *text, size_t len);
        virtual const char *c_str(size_t *len);

    private:
        size_t b_len;
        const char *b_text;
};

btstr::btstr(const char *text, size_t len)
    :b_text(text),  b_len(len)
{

}

const char *btstr::c_str(size_t *len)
{
    *len = b_len;
    return b_text;
}


btcodec::btcodec()
{
    b_len = 0;
    b_text = NULL;
    b_entity = &__INFINITE;
}

btcodec::~btcodec()
{
    TRY_KILL(b_entity);
    delete[] b_text;
}

int btcodec::bload(const char *buffer, int len)
{
    assert(b_text == NULL);
    b_len  = len;
    b_text = new char[len];
    assert(b_text != NULL);
    memcpy(b_text, buffer, len);
    return 0;
}

bentity& btcodec::bget()
{
    TRY_KILL(b_entity);
    b_entity = bobject(0);
    return *b_entity;
}

const char *btcodec::b_str(int off, size_t *len)
{
    if (off >= b_len){
        return NULL;
    }
    *len = (b_len-off);
    return b_text+off;
}

int bentity::bget(int64_t *ival)
{
    return -1;
}

bentity& bentity::bget(int index)
{
    return __INFINITE;
}

bentity& bentity::bget(const char *name)
{
    return __INFINITE;
}

bentity::~bentity()
{
}

const char *bentity::b_str(size_t *len)
{
    printf("bad call!\n");
    return NULL;
}

const char *bentity::c_str(size_t *len)
{
    return NULL;
}

bentity *btcodec::bobject(int offset)
{
    b_off = offset;
    return bobject();
}

int btcodec::getbyte()
{
    int retval = '#';
    if (b_off < b_len)
        retval = b_text[b_off++];
    return retval;
}

bentity * btcodec::bobject()
{
    int soff = b_off;
    int64_t valOfInt = 0;
    int type = getbyte();
    bentity * obj, * retval = &__INFINITE;
    switch(type){
        case 'd':
            for (obj=bobject(); obj!=&__INFINITE; obj=bobject()){
                if (obj == &__FINISH){
                    retval = new btdict(this, soff, b_off-soff);
                    break;
                }
                TRY_KILL(obj);
            }
            break;
        case 'l':
            for (obj=bobject(); obj!=&__INFINITE; obj=bobject()){
                if (obj == &__FINISH){
                    retval = new btlist(this, soff, b_off-soff);
                    break;
                }
                TRY_KILL(obj);
            }
            break;
        case 'i':
            valOfInt = 0;
            type=getbyte();
            if (type<'0' || type>'9')
                break;
            for (; type>='0'&&type<='9'; type=getbyte())
                valOfInt = valOfInt*10 + (type-'0');
            if (type=='e')
                retval = new btint(valOfInt);
            break;
        case 'e':
            retval = &__FINISH;
            break;
        default:
            valOfInt = 0;
            if (type<'0' || type>'9')
                break;
            for (; type>='0'&&type<='9'; type=getbyte())
                valOfInt = valOfInt*10 + (type-'0');
            if (type==':' && b_off+valOfInt<+b_len){
                retval = new btstr(&b_text[b_off], valOfInt);
                b_off += valOfInt;
            }
            break;
    }
    return retval;
}
