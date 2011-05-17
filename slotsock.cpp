#include <stdio.h>
#include <assert.h>
#include <winsock2.h>

#include "module.h"
#include "slotwait.h"
#include "slotsock.h"

#define MAX_CONN 1000

struct sockcb {
	struct sockcb * s_next;

	int s_file;
	slotcb s_read;
	slotcb s_write;
};

static int _sock_ref = 0;
static WSAEVENT _sock_wait = WSA_INVALID_EVENT;
struct sockcb _sockcb_list[MAX_CONN] = {0};

struct sockcb * _sockcb_free = 0;
struct sockcb * _sockcb_active = 0;

void sock_created(int sockfd)
{
	int i;
	DWORD all_events;
   	struct sockcb * sockcbp;

	assert(_sock_wait != WSA_INVALID_EVENT);
	all_events = FD_CONNECT| FD_ACCEPT| FD_READ| FD_WRITE| FD_CLOSE;
	WSAEventSelect(sockfd, _sock_wait, all_events);

	sockcbp = _sockcb_free;
	assert(sockcbp != NULL);
	_sockcb_free = _sockcb_free->s_next;

	sockcbp->s_file = sockfd;
	sockcbp->s_read = 0;
	sockcbp->s_write = 0;

	sockcbp->s_next = _sockcb_active;
	_sockcb_active = sockcbp;
	_sock_ref++;
	return;
}

void sock_closed(int sockfd)
{
	int i;
	struct sockcb * sockcbp;
	struct sockcb ** sockcbpp;

	sockcbpp = &_sockcb_active;
	for (sockcbp = _sockcb_active; 
			sockcbp != NULL; sockcbp = sockcbp->s_next) {
		if (sockfd == sockcbp->s_file) {
			*sockcbpp = sockcbp->s_next;
			sockcbp->s_file = -1;

			sockcbp->s_next = _sockcb_free;
			_sockcb_free = sockcbp;
			break;
		}

		sockcbpp = &sockcbp->s_next;
	}

	WSAEventSelect(sockfd, _sock_wait, 0);
	_sock_ref--;
}

static struct sockcb * sock_attach(int sockfd, struct waitcb * waitcbp)
{
	int index;
	struct sockcb * sockcbp;

	if (waitcbp->wt_value >= 0 &&
		   	waitcbp->wt_value < MAX_CONN) {
		index = waitcbp->wt_value;
		sockcbp = _sockcb_list + index;
		if (sockcbp->s_file == sockfd)
			return sockcbp;
	}

	for (sockcbp = _sockcb_active; 
			sockcbp != NULL; sockcbp = sockcbp->s_next) {
		if (sockfd == sockfd) {
			waitcbp->wt_value = sockcbp - _sockcb_list;
			return sockcbp;
		}
	}

	return 0;
}

int getaddrbyname(const char * name, struct sockaddr_in * addr)
{
	char buf[1024];
	in_addr in_addr1;
	u_long peer_addr;
	char * port, * hostname;
	struct hostent * phost;
	struct sockaddr_in * p_addr;

	strcpy(buf, name);
	hostname = buf;

	port = strchr(buf, ':');
	if (port != NULL) {
		*port++ = 0;
	}

	p_addr = (struct sockaddr_in *)addr;
	p_addr->sin_family = AF_INET;
	p_addr->sin_port   = htons(port? atoi(port): 3478);

	peer_addr = inet_addr(hostname);
	if (peer_addr != INADDR_ANY &&
		peer_addr != INADDR_NONE) {
		p_addr->sin_addr.s_addr = peer_addr;
		return 0;
	}

	phost = gethostbyname(hostname);
	if (phost == NULL) {
		return -1;
	}

	memcpy(&in_addr1, phost->h_addr, sizeof(in_addr1));
	p_addr->sin_addr = in_addr1;
	return 0;
}

static void do_quick_scan(void * upp)
{
	int error;
	long events;
	struct sockcb * sockcbp;
	WSANETWORKEVENTS network_events;

	for (sockcbp = _sockcb_active; 
			sockcbp != NULL; sockcbp = sockcbp->s_next) {
		error = WSAEnumNetworkEvents(sockcbp->s_file, 0, &network_events);
		if (error != 0)
			continue;
	   
		events = network_events.lNetworkEvents;
		if (FD_READ & events)
		   	slot_wakeup(&sockcbp->s_read);

		if (FD_WRITE & events)
		   	slot_wakeup(&sockcbp->s_write);

		if (FD_CLOSE & events)
		   	slot_wakeup(&sockcbp->s_read);

		if (FD_ACCEPT & events)
		   	slot_wakeup(&sockcbp->s_read);

		if (FD_CONNECT & events)
		   	slot_wakeup(&sockcbp->s_write);
	}
}

void sock_selscan(void * upp)
{
	DWORD state;
	DWORD timeout = 20;

	if (_sock_ref > 0) { 
		state = WaitForSingleObject(_sock_wait, timeout);

		switch (state) {
			case WAIT_OBJECT_0:
				ResetEvent(_sock_wait);
				do_quick_scan(0);
				break;

			case WAIT_TIMEOUT:
				break;

			default:
				assert(0);
				break;
		}
	}
}

int sendto2(int sockfd, void * buf, size_t len,
		const struct sockaddr_in * so_addr, struct waitcb * waitcbp)
{
   	int count;
   	struct sockcb * sockcbp;
	
	count = sendto(sockfd, (char *)buf, len, 0,
		   	(struct sockaddr *)so_addr, sizeof(*so_addr));
	if (count == -1 &&
		   	WSAGetLastError() == WSAEWOULDBLOCK) {
		sockcbp = sock_attach(sockfd, waitcbp);
		assert(sockcbp != NULL);
		slot_insert(&sockcbp->s_write, waitcbp);
	}

	return count;
}

int recvfrom2(int sockfd, void * buf, size_t len,
		struct sockaddr_in * so_addr, struct waitcb * waitcbp)
{
	int count;
   	struct sockcb * sockcbp;
	int so_len = sizeof(*so_addr);
	
	count = recvfrom(sockfd, (char *)buf, len, 0,
		   	(struct sockaddr *)so_addr, &so_len);
	if (count == -1 &&
		   	WSAGetLastError() == WSAEWOULDBLOCK) {
		sockcbp = sock_attach(sockfd, waitcbp);
		assert(sockcbp != NULL);
		slot_insert(&sockcbp->s_read, waitcbp);
	}

	return count;
}

static struct waitcb _sock_waitcb;
static void module_init(void)
{
	int i;
	struct sockcb * sockcbp;

	_sockcb_free = 0;
	_sockcb_active = 0;
	for (i = 0; i < MAX_CONN; i++) {
		sockcbp = &_sockcb_list[i];
		sockcbp->s_next = _sockcb_free;
		_sockcb_free = sockcbp;
	}
   
	_sock_wait = WSACreateEvent();
	waitcb_init(&_sock_waitcb, do_quick_scan, 0);
	_sock_waitcb.wt_flags &= ~WT_EXTERNAL;
	waitcb_switch(&_sock_waitcb);
}

static void module_clean(void)
{
   	_sock_wait = WSA_INVALID_EVENT;
   	WSACloseEvent(_sock_wait);
}

struct module_stub slotsock_mod = {
	module_init, module_clean
};

