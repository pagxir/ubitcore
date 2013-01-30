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
#include "kad_route.h"
#include "udp_daemon.h"

static void parse_request(void *upp);

static BOOL WINAPI console_closed(DWORD ctrl_type)
{
	slotwait_stop();
	return TRUE;
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

static unsigned __stdcall input_routine(void *upp)
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
			if (!memcmp(buf, "quit:", 5)) {
				continue;
		   	}

			if (!strcmp(buf, "quit")) {
				fclose(rc_fp);
				goto ui_quited;
		   	}
		   
			ipccb_switch(&ui.ui_ipccb);
		   	WaitForSingleObject(ui.ui_event, INFINITE);
			Sleep(30);
		}
		fclose(rc_fp);
	}

   	for ( ; ; ) {
	   	retp = fgets(buf, sizeof(buf), stdin);
		if (retp == NULL) {
			fprintf(stderr, "ui input return NULL\n");
			break;
		}
	   
		crlf_strip(retp);
		if (!strcmp(buf, "quit")) {
		   	fprintf(stderr, "ui request quit\n");
			break;
		}

		if (*retp == 0) {
		   	fprintf(stderr, "ui: ");
			continue;
		}

		ipccb_switch(&ui.ui_ipccb);
		WaitForSingleObject(ui.ui_event, INFINITE);
	}

ui_quited:
	rc_fp = fopen("clientrc.txt", "r");
	if (rc_fp != NULL) {
		while (!feof(rc_fp)) {
		   	retp = fgets(buf, sizeof(buf), rc_fp);
		   	if (retp == NULL) {
			   	break;
		   	}
		   
			crlf_strip(retp);
			if (memcmp(buf, "quit:", 5)) {
				continue;
		   	}
		   
			ipccb_switch(&ui.ui_ipccb);
		   	WaitForSingleObject(ui.ui_event, INFINITE);
		}
		fclose(rc_fp);
	}

	slotwait_stop();
	CloseHandle(ui.ui_event);
	_endthreadex( 0 );
    return 0;
}

static uintptr_t h_input;
static void parse_request(void *upp)
{
	int count;
	char server[300];
	char ident1[120];
	char ident0[160];
	struct _user_input *uip;

	uip = (struct _user_input *)upp;

	count = -1;
	if (!strncmp(uip->ui_buf, "myid ", 5)) {
	   	count = sscanf(uip->ui_buf, "%*s %s", ident0);
	   	hex_decode(ident0, ident1, sizeof(ident1));
	   	kad_setup(ident1);
   	} else if (!strncmp(uip->ui_buf, "peer ", 5)) {
	   	count = sscanf(uip->ui_buf, "%*s %s", ident0);
	   	hex_decode(ident0, ident1, sizeof(ident1));
	   	kad_getpeers(ident1);
   	} else if (!strncmp(uip->ui_buf, "find ", 5)) {
	   	count = sscanf(uip->ui_buf, "%*s %s", ident0);
	   	hex_decode(ident0, ident1, sizeof(ident1));
	   	kad_findnode(ident1);
   	} else if (!strncmp(uip->ui_buf, "blur ", 5)) {
	   	sscanf(uip->ui_buf, "%*s %d", &count);
		printf("blur bucket: %d\n", count);
   	} else if (!strncmp(uip->ui_buf, "ping ", 5)) {
	   	count = sscanf(uip->ui_buf, "%*s %s", server);
	   	kad_pingnode(server);
   	} else if (!strncmp(uip->ui_buf, "dump", 4)) {
	   	sscanf(uip->ui_buf, "%*s %d", &count);
		kad_route_dump(count);
   	}
   
	SetEvent(uip->ui_event);
	return;
} 

static struct waitcb _ui_stop;
static struct waitcb _ui_start;

static void ui_stat_routine(void *upp)
{
	unsigned id;
	HANDLE handle;

	if (upp == &_ui_start) {
	   	h_input = _beginthreadex( NULL, 0, &input_routine, NULL, 0, &id);
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

	slotwait_atstart(&_ui_start);
	slotwait_atstop(&_ui_stop);
}

static void module_clean(void)
{
	waitcb_clean(&_ui_start);
	waitcb_clean(&_ui_stop);
}

struct module_stub ui_mod = {
	module_init, module_clean
};

