/*
 * Copyright 2022 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */

#ifdef AYLA_SCM_SUPPORT
#include <unistd.h>
#endif

#include <ayla/utypes.h>
#include <ayla/endian.h>
#include <al/al_err.h>
#include <ayla/log.h>
#include <ayla/mod_log.h>
#include <lwip/sockets.h>
#include <platform/pfm_net_socket.h>

STAILQ_HEAD(pfm_net_socket_head, pfm_net_socket);

static struct pfm_net_socket_head pfm_net_socket_head =
	STAILQ_HEAD_INITIALIZER(pfm_net_socket_head);

void pfm_net_log(const char *fmt, ...)
{
	ADA_VA_LIST args;

	ADA_VA_START(args, fmt);
	log_put_va(MOD_LOG_SSL, fmt, args);
	ADA_VA_END(args);
}

void pfm_net_socket_select_add(struct pfm_net_socket *ns)
{
	if (ns->in_list) {
		return;
	}
	ns->in_list = 1;
	STAILQ_INSERT_TAIL(&pfm_net_socket_head, ns, list);
}

void pfm_net_socket_select_remove(struct pfm_net_socket *ns)
{
	if (!ns->in_list) {
		return;
	}
	ns->in_list = 0;
	STAILQ_REMOVE(&pfm_net_socket_head, ns, pfm_net_socket, list);
}

int pfm_net_socket_select(unsigned int timeout_ms)
{
	struct pfm_net_socket *ns;
	struct timeval wait_time;
	u8 flags;
	fd_set rd;
	fd_set wr;
	fd_set ex;
	int fds;
	int max_fd = -1;

	FD_ZERO(&rd);
	FD_ZERO(&wr);
	FD_ZERO(&ex);

	STAILQ_FOREACH(ns, &pfm_net_socket_head, list) {
		flags = ns->select_flags &
		    (PFM_NETF_READ | PFM_NETF_WRITE | PFM_NETF_EXCEPT);
		if (flags & PFM_NETF_READ) {
			FD_SET(ns->sock, &rd);
		}
		if (flags & PFM_NETF_WRITE) {
			FD_SET(ns->sock, &wr);
		}
		if (flags & PFM_NETF_EXCEPT) {
			FD_SET(ns->sock, &ex);
		}
		if (flags && max_fd < ns->sock) {
			max_fd = ns->sock;
		}
	}
	if (max_fd < 0) {
		return -1;
	}
	if (timeout_ms > PFM_NET_SOCKET_MAX_WAIT) {
		timeout_ms = PFM_NET_SOCKET_MAX_WAIT;
	}
	wait_time.tv_sec = timeout_ms / 1000;
	wait_time.tv_usec = (timeout_ms % 1000) * 1000;

	fds = select(max_fd + 1, &rd, &wr, &ex, &wait_time);

	/*
	 * Repeat list traversal from the beginning after every callback,
	 * in case the list changes by a close() during the callback.
	 * The rd/wr/ex flags are cleared before any callback so it won't
	 * be repeated.
	 * Only a few sockets are expected to be in the list.
	 */
	while (fds > 0) {
		fds--;
		STAILQ_FOREACH(ns, &pfm_net_socket_head, list) {
			flags = 0;
			if (FD_ISSET(ns->sock, &rd)) {
				FD_CLR(ns->sock, &rd);
				flags |= PFM_NETF_READ;
			}
			if (FD_ISSET(ns->sock, &wr)) {
				FD_CLR(ns->sock, &wr);
				flags |= PFM_NETF_WRITE;
			}
			if (FD_ISSET(ns->sock, &ex)) {
				FD_CLR(ns->sock, &ex);
				flags |= PFM_NETF_EXCEPT;
			}
			if (flags) {
				if (ns->select_cb) {
					ns->select_cb(ns->arg, flags);
				}
				break;
			}
		}
	}
	return 0;
}
