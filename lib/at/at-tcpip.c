/*
 * Copyright 2018-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <ctype.h>
#include <hal/kernel.h>
#include <hal/timer.h>
#include <hal/console.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <assert.h>

#include "cmsis_os.h"

#include "compat_param.h"
#include "compat_if.h"
#include "if_dl.h"
#include "if_media.h"
#include "ethernet.h"
#include "route.h"
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include "libifconfig.h"

#include "lwip/netif.h"
#include "lwip/netifapi.h"
#include "lwip/inet_chksum.h"
#include "lwip/prot/dhcp.h"
#include "lwip/icmp.h"

#include <at.h>
#include <wise_err.h>
#include <wise_log.h>
#include <wise_wifi.h>
#include <wise_event_loop.h>
#include "esp_transport.h"
#include "esp_transport_tcp.h"
#include "esp_transport_ssl.h"
#include "esp_transport_internal.h"

#define AT_SOCKET_MAX_CONN_NUM 5

#define at_tcpip_dbg(format, ... ) printf(format, ##__VA_ARGS__)

#if LWIP_IPV6

#define IS_IPV6_ADDRESS(hostname, addr) \
		(ipaddr_aton(hostname, &addr) && IP_IS_V6(&addr) && \
		 !ip6_addr_isipv4mappedipv6(ip_2_ip6(&addr)))

#endif

typedef enum {
	CONN_IP_ACQUIRED = 2,  	/* Connected to an AP and IP addr quired */
	CONN_NOT_READY = 5,	/* Not connected to an AP or IP addr not acquired */
	CONN_ACTIVE = 3,
} conn_stat;

typedef enum {
	CONN_TCP = 0,
	CONN_UDP,
	CONN_SSL,
	CONN_UNKONWN
} conn_type;

typedef enum {
	REMOTE_FIXED = 0, 	/* UDP peer for tx is fixed at AT+CIPSTART */
	REMOTE_RX_ONCE,		/* UDP peer for tx will be changed once on the first rx */
	REMOTE_LAST_RX		/* UDP peer for tx will be changed on every rx */
} udp_tx_remote;

struct conn {
	int id;
	int fd;
	int server;
	conn_type type;
	struct addrinfo *ai;
	uint16_t local_port;
	int remote_changed;	/* for REMOTE_RX_ONCE */
	udp_tx_remote udp_mode;
	int pass;		/* 0 : normal, 1 : pass-through */
	esp_transport_list_handle_t tlist;
	esp_transport_handle_t tcp;
	esp_transport_handle_t ssl;
	int (*at_write) (struct conn *connn, const char *buffer, int len, int timeout_ms);
	int (*at_read) (struct conn *connn, struct sockaddr_storage *from, char *buffer, int len, int timeout_ms);
	struct list_head list;
};

static struct addrinfo * at_build_remote_ai(char *r_host,
	int r_port);

#ifdef CONFIG_ATCMD_AT_CIPDINFO

static int ipdinfo = 0; 	/* 0 : default, 1 : show remote addr, port */
#endif
extern int wise_wpas_cli(int argc, char *argv[]);
extern void eloop_unregister_read_sock(int sock);

#define REGISTER_SOCKET(sock, handler, ret)			\
do {								\
	char str1[8], str2[12];					\
	char *argv[3] = {"REGISTER_SOCKET", NULL, NULL}; 	\
	snprintf(str1, sizeof(str1), "%d", sock);		\
	snprintf(str2, sizeof(str2), "%x", handler);		\
	argv[1] = str1;						\
	argv[2] = str2; 					\
	ret = wise_wpas_cli(3, argv);				\
} while (0)

#define UNREGISTER_SOCKET(sock, ret)				\
do {								\
	char str1[8];						\
	char *argv[2] = {"UNREGISTER_SOCKET", NULL}; 		\
	snprintf(str1, sizeof(str1), "%d", sock);		\
	argv[1] = str1;						\
	ret = wise_wpas_cli(2, argv);				\
} while (0)

#define register_socket(sock, handler) 	({ int __ret; REGISTER_SOCKET(sock, handler, __ret); __ret; })
#define unregister_socket(sock) 	({ int __ret; UNREGISTER_SOCKET(sock, __ret); __ret; })

/* The list of created connection links */
static LIST_HEAD_DEF(conns);


#ifdef CONFIG_ATCMD_AT_CIPSTA

/* Like strncpy but make sure the resulting string is always 0 terminated. */
static char *safe_strncpy(char *dst, const char *src, size_t size)
{
	dst[size-1] = '\0';
	return strncpy(dst,src,size-1);
}

static int INET_rresolve(char *name, size_t len,
		struct sockaddr_in *sin, int numeric);
static void INET_reserror(char *text);
static char *INET_print(unsigned char *ptr);
static char *INET_sprint(struct sockaddr *sap, int numeric);
static int INET_input(char *bufp, struct sockaddr *sap);
#endif

#ifdef CONFIG_ATCMD_AT_CIPSTAMAC
static char *pr_ether(unsigned char *ptr);
static int in_ether(char *bufp, struct sockaddr *sap);
#endif
#ifdef CONFIG_ATCMD_AT_CIPSTA

/* This structure defines protocol families and their handlers. */
static struct aftype {
	char *name;
	char *(*print)(unsigned char *);
	char *(*sprint)(struct sockaddr *, int numeric);
	int (*input)(char *bufp, struct sockaddr *);
	void (*herror)(char *text);
} inet_aftype = {
	"inet",
	INET_print,
	INET_sprint,
	INET_input,
	INET_reserror
};

#ifdef CONFIG_LWIP_IPV6

#ifndef IN6_IS_ADDR_V4MAPPED
#define IN6_IS_ADDR_V4MAPPED(a) \
	((((__const uint32_t *) (a))[0] == 0) \
	&& (((__const uint32_t *) (a))[1] == 0) \
	&& (((__const uint32_t *) (a))[2] == htonl (0xffff)))
#endif

static char * fix_v4_address(char *buf, struct in6_addr *in6)
{
	if (IN6_IS_ADDR_V4MAPPED(in6->s6_addr)) {
		char *s =strchr(buf, '.');
		if (s) {
			while (s > buf && *s != ':')
				--s;
			if (*s == ':') ++s;
			else s = NULL;
		}
		if (s) return s;
	}
	return buf;
}

static int INET6_resolve(char *name, struct sockaddr_in6 *sin6)
{
	struct addrinfo req, *ai;
	int s;

	memset (&req, '\0', sizeof req);
	req.ai_family = AF_INET6;
	if ((s = getaddrinfo(name, NULL, &req, &ai))) {
		fprintf(stderr, "getaddrinfo: %s: %d\n", name, s);
		return -1;
	}
	memcpy(sin6, ai->ai_addr, sizeof(struct sockaddr_in6));

	freeaddrinfo(ai);

	return (0);
}

#ifndef IN6_IS_ADDR_UNSPECIFIED
#define IN6_IS_ADDR_UNSPECIFIED(a) \
        (((uint32_t *) (a))[0] == 0 && ((uint32_t *) (a))[1] == 0 && \
         ((uint32_t *) (a))[2] == 0 && ((uint32_t *) (a))[3] == 0)
#endif

