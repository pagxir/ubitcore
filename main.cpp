#include <stdio.h>
#include <winsock.h>

#include "module.h"
#include "slotwait.h"

extern struct module_stub ui_mod;
extern struct module_stub timer_mod;
extern struct module_stub slotsock_mod;
extern struct module_stub udp_daemon_mod;
static struct module_stub * module_stub_list[] = {
	&timer_mod, &slotsock_mod, &udp_daemon_mod,
   	&ui_mod, 0
};

int main(int argc, char * argv[])
{
	WSADATA data;
	WSAStartup(0x101, &data);
	initialize_modules(module_stub_list);

   	slotwait_held(0);
	slotwait_start();
	while (slotwait_step());

	cleanup_modules(module_stub_list);
	WSACleanup();
	return 0;
}

