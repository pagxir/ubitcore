#include <stdio.h>
#include <assert.h>
#include <process.h>
#include <windows.h>

#include "ui.h"
#include "utils.h"
#include "module.h"
#include "callout.h"
#include "slotwait.h"
#include "slotsock.h"
#include "udp_daemon.h"

static LONG _cmd_lck = 0;
static LONG _cmd_yes = 0;
static char _cmd_buf[1024];

static void input_routine(void *upp)
{
	char buf[1024];
	char *retp = 0;
	char *carp = 0;
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
static struct waitcb _timer_input;
static void parse_request(void *upp)
{
	int count;
	char server[200];
	char ident1[20];
	char ident0[60];

   	if (InterlockedExchange(&_cmd_lck, 1)) {
	   	callout_reset(&_timer_input, 500);
		return;
	}

	if (!strcmp(_cmd_buf, "quit")) {
	   	slotwait_stop();
		return;
	}

	if (InterlockedExchange(&_cmd_yes, 0)) {
		if (!strncmp(_cmd_buf, "setident ", 9)) {
			count = sscanf(_cmd_buf, "%*s %s", ident0);
			hex_decode(ident0, ident1, sizeof(ident1));
			kad_setident(ident1);
		} else if (!strncmp(_cmd_buf, "get_peers ", 9)) {
			count = sscanf(_cmd_buf, "%*s %s %s", ident0, server);
			hex_decode(ident0, ident1, sizeof(ident1));
			kad_getpeers(ident1, server);
			fprintf(stderr, "kad_getpeers\n");
		} else if (!strncmp(_cmd_buf, "find_node ", 9)) {
			count = sscanf(_cmd_buf, "%*s %s %s", ident0, server);
			hex_decode(ident0, ident1, sizeof(ident1));
			kad_findnode(ident1, server);
			fprintf(stderr, "kad_findnode\n");
		} else if (!strncmp(_cmd_buf, "ping_node ", 9)) {
			count = sscanf(_cmd_buf, "%*s %s", server);
			kad_pingnode(server);
			fprintf(stderr, "kad_pingnode\n");
		}
	}

   	InterlockedExchange(&_cmd_lck, 0);
	callout_reset(&_timer_input, 500);
} 

static void module_init(void)
{
	h_input = _beginthread(input_routine, 0, 0);
	waitcb_init(&_timer_input, parse_request, 0);
	callout_reset(&_timer_input, 500);
}

static void module_clean(void)
{
	HANDLE handle;
	handle = (HANDLE)h_input;
	WaitForSingleObject(handle, INFINITE);
	CloseHandle(handle);
	waitcb_clean(&_timer_input);
}

struct module_stub ui_mod = {
	module_init, module_clean
};

