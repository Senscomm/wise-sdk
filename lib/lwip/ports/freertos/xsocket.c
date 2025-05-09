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

/*
 * xsocket.c - yet another socket implementation for WISE
 */

#include <stdarg.h>
#include <string.h>
#include <hal/kernel.h>
#include <sys/types.h>

#include "lwip/opt.h"
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <if_dl.h>
#include <poll.h>

#include <vfs.h>

#include "lwip/priv/sockets_priv.h"
#include "lwip/api.h"
#include "lwip/ip_addr.h"
#include "lwip/igmp.h"
#include "lwip/tcp.h"
#include "lwip/raw.h"
#include "lwip/udp.h"
#include "lwip/memp.h"
#include "lwip/pbuf.h"
#include "lwip/netif.h"
#include "lwip/netifapi.h"
#include "lwip/priv/tcpip_priv.h"
#include "lwip/mld6.h"
#if LWIP_CHECKSUM_ON_COPY
#include "lwip/inet_chksum.h"
#endif

#ifdef DEBUG_SOCK
#define dbg(...) printk(__VA_ARGS__)
#else
#define dbg(...)
#endif

/**
 * struct sock - socket data structure
 *
 * @file: VFS file object
 * @domain: socket family (e.g., AF_INET)
 * @type: socket type (e.g., SOCK_STREAM, SOCK_DGRAM, SOCK_RAW)
 * @proto: protocol specified in socket creation
 * @conn: pointer to low-level struct netconn object
 * @sem[]: semaphores signalled on TX(0), RX(1) , and error(2), respectively.
 * @tx_would_block: 1 if sendXX() or write() will block
 * @errors: 1 if there has been an error in socket operation
 * @lastdata: points to the lastly received pbuf or netbuf
 *
 */
struct sock {
	struct file file;
	int domain;
	int type;
	int proto;
	struct netconn *conn;
	sys_sem_t sem[3]; /* 0: TX, 1: RX, 2: Err */
	int tx_would_block;
	int errors;
	union {
		struct netbuf *netbuf;
		struct pbuf *pbuf;
	} lastdata;
};

static struct fops socket_fops;

/*
 * Helper functions
 */

#define file_to_sock(file) container_of(file, struct sock, file)
#define netconn_group(conn) NETCONNTYPE_GROUP(netconn_type(conn))

#if 0 && LWIP_IPV6_MLD
/** Register a new MLD6 membership. On socket close, the membership is dropped automatically.
 *
 * ATTENTION: this function is called from tcpip_thread (or under CORE_LOCK).
 *
 * @return 1 on success, 0 on failure
 */
static int
lwip_socket_register_mld6_membership(int s, unsigned int if_idx, const ip6_addr_t *multi_addr)
{
  struct lwip_sock *sock = get_socket(s);
  int i;

  if (!sock) {
    return 0;
  }

  for (i = 0; i < LWIP_SOCKET_MAX_MEMBERSHIPS; i++) {
    if (socket_ipv6_multicast_memberships[i].sock == NULL) {
      socket_ipv6_multicast_memberships[i].sock   = sock;
      socket_ipv6_multicast_memberships[i].if_idx = (u8_t)if_idx;
      ip6_addr_copy(socket_ipv6_multicast_memberships[i].multi_addr, *multi_addr);
      done_socket(sock);
      return 1;
    }
  }
  done_socket(sock);
  return 0;
}

/** Unregister a previously registered MLD6 membership. This prevents dropping the membership
 * on socket close.
 *
 * ATTENTION: this function is called from tcpip_thread (or under CORE_LOCK).
 */
static void
lwip_socket_unregister_mld6_membership(int s, unsigned int if_idx, const ip6_addr_t *multi_addr)
{
  struct lwip_sock *sock = get_socket(s);
  int i;

  if (!sock) {
    return;
  }

  for (i = 0; i < LWIP_SOCKET_MAX_MEMBERSHIPS; i++) {
    if ((socket_ipv6_multicast_memberships[i].sock   == sock) &&
        (socket_ipv6_multicast_memberships[i].if_idx == if_idx) &&
        ip6_addr_cmp(&socket_ipv6_multicast_memberships[i].multi_addr, multi_addr)) {
      socket_ipv6_multicast_memberships[i].sock   = NULL;
      socket_ipv6_multicast_memberships[i].if_idx = NETIF_NO_INDEX;
      ip6_addr_set_zero(&socket_ipv6_multicast_memberships[i].multi_addr);
      break;
    }
  }
  done_socket(sock);
}

/** Drop all MLD6 memberships of a socket that were not dropped explicitly via setsockopt.
 *
 * ATTENTION: this function is NOT called from tcpip_thread (or under CORE_LOCK).
 */
static void
lwip_socket_drop_registered_mld6_memberships(int s)
{
  struct lwip_sock *sock = get_socket(s);
  int i;

  if (!sock) {
    return;
  }

  for (i = 0; i < LWIP_SOCKET_MAX_MEMBERSHIPS; i++) {
    if (socket_ipv6_multicast_memberships[i].sock == sock) {
      ip_addr_t multi_addr;
      u8_t if_idx;

      ip_addr_copy_from_ip6(multi_addr, socket_ipv6_multicast_memberships[i].multi_addr);
      if_idx = socket_ipv6_multicast_memberships[i].if_idx;

      socket_ipv6_multicast_memberships[i].sock   = NULL;
      socket_ipv6_multicast_memberships[i].if_idx = NETIF_NO_INDEX;
      ip6_addr_set_zero(&socket_ipv6_multicast_memberships[i].multi_addr);

      netconn_join_leave_group_netif(sock->conn, &multi_addr, if_idx, NETCONN_LEAVE);
    }
  }
  done_socket(sock);
}
#endif /* LWIP_IPV6_MLD */



/**
 * sock_alloc() - allocate and initialize a socket object
 * @conn: the underlying low-level netconn object
 */
static struct sock *sock_alloc(struct netconn *conn)
{
	int i;
	struct sock *sock;
	struct file *file;

	/* Allocate a socket object and initialize it */
	if ((sock = malloc(sizeof(*sock))) == NULL)
		return NULL;

	memset(sock, 0, sizeof(*sock));
	sock->conn = conn;
	for (i = 0; i < 3; i++) {
		if (sys_sem_new(&sock->sem[i], 0)) {
			while (--i >= 0)
				sys_sem_free(&sock->sem[i]);
			free(sock);
			return NULL;
		}
	}

	conn->private = (void *) sock;

	file = &sock->file;
	vfs_init_file(file);
	file->f_flags = O_RDWR;
	file->f_type = FTYPE_SOCKET;
	file->f_fops = &socket_fops;

	return sock;
}

/**
 * sock_free() - free a struct sock object
 * @sock: socket to free
 */
static void sock_free(struct sock* sock)
{
	int i;

	for (i = 0; i < 3; i++)
		sys_sem_free(&sock->sem[i]);

	sock->conn = NULL;
	if (sock->lastdata.pbuf) {
		if (sock->type == SOCK_STREAM)
			pbuf_free(sock->lastdata.pbuf);
		else
			netbuf_delete(sock->lastdata.netbuf);
	}
	vfs_destroy_file(&sock->file);
	free(sock);
}

static void socket_get(struct sock *sock)
{
	file_get(&sock->file);
}

static void socket_put(struct sock *sock)
{
	file_put(&sock->file);
}

/**
 * fd_to_socket() - obtain the struct sock object from file descriptor number
 * @sockfd: file descriptor of a socket
 */
static struct sock *fd_to_socket(int sockfd)
{
	struct file *file;
	struct sock *sock;

	if ((file = vfs_fd_to_file(sockfd)) == NULL)
		return NULL;

	sock = container_of(file, struct sock, file);
	return sock;
}

struct endpoint {
	ip_addr_t addr;
	uint16_t port;
};

/*
 * NB: LWIP ip_addr_t has different definition depending on
 *     (LWIP_IPV4, LWIP_IPV6) definitions as shown below:
 *    - (1, 0) : ip_addr_t == ip4_addr_t
 *    - (0, 1) : ip_addr_t == ip6_addr_t
 *    - (1, 1) : ip_addr_t is declared as a union of ip4_addr_t and ip6_addr_t
 *               and additional type field
 */

#if LWIP_IPV4 == 1 && LWIP_IPV6 == 1
#define set_addr_type(paddr, t) do { (paddr)->type = (t);	} while (0)
#else
#define set_addr_type(paddr, t)
#endif

/**
 * sockaddr_to_endpoint() - convert from struct sockaddr to struct endpoint
 * @ep: IP address (v4 or v6) and port
 * @sa: points to the buffer for struct sockaddr object
 */
static
void sockaddr_to_endpoint(struct endpoint *ep, const struct sockaddr *sa)
{
#if LWIP_IPV6 == 1
	if (sa->sa_family == AF_INET6) {
		struct sockaddr_in6 *sin6 = (void *) sa;
		ip6_addr_t *ip6addr = ip_2_ip6(&ep->addr);

		memset(ip6addr, 0, sizeof(ip6_addr_t));
		memcpy(ip6addr->addr, sin6->sin6_addr.s6_addr,
		       sizeof(sin6->sin6_addr.s6_addr));
		if (ip6_addr_has_scope(ip6addr, IP6_UNKNOWN))
			ip6_addr_set_zone(ip6addr, sin6->sin6_scope_id);
		ep->addr.type = IPADDR_TYPE_V6;
		ep->port = ntohs(sin6->sin6_port);
	} else
#endif
	{
		struct sockaddr_in *sin = (void *) sa;
		ip4_addr_t *ip4addr = (void *)ip_2_ip4(&ep->addr);

		ip4addr->addr = sin->sin_addr.s_addr;
		ep->port = ntohs(sin->sin_port);
		set_addr_type(&ep->addr, IPADDR_TYPE_V4);
	}
}

/**
 * endpoint_to_sockaddr() - from ip/port to sockaddr
 *
 */
static void endpoint_to_sockaddr(struct endpoint *ep, struct sockaddr *sa,
				 socklen_t *salen)
{
	struct sockaddr_storage ss = {0, };

#if LWIP_IPV6 == 1
	if ((IP_IS_ANY_TYPE_VAL(ep->addr) || IP_IS_V6_VAL(ep->addr))) {
		struct sockaddr_in6 *sin6 = (void *) &ss;
		ip6_addr_t *ip6addr = ip_2_ip6(&ep->addr);

		sin6->sin6_len = sizeof(*sin6);
		sin6->sin6_family = AF_INET6;
		sin6->sin6_port = htons(ep->port);
		inet6_addr_from_ip6addr(&sin6->sin6_addr, ip6addr);
		sin6->sin6_scope_id = ip6_addr_zone(ip6addr);
	} else
#endif
	{
		struct sockaddr_in *sin = (void *) &ss;
		ip4_addr_t *ip4addr = (void *) ip_2_ip4(&ep->addr);

		sin->sin_len = sizeof(*sin);
		sin->sin_family = AF_INET;
		sin->sin_port = htons(ep->port);
		sin->sin_addr.s_addr = ip4addr->addr;
		memset(&sin->sin_zero, 0, sizeof(sin->sin_zero));
	}

	memcpy(sa, &ss, min(ss.ss_len, *salen));
	*salen = ss.ss_len;
}

static struct {
	sa_family_t family;
	socklen_t addrlen;
} sockaddr_lens[] = {
	{ AF_INET,  sizeof(struct sockaddr_in) },
	{ AF_INET6, sizeof(struct sockaddr_in6) },
	{ AF_LINK,  sizeof(struct sockaddr_dl) },
};

static
int sockaddr_is_valid(struct sock *sock, const struct sockaddr *addr,
		      socklen_t addrlen, int local)
{
	int i;

	if (local && (addr == NULL || addrlen == 0))
		return 0;

	if (addr->sa_family != sock->domain)
		return 0;

	/* Is addrlen correctly set? */
	for (i = 0; i < ARRAY_SIZE(sockaddr_lens); i++) {
		if (sockaddr_lens[i].family != addr->sa_family)
		    continue;
		if (sockaddr_lens[i].addrlen > addrlen)
			return 0;
		else
			break;
	}
	if (i == ARRAY_SIZE(sockaddr_lens))
		return 0; /* Unknown address family */


	/* FIXME: add more tests */
	return 1;
}

