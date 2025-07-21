/*
 * Copyright 2022 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_PFM_NET_SOCKET_H__
#define __AYLA_PFM_NET_SOCKET_H__

#include <sys/queue.h>
#include <ayla/utypes.h>
#include <al/al_compiler.h>

/*
 * Socket flags.
 */
#define PFM_NETF_READ	 BIT(0)
#define PFM_NETF_WRITE	 BIT(1)
#define PFM_NETF_EXCEPT	 BIT(2)

/*
 * Socket select list.
 */
struct pfm_net_socket {
	int sock;
	u8 select_flags;
	void (*select_cb)(void *arg, u8 select_flags);
	void *arg;

	/* internal fields, for use in pfm_net_socket.c only */
	STAILQ_ENTRY(pfm_net_socket) list;
	u8 in_list;
};

/*
 * Log messages (under subsystem "ssl" for now).
 */
void pfm_net_log(const char *fmt, ...) ADA_ATTRIB_FORMAT(1, 2);

/*
 * Add socket to select list.
 */
void pfm_net_socket_select_add(struct pfm_net_socket *ns);

/*
 * Remove socket from select list.
 */
void pfm_net_socket_select_remove(struct pfm_net_socket *ns);

/*
 * Select - wait for event on listed sockets.
 * This should be used by the ADA thread only.
 * Returns 0 on success, -1 on error.
 */
int pfm_net_socket_select(unsigned int timeout_ms);

#define PFM_NET_SOCKET_MAX_WAIT	0x7fffffff	/* max select wait in ms */

/*
 * Initialize the socket layer and start LwIP.
 */
void pfm_net_socket_init(void);

/*
 * Initialize DNS.
 */
void pfm_net_dns_init(void);

#endif /* __AYLA_PFM_NET_SOCKET_H__ */