static int INET6_rresolve(char *name, struct sockaddr_in6 *sin6, int numeric)
{
	if (sin6->sin6_family != AF_INET6) {
#ifdef DEBUG
		fprintf(stderr, _("rresolve: unsupported address family %d !\n"),
			sin6->sin6_family);
#endif
		errno = EAFNOSUPPORT;
		return (-1);
	}
	if (numeric & 0x7FFF) {
		inet_ntop( AF_INET6, &sin6->sin6_addr, name, 80);
		return (0);
	}
	if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
		if (numeric & 0x8000)
			strcpy(name, "default");
		else
			strcpy(name, "[::]");
		return (0);
	}

	return (0);
}

/* Display an Internet socket address. */
static char *INET6_print(unsigned char *ptr)
{
	static char name[80];

	inet_ntop(AF_INET6, (struct in6_addr *) ptr, name, 80);
	return fix_v4_address(name, (struct in6_addr *)ptr);
}

/* Display an Internet socket address. */
/* dirty! struct sockaddr usually doesn't suffer for inet6 addresses, fst. */
static char *INET6_sprint(struct sockaddr *sap, int numeric)
{
	static char buff[128];

	if (sap->sa_family == 0xFFFF || sap->sa_family == 0)
		return safe_strncpy(buff, "[NONE SET]", sizeof(buff));
	if (INET6_rresolve(buff, (struct sockaddr_in6 *) sap, numeric) != 0)
		return safe_strncpy(buff, "[UNKNOWN]", sizeof(buff));
	return (fix_v4_address(buff, &((struct sockaddr_in6 *)sap)->sin6_addr));
}

static int INET6_input(char *bufp, struct sockaddr *sap)
{
	return (INET6_resolve(bufp, (struct sockaddr_in6 *) sap));
}

static void INET6_reserror(char *text)
{
	printf("%s", text);
}

static struct aftype inet6_aftype = {
	"inet6",
	INET6_print,
	INET6_sprint,
	INET6_input,
	INET6_reserror
};

#endif

#endif

static void *xsin_addr(void *ptr)
{
	struct sockaddr *sa = ptr;

	return (sa->sa_family == AF_INET) ?
		ptr + offsetof(struct sockaddr_in, sin_addr) :
#if LWIP_IPV6
		ptr + offsetof(struct sockaddr_in6, sin6_addr)
#else
		ptr
#endif
		;

}

static struct addrinfo *resolv_hostname(const char *hostname,
					const char *service,
					int family, int socktype,
					int flags)
{
	struct addrinfo hint = {
		.ai_flags = AI_CANONNAME | flags,
		.ai_family = family,
		.ai_socktype = socktype,
	};
	struct addrinfo *ai;
	int ret;

	ret = getaddrinfo(hostname, service, &hint, &ai);
	if (ret != 0)
		return NULL;

	return ai;
}

__maybe_unused static struct addrinfo * at_build_remote_ai(char *r_host,
	int r_port) {
	struct addrinfo *ai = NULL;
	char portstr[6] = {0};
#if LWIP_IPV6
	char dst[INET6_ADDRSTRLEN];
#else
	char dst[INET_ADDRSTRLEN];
#endif

	int family = AF_INET;
	ip_addr_t addr __maybe_unused;

	if (r_port > 0)
		snprintf(portstr, sizeof(portstr), "%d", r_port);
#if LWIP_IPV6
	if (IS_IPV6_ADDRESS(r_host, addr)) {
		family = AF_INET6;
	}
#endif /* LWIP_IPV6 */


	ai = resolv_hostname(r_host, r_port > 0 ? portstr : NULL, family, 0, 0);

	if (ai == NULL ||
		inet_ntop(ai->ai_family, xsin_addr(ai->ai_addr), dst, sizeof(dst)) < 0) {
		if (ai)
			freeaddrinfo(ai);
		printf("Unknown host %s\n", r_host);
		return NULL;
	}

	return ai;
}

#ifdef CONFIG_ATCMD_AT_CIPSTAMAC

/* This structure defines hardware protocols and their handlers. */
static struct hwtype {
	char *name;
	int alen;
	char *(*print) (unsigned char *);
	int (*input) (char *, struct sockaddr *);
} ether_hwtype = {
	"ether",
	ETHER_ADDR_LEN,
	pr_ether,
	in_ether
};

static int INET_resolve(char *name, struct sockaddr_in *sin)
{
	/* Grmpf. -FvK */
	sin->sin_family = AF_INET;
	sin->sin_port = 0;

	/* Default is special, meaning 0.0.0.0. */
	if (!strcmp(name, "default")) {
		sin->sin_addr.s_addr = INADDR_ANY;
		return (1);
	}
	/* Look to see if it's a dotted quad. */
	if (inet_aton(name, &sin->sin_addr)) {
		return 0;
	}
	return -1;
}

/* numeric: & 0x8000: default instead of *,
 *	    & 0x4000: host instead of net,
 *	    & 0x0fff: don't resolve
 */
static int INET_rresolve(char *name, size_t len, struct sockaddr_in *sin,
			 int numeric)
{
	u_int32_t ad;

	/* Grmpf. -FvK */
	if (sin->sin_family != AF_INET) {
#if 0
		printk("rresolve: unsupported address family %d !\n",
				sin->sin_family);
#endif
		errno = EAFNOSUPPORT;
		return (-1);
	}
	ad = sin->sin_addr.s_addr;
#if 0
	printk("rresolve: %08lx, num %08x \n",
			ad, numeric);
#endif
	if (ad == INADDR_ANY) {
		if ((numeric & 0x0FFF) == 0) {
			if (numeric & 0x8000)
				safe_strncpy(name, "default", len);
			else
				safe_strncpy(name, "*", len);
			return (0);
		}
	}

	if (!(numeric & 0x0FFF)) {
		/* host dns - not supported */
		errno = EAFNOSUPPORT;
		return (-1);
	}

	safe_strncpy(name, inet_ntoa(sin->sin_addr), len);
	return (0);
}

static void INET_reserror(char *text)
{
	printk("%s", text);
}

/* Display an Internet socket address. */
static char *INET_print(unsigned char *ptr)
{
	return (inet_ntoa((*(struct in_addr *)ptr)));
}

/* Display an Internet socket address. */
static char *INET_sprint(struct sockaddr *sap, int numeric)
{
	static char buff[128];

	if (sap->sa_family == 0xFFFF
			|| sap->sa_family == 0)
		return safe_strncpy(buff, "[NONE SET]", sizeof(buff));

	if (INET_rresolve(buff, sizeof(buff),
				(struct sockaddr_in *)sap,
				numeric) != 0)
		return (NULL);

	return (buff);
}

static int INET_input(char *bufp, struct sockaddr *sap)
{
	return (INET_resolve(bufp, (struct sockaddr_in *) sap));
}
#endif

#ifdef CONFIG_ATCMD_AT_CIPSTART

static char *pr_ether(unsigned char *ptr)
{
	return ether_ntoa((const struct ether_addr *) ptr);
}

static int in_ether(char *bufp, struct sockaddr *sap)
{
	struct ether_addr *ether = ether_aton(bufp);

	if (ether) {
		memcpy(sap->sa_data, ether, sizeof(*ether));
		sap->sa_len = sizeof(*ether);
		assert(sizeof(*ether) == ETHER_ADDR_LEN);
		return 0;
	}
	return -1;
}