static
void sock_map_ipv4_to_ipv4_mapped_ipv6(struct netconn *conn, ip_addr_t *addr)
{
#if LWIP_IPV4 && LWIP_IPV6
	if (NETCONNTYPE_ISIPV6(netconn_type(conn)) && IP_IS_V4_VAL(*addr)) {
		ip4_2_ipv4_mapped_ipv6(ip_2_ip6(addr), ip_2_ip4(addr));
		IP_SET_TYPE_VAL(*addr, IPADDR_TYPE_V6);
	}
#endif
}

void sock_unmap_ipv4_mapped_ipv6_to_ipv4(ip_addr_t *addr)
{
#if LWIP_IPV4 && LWIP_IPV6
	/* Dual-stack: Unmap IPv4 mapped IPv6 addresses */
	if (IP_IS_V6_VAL(*addr) && ip6_addr_isipv4mappedipv6(ip_2_ip6(addr))) {
		unmap_ipv4_mapped_ipv6(ip_2_ip4(addr), ip_2_ip6(addr));
		IP_SET_TYPE_VAL(*addr, IPADDR_TYPE_V4);
	}
#endif /* LWIP_IPV4 && LWIP_IPV6 */
}



/*
 * Socket operations
 */

#include <freebsd/if_dl.h>

static
int sock_bind_ll(struct sock *sock, const struct sockaddr *addr,
			socklen_t addrlen)
{
	struct sockaddr_dl *ll = (void *) addr;
	int err, retval = 0;

	if (addrlen != sizeof(*ll)
			|| (ll->sdl_family != AF_LINK
				&& ll->sdl_family != AF_IEEE80211)) {
		return -EINVAL;
	}

	if ((err = netconn_bind_if(sock->conn, ll->sdl_index)))
		retval = err_to_errno(err);

	return retval;
}

static
int sock_getaddrname(struct sock *sock, struct sockaddr *addr,
		     socklen_t *addrlen, int local)
{
	int err, retval = 0;
	struct endpoint ep;

	err = netconn_getaddr(sock->conn, &ep.addr, &ep.port, local);
	if (err) {
		retval = -err_to_errno(err);
		goto out;
	}

	sock_map_ipv4_to_ipv4_mapped_ipv6(sock->conn, &ep.addr);

	if (addr && addrlen)
		endpoint_to_sockaddr(&ep, addr, addrlen);

 out:
	return retval;
}

struct _ifreq {
  char ifr_name[IFNAMSIZ]; /* Interface name */
};

#if !LWIP_SOCKET && !LWIP_TCPIP_CORE_LOCKING
/** Maximum optlen used by setsockopt/getsockopt */
#define LWIP_SETGETSOCKOPT_MAXOPTLEN LWIP_MAX(16, sizeof(struct _ifreq))

/** This struct is used to pass data to the set/getsockopt_internal
 * functions running in tcpip_thread context (only a void* is allowed) */
struct lwip_setgetsockopt_data {
	/** socket index for which to change options */
	int s;
	/** level of the option to process */
	int level;
	/** name of the option to process */
	int optname;
	/** set: value to set the option to
	 * get: value of the option is stored here */
#if LWIP_MPU_COMPATIBLE
	u8_t optval[LWIP_SETGETSOCKOPT_MAXOPTLEN];
#else
	union {
		void *p;
		const void *pc;
	} optval;
#endif
	/** size of *optval */
	socklen_t optlen;
	/** if an error occurs, it is temporarily stored here */
	int err;
	/** semaphore to wake up the calling task */
	void* completed_sem;
};
#endif /* !LWIP_TCPIP_CORE_LOCKING */

#define LWIP_SOCKOPT_CHECK_OPTLEN(sock, optlen, opttype) \
	do {						 \
		if ((optlen) < sizeof(opttype)) {	 \
			return -EINVAL;			 \
		}					 \
	} while(0)

#define LWIP_SOCKOPT_CHECK_OPTLEN_CONN(sock, optlen, opttype)		\
	do {								\
		LWIP_SOCKOPT_CHECK_OPTLEN(sock, optlen, opttype);	\
		if ((sock)->conn == NULL) 				\
			return -EINVAL;					\
	} while(0)

#define LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB(sock, optlen, opttype)	\
	do {								\
		LWIP_SOCKOPT_CHECK_OPTLEN(sock, optlen, opttype);	\
		if ((sock)->conn == NULL || (sock)->conn->pcb.tcp == NULL)  \
			return -EINVAL;					\
	} while(0)

#define LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB_TYPE(sock, optlen, opttype, nctype) \
	do {								\
		LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB(sock, optlen, opttype); \
		if (NETCONNTYPE_GROUP(netconn_type((sock)->conn)) != nctype) \
			return -ENOPROTOOPT;				\
	} while(0)

#if LWIP_SO_SNDRCVTIMEO_NONSTANDARD
#define LWIP_SO_SNDRCVTIMEO_OPTTYPE 		int
#define LWIP_SO_SNDRCVTIMEO_SET(optval, val) (*(int *)(optval) = (val))
#define LWIP_SO_SNDRCVTIMEO_GET_MS(optval)   ((long)*(const int*)(optval))
#else
#define LWIP_SO_SNDRCVTIMEO_OPTTYPE 		struct timeval
#define LWIP_SO_SNDRCVTIMEO_SET(optval, val)				\
	do {								\
		struct timeval *tv = (void *) (optval);			\
		u32_t loc = (val);					\
		tv->tv_sec = (long)((loc) / 1000U);			\
		tv->tv_usec = (long)(((loc) % 1000U) * 1000U);		\
	} while(0)

#define LWIP_SO_SNDRCVTIMEO_GET_MS(optval)				\
	({								\
		uint32_t __ms__;					\
		const struct timeval *tv = (const struct timeval *) (optval);	\
		__ms__ = tv->tv_sec * 1000 + tv->tv_usec / 1000;	\
		__ms__;							\
	})
#endif

static int lwip_sockopt_to_ipopt(int optname)
{
	/* Map SO_* values to our internal SOF_* values
	 * We should not rely on #defines in socket.h
	 * being in sync with ip.h.
	 */
	switch (optname) {
	case SO_BROADCAST:
		return SOF_BROADCAST;
	case SO_KEEPALIVE:
		return SOF_KEEPALIVE;
	case SO_REUSEADDR:
		return SOF_REUSEADDR;
	default:
		LWIP_ASSERT("Unknown socket option", 0);
		return 0;
	}
}

static int sock_getsockopt(struct sock *sock, int level, int optname,
			   void *optval, socklen_t *optlen)
{
	int err = 0;

	switch (level) {
		/* Level: SOL_SOCKET */
	case SOL_SOCKET:
		switch (optname) {
#if LWIP_TCP
		case SO_ACCEPTCONN:
			LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB(sock, *optlen, int);
			if (NETCONNTYPE_GROUP(sock->conn->type) != NETCONN_TCP)
				return -ENOPROTOOPT;
			if ((sock->conn->pcb.tcp != NULL) &&
			    (sock->conn->pcb.tcp->state == LISTEN))
				*(int *) optval = 1;
			else
				*(int *) optval = 0;
			break;
#endif /* LWIP_TCP */
			/* The option flags */
		case SO_BROADCAST:
		case SO_KEEPALIVE:
#if SO_REUSE
		case SO_REUSEADDR:
#endif /* SO_REUSE */
			if ((optname == SO_BROADCAST) &&
			    (NETCONNTYPE_GROUP(sock->conn->type) != NETCONN_UDP))
				return -ENOPROTOOPT;
			LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB(sock, *optlen, int);

			optname = lwip_sockopt_to_ipopt(optname);
			*(int *)optval = ip_get_option(sock->conn->pcb.ip, optname);
			break;

		case SO_TYPE:
			LWIP_SOCKOPT_CHECK_OPTLEN_CONN(sock, *optlen, int);

			switch (NETCONNTYPE_GROUP(netconn_type(sock->conn))) {
			case NETCONN_RAW:
				*(int *)optval = SOCK_RAW;
				break;
			case NETCONN_TCP:
				*(int *)optval = SOCK_STREAM;
				break;
			case NETCONN_UDP:
				*(int *)optval = SOCK_DGRAM;
				break;
			default: /* unrecognized socket type */
				*(int *)optval = netconn_type(sock->conn);
			}
			break;

		case SO_ERROR:
			LWIP_SOCKOPT_CHECK_OPTLEN(sock, *optlen, int);
			*(int *)optval = err_to_errno(netconn_err(sock->conn));
			break;

#if LWIP_SO_SNDTIMEO
		case SO_SNDTIMEO:
			LWIP_SOCKOPT_CHECK_OPTLEN_CONN(sock, *optlen,
						       LWIP_SO_SNDRCVTIMEO_OPTTYPE);
			LWIP_SO_SNDRCVTIMEO_SET(optval,
						netconn_get_sendtimeout(sock->conn));
			break;
#endif /* LWIP_SO_SNDTIMEO */
#if LWIP_SO_RCVTIMEO
		case SO_RCVTIMEO:
			LWIP_SOCKOPT_CHECK_OPTLEN_CONN(sock, *optlen,
						       LWIP_SO_SNDRCVTIMEO_OPTTYPE);
			LWIP_SO_SNDRCVTIMEO_SET(optval,
						netconn_get_recvtimeout(sock->conn));
			break;
#endif /* LWIP_SO_RCVTIMEO */
#if LWIP_SO_RCVBUF
		case SO_RCVBUF:
			LWIP_SOCKOPT_CHECK_OPTLEN_CONN(sock, *optlen, int);
			*(int *)optval = netconn_get_recvbufsize(sock->conn);
			break;
#endif /* LWIP_SO_RCVBUF */
#if LWIP_SO_LINGER
		case SO_LINGER: {
			s16_t conn_linger;
			struct linger *linger = (struct linger *)optval;
			LWIP_SOCKOPT_CHECK_OPTLEN_CONN(sock, *optlen, struct linger);
			conn_linger = sock->conn->linger;
			if (conn_linger >= 0) {
				linger->l_onoff = 1;
				linger->l_linger = (int)conn_linger;
			} else {
				linger->l_onoff = 0;
				linger->l_linger = 0;
			}
		}
			break;
#endif /* LWIP_SO_LINGER */
#if LWIP_UDP
		case SO_NO_CHECK:
			LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB_TYPE(sock, *optlen, int,
								NETCONN_UDP);
#if LWIP_UDPLITE
			if (udp_is_flag_set(sock->conn->pcb.udp, UDP_FLAGS_UDPLITE))
				/* this flag is only available for UDP, not for UDP lite */
				return -EAFNOSUPPORT;
#endif /* LWIP_UDPLITE */

			*(int *)optval = udp_is_flag_set(sock->conn->pcb.udp,
							 UDP_FLAGS_NOCHKSUM) ? 1 : 0;
			break;
#endif /* LWIP_UDP*/
		default:
			err = -ENOPROTOOPT;
			break;
		}  /* switch (optname) */
		break;

		/* Level: IPPROTO_IP */
	case IPPROTO_IP:
		switch (optname) {
		case IP_TTL:
			LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB(sock, *optlen, int);
			*(int *)optval = sock->conn->pcb.ip->ttl;
			break;
		case IP_TOS:
			LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB(sock, *optlen, int);
			*(int *)optval = sock->conn->pcb.ip->tos;
			break;
#if LWIP_IPV4 && LWIP_MULTICAST_TX_OPTIONS && LWIP_UDP
		case IP_MULTICAST_TTL:
			LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB(sock, *optlen, u8_t);
			if (NETCONNTYPE_GROUP(netconn_type(sock->conn)) != NETCONN_UDP)
				return -ENOPROTOOPT;
			*(u8_t *)optval = udp_get_multicast_ttl(sock->conn->pcb.udp);
			break;
		case IP_MULTICAST_IF:
			LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB(sock, *optlen, struct in_addr);
			if (NETCONNTYPE_GROUP(netconn_type(sock->conn)) != NETCONN_UDP)
				return -ENOPROTOOPT;
			inet_addr_from_ip4addr((struct in_addr *)optval,
					       udp_get_multicast_netif_addr(sock->conn->pcb.udp));
			break;
		case IP_MULTICAST_LOOP:
			LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB(sock, *optlen, u8_t);
			if ((sock->conn->pcb.udp->flags & UDP_FLAGS_MULTICAST_LOOP) != 0) {
				*(u8_t *)optval = 1;
			} else {
				*(u8_t *)optval = 0;
			}
			break;
#endif /* LWIP_IPV4 && LWIP_MULTICAST_TX_OPTIONS && LWIP_UDP */
		default:
			err = -ENOPROTOOPT;
			break;
		}  /* switch (optname) */
		break;

#if LWIP_TCP
		/* Level: IPPROTO_TCP */
	case IPPROTO_TCP:
		/* Special case: all IPPROTO_TCP option take an int */
		LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB_TYPE(sock, *optlen, int, NETCONN_TCP);
		if (sock->conn->pcb.tcp->state == LISTEN)
			return -EINVAL;

		switch (optname) {
		case TCP_NODELAY:
			*(int *)optval = tcp_nagle_disabled(sock->conn->pcb.tcp);
			break;
#if 0 /* FIXME: check if this is standard */
		case TCP_KEEPALIVE:
			*(int *)optval = (int)sock->conn->pcb.tcp->keep_idle;
			break;
#endif
#if LWIP_TCP_KEEPALIVE
		case TCP_KEEPIDLE:
			*(int *)optval = (int)(sock->conn->pcb.tcp->keep_idle / 1000);
			break;
		case TCP_KEEPINTVL:
			*(int *)optval = (int)(sock->conn->pcb.tcp->keep_intvl / 1000);
			break;
		case TCP_KEEPCNT:
			*(int *)optval = (int)sock->conn->pcb.tcp->keep_cnt;
			break;
#endif /* LWIP_TCP_KEEPALIVE */
		default:
			err = -ENOPROTOOPT;
			break;
		}  /* switch (optname) */
		break;
#endif /* LWIP_TCP */

#if LWIP_IPV6
		/* Level: IPPROTO_IPV6 */
	case IPPROTO_IPV6:
		switch (optname) {
		case IPV6_V6ONLY:
			LWIP_SOCKOPT_CHECK_OPTLEN_CONN(sock, *optlen, int);
			*(int *)optval = (netconn_get_ipv6only(sock->conn) ? 1 : 0);
			break;
		default:
			err = -ENOPROTOOPT;
			break;
		}  /* switch (optname) */
		break;
#endif /* LWIP_IPV6 */

#if LWIP_UDP && LWIP_UDPLITE
		/* Level: IPPROTO_UDPLITE */
	case IPPROTO_UDPLITE:
		/* Special case: all IPPROTO_UDPLITE option take an int */
		LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB(sock, *optlen, int);
		/* If this is no UDP lite socket, ignore any options. */
		if (!NETCONNTYPE_ISUDPLITE(netconn_type(sock->conn)))
			return -ENOPROTOOPT;

		switch (optname) {
		case UDPLITE_SEND_CSCOV:
			*(int *)optval = sock->conn->pcb.udp->chksum_len_tx;
			break;
		case UDPLITE_RECV_CSCOV:
			*(int *)optval = sock->conn->pcb.udp->chksum_len_rx;
			break;
		default:
			err = -ENOPROTOOPT;
			break;
		}  /* switch (optname) */
		break;
#endif /* LWIP_UDP */
		/* Level: IPPROTO_RAW */
	case IPPROTO_RAW:
		switch (optname) {
#if LWIP_IPV6 && LWIP_RAW
		case IPV6_CHECKSUM:
			LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB_TYPE(sock, *optlen, int,
								NETCONN_RAW);
			if (sock->conn->pcb.raw->chksum_reqd == 0) {
				*(int *)optval = -1;
			} else {
				*(int *)optval = sock->conn->pcb.raw->chksum_offset;
			}
			break;
#endif /* LWIP_IPV6 && LWIP_RAW */
		default:
			err = -ENOPROTOOPT;
			break;
		}  /* switch (optname) */
		break;
	default:
		err = -ENOPROTOOPT;
		break;
	} /* switch (level) */

	return err;
}

