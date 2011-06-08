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

class btoctal_in
{
	public:
		btoctal_in(const char *octalp, size_t count);
		~btoctal_in();

	public:
		int get_one();
		int get_cur();

	private:
		size_t m_count;
		unsigned char *m_octalp;
};

btoctal_in::btoctal_in(const char *octalp, size_t count)
{
	m_count = count;
	m_octalp = (unsigned char *)octalp;
}

btoctal_in::~btoctal_in()
{
}

int btoctal_in::get_cur()
{
	if (m_count <= 0) {
		return -1;
	}

	return *m_octalp;
}

int btoctal_in::get_one()
{
	if (m_count <= 0) {
		return -1;
	}

	m_count--;
	return *m_octalp++;
}

class btoctal_out {
	public:
		btoctal_out(char *octalp, size_t count);
		~btoctal_out();

	public:
		int put_one(int one);
		int put_octal(const char *buf, size_t len);

	public:
		size_t size(void);

	private:
		size_t m_len;
		size_t m_count;
		char *m_octalp;
};

btoctal_out::btoctal_out(char *octalp, size_t count)
{
	m_octalp = octalp;
	m_count = count;
	m_len = 0;
}

btoctal_out::~btoctal_out()
{
}

int btoctal_out::put_one(int one)
{
	if (m_count > 0) {
	   	*m_octalp++ = one;
		m_count--;
		m_len++;
	}

	return 0;
}

int btoctal_out::put_octal(const char *buf, size_t count)
{
	if (m_count > count) {
		memcpy(m_octalp, buf, count);
		m_octalp += count;
		m_count -= count;
		m_len += count;
	}

	return 0;
}

size_t btoctal_out::size(void)
{
	return m_len;
}

class btentity
{
	public:
		btentity(btentity **entity);
		virtual ~btentity();

	private:
		btentity *m_next;

	public:
		int m_type;
		btentity *m_sibling;
		btentity *m_children;
};

btentity::btentity(btentity **head)
{
	m_type = BTT_NONE;
	m_next = *head;
	*head = this;
	m_sibling = 0;
	m_children = 0;
}

btentity::~btentity()
{
	btentity *next;

	next = m_next;
	m_next = NULL;

	if (next != NULL)
		delete next;
}

btcodec::btcodec()
{
	m_elink = 0;
	m_header = 0;
	m_tailer = &m_header;
}

class btdict: public btentity
{
	public:
		btdict(btentity **head);
};

btdict::btdict(btentity **head)
:btentity(head)
{
	m_type = BTT_DICT;
}

class btlist: public btentity
{
	public:
		btlist(btentity **head);
};

btlist::btlist(btentity **head)
:btentity(head)
{
	m_type = BTT_LIST;
}

class btint: public btentity
{
	public:
		btint(btentity **head, int val);

	public:
		int get(int *ivalp);

	private:
		int m_val;
};

btint::btint(btentity **head, int val)
:btentity(head), m_val(val)
{
	m_type = BTT_INT;
}

int btint::get(int *ivalp)
{
	*ivalp = m_val;
	return 0;
}

class btstr: public btentity
{
	public:
		btstr(btentity **head, btoctal_in &octal, size_t size);
		btstr(btentity **head, const char *str, size_t len);
		~btstr();

	public:
		int match(const char *key);
		const char *get(size_t *elenp);

	private:
		char *m_buf;
		size_t m_len;
};

btstr::btstr(btentity **head, const char *str, size_t len)
:btentity(head)
{
	char *bufp = new char[len + 1];
	assert(bufp != NULL);

	m_len = len;
	m_buf = bufp;
	m_type = BTT_STR;
	memcpy(bufp, str, len);
	bufp[len] = 0;
}