#endif

#define err_exit(err, code)	do {			\
	err = code;					\
	goto exit;					\
} while (0)

void at_tcpip_init(void)
{
}

int g_socket_free_count = AT_SOCKET_MAX_CONN_NUM;

void at_close_conn(struct conn *conn){
	conn_type type = conn->type;
	if (!conn)
		return;

	at_printf("%d,CLOSED\r\n", conn->id);

	if (conn->ai)
		freeaddrinfo(conn->ai);
	if (conn->tcp)
		esp_transport_close(conn->tcp);
	if (conn->ssl)
		esp_transport_close(conn->ssl);
	if (conn->tlist)
		esp_transport_list_destroy(conn->tlist);
	if (conn->fd > 0 && type == CONN_UDP)
		close(conn->fd);
	free(conn);
	g_socket_free_count++;
}

#ifdef CONFIG_ATCMD_AT_CIPSTART

static void at_tcpip_receive(int sock, void *eloop_ctx, void *sock_ctx)
{
#define BUFSIZ	1024
	struct conn *c, *conn = NULL;
	struct sockaddr_storage from;
	struct sockaddr_in *sa_in;
	socklen_t addrlen = sizeof(struct sockaddr);
	char tmp[BUFSIZ], *buf = NULL, *pos;
	int len;
	int res, tot = 0, bufpos = 0, i;

	list_for_each_entry(c, &conns, list) {
		if (c->fd == sock) {
			conn = c;
			break;
		}
	}

	if (conn == NULL) {
		return;
	}

	len = BUFSIZ;
	pos = tmp;
	do {
		res = conn->at_read(conn, &from, pos, len, 0);
		if (res > 0) {
			pos += res;
			len -= res;
			tot += res;
			if (len == 0) {
				buf = realloc(buf, tot);
				memcpy(buf + bufpos, tmp, BUFSIZ);
				bufpos += BUFSIZ;
				pos = tmp;
				len = BUFSIZ;
			}
		}
	} while (res > 0);

	if (tot > 0) {
		if (bufpos > 0) {
			/* Get the last data in tmp[], if any. */
			if (tot % BUFSIZ != 0) {
				buf = realloc(buf, tot);
				memcpy(buf + bufpos, tmp, tot % BUFSIZ);
			}
		} else
			buf = tmp;


		struct sockaddr *ai_addr = conn->ai->ai_addr;
		if (conn->ai->ai_family == AF_INET) {

			struct sockaddr_in *in1 = (struct sockaddr_in *)ai_addr;
			struct sockaddr_in *in2 = (struct sockaddr_in *)&from;

			if (memcmp(&in1->sin_addr, &in2->sin_addr, sizeof(struct in_addr)) ||
					in1->sin_port != in2->sin_port)
				return;
		}
#if LWIP_IPV6
		else if (conn->ai->ai_family == AF_INET6) {

			struct sockaddr_in6 *in6_1 = (struct sockaddr_in6 *)ai_addr;
			struct sockaddr_in6 *in6_2 = (struct sockaddr_in6 *)&from;
			if (memcmp(&in6_1->sin6_addr, &in6_2->sin6_addr, sizeof(struct in6_addr)) ||
				in6_1->sin6_port != in6_2->sin6_port)
				return;
		}
#endif

		if (!conn->pass) {
			at_printf("+IPD:%d,%d", conn->id, tot);
			if (ipdinfo) {
				struct aftype *ap = &inet_aftype;

				sa_in = (struct sockaddr_in *)&from;
#if LWIP_IPV6
				if (sa_in->sin_family == AF_INET6)
					ap = &inet6_aftype;
#endif
				at_printf(",%s,%d:", ap->sprint((struct sockaddr *)sa_in, 1), ntohs(sa_in->sin_port));

			} else
				at_printf(":");
		}

		for (i = 0; i < tot; i++)
			at_printf("%c", buf[i]);

		if (conn->type == CONN_UDP && conn->udp_mode != REMOTE_FIXED) {
			if (conn->udp_mode == REMOTE_LAST_RX || !conn->remote_changed) {
				memcpy(conn->ai->ai_addr, &from, sizeof(from));
				conn->ai->ai_addrlen = addrlen;
				conn->remote_changed = 1;
			}
		}
	} else if (res <= 0) {
		/*
		 * It occurs because the peer has disconnected.
		 * This is inside eloop handler where it is safe to unregister directly
		 */

		eloop_unregister_read_sock(conn->fd);
		list_del(&conn->list);
		at_close_conn(conn);
	}

#undef BUFSIZ
}
#endif
#ifdef CONFIG_ATCMD_AT_CIFSR
static int at_cifsr_exec(int argc, char *argv[])
{
	int err = AT_RESULT_CODE_OK;
	ifconfig_handle_t *h = NULL;
	struct sockaddr sta_sa, sta_ha;
	struct sockaddr sap_sa, sap_ha;
	struct aftype *ap = &inet_aftype;
	struct hwtype *hw = &ether_hwtype;

	h = ifconfig_open();
	if (h == NULL)
		return AT_RESULT_CODE_ERROR;

	if (ifconfig_get_addr(h, "wlan0", &sta_sa) < 0)
		err_exit(err, AT_RESULT_CODE_ERROR);

	if (ifconfig_get_hwaddr(h, "wlan0", &sta_ha) < 0)
		err_exit(err, AT_RESULT_CODE_ERROR);

	if (ifconfig_get_addr(h, "wlan1", &sap_sa) < 0)
			err_exit(err, AT_RESULT_CODE_ERROR);

	if (ifconfig_get_hwaddr(h, "wlan1", &sap_ha) < 0)
		err_exit(err, AT_RESULT_CODE_ERROR);

	at_printf("%s:STAIP,\"%s\"\r\n", argv[AT_CMD_NAME], ap->sprint(&sta_sa, 1));
	at_printf("%s:STAMAC,\"%s\"\r\n", argv[AT_CMD_NAME], hw->print((unsigned char *)sta_ha.sa_data));

#ifdef CONFIG_LWIP_IPV6
	struct aftype *ap6 = &inet6_aftype;
	struct sockaddr_in6 addr6;
	ip6_addr_t ip6addr;
	int index;

	for (index = 0; index < LWIP_IPV6_NUM_ADDRESSES; index++) {
		ifconfig_get_ip6addr(h, "wlan0", (void *) &addr6, index);
		if (addr6.sin6_len == 0) {
			continue;
		}

		inet6_addr_to_ip6addr(&ip6addr, &((addr6).sin6_addr));
		if (ip6_addr_islinklocal(&ip6addr))
			at_printf("%s:STAIP6LL %s \n",  argv[AT_CMD_NAME],
				ap6->sprint((struct sockaddr *)&addr6, 1));

		if (ip6_addr_isglobal(&ip6addr) || ip6_addr_isuniquelocal(&ip6addr))
			at_printf("%s:STAIP6GL %s \n",  argv[AT_CMD_NAME],
				ap6->sprint((struct sockaddr *)&addr6, 1));

	}
#endif

	at_printf("%s:APIP,\"%s\"\r\n", argv[AT_CMD_NAME], ap->sprint(&sap_sa, 1));
	at_printf("%s:APMAC,\"%s\"\r\n", argv[AT_CMD_NAME], hw->print((unsigned char *)sap_ha.sa_data));


exit:
	ifconfig_close(h);

	return err;
}