#if !LWIP_TCPIP_CORE_LOCKING
static void sock_getsockopt_callback(void *arg)
{
	struct lwip_setgetsockopt_data *data = arg;
	struct sock *sock = fd_to_socket(data->s);

	data->err = sock_getsockopt(sock, data->level, data->optname,
#if LWIP_MP_COMPATIBLE
				    data->optval,
#else
				    data->optval.p,
#endif
				    &data->optlen);

	sys_sem_signal((sys_sem_t *) data->completed_sem);
}
#endif /* !LWIP_TCPIP_CORE_LOCKING */

static int sock_setsockopt(struct sock *sock, int level, int optname,
			   const void *optval, socklen_t optlen)
{
	int err = 0;

	switch (level) {

		/* Level: SOL_SOCKET */
	case SOL_SOCKET:
		switch (optname) {

			/* SO_ACCEPTCONN is get-only */

			/* The option flags */
		case SO_BROADCAST:
		case SO_KEEPALIVE:
#if SO_REUSE
		case SO_REUSEADDR:
#endif /* SO_REUSE */
			if ((optname == SO_BROADCAST) &&
			    (NETCONNTYPE_GROUP(sock->conn->type) != NETCONN_UDP))
				return -ENOPROTOOPT;

			optname = lwip_sockopt_to_ipopt(optname);

			LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB(sock, optlen, int);
			if (*(const int *) optval) {
				ip_set_option(sock->conn->pcb.ip, optname);
			} else {
				ip_reset_option(sock->conn->pcb.ip, optname);
			}
			break;

			/* SO_TYPE is get-only */
			/* SO_ERROR is get-only */

#if LWIP_SO_SNDTIMEO
		case SO_SNDTIMEO: {
			long ms_long;
			LWIP_SOCKOPT_CHECK_OPTLEN_CONN(sock, optlen, LWIP_SO_SNDRCVTIMEO_OPTTYPE);
			ms_long = LWIP_SO_SNDRCVTIMEO_GET_MS(optval);
			if (ms_long < 0)
				return -EINVAL;
			netconn_set_sendtimeout(sock->conn, ms_long);
			break;
		}
#endif /* LWIP_SO_SNDTIMEO */
#if LWIP_SO_RCVTIMEO
		case SO_RCVTIMEO: {
			long ms_long;
			LWIP_SOCKOPT_CHECK_OPTLEN_CONN(sock, optlen, LWIP_SO_SNDRCVTIMEO_OPTTYPE);
			ms_long = LWIP_SO_SNDRCVTIMEO_GET_MS(optval);
			if (ms_long < 0)
				return -EINVAL;
			netconn_set_recvtimeout(sock->conn, (u32_t)ms_long);
			break;
		}
#endif /* LWIP_SO_RCVTIMEO */
#if LWIP_SO_RCVBUF
		case SO_RCVBUF:
			LWIP_SOCKOPT_CHECK_OPTLEN_CONN(sock, optlen, int);
			netconn_set_recvbufsize(sock->conn, *(const int *)optval);
			break;
#endif /* LWIP_SO_RCVBUF */
#if LWIP_SO_LINGER
		case SO_LINGER: {
			const struct linger *linger = (const struct linger *)optval;
			LWIP_SOCKOPT_CHECK_OPTLEN_CONN(sock, optlen, struct linger);
			if (linger->l_onoff) {
				int lingersec = linger->l_linger;
				if (lingersec < 0)
					return -EINVAL;
				if (lingersec > 0xFFFF) {
					lingersec = 0xFFFF;
				}
				sock->conn->linger = (s16_t)lingersec;
			} else {
				sock->conn->linger = -1;
			}
		}
			break;
#endif /* LWIP_SO_LINGER */
#if LWIP_UDP
		case SO_NO_CHECK:
			LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB_TYPE(sock, optlen, int, NETCONN_UDP);
#if LWIP_UDPLITE
			if (udp_is_flag_set(sock->conn->pcb.udp, UDP_FLAGS_UDPLITE))
				/* this flag is only available for UDP, not for UDP lite */
				return -EAFNOSUPPORT;
#endif /* LWIP_UDPLITE */
			if (*(const int *)optval) {
				udp_set_flags(sock->conn->pcb.udp, UDP_FLAGS_NOCHKSUM);
			} else {
				udp_clear_flags(sock->conn->pcb.udp, UDP_FLAGS_NOCHKSUM);
			}
			break;
#endif /* LWIP_UDP */
		case SO_BINDTODEVICE: {
			const struct _ifreq *iface;
			err_t err;
			uint8_t if_idx;

			LWIP_SOCKOPT_CHECK_OPTLEN_CONN(sock, optlen, struct _ifreq);

			iface = (const struct _ifreq *)optval;
			if (iface->ifr_name[0] != 0) {
				err = netifapi_netif_name_to_index(iface->ifr_name, &if_idx);
				if (err != ERR_OK)
					return -ENODEV;
			}

			err = netconn_bind_if(sock->conn, if_idx);
			if (err != ERR_OK)
				return -EINVAL;
		}
			break;
		default:
			err = -ENOPROTOOPT;
			break;
		}  /* switch (optname) */
		break;

		/* Level: IPPROTO_IP */
	case IPPROTO_IP:
		switch (optname) {
		case IP_TTL:
			LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB(sock, optlen, int);
			sock->conn->pcb.ip->ttl = (u8_t)(*(const int *)optval);
			break;
		case IP_TOS:
			LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB(sock, optlen, int);
			sock->conn->pcb.ip->tos = (u8_t)(*(const int *)optval);
			break;
#if LWIP_NETBUF_RECVINFO
		case IP_PKTINFO:
			LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB_TYPE(sock, optlen, int, NETCONN_UDP);
			if (*(const int *)optval) {
				sock->conn->flags |= NETCONN_FLAG_PKTINFO;
			} else {
				sock->conn->flags &= ~NETCONN_FLAG_PKTINFO;
			}
			break;
#endif /* LWIP_NETBUF_RECVINFO */
#if LWIP_IPV4 && LWIP_MULTICAST_TX_OPTIONS && LWIP_UDP
		case IP_MULTICAST_TTL:
			LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB_TYPE(sock, optlen, u8_t, NETCONN_UDP);
			udp_set_multicast_ttl(sock->conn->pcb.udp, (u8_t)(*(const u8_t *)optval));
			break;
		case IP_MULTICAST_IF: {
			ip4_addr_t if_addr;
			LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB_TYPE(sock, optlen, struct in_addr, NETCONN_UDP);
			inet_addr_to_ip4addr(&if_addr, (const struct in_addr *)optval);
			udp_set_multicast_netif_addr(sock->conn->pcb.udp, &if_addr);
		}
			break;
		case IP_MULTICAST_LOOP:
			LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB_TYPE(sock, optlen, u8_t, NETCONN_UDP);
			if (*(const u8_t *)optval) {
				udp_set_flags(sock->conn->pcb.udp, UDP_FLAGS_MULTICAST_LOOP);
			} else {
				udp_clear_flags(sock->conn->pcb.udp, UDP_FLAGS_MULTICAST_LOOP);
			}
			break;
#endif /* LWIP_IPV4 && LWIP_MULTICAST_TX_OPTIONS && LWIP_UDP */
#if LWIP_IGMP
		case IP_ADD_MEMBERSHIP:
		case IP_DROP_MEMBERSHIP: {
			/* If this is a TCP or a RAW socket, ignore these options. */
			err_t igmp_err;
			const struct ip_mreq *imr = (const struct ip_mreq *)optval;
			ip4_addr_t if_addr;
			ip4_addr_t multi_addr;
			LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB_TYPE(sock, optlen, struct ip_mreq,
								NETCONN_UDP);
			inet_addr_to_ip4addr(&if_addr, &imr->imr_interface);
			inet_addr_to_ip4addr(&multi_addr, &imr->imr_multiaddr);
			if (optname == IP_ADD_MEMBERSHIP) {
#if LWIP_SOCKET
				if (!lwip_socket_register_membership(s, &if_addr, &multi_addr)) {
					/* cannot track membership (out of memory) */
					err = -ENOMEM;
					igmp_err = ERR_OK;
				} else {
					igmp_err = igmp_joingroup(&if_addr, &multi_addr);
				}
#else
				igmp_err = igmp_joingroup(&if_addr, &multi_addr);
#endif
			} else {
				igmp_err = igmp_leavegroup(&if_addr, &multi_addr);
#if LWIP_SOCKET
				lwip_socket_unregister_membership(s, &if_addr, &multi_addr);
#endif
			}
			if (igmp_err != ERR_OK) {
				err = -EADDRNOTAVAIL;
			}
		}
			break;
#endif /* LWIP_IGMP */
		default:
			err = -ENOPROTOOPT;
			break;
		}  /* switch (optname) */
		break;

#if LWIP_TCP
		/* Level: IPPROTO_TCP */
	case IPPROTO_TCP:
		/* Special case: all IPPROTO_TCP option take an int */
		LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB_TYPE(sock, optlen, int, NETCONN_TCP);
		if (sock->conn->pcb.tcp->state == LISTEN)
			return -EINVAL;

		switch (optname) {
		case TCP_NODELAY:
			if (*(const int *)optval) {
				tcp_nagle_disable(sock->conn->pcb.tcp);
			} else {
				tcp_nagle_enable(sock->conn->pcb.tcp);
			}
			break;
#if 0 /* FIXME: check if this is standard */
		case TCP_KEEPALIVE:
			sock->conn->pcb.tcp->keep_idle = (u32_t)(*(const int *)optval);
			break;
#endif
#if LWIP_TCP_KEEPALIVE
		case TCP_KEEPIDLE:
			sock->conn->pcb.tcp->keep_idle = 1000 * (u32_t)(*(const int *)optval);
			break;
		case TCP_KEEPINTVL:
			sock->conn->pcb.tcp->keep_intvl = 1000 * (u32_t)(*(const int *)optval);
			break;
		case TCP_KEEPCNT:
			sock->conn->pcb.tcp->keep_cnt = (u32_t)(*(const int *)optval);
			break;
#endif /* LWIP_TCP_KEEPALIVE */
		default:
			err = -ENOPROTOOPT;
			break;
		}  /* switch (optname) */
		break;
#endif /* LWIP_TCP*/

#if LWIP_IPV6
		/* Level: IPPROTO_IPV6 */
	case IPPROTO_IPV6:
		switch (optname) {
		case IPV6_V6ONLY:
			LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB(sock, optlen, int);
			if (*(const int *)optval) {
				netconn_set_ipv6only(sock->conn, 1);
			} else {
				netconn_set_ipv6only(sock->conn, 0);
			}
			break;
#if LWIP_IPV6_MLD
		case IPV6_JOIN_GROUP:
		case IPV6_LEAVE_GROUP: {
			/* If this is a TCP or a RAW socket, ignore these options. */
			err_t mld6_err;
			struct netif *netif;
			ip6_addr_t multi_addr;
			const struct ipv6_mreq *imr = (const struct ipv6_mreq *)optval;

			LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB_TYPE(sock, optlen, struct ipv6_mreq, NETCONN_UDP);
			inet6_addr_to_ip6addr(&multi_addr, &imr->ipv6mr_multiaddr);
			LWIP_ASSERT("Invalid netif index", imr->ipv6mr_interface <= 0xFFu);
			netif = netif_get_by_index((u8_t)imr->ipv6mr_interface);
			if (netif == NULL) {
				err = -EADDRNOTAVAIL;
				break;
			}

			if (optname == IPV6_JOIN_GROUP) {
#if LWIP_SOCKET
				if (!lwip_socket_register_mld6_membership(s, imr->ipv6mr_interface, &multi_addr)) {
					/* cannot track membership (out of memory) */
					err = -ENOMEM;
					mld6_err = ERR_OK;
				} else {
					mld6_err = mld6_joingroup_netif(netif, &multi_addr);
				}
#else
				mld6_err = mld6_joingroup_netif(netif, &multi_addr);
#endif
			} else {
				mld6_err = mld6_leavegroup_netif(netif, &multi_addr);
#if LWIP_SOCKET
				lwip_socket_unregister_mld6_membership(s, imr->ipv6mr_interface, &multi_addr);
#endif
			}
			if (mld6_err != ERR_OK) {
				err = -EADDRNOTAVAIL;
			}
		}
			break;
#endif /* LWIP_IPV6_MLD */
		default:
			err = -ENOPROTOOPT;
			break;
		}  /* switch (optname) */
		break;
#endif /* LWIP_IPV6 */

#if LWIP_UDP && LWIP_UDPLITE
		/* Level: IPPROTO_UDPLITE */
	case IPPROTO_UDPLITE:
		/* Special case: all IPPROTO_UDPLITE option take an int */
		LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB(sock, optlen, int);
		/* If this is no UDP lite socket, ignore any options. */
		if (!NETCONNTYPE_ISUDPLITE(netconn_type(sock->conn)))
			return -ENOPROTOOPT;

		switch (optname) {
		case UDPLITE_SEND_CSCOV:
			if ((*(const int *)optval != 0) && ((*(const int *)optval < 8) || (*(const int *)optval > 0xffff))) {
				/* don't allow illegal values! */
				sock->conn->pcb.udp->chksum_len_tx = 8;
			} else {
				sock->conn->pcb.udp->chksum_len_tx = (u16_t) * (const int *)optval;
			}
			break;
		case UDPLITE_RECV_CSCOV:
			if ((*(const int *)optval != 0) && ((*(const int *)optval < 8) || (*(const int *)optval > 0xffff))) {
				/* don't allow illegal values! */
				sock->conn->pcb.udp->chksum_len_rx = 8;
			} else {
				sock->conn->pcb.udp->chksum_len_rx = (u16_t) * (const int *)optval;
			}
			break;
		default:
			err = -ENOPROTOOPT;
			break;
		}  /* switch (optname) */
		break;
#endif /* LWIP_UDP */
		/* Level: IPPROTO_RAW */
	case IPPROTO_RAW:
		switch (optname) {
#if LWIP_IPV6 && LWIP_RAW
		case IPV6_CHECKSUM:
			/* It should not be possible to disable the checksum generation with ICMPv6
			 * as per RFC 3542 chapter 3.1 */
			if (sock->conn->pcb.raw->protocol == IPPROTO_ICMPV6)
				return -EINVAL;

			LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB_TYPE(sock, optlen, int, NETCONN_RAW);
			if (*(const int *)optval < 0) {
				sock->conn->pcb.raw->chksum_reqd = 0;
			} else if (*(const int *)optval & 1) {
				/* Per RFC3542, odd offsets are not allowed */
				return - EINVAL;
			} else {
				sock->conn->pcb.raw->chksum_reqd = 1;
				sock->conn->pcb.raw->chksum_offset = (u16_t) * (const int *)optval;
			}
			break;
#endif /* LWIP_IPV6 && LWIP_RAW */
		default:
			err = -ENOPROTOOPT;
			break;
		}  /* switch (optname) */
		break;
	default:
		err = -ENOPROTOOPT;
		break;
	}  /* switch (level) */

	return err;
}

