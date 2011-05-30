#include <stdio.h>
#include <assert.h>
#include <process.h>
#include <windows.h>

#include "ui.h"
#include "utils.h"
#include "module.h"
#include "callout.h"
#include "slotwait.h"
#include "slotipc.h"
#include "slotsock.h"
#include "udp_daemon.h"

static ipccb_t _ipccb_console;
static void parse_request(void *upp);

static BOOL WINAPI console_closed(DWORD ctrl_type)
{
   	ipccb_switch(&_ipccb_console);
	return TRUE;
}

static void stop_wrapper(void *upp)
{
	slotwait_stop();
	return;
}

struct _user_input {
	char *ui_buf;
	HANDLE ui_event;
	ipccb_t ui_ipccb;
};

static void crlf_strip(char *line)
{
	char *carp;

   	carp = line;
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

	return;
}

static void input_routine(void *upp)
{
	char buf[1024];
	char *retp = 0;
	char *carp = 0;
	LONG oldval = 0;
	FILE *rc_fp = 0;
	struct _user_input ui;

	ui.ui_buf = buf;
	ipccb_init(&ui.ui_ipccb, parse_request, &ui);
	ui.ui_event = CreateEvent(NULL, FALSE, FALSE, NULL);

	rc_fp = fopen("clientrc.txt", "r");
	if (rc_fp != NULL) {
		while (!feof(rc_fp)) {
		   	retp = fgets(buf, sizeof(buf), rc_fp);
		   	if (retp == NULL) {
			   	break;
		   	}
		   
			crlf_strip(retp);
			if (!strcmp(buf, "quit")) {
				fclose(rc_fp);
				goto ui_quited;
		   	}
		   
			ipccb_switch(&ui.ui_ipccb);
		   	WaitForSingleObject(ui.ui_event, INFINITE);
		}
		fclose(rc_fp);
	}

   	for ( ; ; ) {
	   	fprintf(stderr, "ui: ");
	   	retp = fgets(buf, sizeof(buf), stdin);
		if (retp == NULL) {
			fprintf(stderr, "ui input return NULL\n");
			break;
		}
	   
		crlf_strip(retp);
		if (!strcmp(buf, "quit")) {
			break;
		}

		ipccb_switch(&ui.ui_ipccb);
		WaitForSingleObject(ui.ui_event, INFINITE);
	}

ui_quited:
   	ipccb_switch(&_ipccb_console);
	CloseHandle(ui.ui_event);
	_endthread();
}

static uintptr_t h_input;
static void parse_request(void *upp)
{
	int count;
	char server[200];
	char ident1[20];
	char ident0[60];
	struct _user_input *uip;

	uip = (struct _user_input *)upp;

	if (!strncmp(uip->ui_buf, "setident ", 9)) {
	   	count = sscanf(uip->ui_buf, "%*s %s", ident0);
	   	hex_decode(ident0, ident1, sizeof(ident1));
	   	kad_setident(ident1);
   	} else if (!strncmp(uip->ui_buf, "get_peers ", 9)) {
	   	count = sscanf(uip->ui_buf, "%*s %s %s", ident0, server);
	   	hex_decode(ident0, ident1, sizeof(ident1));
	   	kad_getpeers(ident1, server);
	   	fprintf(stderr, "kad_getpeers\n");
   	} else if (!strncmp(uip->ui_buf, "find_node ", 9)) {
	   	count = sscanf(uip->ui_buf, "%*s %s %s", ident0, server);
	   	hex_decode(ident0, ident1, sizeof(ident1));
	   	kad_findnode(ident1, server);
	   	fprintf(stderr, "kad_findnode\n");
   	} else if (!strncmp(uip->ui_buf, "ping_node ", 9)) {
	   	count = sscanf(uip->ui_buf, "%*s %s", server);
	   	kad_pingnode(server);
	   	fprintf(stderr, "kad_pingnode\n");
   	}
   
	SetEvent(uip->ui_event);
	return;
} 

static struct waitcb _ui_stop;
static struct waitcb _ui_start;

static void ui_stat_routine(void *upp)
{
	HANDLE handle;

	if (upp == &_ui_start) {
	   	h_input = _beginthread(input_routine, 0, 0);
	   	SetConsoleCtrlHandler(console_closed, TRUE);
		return;
	}
	
	if (upp == &_ui_stop) {
	   	handle = (HANDLE)h_input;
	   	SetConsoleCtrlHandler(console_closed, FALSE);
	   	CloseHandle(GetStdHandle(STD_INPUT_HANDLE));
	   	WaitForSingleObject(handle, INFINITE);
	   	CloseHandle(handle);
	}

	return;
}

static void module_init(void)
{
	waitcb_init(&_ui_start, ui_stat_routine, &_ui_start);
	waitcb_init(&_ui_stop, ui_stat_routine, &_ui_stop);
	ipccb_init(&_ipccb_console, stop_wrapper, 0);

	slotwait_atstart(&_ui_start);
	slotwait_atstop(&_ui_stop);
}

static void module_clean(void)
{
	fprintf(stderr, "ui module clean1\n");
	ipccb_clean(&_ipccb_console);
	waitcb_clean(&_ui_start);
	waitcb_clean(&_ui_stop);
}

struct module_stub ui_mod = {
	module_init, module_clean
};

