#include <stdio.h>
#include <assert.h>
#include <process.h>
#include <windows.h>

#include "ui.h"
#include "timer.h"
#include "module.h"
#include "slotwait.h"
#include "slotsock.h"

static LONG _cmd_lck = 0;
static LONG _cmd_yes = 0;
static char _cmd_buf[1024];

static void input_routine(void * upp)
{
	char buf[1024];
	char * retp = 0;
	char * carp = 0;
	LONG oldval = 0;

	for ( ; ; ) {
	   	retp = fgets(buf, sizeof(buf), stdin);
		if (retp != NULL) {
			carp = retp;
			while (*carp) {
				if (*carp == '\r') {
					*carp = 0;
					break;
				}

				if (*carp == '\n') {
					*carp = 0;
					break;
				}
				carp++;
			}
		}

	   	oldval = InterlockedExchange(&_cmd_lck, 1);
	   	while (oldval != 0) {
		   	Sleep(100);
		   	oldval = InterlockedExchange(&_cmd_lck, 1);
	   	}
	   
		strcpy(_cmd_buf, retp? buf: "quit");
	   	InterlockedExchange(&_cmd_yes, 1);
	   	InterlockedExchange(&_cmd_lck, 0);

		if (retp == 0 || !strcmp(buf, "quit"))
			break;
	}

	_endthread();
}

static uintptr_t h_input;
static struct waitcb _wait_input;
static void parse_request(void * upp)
{
   	if (InterlockedExchange(&_cmd_lck, 1)) {
	   	callout_reset(&_wait_input, 500);
		return;
	}

	if (!strcmp(_cmd_buf, "quit")) {
	   	slotwait_stop();
		return;
	}

	if (InterlockedExchange(&_cmd_yes, 0)) {
	   	fprintf(stderr, "receive command: %s\n", _cmd_buf);
	}

   	InterlockedExchange(&_cmd_lck, 0);
	callout_reset(&_wait_input, 500);
} 

static void module_init(void)
{
	h_input = _beginthread(input_routine, 0, 0);
	waitcb_init(&_wait_input, parse_request, 0);
	callout_reset(&_wait_input, 500);
}

static void module_clean(void)
{
	HANDLE handle;
	handle = (HANDLE)h_input;
	WaitForSingleObject(handle, INFINITE);
	CloseHandle(handle);
}

struct module_stub ui_mod = {
	module_init, module_clean
};