ATPLUS(CIFSR, NULL, NULL, NULL, at_cifsr_exec);
#endif /* CONFIG_ATCMD_AT_CIFSR */

#ifdef CONFIG_ATCMD_AT_CIPSTA
static int at_cipsta_query(int argc, char *argv[])
{
	int err = AT_RESULT_CODE_OK;
	ifconfig_handle_t *h = NULL;
	struct sockaddr sa, gw, nm;
	struct aftype *ap = &inet_aftype;

	h = ifconfig_open();
	if (h == NULL)
		return AT_RESULT_CODE_ERROR;

	if (ifconfig_get_addr(h, "wlan0", &sa) < 0)
		err_exit(err, AT_RESULT_CODE_ERROR);

	if (ifconfig_get_gateway(h, "wlan0", &gw) < 0)
		err_exit(err, AT_RESULT_CODE_ERROR);

	if (ifconfig_get_netmask(h, "wlan0", &nm) < 0)
		err_exit(err, AT_RESULT_CODE_ERROR);

	at_printf("%s:ip,%s\r\n", argv[AT_CMD_NAME], ap->sprint(&sa, 1));
	at_printf("%s:gateway,%s\r\n", argv[AT_CMD_NAME], ap->sprint(&gw, 1));
	at_printf("%s:netmask,%s\r\n", argv[AT_CMD_NAME], ap->sprint(&nm, 1));

#ifdef CONFIG_LWIP_IPV6
	struct aftype *ap6 = &inet6_aftype;
	struct sockaddr_in6 addr6;
	ip6_addr_t ip6addr;
	int index;

	for (index = 0; index < LWIP_IPV6_NUM_ADDRESSES; index++) {
		ifconfig_get_ip6addr(h, "wlan0", (void *) &addr6, index);
		if (addr6.sin6_len == 0) {
			continue;
		}

		inet6_addr_to_ip6addr(&ip6addr, &((addr6).sin6_addr));
		if (ip6_addr_islinklocal(&ip6addr))
			at_printf("%s:ip6ll %s \n",  argv[AT_CMD_NAME],
				ap6->sprint((struct sockaddr *)&addr6, 1));

		if (ip6_addr_isglobal(&ip6addr) || ip6_addr_isuniquelocal(&ip6addr))
			at_printf("%s:ip6gl %s \n",  argv[AT_CMD_NAME],
				ap6->sprint((struct sockaddr *)&addr6, 1));

	}
#endif

exit:
	ifconfig_close(h);
	return err;
}

static int at_cipsta_set(int argc, char *argv[])
{
	int err = AT_RESULT_CODE_OK;
	char host[64];
	struct sockaddr sa, gw, nm;
	struct aftype *ap = &inet_aftype;
	ifconfig_handle_t *h = NULL;

	h = ifconfig_open();
	if (h == NULL)
		return AT_RESULT_CODE_ERROR;

	safe_strncpy(host, at_strip_args(argv[AT_CMD_PARAM]), (sizeof host));
	if (ap->input(host, &sa) < 0) {
		if (ap->herror)
			ap->herror(host);
		err_exit(err, AT_RESULT_CODE_ERROR);
	}
	if (ifconfig_set_addr(h, "wlan0", sa) < 0)
		err_exit(err, AT_RESULT_CODE_ERROR);

	if (argc > AT_CMD_PARAM + 1) {
		safe_strncpy(host, at_strip_args(argv[AT_CMD_PARAM + 1]), (sizeof host));
		if (ap->input(host, &gw) < 0) {
			if (ap->herror)
				ap->herror(host);
			err_exit(err, AT_RESULT_CODE_ERROR);

		}
		if (ifconfig_set_gateway(h, "wlan0", gw) < 0)
			err_exit(err, AT_RESULT_CODE_ERROR);
	}

	if (argc > AT_CMD_PARAM + 2) {
		safe_strncpy(host, at_strip_args(argv[AT_CMD_PARAM + 2]), (sizeof host));
		if (ap->input(host, &nm) < 0) {
			if (ap->herror)
				ap->herror(host);
			err_exit(err, AT_RESULT_CODE_ERROR);
		}

		if (ifconfig_set_netmask(h, "wlan0", nm) < 0)
			err_exit(err, AT_RESULT_CODE_ERROR);
	}

exit:
	ifconfig_close(h);

	return err;
}

ATPLUS(CIPSTA, NULL, at_cipsta_query, at_cipsta_set, NULL);
#endif /* CONFIG_ATCMD_AT_CIPSTA */

#ifdef CONFIG_ATCMD_AT_CIPSTAMAC
static int at_cipstamac_query(int argc, char *argv[])
{
	int err = AT_RESULT_CODE_OK;
	ifconfig_handle_t *h = NULL;
	struct sockaddr ha;
	struct hwtype *hw = &ether_hwtype;

	h = ifconfig_open();
	if (h == NULL)
		return AT_RESULT_CODE_ERROR;

	if (ifconfig_get_hwaddr(h, "wlan0", &ha) < 0)
		err_exit(err, AT_RESULT_CODE_ERROR);

	at_printf("%s:%s\r\n", argv[AT_CMD_NAME], hw->print((unsigned char *)ha.sa_data));

exit:
	ifconfig_close(h);

	return err;
}

static int at_cipstamac_set(int argc, char *argv[])
{
	int err = AT_RESULT_CODE_OK;
	char host[64];
	struct sockaddr ha;
	struct hwtype *hw = &ether_hwtype;
	ifconfig_handle_t *h = NULL;

	h = ifconfig_open();
	if (h == NULL)
		return AT_RESULT_CODE_ERROR;

	safe_strncpy(host, at_strip_args(argv[AT_CMD_PARAM]), (sizeof host));
	if (hw->input(host, &ha) < 0)
		err_exit(err, AT_RESULT_CODE_ERROR);

	if (ifconfig_set_hwaddr(h, "wlan0", ha) < 0)
		err_exit(err, AT_RESULT_CODE_ERROR);

exit:
	ifconfig_close(h);

	return err;
}

ATPLUS(CIPSTAMAC, NULL, at_cipstamac_query, at_cipstamac_set, NULL);
#endif /* CONFIG_ATCMD_AT_CIPSTAMAC */

#ifdef CONFIG_ATCMD_AT_CIPAP

static int at_cipsap_query(int argc, char *argv[])
{
	int err = AT_RESULT_CODE_OK;
	ifconfig_handle_t *h = NULL;
	struct sockaddr sa, gw, nm;
	struct aftype *ap = &inet_aftype;

	h = ifconfig_open();
	if (h == NULL)
		return AT_RESULT_CODE_ERROR;

	if (ifconfig_get_addr(h, "wlan1", &sa) < 0)
		err_exit(err, AT_RESULT_CODE_ERROR);

	if (ifconfig_get_gateway(h, "wlan1", &gw) < 0)
		err_exit(err, AT_RESULT_CODE_ERROR);

	if (ifconfig_get_netmask(h, "wlan1", &nm) < 0)
		err_exit(err, AT_RESULT_CODE_ERROR);

	at_printf("%s:ip,%s\r\n", argv[AT_CMD_NAME], ap->sprint(&sa, 1));
	at_printf("%s:gateway,%s\r\n", argv[AT_CMD_NAME], ap->sprint(&gw, 1));
	at_printf("%s:netmask,%s\r\n", argv[AT_CMD_NAME], ap->sprint(&nm, 1));

exit:
	ifconfig_close(h);
	return err;
}