#if !LWIP_TCPIP_CORE_LOCKING
static void sock_setsockopt_callback(void *arg)
{
	struct lwip_setgetsockopt_data *data = arg;
	struct sock *sock = fd_to_socket(data->s);

	data->err = sock_setsockopt(sock, data->level, data->optname,
#if LWIP_MPU_COMPATIBLE
				    data->optval,
#else /* LWIP_MPU_COMPATIBLE */
				    data->optval.pc,
#endif /* LWIP_MPU_COMPATIBLE */
				    data->optlen);

	sys_sem_signal((sys_sem_t *) data->completed_sem);
}
#endif /* !LWIP_TCPIP_CORE_LOCKING */

/*
 * Receive functions
 */
#define CMSG_SPACE_IN_PKTINFO CMSG_SPACE(sizeof(struct in_pktinfo))

static
ssize_t sock_datagram_recvmsg(struct sock *sock, struct msghdr *msg, int flags)
{
	struct netbuf *buf;
	int i, err = 0;
	uint16_t buflen, rxlen = 0;

	buf = sock->lastdata.netbuf;
	if (buf == NULL) {
		struct netconn *conn = sock->conn;
		uint8_t non_block;

		non_block = (flags & MSG_DONTWAIT) ? NETCONN_DONTBLOCK : 0;
		err = netconn_recv_udp_raw_netbuf_flags(conn, &buf, non_block);
		if (err)
			return -err_to_errno(err);
		sock->lastdata.netbuf = buf;
	}

	/* Copy @buf->p to @msg->msg_iov[] */
	buflen = buf->p->tot_len;
	for (i = 0; i < msg->msg_iovlen && rxlen < buflen; i++) {
		uint16_t len, remainder = buflen - rxlen;

		len = LWIP_MIN(msg->msg_iov[i].iov_len, remainder);
		pbuf_copy_partial(buf->p, msg->msg_iov[i].iov_base, len, rxlen);
		rxlen += len;
		/* FIXME: msg->msg_iov[i].iov_len = len ? */
	}
	if (rxlen < buflen)
		msg->msg_flags |= MSG_TRUNC;
	/*
	 * Return the real length of the packet or datagram even when
	 * it was longer than the passed buffer.
	 */
	if (flags & MSG_TRUNC)
		rxlen = buflen;

	if (msg->msg_name && msg->msg_namelen) {
		struct endpoint peer;
		peer.addr = *netbuf_fromaddr(buf);
		peer.port = netbuf_fromport(buf);
		endpoint_to_sockaddr(&peer, (void *) msg->msg_name,
				     &msg->msg_namelen);
	}

	msg->msg_flags = 0;
	if (msg->msg_control) {
		uint8_t wrote_msg = 0;
#if LWIP_NETBUF_RECVINFO && LWIP_IPV4
		if ((buf->flags & NETBUF_FLAG_DESTADDR) &&
		    IP_IS_V4(&buf->toaddr)) {
			if (msg->msg_controllen > CMSG_SPACE_IN_PKTINFO) {
				struct cmsghdr *chdr = CMSG_FIRSTHDR(msg);
				struct in_pktinfo *pktinfo;

				pktinfo = (struct in_pktinfo *) CMSG_DATA(chdr);
				chdr->cmsg_level = IPPROTO_IP;
				chdr->cmsg_type = IP_PKTINFO;
				chdr->cmsg_len = CMSG_LEN(sizeof(*pktinfo));
				pktinfo->ipi_ifindex = buf->p->if_idx;
				pktinfo->ipi_addr.s_addr =
					ip_2_ip4(netbuf_destaddr(buf))->addr;
				msg->msg_controllen = CMSG_SPACE_IN_PKTINFO;
				wrote_msg = 1;
			} else {
				msg->msg_flags |= MSG_CTRUNC;
			}
		}
#endif /* LWIP_NETBUF_RECVINFO && LWIP_IPV4 */
		if (!wrote_msg)
			msg->msg_controllen = 0;
	}

	if ((flags & MSG_PEEK) == 0) {
		sock->lastdata.netbuf = NULL;
		netbuf_delete(buf);
	}
	return (ssize_t) rxlen;
}