btstr::btstr(btentity **head, btoctal_in &octal, size_t size)
:btentity(head)
{
	int dot;
	char *bufp = new char[size + 1];
	assert(bufp != NULL);

	m_len = 0;
	m_buf = bufp;
	while (size-- > 0) {
		dot = octal.get_one();
		if (dot == -1)
			break;
		*bufp++ = dot;
		m_len++;
	}

	m_type = BTT_STR;
	*bufp++ = 0;
}

int btstr::match(const char *key)
{
	if (!strncmp(key, m_buf, m_len))
		return key[m_len] == 0;
	return 0;
}

const char *btstr::get(size_t *elenp)
{
	if (elenp != 0)
	   	*elenp = m_len;
	return m_buf;
}

btstr::~btstr()
{
	delete[] m_buf;
}

btcodec::~btcodec()
{
	delete m_elink;
	m_elink = 0;
	m_header = 0;
	m_tailer = 0;
}

int btcodec::parse(btoctal_in &octal)
{
	int dot, val;
	int kt, et, type;
	btentity *entity, **tailer;

	switch (dot = octal.get_one()) {
		case 'd':
			tailer = m_tailer;
			entity = new btdict(&m_elink);
			assert(entity->m_type == BTT_DICT);
			m_tailer = &entity->m_children;
			do {
				kt = parse(octal);
				if (kt != 's')
					break;
				et = parse(octal);
				if (et & 0x80)
					break;
			} while (true);
			m_tailer = &entity->m_sibling;
			*tailer = entity;
			type = (kt & 0x7F)? 0x81: 'd';
			break;

		case 'l':
			tailer = m_tailer;
			entity = new btlist(&m_elink);
			m_tailer = &entity->m_children;
			do {
				et = parse(octal);
				if (et & 0x80)
					break;
			} while (true);
			m_tailer = &entity->m_sibling;
			*tailer = entity;
			type = (et & 0x7F)? 0x82: 'l';
			break;

		case 'i':
			val = 0x0;
			dot = octal.get_one();
			while (isdigit(dot)) {
				val = val * 10 + (dot - '0');
				dot = octal.get_one();
			}
			tailer = m_tailer;
			entity = new btint(&m_elink, val);
			m_tailer = &entity->m_sibling;
			*tailer = entity;
			type = (dot == 'e')? 'i': 0x83;
			break;

		case 'e':
			type = 0x80;
			break;

		case '0': case '1': case '2':
		case '3': case '4': case '5':
		case '6': case '7': case '8':
		case '9': /* 0 ~ 9 pass througth */
			val = 0;
			do {
				val = val * 10 + (dot - '0');
				dot = octal.get_one();
			} while (isdigit(dot));

			if (dot != ':') {
				type = 0x84;
				break;
			}
			tailer = m_tailer;
			entity = new btstr(&m_elink, octal, val);
			m_tailer = &entity->m_sibling;
			*tailer = entity;
			type = 's';
			break;

		default:
			type = 0x85;
			break;
	}

	return type;
}

int btcodec::load(const char *buf, size_t len)
{
	int type;
	btoctal_in octal0(buf, len);

	type = parse(octal0);
	return (type & 0x80)? -1: 0;
}

btentity *btcodec::str(const char *str, size_t len)
{
	btstr *strp;
	strp = new btstr(&m_elink, str, len);
	return strp;
}

int btcodec::output(btoctal_out &octal, btentity *entity)
{
	int ival;
	size_t elen;
	char buf[256];
	const char *valp;
	btentity *sibling;

	switch (entity->m_type) {
		case BTT_DICT:
			octal.put_one('d');
			sibling = entity->m_children;
			while (sibling) {
				output(octal, sibling);
				sibling = sibling->m_sibling;
			}
			octal.put_one('e');
			break;

		case BTT_LIST:
			octal.put_one('l');
			sibling = entity->m_children;
			while (sibling) {
				output(octal, sibling);
				sibling = sibling->m_sibling;
			}
			octal.put_one('e');
			break;

		case BTT_INT:
			octal.put_one('i');
			((btint *)entity)->get(&ival);
			sprintf(buf, "%u", ival);
			octal.put_octal(buf, strlen(buf));
			octal.put_one('e');
			break;

		case BTT_STR:
			valp = ((btstr *)entity)->get(&elen);
			sprintf(buf, "%u:", elen);
			octal.put_octal(buf, strlen(buf));
			octal.put_octal(valp, elen);
			break;

		case BTT_WRAP:
		   	output(octal, entity->m_children);
			break;
	}

	return 0;
}