static int at_cipsap_set(int argc, char *argv[])
{
	int err = AT_RESULT_CODE_OK;
	char host[64];
	struct sockaddr sa, gw, nm;
	struct aftype *ap = &inet_aftype;
	ifconfig_handle_t *h = NULL;

	h = ifconfig_open();
	if (h == NULL)
		return AT_RESULT_CODE_ERROR;

	safe_strncpy(host, at_strip_args(argv[AT_CMD_PARAM]), (sizeof host));
	if (ap->input(host, &sa) < 0) {
		if (ap->herror)
			ap->herror(host);
		err_exit(err, AT_RESULT_CODE_ERROR);
	}

	if (ifconfig_set_addr(h, "wlan1", sa) < 0)
		err_exit(err, AT_RESULT_CODE_ERROR);

	if (argc > AT_CMD_PARAM + 1) {
		safe_strncpy(host, at_strip_args(argv[AT_CMD_PARAM + 1]), (sizeof host));
		if (ap->input(host, &gw) < 0) {
			if (ap->herror)
				ap->herror(host);
			err_exit(err, AT_RESULT_CODE_ERROR);

		}
		if (ifconfig_set_gateway(h, "wlan1", gw) < 0)
			err_exit(err, AT_RESULT_CODE_ERROR);
	}

	if (argc > AT_CMD_PARAM + 2) {
		safe_strncpy(host, at_strip_args(argv[AT_CMD_PARAM + 2]), (sizeof host));
		if (ap->input(host, &nm) < 0) {
			if (ap->herror)
				ap->herror(host);
			err_exit(err, AT_RESULT_CODE_ERROR);
		}
		if (ifconfig_set_netmask(h, "wlan1", nm) < 0)
			err_exit(err, AT_RESULT_CODE_ERROR);
	}

exit:
	ifconfig_close(h);

	return err;
}

ATPLUS(CIPAP, NULL, at_cipsap_query, at_cipsap_set, NULL);
#endif /* CONFIG_ATCMD_AT_CIPAP */

#ifdef CONFIG_ATCMD_AT_CIPAPMAC

static int at_cipsapmac_query(int argc, char *argv[])
{
	int err = AT_RESULT_CODE_OK;
	ifconfig_handle_t *h = NULL;
	struct sockaddr ha;
	struct hwtype *hw = &ether_hwtype;

	h = ifconfig_open();
	if (h == NULL)
		return AT_RESULT_CODE_ERROR;

	if (ifconfig_get_hwaddr(h, "wlan1", &ha) < 0)
		err_exit(err, AT_RESULT_CODE_ERROR);

	at_printf("%s:%s\r\n", argv[AT_CMD_NAME], hw->print((unsigned char *)ha.sa_data));

exit:
	ifconfig_close(h);

	return err;
}

static int at_cipsapmac_set(int argc, char *argv[])
{
	int err = AT_RESULT_CODE_OK;
	char host[64];
	struct sockaddr ha;
	struct hwtype *hw = &ether_hwtype;
	ifconfig_handle_t *h = NULL;

	h = ifconfig_open();
	if (h == NULL)
		return AT_RESULT_CODE_ERROR;

	safe_strncpy(host, at_strip_args(argv[AT_CMD_PARAM]), (sizeof host));
	if (hw->input(host, &ha) < 0)
		err_exit(err, AT_RESULT_CODE_ERROR);

	if (ifconfig_set_hwaddr(h, "wlan1", ha) < 0)
		err_exit(err, AT_RESULT_CODE_ERROR);

exit:
	ifconfig_close(h);

	return err;
}


ATPLUS(CIPAPMAC, NULL, at_cipsapmac_query, at_cipsapmac_set, NULL);
#endif /* CONFIG_ATCMD_AT_CIPSTAMAC */

#ifdef CONFIG_ATCMD_AT_PING
/** ping identifier - must fit on a u16_t */
#ifndef PING_ID
#define PING_ID        0xAFAF
#endif

static void ping_prepare_echo( struct icmp_echo_hdr *iecho, u16_t len, u16 seqno, bool ipv6)
{
	size_t i;
	size_t data_len = len - sizeof(struct icmp_echo_hdr);
#if LWIP_IPV6
	int icmp_type_req = (ipv6 == true) ? ICMP6_TYPE_EREQ : ICMP_ECHO;
#else
	int icmp_type_req = ICMP_ECHO;
#endif

	ICMPH_TYPE_SET(iecho, icmp_type_req);
	ICMPH_CODE_SET(iecho, 0);
	iecho->chksum = 0;
	iecho->id     = PING_ID;
	iecho->seqno  = lwip_htons(seqno);

	/* fill the additional data buffer with some data */
	for(i = 0; i < data_len; i++) {
		((char*)iecho)[sizeof(struct icmp_echo_hdr) + i] = (char)i;
	}

	if (!ipv6)
		iecho->chksum = inet_chksum(iecho, len);
}

static int at_ping_set(int argc, char *argv[])
{
	int err = AT_RESULT_CODE_OK;
	struct sockaddr_storage from;
	struct addrinfo *ai = NULL;
	struct timeval timeout;
	struct icmp_echo_hdr *iecho = NULL;
	char *hostname, *buf = NULL;
	int interval = 1000;
	int size = sizeof(*iecho) + 56;
	int sock = -1, ret;
	int len, fromlen = sizeof(from);
	struct ip_hdr *iphdr;
	struct icmp_echo_hdr *echo_reply;
	unsigned t, rtt;
	ip_addr_t addr __maybe_unused;
	int family = AF_INET, proto = IPPROTO_ICMP;;
	bool is_ipv6 = false;
	int hdr_offset = 0, echo_type_resp = 0;

	hostname = at_strip_args(argv[AT_CMD_PARAM]);

	ai = at_build_remote_ai(hostname, -1);

#if LWIP_IPV6

	if (ai->ai_family == AF_INET6) {
		family = AF_INET6;
		proto = IP6_NEXTH_ICMP6;
		is_ipv6 = true;
	}

#endif

	iecho = malloc(size);
	if (iecho == NULL)
		err_exit(err, AT_RESULT_CODE_ERROR);

	buf = malloc(size);
	if (buf == NULL)
		err_exit(err, AT_RESULT_CODE_ERROR);

	sock = socket(family, SOCK_RAW, proto);
	if (sock < 0)
		err_exit(err, AT_RESULT_CODE_ERROR);

	timeout.tv_sec = interval / 1000;
	timeout.tv_usec = (interval % 1000) * 1000;

	ret = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
	if (ret < 0)
		err_exit(err, AT_RESULT_CODE_ERROR);

	t = hal_timer_value();
	ping_prepare_echo(iecho, (u16_t) size, 0, is_ipv6);
	ret = sendto(sock, iecho, size, 0, (void *) ai->ai_addr, ai->ai_addrlen);
	if (ret < 0)
		err_exit(err, AT_RESULT_CODE_ERROR);

wait_reply:
	len = recvfrom(sock, buf, size, 0, (struct sockaddr *) &from,
			(socklen_t *) &fromlen);
	if (len < 0 || len < sizeof(struct ip_hdr) + sizeof(struct icmp_echo_hdr)) {
		at_printf("%s:TIMEOUT\r\n", argv[AT_CMD_NAME]);
		err_exit(err, AT_RESULT_CODE_ERROR);
	}

	iphdr = (struct ip_hdr *)buf;
#if LWIP_IPV6
	if (is_ipv6) {
		hdr_offset = IP6_HLEN;
		echo_type_resp = ICMP6_TYPE_EREP;
	} else
#endif
	{
		iphdr = (struct ip_hdr *)buf;
		hdr_offset = IPH_HL(iphdr) * 4;
		echo_type_resp = ICMP_ER;
	}
	echo_reply = (struct icmp_echo_hdr *)(buf + hdr_offset);
	if (ICMPH_TYPE(echo_reply) != echo_type_resp)
		goto wait_reply;

	rtt = tick_to_us(hal_timer_value() - t); /* microsecond */
	at_printf("%s:%dms\r\n", argv[AT_CMD_NAME], rtt/1000);
exit:
	if (sock > 0)
		close(sock);
	if (ai)
		freeaddrinfo(ai);
	if (iecho)
		free(iecho);
	if (sock)
		close(sock);
	return err;
}