#if LWIP_TCP
ssize_t sock_tcp_recv(struct sock *sock, void *buf, size_t len, int flags)
{
	struct netconn *conn = sock->conn;
	struct pbuf *p;
	ssize_t rxlen = 0; /* received bytes */
	int err, copylen;
	uint8_t ncflags = NETCONN_NOAUTORCVD;

	ncflags |= (flags & MSG_DONTWAIT) ? NETCONN_DONTBLOCK : 0;

	do {
		if ((p = sock->lastdata.pbuf) == NULL) {
			err = netconn_recv_tcp_pbuf_flags(conn, &p, ncflags);
			if (err)
				goto error;
			sock->lastdata.pbuf = p;
		}
		/* Make subsequent iteration non-blocking */
		ncflags |= NETCONN_DONTBLOCK;

		/* copy @copylen bytes from @p to @buf + @rxlen */
		copylen = LWIP_MIN(len, p->tot_len);
		pbuf_copy_partial(p, buf + rxlen, copylen, 0);
		rxlen += copylen;
		len -= copylen;

		if (flags & MSG_PEEK)
			continue;

		if (p->tot_len > copylen) {
			p = pbuf_free_header(p, copylen);
		} else {
			pbuf_free(p);
			p = NULL;
		}
		sock->lastdata.pbuf = p;
	} while (len > 0 && !(flags & MSG_PEEK));

 done:
	if (rxlen > 0 && !(flags & MSG_PEEK))
		netconn_tcp_recvd(conn, rxlen);
	return rxlen;

 error:
	if (rxlen > 0) {
/* XXX: why do we need this?
 */
#if 0
		if (err == ERR_CLSD && conn->callback) {
			LOCK_TCPIP_CORE();
			conn->callback(conn, NETCONN_EVT_RCVPLUS, 0);
			UNLOCK_TCPIP_CORE();
		}
#endif
		goto done;
	} else if (err == ERR_CLSD) { /* rxlen == 0 */
		return 0;
	}
	return -err_to_errno(err);
}
#endif
static
ssize_t sock_recvfrom(struct sock *sock, void *buf, size_t len, int flags,
		      struct sockaddr *from, socklen_t *addrlen)
{
	ssize_t retval = 0;

	if (sock->type == SOCK_STREAM) {
#if LWIP_TCP
		retval = sock_tcp_recv(sock, buf, len, flags);
#endif
		goto out;
	} else {
		struct iovec vec = {
			.iov_base = buf, .iov_len = len,
		};
		struct msghdr msg = {
			.msg_iov = &vec,
			.msg_iovlen = 1,
			.msg_name = from,
			.msg_namelen = (addrlen == NULL) ? 0 : *addrlen,
		};

		if ((retval = sock_datagram_recvmsg(sock, &msg, flags)) < 0)
			goto out;

		if (addrlen)
			*addrlen = msg.msg_namelen;
	}
 out:
	return retval;
}

static int sock_msg_len(struct msghdr *msg)
{
	int i, buflen = 0;
	struct iovec *vec;

	for (i = 0, vec = msg->msg_iov; i < msg->msg_iovlen; i++, vec++) {
		if (vec->iov_base == NULL ||
		    vec->iov_len <= 0 ||
		    (ssize_t) vec->iov_len < 0 ||
		    (ssize_t) buflen + vec->iov_len < 0)
			return 0;
		buflen += vec->iov_len;
	}
	return buflen;
}

static ssize_t sock_recvmsg(struct sock *sock, struct msghdr *msg, int flags)
{
	int retval = 0;
#if LWIP_TCP
    int i = 0;
	struct iovec *iov;
#endif

	if (sock->type != SOCK_STREAM) {
		/* SOCK_DGRAM or SOCK_RAW */
		retval = sock_datagram_recvmsg(sock, msg, flags);
		goto out;
	}
#if LWIP_TCP

	/* SOCK_STREAM */
	retval = 0;
	for (i = 0, iov = msg->msg_iov; i < msg->msg_iovlen; i++, iov++) {
		ssize_t len;

		len = sock_tcp_recv(sock, iov->iov_base, iov->iov_len, flags);
		retval += (len > 0) ? len: 0;
		if (len < 0 || len < iov->iov_len || (flags & MSG_PEEK)) {
			retval = (retval == 0) ? len : retval;
			break;
		}
		flags |= MSG_DONTWAIT;
	}
#endif
 out:
	return retval;
}

/*
 * Send functions
 */

static
int sock_validate_dest_addr(struct sock *sock, const struct sockaddr *to,
			    socklen_t addrlen)
{
	if (to == NULL && addrlen == 0)
		return 1;

	return sockaddr_is_valid(sock, to, addrlen, 0);
}

static
ssize_t sock_send(struct sock *sock, const void *buf, size_t len, int flags);

static
ssize_t sock_sendto(struct sock *sock, const void *buf, size_t len, int flags,
		    const struct sockaddr *to, socklen_t addrlen)
{
	ssize_t retval = 0;
	err_t err;
	struct netbuf netbuf = {0, };
	struct endpoint remote;

	if (sock->type == SOCK_STREAM) {
		retval = sock_send(sock, buf, len, flags);
		return retval;
	}

	/* SOCK_DGRAM or SOCK_RAW */
	if (len > LWIP_MIN(0xffff, SSIZE_MAX) ||
	    !sock_validate_dest_addr(sock, to, addrlen)) {
		retval = -EINVAL;
		goto out;
	}

    memset(&remote, 0, sizeof(remote));
	if (to) {
		sockaddr_to_endpoint(&remote, to);
	} else {
		ip_addr_set_any(NETCONNTYPE_ISIPV6(netconn_type(sock->conn)),
				&remote.addr);
	}
	netbuf.addr = remote.addr; /* structure copy */
	netbuf_fromport(&netbuf) = remote.port; /* FIMXE: why from not to ? */

#if LWIP_NETIF_TX_SINGLE_PBUF
	if (netbuf_alloc(&netbuf, len) == NULL) {
		retval = -ENOMEM;
		goto out;
	}
#if LWIP_CHECKSUM_ON_COPY
	if (sock->type == SOCK_RAW) {
		uint16_t checksum;
		checksum = LWIP_CHKSUM_COPY(netbuf.p->payload, buf, len);
		netbuf_set_chksum(&netbuf, checksum);
	} else
#endif /* LWIP_CHECKSUM_ON_COPY */
	{
		MEMCPY(netbuf.p->payload, buf, len);
	}
#else /* LWIP_NETIF_TX_SINGLE_PBUF */
	if ((err = netbuf_ref(&netbuf, buf, len)) < 0) {
		retval = -err_to_errno(err);
		goto out;
	}
#endif /* LWIP_NETIF_TX_SINGLE_PBUF */

	sock_unmap_ipv4_mapped_ipv6_to_ipv4(&netbuf.addr);

	err = netconn_send(sock->conn, &netbuf);
	retval = err? -err_to_errno(err) : len;
	netbuf_free(&netbuf);
 out:
	return retval;
}

static
ssize_t sock_send(struct sock *sock, const void *buf, size_t len, int flags)
{
	ssize_t retval = 0;
	size_t wlen;
	uint8_t wflags = NETCONN_COPY;
	err_t err;

	if (sock->type != SOCK_STREAM)
		return sock_sendto(sock, buf, len, flags, NULL, 0);

	/* SOCK_STREAM */
	wflags |= (flags & MSG_MORE) ? NETCONN_MORE : 0;
	wflags |= (flags & MSG_DONTWAIT) ? NETCONN_DONTBLOCK : 0;
	err = netconn_write_partly(sock->conn, buf, len, wflags, &wlen);
	retval = (err) ? -err_to_errno(err) : (ssize_t) wlen;
	return retval;
}

static
ssize_t sock_sendmsg(struct sock *sock, const struct msghdr *msg, int flags)
{
	struct netbuf buf;
	struct iovec *iov;
	void *ptr __unused;
	ssize_t retval = 0, buflen;
	size_t wlen;
	uint8_t wflags = NETCONN_COPY;
	int i;
	err_t err;

	if (msg == NULL || msg->msg_iov == NULL ||
	    (msg->msg_iovlen <= 0 || msg->msg_iovlen > 0xffff) ||
	    (flags & ~(MSG_DONTWAIT | MSG_MORE))) {
		return -EINVAL;
	}

	if (sock->type == SOCK_STREAM) {
		wflags |= (flags & MSG_MORE) ? NETCONN_MORE : 0;
		wflags |= (flags & MSG_DONTWAIT) ? NETCONN_DONTBLOCK : 0;

		err = netconn_write_vectors_partly(sock->conn,
						   (void *) msg->msg_iov,
						   (uint16_t) msg->msg_iovlen,
						   wflags,
						   &wlen);
		retval = (err) ? -err_to_errno(err) : (ssize_t) wlen;
		return retval;
	}

	/* SOCK_DGRAM or SOCK_RAW */
	memset(&buf, 0, sizeof(struct netbuf));
	if (msg->msg_name) {
		struct endpoint remote;
		sockaddr_to_endpoint(&remote, msg->msg_name);
		buf.addr = remote.addr;
		netbuf_fromport(&buf) = remote.port;
	}

	buflen = sock_msg_len((void *) msg);
	if (buflen == 0 || buflen > 0xffff) {
		retval = -EMSGSIZE;
		goto out;
	}

#if LWIP_NETIF_TX_SINGLE_PBUF
	/* Allocate a new netbuf and copy the data into it */
	if (netbuf_alloc(&buf, (uint16_t) buflen) == NULL) {
		retval = -ENOMEM;
		goto out;
	}
	/* Flatten I/O vector */
	ptr = buf.p->payload;
	for (i = 0, iov = msg->msg_iov; i < msg->msg_iovlen; i++, iov++) {
		MEMCPY(ptr, iov->iov_base, iov->iov_len);
		ptr += iov->iov_len;
	}
	retval = ptr - (void *) buf.p->payload;
#if LWIP_CHECKSUM_ON_COPY
	do {
		uint16_t checksum = ~inet_chksum_pbuf(buf.p);
		netbuf_set_chksum(&buf, checksum);
	} while (0);
#endif /* LWIP_CHECKSUM_ON_COPY */
#else /* LWIP_NETIF_TX_SINGLE_PBUF */

	for (i = 0, iov = msg->msg_iov; i < msg->msg_iovlen; i++, iov++) {
		struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, 0, PBUF_REF);
		if (p == NULL) {
			retval = -ENOMEM;
			goto out;
		}
		p->payload = iov->iov_base;
		p->len = p->tot_len = (uint16_t ) iov->iov_len;

		if (buf.p == NULL) {
			buf.p = buf.ptr = p;
		} else {
			if (buf.p->tot_len + p->len > 0xffff) {
				pbuf_free(p);
				retval = -EMSGSIZE;
				goto out;
			}
			pbuf_cat(buf.p, p);
		}
	}

	retval = netbuf_len(&buf);
#endif /* LWIP_NETIF_TX_SINGLE_PBUF */

#ifdef __WISE__
	if (msg->msg_flags & MSG_CRITICAL) {
		buf.p->flags |= PBUF_FLAG_CRITICAL;
	}
#endif

	sock_unmap_ipv4_mapped_ipv6_to_ipv4(&buf.addr);

	err = netconn_send(sock->conn, &buf);
	if (err)
		retval = -err_to_errno(err);
	netbuf_free(&buf);
 out:
	return retval;
}

/* FIXME; IGMP */

#if 0 && LWIP_IGMP
/** Register a new IGMP membership. On socket close, the membership is dropped automatically.
 *
 * ATTENTION: this function is called from tcpip_thread (or under CORE_LOCK).
 *
 * @return 1 on success, 0 on failure
 */
static int
sock_register_membership(struct sock *sock, const ip4_addr_t *if_addr,
			 const ip4_addr_t *multi_addr)
{
	int i;

	for (i = 0; i < LWIP_SOCKET_MAX_MEMBERSHIPS; i++) {
		if (socket_ipv4_multicast_memberships[i].sock == NULL) {
			socket_ipv4_multicast_memberships[i].sock = sock;
			ip4_addr_copy(socket_ipv4_multicast_memberships[i].if_addr, *if_addr);
			ip4_addr_copy(socket_ipv4_multicast_memberships[i].multi_addr, *multi_addr);
			return 1;
		}
	}
	return 0;
}

