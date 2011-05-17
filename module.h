#ifndef _MODULE_H_
#define _MODULE_H_

struct module_stub {
	void (* init)(void);
	void (* clean)(void);
};

void initialize_modules(struct module_stub * list[]);
void cleanup_modules(struct module_stub * list[]);

void init_stub(void);
void clean_stub(void);

#endif