ATPLUS(PING, NULL, NULL, at_ping_set, NULL);
#endif /* CONFIG_ATCMD_AT_PING */

#ifdef CONFIG_ATCMD_AT_CIPSTART

static int at_tcp_send(struct conn * conn, const char *buffer, int len, int timeout_ms)
{
	esp_transport_handle_t t = (conn->type == CONN_TCP ? conn->tcp : conn->ssl);

	if (buffer && len)
		return esp_transport_write(t, buffer, len, timeout_ms);
	return -1;
}

static int at_tcp_recv(struct conn * conn, struct sockaddr_storage *from,
		char *buffer, int len, int timeout_ms)
{
	esp_transport_handle_t t = (conn->type == CONN_TCP ? conn->tcp : conn->ssl);
	memcpy(from, conn->ai->ai_addr, conn->ai->ai_addrlen);
	return esp_transport_read(t, buffer, len, 0);;
}

static int at_udp_recv(struct conn * conn, struct sockaddr_storage *from,
		char *buffer, int len, int timeout_ms)
{
	socklen_t addrlen = sizeof(struct sockaddr_storage);

	return recvfrom(conn->fd, buffer, len, MSG_DONTWAIT, (struct sockaddr *) from, &addrlen);
}


static int at_udp_send(struct conn * conn, const char *buffer, int len, int timeout_ms)
{
	struct sockaddr *sa = conn->ai->ai_addr;
	socklen_t sa_len = conn->ai->ai_addrlen;

	if (buffer && len)
		return sendto(conn->fd, buffer, len, 0, sa, sa_len);

	return -1;
}

static int at_tcp_connect(struct conn *conn, int id, char *r_host,
							uint16_t r_port, esp_transport_keep_alive_t *keep_alive) {
	int err = AT_RESULT_CODE_OK;
	esp_transport_list_handle_t tlist;
	esp_transport_handle_t tcp = NULL;
	char t_name[20];

	tlist = esp_transport_list_init();

	if (tlist) {

		tcp = esp_transport_tcp_init();
		if (tcp) {
			sprintf(t_name, "attcp-%d", id);
			esp_transport_list_add(tlist, tcp, t_name);
			esp_transport_tcp_set_keep_alive(tcp, keep_alive);
		}
		else
			err_exit(err, AT_RESULT_CODE_ERROR);
	}
	else {
		err_exit(err, AT_RESULT_CODE_ERROR);
	}

	err = esp_transport_connect(tcp, r_host, r_port, 0);
	if (!err) {
		conn->id = id;
		conn->tlist = tlist;
		conn->tcp = tcp;
		conn->ssl = NULL;
		conn->fd = esp_transport_get_socket(tcp);
		conn->at_write = at_tcp_send;
		conn->at_read = at_tcp_recv;
	}
exit:
	if (err != AT_RESULT_CODE_OK) {
		if (tcp)
			esp_transport_close(tcp);
		if (tlist)
			esp_transport_list_destroy(tlist);
	}
	return err;

}

static int at_tcp_ssl_connect(struct conn *conn, int id, char *r_host,
							uint16_t r_port, esp_transport_keep_alive_t *keep_alive) {
	int err = AT_RESULT_CODE_OK;
	esp_transport_list_handle_t tlist;
	esp_transport_handle_t tcp = NULL;
	esp_transport_handle_t ssl = NULL;
	char t_name[20];

	tlist = esp_transport_list_init();

	if (tlist) {

		tcp = esp_transport_tcp_init();
		if (tcp) {
			sprintf(t_name, "attcp-%d", id);
			esp_transport_list_add(tlist, tcp, t_name);
		}
		else
			err_exit(err, AT_RESULT_CODE_ERROR);

		ssl = esp_transport_ssl_init();
		if (ssl) {
			sprintf(t_name, "atssl-%d", id);
			esp_transport_list_add(tlist, ssl, t_name);
			esp_transport_ssl_set_keep_alive(ssl, keep_alive);
		}
		else
			err_exit(err, AT_RESULT_CODE_ERROR);
	}
	else {
		err_exit(err, AT_RESULT_CODE_ERROR);
	}

	err = esp_transport_connect(ssl, r_host, r_port, 0);
	if (!err) {
		conn->id = id;
		conn->tlist = tlist;
		conn->tcp = tcp;
		conn->ssl = ssl;
		conn->fd = esp_transport_get_socket(ssl);
		conn->at_write = at_tcp_send;
		conn->at_read = at_tcp_recv;
	}
exit:
	if (err != AT_RESULT_CODE_OK) {
		if (tcp)
			esp_transport_close(tcp);
		if (ssl)
			esp_transport_close(ssl);
		if (tlist)
			esp_transport_list_destroy(tlist);
	}

	return err;

}