int btcodec::encode(void *buf, size_t size)
{
	btoctal_out octal((char *)buf, size);

	if (m_header != NULL)
	   	output(octal, m_header);

	return octal.size();
}

btentity *btcodec::root(void)
{
	return m_header;
}

btfastvisit::btfastvisit()
{
}

btfastvisit::~btfastvisit()
{
}

int btfastvisit::bget(int *ivalp)
{
	if (m_type == BTT_INT)
		return m_intp->get(ivalp);
	return -1;
}

btfastvisit &btfastvisit::bget(int index)
{
	int type = BTT_NONE;
	btentity *entity;

	if (m_type == BTT_LIST) {
		entity = m_listp->m_children;
		while (entity && index-- > 0)
			entity = entity->m_sibling;
		type = set_entity(entity);
	}

	m_type = type;
	return *this;
}

int btfastvisit::set_entity(btentity *entity)
{
	int type = BTT_NONE;

	if (entity != NULL) {
		switch (entity->m_type) {
			case BTT_DICT:
				m_dictp = (btdict *)entity;
				type = BTT_DICT;
				break;

			case BTT_LIST:
				m_listp = (btlist *)entity;
				type = BTT_LIST;
				break;

			case BTT_INT:
				m_intp = (btint *)entity;
				type = BTT_INT;
				break;

			case BTT_STR:
				m_strp = (btstr *)entity;
				type = BTT_STR;
				break;

			default:
				assert(0);
		}
	}

	return type;
}

btfastvisit &btfastvisit::bget(const char *name)
{
	int type;
	btstr *kep;
	btentity *entity;

	kep = 0;
	type = BTT_NONE;
	if (m_type == BTT_DICT) {
		kep = (btstr *)m_dictp->m_children;
		while (kep != NULL) {
			entity = kep->m_sibling;
			if (entity == NULL) {
				fprintf(stderr, "bad dict table: %s\n", name);
				break;
			}

			if (kep->match(name)) {
				type = set_entity(entity);
				break;
			}

			kep = (btstr *)entity->m_sibling;
		}
	}

	m_type = type;
	return *this;
}

const char *btfastvisit::c_str(size_t *len)
{
	if (m_type == BTT_STR)
		return m_strp->get(len);
	return NULL;
}

btfastvisit &btfastvisit::operator() (btcodec *codecp)
{
	int type;
	btentity *entity;

	type = BTT_NONE;
	entity = codecp->root();
	if (entity != NULL)
		type = set_entity(entity);

	m_type = type;
	return *this;
}

btentity *btfastvisit::bget(void)
{
	btentity *entity = 0;

	switch (m_type) {
		case BTT_DICT:
			entity = m_dictp;
			break;

		case BTT_LIST:
			entity = m_listp;
			break;

		case BTT_STR:
			entity = m_strp;
			break;

		case BTT_INT:
			entity = m_intp;
			break;
	}

	return entity;
}

void btfastvisit::replace(btentity *entity)
{
	switch (m_type) {
		case BTT_DICT:
			m_dictp->m_type = BTT_WRAP;
			m_dictp->m_children = entity;
			break;

		case BTT_LIST:
			m_listp->m_type = BTT_WRAP;
			m_listp->m_children = entity;
			break;

		case BTT_STR:
			m_strp->m_type = BTT_WRAP;
			m_strp->m_children = entity;
			break;

		case BTT_INT:
			m_intp->m_type = BTT_WRAP;
			m_intp->m_children = entity;
			break;
	}
}