/** Unregister a previously registered membership. This prevents dropping the membership
 * on socket close.
 *
 * ATTENTION: this function is called from tcpip_thread (or under CORE_LOCK).
 */
static void
socket_unregister_membership(int s, const ip4_addr_t *if_addr, const ip4_addr_t *multi_addr)
{
  struct lwip_sock *sock = get_socket(s);
  int i;

  if (!sock) {
    return;
  }

  for (i = 0; i < LWIP_SOCKET_MAX_MEMBERSHIPS; i++) {
    if ((socket_ipv4_multicast_memberships[i].sock == sock) &&
        ip4_addr_cmp(&socket_ipv4_multicast_memberships[i].if_addr, if_addr) &&
        ip4_addr_cmp(&socket_ipv4_multicast_memberships[i].multi_addr, multi_addr)) {
      socket_ipv4_multicast_memberships[i].sock = NULL;
      ip4_addr_set_zero(&socket_ipv4_multicast_memberships[i].if_addr);
      ip4_addr_set_zero(&socket_ipv4_multicast_memberships[i].multi_addr);
      break;
    }
  }
  done_socket(sock);
}

/** Drop all memberships of a socket that were not dropped explicitly via setsockopt.
 *
 * ATTENTION: this function is NOT called from tcpip_thread (or under CORE_LOCK).
 */
static void
lwip_socket_drop_registered_memberships(int s)
{
  struct lwip_sock *sock = get_socket(s);
  int i;

  if (!sock) {
    return;
  }

  for (i = 0; i < LWIP_SOCKET_MAX_MEMBERSHIPS; i++) {
    if (socket_ipv4_multicast_memberships[i].sock == sock) {
      ip_addr_t multi_addr, if_addr;
      ip_addr_copy_from_ip4(multi_addr, socket_ipv4_multicast_memberships[i].multi_addr);
      ip_addr_copy_from_ip4(if_addr, socket_ipv4_multicast_memberships[i].if_addr);
      socket_ipv4_multicast_memberships[i].sock = NULL;
      ip4_addr_set_zero(&socket_ipv4_multicast_memberships[i].if_addr);
      ip4_addr_set_zero(&socket_ipv4_multicast_memberships[i].multi_addr);

      netconn_join_leave_group(sock->conn, &multi_addr, &if_addr, NETCONN_LEAVE);
    }
  }
  done_socket(sock);
}
#endif /* LWIP_IGMP */


/*
 * Socket file operations
 */

static int sock_release(struct file *file)
{
	struct sock *sock = file_to_sock(file);
	int err, retval;

	if (sock->conn == NULL)
		assert(sock->lastdata.pbuf == NULL);

#if LWIP_IGMP
#if LWIP_SOCKET
	sock_drop_registered_memberships(sock);
#endif
#endif
#if LWIP_IPV6_MLD
#if LWIP_SOCKET
	sock_drop_registered_mld6_membership(sock);
#endif
#endif

	err = netconn_delete(sock->conn);
	retval = -err_to_errno(err);

	sock_free(sock);
	return retval;
}

static
ssize_t sock_read(struct file *file, void *buf, size_t len, off_t *pos)
{
	struct sock *sock = file_to_sock(file);
	return sock_recvfrom(sock, buf, len, 0, NULL, NULL);
}

static
ssize_t sock_readv(struct file *file, const struct iovec *iov, int iovcnt)
{
	struct sock *sock = file_to_sock(file);
	struct msghdr msg = {
		.msg_iov = (void *) iov,
		.msg_iovlen = iovcnt,
	};

	return sock_recvmsg(sock, &msg, 0);
}

static
ssize_t sock_write(struct file *file, void *buf, size_t count, off_t *pos)
{
	struct sock *sock = file_to_sock(file);
	return sock_send(sock, buf, count, 0);
}

static
ssize_t sock_writev(struct file *file, const struct iovec *iov, int iovcnt)
{
	struct sock *sock = file_to_sock(file);
	struct msghdr msg = {
		.msg_iov = (void *) iov,
		.msg_iovlen = iovcnt,
	};
	return sock_sendmsg(sock, &msg, 0);
}

static struct pbuf *sock_last_pbuf(struct sock *sock)
{
	if (sock->lastdata.netbuf == NULL)
		return NULL;

	return (sock->type != SOCK_STREAM) ?
		sock->lastdata.netbuf->p :
		sock->lastdata.pbuf;
}

/* FIXME: it is not working */
static size_t sock_available_rx_bytes(struct sock *sock)
{
	struct pbuf *pb;
	size_t received = 0;

#if LWIP_SO_RCVBUF
	SYS_ARCH_GET(sock->conn->recv_avail, received);
	if ((ssize_t) received < 0) {
		received = 0;
	}
#endif
	/* Check if there is data left from the last recv operation. /maq 041215 */
	if ((pb = sock_last_pbuf(sock)))
		received += pb->tot_len;

	return received;
}

static int sock_ioctl(struct file *file, unsigned int cmd, void *argp)
{
	struct sock *sock = file_to_sock(file);
	int retval = 0;
	int val = 0;
	err_t err;

#define result ((int *) argp)

	switch (cmd) {
#if LWIP_SO_RCVBUF || LWIP_FIONREAD_LINUXMODE
	case FIONREAD:
		if (!argp) {
			retval = -EINVAL;
			goto out;
		}
#if LWIP_FIONREAD_LINUXMODE
		if (sock->type != SOCK_STREAM) {
			struct netconn *conn = sock->conn;
			struct netbuf *nb = sock->lastdata.netbuf;
			int flags = NETCONN_DONTBLOCK;

			if (nb ==  NULL) {
				err = netconn_recv_udp_raw_netbuf_flags(conn,
									&nb,
									flags);
				if (err) {
					retval = -err_to_errno(err);
					*result = 0;
					goto out;
				}
			}
			*result = nb->p->tot_len;
			goto out;
		}
#endif /* LWIP_FIONREAD_LINUXMODE */

#if LWIP_SO_RCVBUF
		/*
		 * We come here if either LWIP_FIONREAD_LINUXMODE==0 or this is
		 * a TCP socket
		 */
		*result = sock_available_rx_bytes(sock);
		goto out;
#else /* LWIP_SO_RCVBUF */
		break;
#endif /* LWIP_SO_RCVBUF */
#endif /* LWIP_SO_RCVBUF || LWIP_FIONREAD_LINUXMODE */

	case (long)FIONBIO:
		val = (argp && *(int *)argp) ? 1 : 0;
		netconn_set_nonblocking(sock->conn, val);
		goto out;

	default:
#if LWIP_NETIF_API
		err = netifapi_netif_ioctl(cmd, argp);
		retval = -err_to_errno(err);
		goto out;
#endif
		break;
	}
 out:
	return retval;
}

int sock_fcntl(struct file *file, int cmd, va_list ap)
{
	struct sock *sock = file_to_sock(file);
	int val, retval = 0;

	switch (cmd) {
	case F_GETFL:
		retval = netconn_is_nonblocking(sock->conn) ? O_NONBLOCK : 0;

		if (sock->type == SOCK_STREAM) {
#if LWIP_TCPIP_CORE_LOCKING
			LOCK_TCPIP_CORE();
#else
			SYS_ARCH_DECL_PROTECT(lev);
			/* the proper thing to do here would be to get into the tcpip_thread,
			   but locking should be OK as well since we only *read* some flags */
			SYS_ARCH_PROTECT(lev);
#endif
#if LWIP_TCP
			if (sock->conn->pcb.tcp) {
				int flags = 0;

				if (!(sock->conn->pcb.tcp->flags & TF_RXCLOSED))
					flags |= O_RDONLY;
				if (!(sock->conn->pcb.tcp->flags & TF_FIN))
					flags |= O_WRONLY;
				if (flags == (O_RDONLY | O_WRONLY))
					retval |= O_RDWR;
				else
					retval |= flags;
			}
#endif
#if LWIP_TCPIP_CORE_LOCKING
			UNLOCK_TCPIP_CORE();
#else
			SYS_ARCH_UNPROTECT(lev);
#endif
		} else {
			retval |= O_RDWR;
		}
		break;
	case F_SETFL:
		val = va_arg(ap, int);
		val &= ~O_ACCMODE;
		if ((val & ~O_NONBLOCK) == 0) {
			/* only O_NONBLOCK, all other bits are zero */
			netconn_set_nonblocking(sock->conn, val & O_NONBLOCK);
			retval = 0;
		} else {
			retval = -ENOSYS;
		}
		break;
	default:
		return -ENOSYS;

	}

	return retval;
}

static
void socket_callback(struct netconn *conn, enum netconn_evt event,
		     u16_t len)
{
	struct sock *sock;

	if (conn == NULL)
		return;
	if ((sock = conn->private) == NULL)
		return;

	/*
	 * This does not look good, but works. Until I have a better
	 * way, let's go this way.
	 */
	switch (event) {
	case NETCONN_EVT_RCVPLUS:
	case NETCONN_EVT_RCVMINUS:
		break;
	case NETCONN_EVT_SENDPLUS:
		sock->tx_would_block = 0;
		sys_sem_signal(&sock->sem[0]);
		break;
	case NETCONN_EVT_SENDMINUS:
		sock->tx_would_block = 1;
		osSemaphoreAcquire(sock->sem[0].sem, 0);
		break;
	case NETCONN_EVT_ERROR:
		sock->errors++;
		osSemaphoreAcquire(sock->sem[0].sem, 2);
		break;
	default:
		assert(0);
		break;
	}
}

static int sys_mbox_is_pending(sys_mbox_t *mbox)
{
	int ret = 0;

	if (sys_mbox_valid(mbox))
	    ret = osSemaphoreGetCount(mbox->mbx);

	return ret;
}

static
unsigned sock_poll(struct file *file, struct poll_table *pt, struct pollfd *pfd)
{
	struct sock *sock = file_to_sock(file);
	unsigned mask = 0;
	bool connected = true;

	if (pfd->events & POLLOUT)
		poll_add_wait(file, sock->sem[0].sem, pt);

	if (pfd->events & POLLIN) {
#if LWIP_TCP
		if (sys_mbox_valid(&sock->conn->acceptmbox))
			poll_add_wait(file, sock->conn->acceptmbox.mbx, pt);
#endif
		if (sys_mbox_valid(&sock->conn->recvmbox))
			poll_add_wait(file, sock->conn->recvmbox.mbx, pt);
	}

	poll_add_wait(file, sock->sem[2].sem, pt);

	mask |= sock->errors ? POLLERR : 0;
	mask |= sock_available_rx_bytes(sock) ? POLLIN : 0;
#if LWIP_TCP
	mask |= sys_mbox_is_pending(&sock->conn->acceptmbox) ? POLLIN : 0;
#endif
	mask |= sys_mbox_is_pending(&sock->conn->recvmbox) ? POLLIN : 0;
	/*
	 * For a TCP socket, conn->state is in NETCONN_CONNECT while it is
	 * trying to be, but not yet connected.
	 * Otherwise, conn->state will always be NETCONN_NONE, which can be
	 * regarded as 'connected' in terms of poll, especially POLLOUT.
	 */
	connected = sock->conn->state != NETCONN_CONNECT;
	mask |= connected && sock->tx_would_block == 0 ? POLLOUT : 0;
	return mask;
}

struct netconn *sock_get_netconn(int sockfd)
{
	struct sock *sock = fd_to_socket(sockfd);

	return sock->conn;
}

static struct fops socket_fops = {
	.release  = sock_release,
	.read     = sock_read,
	.write    = sock_write,
	.poll     = sock_poll,
	.ioctl    = sock_ioctl,
	.fcntl    = sock_fcntl,
	.readv    = sock_readv,
	.writev   = sock_writev,
};

/*
 * Socket library functions implementation
 */

#define file_is_socket(file) ((file)->f_type == FTYPE_SOCKET)