static int at_udp_connect(struct conn *conn, int id, char *r_host,
	uint16_t r_port, uint16_t l_port, udp_tx_remote udpmode) {
	int err = AT_RESULT_CODE_OK;
	struct sockaddr_storage sa;
	socklen_t sa_len;
	struct sockaddr_in *sa_in;
	struct addrinfo *ai = conn->ai;

	if (ai == NULL)
		err_exit(err, AT_RESULT_CODE_ERROR);

	if ((conn->fd = socket(ai->ai_family,  SOCK_DGRAM, 0)) < 0)
		err_exit(err, AT_RESULT_CODE_ERROR);

#if 0
	printf("local port:%d \n", l_port);
#endif
	memset(&sa, 0, sizeof(sa));
	if (ai->ai_family == AF_INET) {

		sa_in = (struct sockaddr_in *)&sa;
		sa_in->sin_family = AF_INET;
		sa_in->sin_addr.s_addr = htonl(INADDR_ANY);
		sa_in->sin_port = htons(l_port);
		sa_len = ai->ai_addrlen;

	}
#ifdef CONFIG_LWIP_IPV6
	else if (ai->ai_family == AF_INET6) {
		const struct in6_addr in6addr_any = IN6ADDR_ANY_INIT;
		struct sockaddr_in6 *sa_in6;

		sa_in6 = (struct sockaddr_in6 *)&sa;
		sa_in6->sin6_family = AF_INET6;
		sa_in6->sin6_addr = in6addr_any;
		sa_in6->sin6_port = htons(l_port);
		sa_len = ai->ai_addrlen;
	}
#endif
	else {
		err_exit(err, AT_RESULT_CODE_ERROR);
	}

	if (bind(conn->fd, (struct sockaddr *) &sa, sa_len) < 0) {
		at_printf("%d,CLOSED\r\n", id);
		err_exit(err, AT_RESULT_CODE_ERROR);
	}
	conn->id = id;
	conn->local_port = l_port;
	conn->udp_mode = udpmode;
	conn->at_write = at_udp_send;
	conn->at_read = at_udp_recv;

exit:

	if ((err != AT_RESULT_CODE_OK) && ai)
		freeaddrinfo(ai);
	return err;

}

/*
 * AT+CIPSTART=<link ID>,<"type">,<"remote host">,<remote port>[,<local port>,<mode>]
 * AT+CIPSTART=1, TCP, 192.168.3.98, 9001
 * AT+CIPSTART=3,"UDP","192.168.3.98",8080,1113,0
 * AT+CIPSTART=4, SSL, 192.168.3.98, 9002
 */

static int at_cipstart_set(int argc, char *argv[])
{
	int err = AT_RESULT_CODE_OK;
	struct conn *c, *conn = NULL;
	int id;
	char *r_host;
	char *protocol;
	conn_type type = CONN_UNKONWN;
	uint16_t r_port;
	uint16_t l_port = 0;
	udp_tx_remote udpmode = REMOTE_FIXED;
	esp_transport_keep_alive_t  keep_alive_cfg = {
		.keep_alive_interval = 5,
		.keep_alive_idle = 4,
		.keep_alive_enable = false,
		.keep_alive_count = 3
    };
	if (g_socket_free_count <=0) {
#if 0
		printf("No available socket\n");
#endif
		err_exit(err, AT_RESULT_CODE_ERROR);
	}

	id = atoi(argv[AT_CMD_PARAM]);

	list_for_each_entry(c, &conns, list) {
		if (c->id == id) {
#if 0
			printf("ALREADY CONNECTED\r\n");
#endif
			err_exit(err, AT_RESULT_CODE_ERROR);
		}
	}

	protocol = at_strip_args(argv[AT_CMD_PARAM + 1]);

	if (!strcmp(protocol, "TCP"))
		type = CONN_TCP;
	else if (!strcmp(protocol, "UDP"))
		type = CONN_UDP;
	else if (!strcmp(protocol, "SSL"))
		type = CONN_SSL;
	else
		err_exit(err, AT_RESULT_CODE_ERROR);

	r_host = at_strip_args(argv[AT_CMD_PARAM + 2]);
	r_port = atoi(argv[AT_CMD_PARAM + 3]);

	if ((type == CONN_TCP || type == CONN_SSL) && argc > 6) {
		keep_alive_cfg.keep_alive_interval = atoi(argv[AT_CMD_PARAM + 4]);
		keep_alive_cfg.keep_alive_enable = true;
	}
	conn = (struct conn *)zalloc(sizeof(*conn));
	g_socket_free_count--;
	conn->type = type;

	if ((conn->ai = at_build_remote_ai(r_host, r_port)) == NULL)
		err_exit(err, AT_RESULT_CODE_ERROR);

	if (type == CONN_TCP)
		err = at_tcp_connect(conn, id, r_host, r_port, &keep_alive_cfg);
	else if (type == CONN_SSL)
		err = at_tcp_ssl_connect(conn, id, r_host, r_port, &keep_alive_cfg);
	else if (type == CONN_UDP) {

		if (argc > 6)
			l_port = atoi(argv[AT_CMD_PARAM + 4]);
		if (argc > 7)
			udpmode = (udp_tx_remote)atoi(argv[AT_CMD_PARAM + 5]);
		err = at_udp_connect(conn, id, r_host, r_port, l_port, udpmode);
	}

	if (err != AT_RESULT_CODE_OK) {
#if 0
		printf("link id: %d, CONNECT Fail\r\n", conn->id);
#endif
		err_exit(err, AT_RESULT_CODE_ERROR);
	}
	assert(conn->fd > 0);

	if (register_socket(conn->fd , at_tcpip_receive) < 0)
		err_exit(err, AT_RESULT_CODE_ERROR);

	list_add_tail(&conn->list, &conns);

	at_printf("link id: %d, CONNECT\r\n", conn->id);

exit:

	if (err != AT_RESULT_CODE_OK) {
		if (conn) {
			at_close_conn(conn);
		}
	}

	return err;
}

ATPLUS(CIPSTART, NULL, NULL, at_cipstart_set, NULL);
#endif /* CONFIG_ATCMD_AT_CIPSTART */

#ifdef CONFIG_ATCMD_AT_CIPCLOSE
static int at_cipclose_set(int argc, char *argv[])
{
	int err = AT_RESULT_CODE_OK;
	struct conn *c, *conn = NULL;
	int id = atoi(argv[AT_CMD_PARAM]);

	list_for_each_entry(c, &conns, list) {
		if (c->id == id) {
			conn = c;
			break;
		}
	}

	if (conn == NULL || conn->fd <= 0) {
		at_printf("UNLINK\r\n");
		err_exit(err, AT_RESULT_CODE_ERROR);
	}

	if (unregister_socket(conn->fd) < 0)
		err_exit(err, AT_RESULT_CODE_ERROR);

	list_del(&conn->list);
	at_close_conn(conn);
	g_socket_free_count++;

exit:

	return err;
}

ATPLUS(CIPCLOSE, NULL, NULL, at_cipclose_set, NULL);
#endif /* CONFIG_ATCMD_AT_CIPCLOSE */

#ifdef CONFIG_ATCMD_AT_CIPDINFO
static int at_cipdinfo_query(int argc, char *argv[])
{
	at_printf("+CIPDINFO:%s\n", ipdinfo ? "TRUE" : "FALSE");
	return AT_RESULT_CODE_OK;
}

static int at_cipdinfo_set(int argc, char *argv[])
{
	ipdinfo = atoi(argv[AT_CMD_PARAM]);
	return AT_RESULT_CODE_OK;
}
ATPLUS(CIPDINFO, NULL, at_cipdinfo_query, at_cipdinfo_set, NULL);
#endif

#ifdef CONFIG_ATCMD_AT_CIPSEND
/*
 * AT+CIPSEND=<link ID>,<length>
 *
 * Passthrough Mode:
 *
 * AT+CIPSEND=<link ID>
 */
