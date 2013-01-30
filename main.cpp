#include <stdio.h>

#include <wait/platform.h>
#include <wait/module.h>
#include <wait/slotwait.h>

extern struct module_stub ui_mod;
extern struct module_stub timer_mod;
extern struct module_stub slotipc_mod;
extern struct module_stub slotsock_mod;
extern struct module_stub udp_daemon_mod;
extern struct module_stub player_mod;
extern struct module_stub webcrack_mod;
extern struct module_stub kad_route_mod;
static struct module_stub *module_stub_list[] = {
	&timer_mod, &slotsock_mod, &udp_daemon_mod,
	&webcrack_mod, &kad_route_mod,
   	0
};

int main(int argc, char *argv[])
{
   	slotwait_held(0);
	initialize_modules(module_stub_list);

	slotwait_start();
	while (slotwait_step());

#if 0
	slotwait_stoped();
	while (slotwait_step());
#endif

	cleanup_modules(module_stub_list);
	return 0;
}