/* Socket to lwip netconn type mapping */
static int sock_get_netconn_type(int domain, int type, int proto)
{
	static struct {
		int domain;
		int type;
		int proto;
		int netconn_type;
	} sock_netconn_map[] = {
		{AF_INET,       SOCK_STREAM, -1,              NETCONN_TCP},
		{AF_INET,       SOCK_DGRAM,  IPPROTO_UDPLITE, NETCONN_UDPLITE},
		{AF_INET,       SOCK_DGRAM,  -1,              NETCONN_UDP},
		{AF_INET,       SOCK_RAW,    -1,              NETCONN_RAW},
		/* Link-layer */
		{AF_LINK,       SOCK_RAW,    -1,              NETCONN_RAW_LINK},
        /* Raw IEEE80211 */
		{AF_IEEE80211,  SOCK_RAW,    -1,              NETCONN_RAW_IEEE80211},
        /* Routing */
		{AF_ROUTE, 		SOCK_RAW,    -1,              NETCONN_RAWRT},
#if LWIP_IPV6
		{AF_INET6, 		SOCK_STREAM, -1,              NETCONN_TCP_IPV6},
		{AF_INET6, 		SOCK_DGRAM,  -1,     NETCONN_UDP_IPV6},
		{AF_INET6, 		SOCK_DGRAM,  IPPROTO_UDPLITE, NETCONN_UDPLITE_IPV6},
		{AF_INET6, 		SOCK_RAW,    -1,              NETCONN_RAW_IPV6},
#endif
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(sock_netconn_map); i++) {
		if (sock_netconn_map[i].domain == domain &&
		    sock_netconn_map[i].type == type &&
		    (sock_netconn_map[i].proto == -1 || /* do not check proto */
		     sock_netconn_map[i].proto == proto))
			return sock_netconn_map[i].netconn_type;
	}
	return NETCONN_INVALID;
}

static int
_os_socket(int domain, int type, int proto);

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(os_socket, &os_socket, &_os_socket);
#else
__func_tab__ int
(*os_socket)(int domain, int type, int proto) = _os_socket;
#endif

/**
 * socket() - create an endpoint for communication
 */

static int _os_socket(int domain, int type, int proto)
{
	struct sock *sock = NULL;
    struct file *file;
	struct netconn *conn = NULL;
	int fd = -1, ret, nctype;

	/* Create a netconn */
	nctype = sock_get_netconn_type(domain, type, proto);
	if (nctype == NETCONN_INVALID) {
		ret = -EINVAL;
		goto error;
	}

	conn = netconn_new_with_proto_and_callback(nctype,
						   type == SOCK_RAW ? proto : 0,
						   socket_callback);
	if (!conn) {
		ret = -ENOMEM;
		goto error;
	}
#if LWIP_NETBUF_RECVINFO
	if (type == SOCK_DGRAM)
		conn->flags &= ~NETCONN_FLAG_PKTINFO;
#endif

	/* Allocate a free file descriptor number and a socket */
	if ((fd = vfs_get_free_fd()) < 0) {
		ret = fd;
		goto error;
	}
	if ((sock = sock_alloc(conn)) == NULL) {
		ret = -ENOMEM;
		goto error;
	}
	conn->sock.domain = sock->domain = domain;
	conn->sock.type = sock->type = type;
	conn->sock.protocol = sock->proto = proto; /* host byteorder */

    file = &sock->file;
    atomic_inc(&file->f_refcnt);
	vfs_install_fd(fd, file);
	return fd;

 error:
	if (conn)
		netconn_delete(conn);
	if (sock)
		sock_free(sock);
	if (fd >= 0)
		vfs_free_fd(fd);
	if (ret < 0) {
		errno = -ret;
		ret = -1;
	}

	return ret;
}

/**
 * bind() - bind a name to a socket
 */
int os_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
	struct sock *sock = NULL;
	struct endpoint local;
	int retval = 0, err;

	if ((sock = fd_to_socket(sockfd)) == NULL) {
		retval = -EBADF;
		goto out;
	}
	if (!file_is_socket(&sock->file)) {
		retval = -ENOTSOCK;
		goto out;
	}
	if (!sockaddr_is_valid(sock, addr, addrlen, 1)) {
		retval = -EINVAL;
		goto out;
	}

	socket_get(sock);

	if (sock->domain == AF_LINK || sock->domain == AF_IEEE80211) {
		retval = sock_bind_ll(sock, addr, addrlen);
		socket_put(sock);
		goto out;
	}

	/* @sock->domain != AF_LINK */

	sockaddr_to_endpoint(&local, addr);
	sock_unmap_ipv4_mapped_ipv6_to_ipv4(&local.addr);

	if ((err = netconn_bind(sock->conn, &local.addr, local.port))) {
		retval = -err_to_errno(err);
		socket_put(sock);
		goto out;
	}
	socket_put(sock);
 out:
	if (retval < 0) {
		errno = -retval;
		retval = -1;
	}
	return retval;
}

/**
 * listen() - listen for connections on a socket
 */
int os_listen(int sockfd, int backlog)
{
	struct sock *sock;
	int err, retval = 0;

	if ((sock = fd_to_socket(sockfd)) == NULL) {
		retval = -EBADF;
		goto out;
	} else if (!file_is_socket(&sock->file)) {
		retval = -ENOTSOCK;
		goto out;
	} else if (sock->type != SOCK_STREAM) {
		retval = -EOPNOTSUPP;
		goto out;
	}

	socket_get(sock);
	backlog = LWIP_MIN(LWIP_MAX(backlog, 0), 0xff);
	err = netconn_listen_with_backlog(sock->conn, (uint8_t) backlog);
	if (err != ERR_OK) {
		retval = err_to_errno(err);
		socket_put(sock);
		goto out;
	}
	socket_put(sock);
 out:
	if (retval < 0) {
		errno = -retval;
		retval = -1;
	}
	return retval;
}

/*
 * accept - accept a connection on a socket
 */
int os_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
	struct sock *sock, *newsock = NULL;
	struct netconn *newconn = NULL;
	int newfd, err, retval = 0;

	if ((sock = fd_to_socket(sockfd)) == NULL) {
		retval = -EBADF;
		goto out;
	} else if (!file_is_socket(&sock->file)) {
		retval = -ENOTSOCK;
		goto out;
	} else if (sock->type != SOCK_STREAM) {
		retval = -EOPNOTSUPP;
		goto out;
	}

	socket_get(sock);
	err = netconn_accept(sock->conn, &newconn);
	if (err) {
		dbg("%s: accept failed for fd=%d, err=%d\n",
		    __func__, sockfd, err);
		retval = (err == ERR_CLSD) ? -EINVAL : -err_to_errno(err);
		socket_put(sock);
		goto out;
	}

	/* Allocatte a new fd and a socket object for @newconn */
	newfd = vfs_get_free_fd();
	if (newfd < 0) {
		retval = -ENFILE;
		goto error;
	}

	if ((newsock = sock_alloc(newconn)) == NULL) {
		retval = -ENOMEM;
		goto error;
	}
	newsock->domain = sock->domain;
	newsock->type = sock->type;
	newsock->proto = sock->proto;
	/* NB: sock_alloc only init f_refcnt, need inc here */
	atomic_inc(&newsock->file.f_refcnt);

	/*
	 * FIXME:
	 * Some stuffs handling incoming frames that arrived right after
	 * lwip_accept() is returned, but before a new socket is created.
	 *
	 * I am not sure if that is even necessary.
	 */

	if ((addr != NULL) && (addrlen != NULL)) {
		struct endpoint peer;

		err = netconn_peer(newconn, &peer.addr, &peer.port);
		if (err != ERR_OK) {
			dbg("%s(%d): netconn_peer failed, err=%d\n", __func__,
			    sockfd, err);
			retval = -err_to_errno(err);
			socket_put(sock);
			goto error;
		}
		endpoint_to_sockaddr(&peer, addr, addrlen);
	}

	vfs_install_fd(newfd, &newsock->file);
	socket_put(sock);

	return newfd;
 error:
	if (newconn)
		netconn_delete(newconn);
	if (newsock)
		sock_free(newsock);
	if (newfd > 0)
		vfs_free_fd(newfd);
 out:
	if (retval < 0) {
		errno = -retval;
		retval = -1;
	}
	return retval;
}

/**
 * connect() - initiate a connection on a socket
 */
int os_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
	struct sock *sock;
	int err, retval = 0;

	if ((sock = fd_to_socket(sockfd)) == NULL) {
		retval = -EBADF;
		goto out;
	} else if (!file_is_socket(&sock->file)) {
		retval = -ENOTSOCK;
		goto out;
	} else if (addr->sa_family != AF_UNSPEC &&
		   addr->sa_family != sock->domain) {
		retval = -EINVAL;
		goto out;
	}

	socket_get(sock);

	if (addr->sa_family == AF_UNSPEC) {
		err = netconn_disconnect(sock->conn);
	} else {
		struct endpoint remote;

		sockaddr_to_endpoint(&remote, addr);
		sock_unmap_ipv4_mapped_ipv6_to_ipv4(&remote.addr);

		err = netconn_connect(sock->conn, &remote.addr, remote.port);
	}
	if (err)
		retval = -err_to_errno(err);

	socket_put(sock);
 out:
	if (retval < 0) {
		errno = -retval;
		retval = -1;
	}
	return retval;
}

/**
 * shutdown() - shut down part of a full-duplex connection
 */
int os_shutdown(int sockfd, int how)
{
	struct sock *sock;
	int err, retval = 0;
	uint8_t shutdown[2] = {0, 0}; /* 0: rx, 1: tx */

	if ((sock = fd_to_socket(sockfd)) == NULL) {
		retval = -EBADF;
		goto out;
	} else if (!file_is_socket(&sock->file)) {
		retval = -ENOTSOCK;
		goto out;
	} else if (sock->type != SOCK_STREAM) {
		retval = -EOPNOTSUPP;
		goto out;
	} else if (sock->conn == NULL) {
		retval = -ENOTCONN;
		goto out;
	}

	switch (how) {
	case SHUT_RD:
		shutdown[0] = 1;
		break;
	case SHUT_WR:
		shutdown[1] = 1;
		break;
	case SHUT_RDWR:
		shutdown[0] = shutdown[1] = 1;
		break;
	default:
		retval = -EINVAL;
		goto out;
	}
	socket_get(sock);
	err = netconn_shutdown(sock->conn, shutdown[0], shutdown[1]);
	retval = -err_to_errno(err);
	socket_put(sock);
 out:
	if (retval < 0) {
		errno = -retval;
		retval = -1;
	}
	return retval;
}

/*
 * getpeername - get name of connected peer socket
 */
int os_getpeername (int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
	struct sock *sock;
	int retval = 0;

	if ((sock = fd_to_socket(sockfd)) == NULL) {
		retval = -EBADF;
		goto out;
	} else if (!file_is_socket(&sock->file)) {
		retval = -ENOTSOCK;
		goto out;
	}
	socket_get(sock);
	retval = sock_getaddrname(sock, addr, addrlen, 0);
	socket_put(sock);
 out:
	if (retval < 0) {
		errno = -retval;
		retval = -1;
	}
	return retval;
}

/**
 * getsockname() - get socket name
 */

int os_getsockname (int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
	struct sock *sock;
	int retval = 0;

	if ((sock = fd_to_socket(sockfd)) == NULL) {
		retval = -EBADF;
		goto out;
	} else if (!file_is_socket(&sock->file)) {
		retval = -ENOTSOCK;
		goto out;
	}
	socket_get(sock);
	retval = sock_getaddrname(sock, addr, addrlen, 1);
	socket_put(sock);
 out:
	if (retval < 0) {
		errno = -retval;
		retval = -1;
	}
	return retval;
}

/**
 * getsockopt(), setsockopt() - get and set options on sockets
 */

#define LWIP_SETGETSOCKOPT_DATA_VAR_REF(name) \
	API_VAR_REF(name)
