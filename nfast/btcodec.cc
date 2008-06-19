#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include "btcodec.h"

#define TRY_KILL(en) \
    if (en != &__INFINITE){ \
        delete en; \
        en = &__INFINITE; \
    }

class btlist: public bentity
{
    public:
        btlist(btcodec *codec, int off);
        const char *b_str(size_t *len);
        virtual bentity& bget(int index);
        ~btlist();

    private:
        bentity *b_entity;
        btcodec *b_codec;
        int b_off;
};

btlist::btlist(btcodec *codec, int off):
    b_codec(codec), b_off(off), b_entity(&__INFINITE)
{

}

btlist::~btlist()
{
    TRY_KILL(b_entity);
}

const char *btlist::b_str(size_t *len)
{
    bentity* en = &__INFINITE;
    const char *p = b_codec->b_str(b_off, len);
    if (p != NULL){
        size_t eln;
        int offset = 1;
        assert(p[0] == 'l');
        while (offset<*len && p[offset]!='e'){
            TRY_KILL(en);
            en = b_codec->balloc(b_off+offset);
            if (!en->b_str(&eln)){
                break;
            }
            offset += eln;
        }
        assert(p[offset] == 'e');
        *len = offset+1;
    }
    TRY_KILL(en);
    return p;
}

bentity& btlist::bget(int index)
{
    int i;
    int off = 1;
    size_t len;
    bentity* en = &__INFINITE;
    for (i=0; i<index; i++){
        en = b_codec->balloc(b_off+off);
        if (!en->b_str(&len)){
            TRY_KILL(en);
            return __INFINITE;
        }
        off += len;
    }
    TRY_KILL(b_entity);
    b_entity = b_codec->balloc(off+b_off);
    return *b_entity;
}

class btdict: public bentity
{
    public:
        btdict(btcodec *codec, int off);
        virtual const char *b_str(size_t *len);
        virtual bentity& bget(const char *name);
        ~btdict();

    private:
        bentity *b_entity;
        btcodec *b_codec;
        int b_off;
};

btdict::btdict(btcodec *codec, int off):
    b_codec(codec), b_off(off), b_entity(&__INFINITE)
{

}

btdict::~btdict()
{
    TRY_KILL(b_entity);
}

const char *btdict::b_str(size_t *len)
{
    bentity* en = &__INFINITE;
    const char *p = b_codec->b_str(b_off, len);
    if (p != NULL){
        size_t eln;
        int offset = 1;
        assert(p[0] == 'd');
        while (offset<*len && p[offset]!='e'){
            TRY_KILL(en);
            en = b_codec->balloc(b_off+offset);
            if (!en->b_str(&eln)){
                break;
            }
            offset += eln;
        }
        assert(p[offset] == 'e');
        *len = offset+1;
    }
    TRY_KILL(en);
    return p;
}

bentity& btdict::bget(const char *name)
{
    int i;
    int off = 1;
    size_t len = 0;
    size_t eln = 0;

    bentity* en =  b_codec->balloc(b_off+off);
    while (en->b_str(&eln) != NULL){
        off += eln;
        const char *p = en->c_str(&len);
        if (strlen(name)==len && 0==memcmp(name, p, len)){
            TRY_KILL(en);
            en = b_codec->balloc(b_off+off);
            break;
        }
        TRY_KILL(en);
        en = b_codec->balloc(b_off+off);
        if (en->b_str(&eln) == NULL){
            TRY_KILL(en);
            break;
        }
        off += eln;
        TRY_KILL(en);
        en = b_codec->balloc(b_off+off);
    }
    TRY_KILL(b_entity);
    b_entity = en;
    return *b_entity;
}

class btint: public bentity
{
    public:
        btint(btcodec *codec, int off);
        virtual int bget(int *ival);
        virtual const char *b_str(size_t *len);

    private:
        btcodec *b_codec;
        int b_off;
};

const char *btint::b_str(size_t *len)
{
    int off = 0;
    const char *p = b_codec->b_str(b_off, len);
    if (p != NULL){
        assert(p[off]=='i');
        while(off<*len && p[off]!='e'){
            off++;
        }
        assert(p[off]=='e');
        *len = off+1;
    }
    return p;
}

int btint::bget(int *ival)
{
    size_t len;
    *ival = atoi(b_codec->b_str(b_off+1, &len));
    return 0;
}

btint::btint(btcodec *codec, int off):
    b_codec(codec), b_off(off)
{

}

class btstr: public bentity
{
    public:
        btstr(btcodec *codec, int off);
        virtual const char *b_str(size_t *len);
        virtual const char *c_str(size_t *len);

    private:
        btcodec *b_codec;
        int b_off;
};

btstr::btstr(btcodec *codec, int off):
    b_codec(codec), b_off(off)
{

}

const char *btstr::b_str(size_t *len)
{
    int off = 0;
    const char *p = b_codec->b_str(b_off, len);
    if (p != NULL){
        assert(*len>3);
        while (off<*len && p[off]!=':'){
            off++;
        }
        assert(off<*len && p[off]==':');
        int save = *len;
        *len = atoi(p)+off+1;
        assert(*len <= save);
    }
    return p;
}

const char *btstr::c_str(size_t *len)
{
    int off=0;
    const char *p = b_str(len);

    if (p != NULL){
        while(p[off] != ':'){
            off++;
        }
        p += (off+1);
        *len -= (off+1);
    }
    return p;
}


btcodec::btcodec()
{
    b_len = 0;
    b_text = NULL;
    b_entity = &__INFINITE;
}

btcodec::~btcodec()
{
    if (b_entity != &__INFINITE){
        delete b_entity;
    }
    delete[] b_text;
}

int btcodec::bload(const char *buffer, int len)
{
    if (b_len < len){
        delete[] b_text;
        b_text = new char[len];
    }
    b_len = len;
    assert(b_text != NULL);
    memcpy(b_text, buffer, len);
    return 0;
}

bentity& btcodec::bget()
{
    if (b_entity != &__INFINITE){
        delete b_entity;
        b_entity = &__INFINITE;
    }
    b_entity = balloc(0);
    return *b_entity;
}

bentity* btcodec::balloc(int off)
{
    bentity *entity=&__INFINITE;
    if (off < b_len){
        entity = &__INFINITE;
    }if (b_text[off] == 'l'){
        entity = new btlist(this, off);
    }else if (b_text[off] == 'd'){
        entity = new btdict(this, off);
    }else if (b_text[off] == 'i'){
        entity = new btint(this, off);
    }else if (isdigit(b_text[off])){
        entity = new btstr(this, off);
    }
    return entity;
}

const char *btcodec::b_str(int off, size_t *len)
{
    if (off >= b_len){
        return NULL;
    }
    *len = (b_len-off);
    return b_text+off;
}

int bentity::bget(int *ival)
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
    return NULL;
}

const char *bentity::c_str(size_t *len)
{
    return NULL;
}

bentity __INFINITE;
