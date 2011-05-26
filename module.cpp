#include <stdio.h>

#include "module.h"

void init_stub(void)
{
	fprintf(stderr, "init_stub\n");
	return;
}

void clean_stub(void)
{
	fprintf(stderr, "clean_stub\n");
	return;
}

void initialize_modules(struct module_stub *list[])
{
	void (* initp)(void);
	struct module_stub **stubp;

	stubp = list;
	while (*stubp) {
		initp = (*stubp)->init;
		if (initp != 0)
			(void)initp();
		stubp++;
	}
}

void cleanup_modules(struct module_stub *list[])
{
	void (* cleanp)(void);
	struct module_stub **stubp;

	stubp = list;
	while (*stubp) {
		cleanp = (*stubp)->clean;
		if (cleanp != 0)
			(void)cleanp();
		stubp++;
	}
}