#define LWIP_SETGETSOCKOPT_DATA_VAR_FREE(name)  \
	API_VAR_FREE(MEMP_SOCKET_SETGETSOCKOPT_DATA, name)

int os_getsockopt(int sockfd, int level, int optname, void *optval,
		  socklen_t *optlen)
{
	struct sock *sock = fd_to_socket(sockfd);
	int err, retval = 0;
#if !LWIP_TCPIP_CORE_LOCKING
	struct lwip_setgetsockopt_data *ptr;
	API_VAR_DECLARE(struct lwip_setgetsockopt_data, data);
#endif /* !LWIP_TCPIP_CORE_LOCKING */

	if (sock == NULL) {
		retval = -EBADF;
		goto out;
	} else if (!file_is_socket(&sock->file)) {
		retval = -ENOTSOCK;
		goto out;
	} else if (optval == NULL || optlen == NULL) {
		retval = -EFAULT;
		goto out;
	}
	socket_get(sock);

#if LWIP_TCPIP_CORE_LOCKING
	LOCK_TCPIP_CORE();
	err = sock_getsockopt(sock, level, optname, optval, optlen);
	retval = err? -err_to_errno(err) : 0;
	UNLOCK_TCPIP_CORE();
#else /* LWIP_TCPIP_CORE_LOCKING */

#if LWIP_MPU_COMPATIBLE
	if (*optlen > LWIP_SETGETSOCKOPT_MAXOPTLEN ||
	    (data = memp_malloc(MEMP_SOCKET_SETGETSOCKOPT_DATA)) == NULL) {
		retval = -ENOBUFS;
		socket_put(sock);
		goto out;
	}
#endif /* LWIP_MPU_COMPATIBLE */

	ptr = &LWIP_SETGETSOCKOPT_DATA_VAR_REF(data);
	ptr->s = sockfd;
	ptr->level = level;
	ptr->optname = optname;
	ptr->optlen = *optlen;
#if !LWIP_MPU_COMPATIBLE
	ptr->optval.p = optval;
#endif /* !LWIP_MPU_COMPATIBLE */
	ptr->err = 0;
#if LWIP_NETCONN_SEM_PER_THREAD
	ptr->completed_sem = LWIP_NETCONN_THREAD_SEM_GET();
#else
	ptr->completed_sem = &sock->conn->op_completed;
#endif
	if ((err = tcpip_callback(sock_getsockopt_callback, ptr))) {
		LWIP_SETGETSOCKOPT_DATA_VAR_FREE(data);
		retval = -err_to_errno(err);
		socket_put(sock);
		goto out;
	}
	sys_arch_sem_wait((sys_sem_t *) ptr->completed_sem, 0);

	/* Write back optlen and optval */
	*optlen = ptr->optlen;
#if LWIP_MPU_COMPATIBLE
	MEMCPY(optval, ptr->optval, ptr->optlen);
#endif /* LWIP_MPU_COMPATIBLE */

	/* Maybe lwip_getsockopt_internal has changed err */
	retval = ptr->err ? -err_to_errno(ptr->err) : 0;
	LWIP_SETGETSOCKOPT_DATA_VAR_FREE(data);
#endif /* LWIP_TCPIP_CORE_LOCKING */

	socket_put(sock);
 out:
	if (retval < 0) {
		errno = -retval;
		retval = -1;
	}
	return retval;
}

int os_setsockopt(int sockfd, int level, int optname, const void *optval,
		  socklen_t optlen)
{
	struct sock *sock = fd_to_socket(sockfd);
	int err, retval = 0;
#if !LWIP_TCPIP_CORE_LOCKING
	struct lwip_setgetsockopt_data *ptr;
	API_VAR_DECLARE(struct lwip_setgetsockopt_data, data);
#endif /* !LWIP_TCPIP_CORE_LOCKING */

	if (sock == NULL) {
		retval = -EBADF;
		goto out;
	} else if (!file_is_socket(&sock->file)) {
		retval = -ENOTSOCK;
		goto out;
	} else if (optval == NULL) {
		retval = -EFAULT;
		goto out;
	}
	socket_get(sock);

#if LWIP_TCPIP_CORE_LOCKING
	LOCK_TCPIP_CORE();
	err = sock_setsockopt(sock, level, optname, optval, optlen);
	retval = err ? -err_to_errno(err) : 0;
	UNLOCK_TCPIP_CORE();
#else /* LWIP_TCPIP_CORE_LOCKING */

#if LWIP_MPU_COMPATIBLE
	if (*optlen > LWIP_SETGETSOCKOPT_MAXOPTLEN ||
	    (data = memp_malloc(MEMP_SOCKET_SETGETSOCKOPT_DATA)) == NULL) {
		retval = -ENOBUFS;
		socket_put(sock);
		goto out;
	}
#endif /* LWIP_MPU_COMPATIBLE */

	ptr = &LWIP_SETGETSOCKOPT_DATA_VAR_REF(data);
	ptr->s = sockfd;
	ptr->level = level;
	ptr->optname = optname;
	ptr->optlen = optlen;
#if LWIP_MPU_COMPATIBLE
	MEMCPY(ptr->optval, optval, optlen);
#else
	ptr->optval.pc = (const void *) optval;
#endif /* !LWIP_MPU_COMPATIBLE */
	ptr->err = 0;
#if LWIP_NETCONN_SEM_PER_THREAD
	ptr->completed_sem = LWIP_NETCONN_THREAD_SEM_GET();
#else
	ptr->completed_sem = &sock->conn->op_completed;
#endif
	if ((err = tcpip_callback(sock_setsockopt_callback, ptr))) {
		LWIP_SETGETSOCKOPT_DATA_VAR_FREE(data);
		retval = -err_to_errno(err);
		socket_put(sock);
		goto out;
	}
	sys_arch_sem_wait((sys_sem_t *) ptr->completed_sem, 0);

	/* Maybe lwip_getsockopt_internal has changed err */
	retval = ptr->err ? -err_to_errno(ptr->err) : 0;
	LWIP_SETGETSOCKOPT_DATA_VAR_FREE(data);
#endif /* LWIP_TCPIP_CORE_LOCKING */

	socket_put(sock);
 out:
	if (retval < 0) {
		errno = -retval;
		retval = -1;
	}
	return retval;
}

/**
 * recv(), recvfrom(), recvmsg() - receive a message from a socket
 */

ssize_t os_recvfrom(int sockfd, void *buf, size_t len, int flags,
		    struct sockaddr *from, socklen_t *addrlen)
{
	struct sock *sock;
	ssize_t retval = 0;

	if ((sock = fd_to_socket(sockfd)) == NULL) {
		retval = -EBADF;
		goto out;
	} else if (!file_is_socket(&sock->file)) {
		retval = -ENOTSOCK;
		goto out;
	}

	socket_get(sock);
	retval = sock_recvfrom(sock, buf, len, flags, from, addrlen);
	socket_put(sock);
 out:
	if (retval < 0) {
		errno = -retval;
		retval = -1;
	}
	return retval;
}

ssize_t os_recv(int sockfd, void *buf, size_t len, int flags)
{
	return os_recvfrom(sockfd, buf, len, flags, NULL, NULL);
}

ssize_t os_recvmsg(int sockfd, struct msghdr *msg, int flags)
{
	struct sock *sock;
	int buflen, retval = 0;

	if ((sock = fd_to_socket(sockfd)) == NULL) {
		retval = -EBADF;
		goto out;
	} else if (!file_is_socket(&sock->file)) {
		retval = -ENOTSOCK;
		goto out;
	} else if ((buflen = sock_msg_len(msg)) < 0) {
		retval = -EINVAL;
		goto out;
	}
	socket_get(sock);
	retval = sock_recvmsg(sock, msg, flags);
	socket_put(sock);
 out:
	if (retval < 0) {
		errno = -retval;
		retval = -1;
	}
	return retval;
}

/*
 * send(), sendto(), sendmsg() - send a message on a socket
 */
ssize_t os_sendto(int sockfd, const void *buf, size_t size, int flags,
		  const struct sockaddr *to, socklen_t tolen)
{
	struct sock *sock;
	ssize_t retval;

	if ((sock = fd_to_socket(sockfd)) == NULL) {
		retval = -EBADF;
		goto out;
	} else if (!file_is_socket(&sock->file)) {
		retval = -ENOTSOCK;
		goto out;
	}
	socket_get(sock);
	retval = sock_sendto(sock, buf, size, flags, to, tolen);
	socket_put(sock);
 out:
	if (retval < 0) {
		errno = -retval;
		retval = -1;
	}
	return retval;
}

ssize_t os_send(int sockfd, const void *buf, size_t size, int flags)
{
	struct sock *sock;
	ssize_t retval;

	if ((sock = fd_to_socket(sockfd)) == NULL) {
		retval = -EBADF;
		goto out;
	} else if (!file_is_socket(&sock->file)) {
		retval = -ENOTSOCK;
		goto out;
	}
	socket_get(sock);
	retval = sock_send(sock, buf, size, flags);
	socket_put(sock);
 out:
	if (retval < 0) {
		errno = -retval;
		retval = -1;
	}
	return retval;
}

ssize_t os_sendmsg(int sockfd, const struct msghdr *msg, int flags)
{
	struct sock *sock;
	ssize_t retval;

	if ((sock = fd_to_socket(sockfd)) == NULL) {
		retval = -EBADF;
		goto out;
	} else if (!file_is_socket(&sock->file)) {
		retval = -ENOTSOCK;
		goto out;
	}
	socket_get(sock);
	retval = sock_sendmsg(sock, msg, flags);
	socket_put(sock);
 out:
	if (retval < 0) {
		errno = -retval;
		retval = -1;
	}
	return retval;
}


/**
 * inet_ntop(), inet_pton(), inet_ntoa(), inet_aton(), inet_addr()
 * - Internet address manipulation routines
 */

const char *os_inet_ntop(int af, const void *src, char *dst, socklen_t size)
{
	  const char *p = NULL;

	  if ((int) size < 0) {
		  errno = ENOSPC;
		  return NULL;
	  }

	  switch (af) {
#if LWIP_IPV4
	  case AF_INET:
		  p = ip4addr_ntoa_r(src, dst, (int) size);
		  if (p == NULL) {
			  errno = ENOSPC;
		  }
		  break;
#endif
#if LWIP_IPV6
	  case AF_INET6:
		  p = ip6addr_ntoa_r(src, dst, (int) size);
		  if (p == NULL) {
			  errno = ENOSPC;
		  }
		  break;
#endif
	  default:
		  errno = EAFNOSUPPORT;
		  break;
	  }
	  return p;
}

int os_inet_pton(int af, const char *src, void *dst)
{
	int err;

	switch (af) {
#if LWIP_IPV4
	case AF_INET:
		err = ip4addr_aton(src, (ip4_addr_t *)dst);
		break;
#endif
#if LWIP_IPV6
	case AF_INET6: {
		/* convert into temporary variable since ip6_addr_t might be larger
		   than in6_addr when scopes are enabled */
		ip6_addr_t addr;
		err = ip6addr_aton(src, &addr);
		if (err) {
			memcpy(dst, &addr.addr, sizeof(addr.addr));
		}
		break;
	}
#endif
	default:
		err = -1;
		errno = EAFNOSUPPORT;
		break;
	}
	return err;
}

#if LWIP_IPV4

char *os_inet_ntoa(struct in_addr in)
{
	return ip4addr_ntoa((void *) &in);
}

int os_inet_aton(const char *cp, struct in_addr *inp)
{
	return ip4addr_aton(cp, (ip4_addr_t *) inp);
}

in_addr_t os_inet_addr(const char *cp)
{
	return ipaddr_addr(cp);
}

#endif

#if LWIP_IPV6

char *os_inet6_ntoa(struct in6_addr in)
{
	return ip6addr_ntoa((void *) &in);
}

int os_inet6_aton(const char *cp, struct in6_addr *inp)
{
	return ip6addr_aton(cp, (ip6_addr_t *) inp);
}

#endif