static int at_cipsend_set(int argc, char *argv[])
{
	int err = AT_RESULT_CODE_OK;
	struct conn *c, *conn = NULL, *temp_conn = NULL;
	char *hostname;
	struct addrinfo *ai = NULL;
	char dst[INET_ADDRSTRLEN];
	char portstr[6];
	uint16_t port;
	int id, size = 0;
	char *buf = NULL;
	int b, len = 0;
	int ret = 0;

	if (argc < AT_CMD_PARAM)
		err_exit(err, AT_RESULT_CODE_ERROR);

	id = atoi(argv[AT_CMD_PARAM]);

	list_for_each_entry(c, &conns, list) {
		if (c->id == id) {
			conn = c;
			break;
		}
	}

	if (conn == NULL) {
#if 0
		printf("link is not valid\r\n");
#endif
		err_exit(err, AT_RESULT_CODE_ERROR);
	}

	if (argc > (AT_CMD_PARAM + 1)) {
		size = atoi(argv[AT_CMD_PARAM + 1]);
		conn->pass = false;
	} else {
		conn->pass = true;
	}

	if (size > CONFIG_AT_CIPSEND_MAX) {
#if 0
		printf("too long\r\n");
#endif
		err_exit(err, AT_RESULT_CODE_ERROR);
	}

	if (argc > (AT_CMD_PARAM + 2)) {
		if (conn->type != CONN_UDP)
			err_exit(err, AT_RESULT_CODE_ERROR);

		if (argc < (AT_CMD_PARAM + 4)) {
#if 0
			printf("no port\r\n");
#endif
			err_exit(err, AT_RESULT_CODE_ERROR);
		}

		hostname = at_strip_args(argv[AT_CMD_PARAM + 2]);
		port = atoi(argv[AT_CMD_PARAM + 3]);
		snprintf(portstr, sizeof(portstr), "%d", port);
		ai = resolv_hostname(hostname, portstr, AF_INET, 0, 0);
		if (ai == NULL ||
			inet_ntop(ai->ai_family, xsin_addr(ai->ai_addr), dst, sizeof(dst)) < 0) {
#if 0
			printf("ip error\r\n");
#endif
			err_exit(err, AT_RESULT_CODE_ERROR);
		}

		temp_conn = (struct conn *)zalloc(sizeof(*conn));
		memcpy(temp_conn, conn, sizeof(*temp_conn));
		temp_conn->ai = ai;
	}

	buf = (char *)zalloc(CONFIG_AT_CIPSEND_MAX);
	if (buf == NULL) {
#if 0
		printf("insufficient memory\r\n");
#endif
		err_exit(err, AT_RESULT_CODE_ERROR);
	}

	at_printf(">");

	if (!conn->pass) {
		while (len < size) {
			while ((b = at_getchar()) < 0);
			buf[len++] = (char)b;
		}

		at_printf("\nRecv %d bytes\r\n", len);

		len = conn->at_write(temp_conn ? temp_conn : conn, buf, len, 0);

		if (len != size) {
			at_printf("\r\nSEND FAIL\r\n");
			err_exit(err, AT_RESULT_CODE_ERROR);
		}
		at_printf("\r\nSEND OK\r\n");
	} else {
		do {
			size = CONFIG_AT_CIPSEND_MAX;
			while (len < size) {
				if ((b = at_getchar_timeout(20)) < 0)
					break;
				buf[len++] = (char)b;
			}
			if (len == 3 && !strncmp((const char *)buf, "+++", len))
				break;
			else if (len == 0)
				continue;

			ret = conn->at_write(temp_conn ? temp_conn : conn, buf, len, 0);

			if (ret < len)  { /* NB : attempt to reconnect? */
				at_printf("\r\nSEND FAIL\r\n");
				err_exit(err, AT_RESULT_CODE_ERROR);
			}
			len = 0;
		} while (1);
	}
exit:

	if (buf)
		free(buf);
	if (temp_conn)
		free(temp_conn);
	return err;
}

ATPLUS(CIPSEND, NULL, NULL, at_cipsend_set, NULL);
#endif /* CONFIG_ATCMD_AT_CIPSEND */

#ifdef CONFIG_ATCMD_AT_CIPSTATUS

#define inet_addr_to_ip4addr(target_ipaddr, source_inaddr)	\
	(ip4_addr_set_u32(target_ipaddr, (source_inaddr)->s_addr))
static int at_cipstatus_exec(int argc, char *argv[])
{
	int err = AT_RESULT_CODE_OK;
	int ap = 0, ip = 0, en = 0, stat;
	wise_err_t werr;
	wifi_ap_record_t ap_info;
	ifconfig_handle_t *h = NULL;
	struct sockaddr sa;
	struct sockaddr_in *sa_in;
	ip4_addr_t ipaddr;
	struct conn *c;
	uint8_t wlan_if = atoi(argv[AT_CMD_IFC]);

	werr = wise_wifi_sta_get_ap_info(&ap_info, wlan_if);
	if (werr == WISE_OK)
		ap = 1;

	h = ifconfig_open();
	if (h == NULL)
		return AT_RESULT_CODE_ERROR;

	if (ifconfig_get_addr(h, "wlan0", &sa) < 0)
		err_exit(err, AT_RESULT_CODE_ERROR);

	sa_in = (struct sockaddr_in *)&sa;
	inet_addr_to_ip4addr(&ipaddr, (const struct in_addr *)&sa_in->sin_addr);
	if (!ip4_addr_isany_val(ipaddr))
		ip = 1;

	en = list_empty(&conns) ? 0 : 1;

	if (ap && ip) {
		if (en)
			stat = CONN_ACTIVE;
		else
			stat = CONN_IP_ACQUIRED;
	} else
		stat = CONN_NOT_READY;

	at_printf("STATUS:%d\n", stat);


	if (stat == CONN_ACTIVE) {
		list_for_each_entry(c, &conns, list) {
			struct sockaddr_storage sa;
			socklen_t addrlen = sizeof(sa);
			char addr_str[INET6_ADDRSTRLEN];

			getsockname(c->fd, (struct sockaddr *)&sa, &addrlen);

			/* check IPv4 or IPv6 or DNS */
			if (c->ai->ai_canonname) {
				snprintf(addr_str, sizeof(addr_str), "%s", c->ai->ai_canonname);
			} else {
				/* IPV4 case */
				if (c->ai->ai_family == AF_INET) {
					struct sockaddr_in *ipv4 = (struct sockaddr_in *)c->ai->ai_addr;
					inet_ntop(AF_INET, &(ipv4->sin_addr), addr_str, sizeof(addr_str));
				}
#ifdef LWIP_IPV6
				/* IPV6 case */
				else if (c->ai->ai_family == AF_INET6) {
					struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)c->ai->ai_addr;
					inet_ntop(AF_INET6, &(ipv6->sin6_addr), addr_str, sizeof(addr_str));
				}
#endif
			}


			at_printf("+CIPSTATUS:%d,\"%s\",\"%s\",%d,%d,%d\n",
				c->id,
				(c->type == CONN_TCP || c->type == CONN_SSL) ? (c->type == CONN_TCP ? "TCP" : "SSL") : "UDP",
				addr_str,
				ntohs(((struct sockaddr_in *)c->ai->ai_addr)->sin_port),
				ntohs(((struct sockaddr_in *)&sa)->sin_port),
				c->server);
		}
	}

exit:
	ifconfig_close(h);

	return err;
}

ATPLUS(CIPSTATUS, NULL, NULL, NULL, at_cipstatus_exec);
#endif /* CONFIG_ATCMD_AT_CIPSTATUS */
