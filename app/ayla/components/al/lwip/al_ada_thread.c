/*
 * Copyright 2022 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ayla/utypes.h>
#include <ayla/endian.h>
#include <ayla/assert.h>
#include <ayla/log.h>
#include <ayla/callback.h>
#include <ayla/timer.h>
#include <ayla/conf.h>
#include <al/al_ada_thread.h>
#include <al/al_os_lock.h>
#include <al/al_net_addr.h>
#include <al/al_clock.h>
#include <platform/pfm_os_thread.h>
#include <platform/pfm_ada_thread.h>
#include <lwip/sockets.h>
#include <lwip/tcpip.h>
#include <lwip/udp.h>
#ifdef AYLA_SCM_SUPPORT
#include <lwip/pbuf.h>
#include <lwip/err.h>
#include <lwip/errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#endif
#include <platform/pfm_net_socket.h>

#include <cmsis_os.h>

#ifdef AYLA_SCM_SUPPORT
#define lwip_send        send
#define lwip_recv        recv
#define lwip_socket      socket
#define lwip_bind        bind
#define lwip_connect     connect
#define lwip_close       close
#define lwip_fcntl       fcntl
#define lwip_getsockname getsockname
#define lwip_setsockopt  setsockopt
#endif

/* TODO with IAR build, errno is undefined here for some reason */
#ifdef __IAR_SYSTEMS_ICC__
int errno;
#endif

struct al_ada_thread {
	struct al_thread *ada_thread;
	osThreadId_t task;
	struct callback_queue cb_queue;
	struct al_lock *cb_queue_lock;
	struct pfm_net_socket wait_sock;	/* socket for select waiting */
	int wakeup_sock;			/* socket to wakeup select */
	struct udp_pcb *wakeup_udp;
	osThreadId_t tcp_task;
	u16 wakeup_port;			/* port in network order */
	u8 sock_init_done;
	struct timer_head timers;
};

static struct al_ada_thread al_ada_thread;

static void pfm_ada_wakeup_from_lwip(void)
{
	struct al_ada_thread *aat = &al_ada_thread;
	struct pbuf *pbuf;
	ip_addr_t dest_ip, *p_dest_ip = &dest_ip;
	err_t err;

	pbuf = pbuf_alloc(PBUF_TRANSPORT, 1, PBUF_RAM);
	if (!pbuf) {
		log_put(LOG_ERR "%s: wakeup pbuf alloc failed", __func__);
		return;
	}
	ip_addr_set_ip4_u32(p_dest_ip, PP_HTONL(IPADDR_LOOPBACK));
	err = udp_sendto(aat->wakeup_udp, pbuf, &dest_ip, aat->wakeup_port);
	pbuf_free(pbuf);
	if (err) {
		log_put(LOG_ERR "%s: wakeup udp_send failed %d",
		    __func__, err);
	}
}

/*
 * Wakeup ADA thread.
 *
 * Wakeup the ADA thread which may be waiting in lwip_select() for a
 * socket event.   This would be better if there was a direct method via LwIP.
 *
 * If the current task is the ADA thread, nothing needs to be done.
 * If the current task is the TCP/IP thread, we send a byte to the wakeup
 * socket via the UDP RAW socket interface, udp_sendto().
 * Otherwise, we send on the wakeup socket (connected to itself) with
 * lwip_send().
 */
void al_ada_wakeup(void)
{
	struct al_ada_thread *aat = &al_ada_thread;
	int rc;
	int sock = aat->wakeup_sock;
	int send_flags = 0;
	const char buf[] = "";
	osThreadId_t current_task;

	if (!aat->sock_init_done) {
		return;			/* early call */
	}
	current_task = osThreadGetId();
	if (aat->task == current_task) {
		return;
	}
	if (aat->tcp_task == current_task) {
		pfm_ada_wakeup_from_lwip();
		return;
	}
	if (sock < 0) {
		log_put(LOG_ERR "%s: wakeup sock not open", __func__);
		return;
	}
	rc = lwip_send(sock, buf, sizeof(buf), send_flags);
	if (rc < 0) {
		log_put(LOG_ERR "%s: wakeup sock %d send err %d",
		    __func__, sock, errno);
		return;
	}
}

/*
 * Wait for next event on ADA thread,
 */
void al_ada_poll(u32 max_wait_in)
{
	struct al_ada_thread *aat = &al_ada_thread;
	struct callback *cb;
	int max_wait;
	void (*func)(void *);
	void *arg;
	int rc;
	static u8 select_err_logged;
	static u8 cb_err_logged;

	max_wait = timer_advance(&aat->timers);
	if (max_wait > max_wait_in) {
		max_wait = max_wait_in;
	}

	/*
	 * process callbacks.
	 */
	al_os_lock_lock(aat->cb_queue_lock);
	while ((cb = callback_dequeue(&aat->cb_queue)) != NULL) {
		func = cb->func;
		arg = cb->arg;
		if (!cb->pending && cb_err_logged) {
			cb_err_logged = 1;
			log_put(LOG_ERR "ada_main cb %p not pending h %p",
			    cb, func);
		}
		ASSERT(cb->pending);
		cb->pending = 0;
		al_os_lock_unlock(aat->cb_queue_lock);
		ASSERT(func);
		func(arg);
		al_os_lock_lock(aat->cb_queue_lock);
		max_wait = 0;		/* don't wait before return */
	}
	al_os_lock_unlock(aat->cb_queue_lock);

	/*
	 * Handle device and socket events.
	 */
	if (max_wait) {
		rc = pfm_net_socket_select(max_wait < 0 ?
		    PFM_NET_SOCKET_MAX_WAIT : (unsigned int)max_wait);
		if (rc < 0 && !select_err_logged) {
			select_err_logged = 1;
			log_put(LOG_ERR "%s: select failed", __func__);
		}
	}
}

/*
 * This event called when wait socket is ready to receive. Read a byte.
 */
static void pfm_al_thread_wakeup_cb(void *arg, u8 flags)
{
	int sock = (int)((char *)arg - (char *)0); /* cast void * arg to int */
	int rc;
	static u8 err_logged;
	char buf[1];

	rc = lwip_recv(sock, buf, sizeof(buf), 0);
	if (rc < 0 && !err_logged) {
		err_logged = 1;
		log_put(LOG_ERR "%s: sock %d recv err %d", __func__, sock, rc);
	}
}

/*
 * Handler for callback in LwIP to initialize raw UDP sockets for wakeup.
 * This happens early and asserts if it cannot succeed.
 */
static void pfm_ada_thread_tcp_init(void *arg)
{
	struct al_ada_thread *aat = &al_ada_thread;
	ip_addr_t src_ip, *p_src_ip = &src_ip;
	err_t err;

	aat->wakeup_udp = udp_new();
	ASSERT(aat->wakeup_udp);

	ip_set_option(aat->wakeup_udp, SOF_REUSEADDR);

	ip_addr_set_ip4_u32(p_src_ip, PP_HTONL(IPADDR_LOOPBACK));
	err = udp_bind(aat->wakeup_udp, p_src_ip, aat->wakeup_port);
	if (err) {
		log_put(LOG_ERR "%s: udp_bind failed %d", __func__, err);
	}

	aat->tcp_task = osThreadGetId();
}

void al_ada_thread_init(struct al_thread *thread)
{
	struct al_ada_thread *aat = &al_ada_thread;
	int sock;
	struct sockaddr_in sa;
	socklen_t slen;
	int rc, optval;

	ASSERT(!aat->ada_thread);
	ASSERT(thread);

	aat->ada_thread = thread;
	aat->task = osThreadGetId();

	pfm_ada_thread_init_pfm();
	pfm_net_socket_init();

	sock = lwip_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0) {
		log_put(LOG_ERR "%s: lwip_socket err %d", __func__, errno);
		ASSERT_NOTREACHED();
	}

	/*
	 * Bind to loopback address and random port.
	 */
	memset(&sa, 0, sizeof(sa));
	sa.sin_len = sizeof(sa);
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = PP_HTONL(IPADDR_LOOPBACK);

	rc = lwip_bind(sock, (struct sockaddr *)&sa, sizeof(sa));
	if (rc < 0) {
		log_put(LOG_ERR "%s: lwip_bind err %d", __func__, errno);
		ASSERT_NOTREACHED();
	}

	/*
	 * Find out which port we got.
	 */
	slen = sizeof(sa);
	rc = lwip_getsockname(sock, (struct sockaddr *)&sa, &slen);
	if (rc < 0) {
		log_put(LOG_ERR "%s: getsockname err %d", __func__, errno);
		ASSERT_NOTREACHED();
	}
	if (slen != sizeof(sa)) {
		log_put(LOG_ERR "%s: getsockname len mismatch", __func__);
		ASSERT_NOTREACHED();
	}

	/*
	 * Connect socket to itself (set default send address).
	 */
	sa.sin_addr.s_addr = PP_HTONL(IPADDR_LOOPBACK);
	rc = lwip_connect(sock, (struct sockaddr *)&sa, slen);
	if (rc < 0) {
		log_put(LOG_ERR "%s: lwip_connect err %d", __func__, errno);
		ASSERT_NOTREACHED();
	}
	aat->wakeup_sock = sock;
	aat->wakeup_port = ntohs(sa.sin_port);
	aat->sock_init_done = 1;

	/*
	 * Set non-blocking.
	 */
	rc = lwip_fcntl(sock, F_SETFL, O_NONBLOCK);
	if (rc < 0) {
		log_put(LOG_ERR "%s: fcntl err err %d", __func__, errno);
		ASSERT_NOTREACHED();
	}

	/*
	 * Set socketopt of SO_REUSEADDR.
	 */
	optval = 1;
	rc = lwip_setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int));
	if (rc < 0) {
		log_put(LOG_ERR "%s: setsockopt err %d", __func__, errno);
		ASSERT_NOTREACHED();
	}

	/*
	 * Add to select set.
	 */
	aat->wait_sock.select_flags = PFM_NETF_READ | PFM_NETF_EXCEPT;
	aat->wait_sock.sock = sock;
	aat->wait_sock.select_cb = pfm_al_thread_wakeup_cb;
	aat->wait_sock.arg = (void *)sock;
	pfm_net_socket_select_add(&aat->wait_sock);

	aat->cb_queue_lock = al_os_lock_create();
	ASSERT(aat->cb_queue_lock);

	if (tcpip_callback(pfm_ada_thread_tcp_init, NULL)) {
		log_put(LOG_ERR "%s: lwip_callback failed", __func__);
		ASSERT_NOTREACHED();
	}
}

/*
 * ADA thread init.
 * Called in ADA thread from common/main.c to initialize locks and queues.
 * Obsolete.  Not used in ADA, kept for PDA apps.
 */
void pfm_ada_thread_init(struct al_thread *thread)
{
	al_ada_thread_init(thread);
}

void pfm_callback_pend(struct callback *cb)
{
	struct al_ada_thread *aat = &al_ada_thread;
	int wake;

	ASSERT(cb);
	ASSERT(aat->cb_queue_lock);
	al_os_lock_lock(aat->cb_queue_lock);
	wake = !cb->pending;
	if (wake) {
		callback_enqueue(&aat->cb_queue, cb);
	}
	al_os_lock_unlock(aat->cb_queue_lock);
	if (wake) {
		al_ada_wakeup();
	}
}

/*
 * Set ADA timer.
 * This should only be called from the ADA thread itself.
 */
void pfm_timer_set(struct timer *timer, unsigned long ms)
{
	struct al_ada_thread *aat = &al_ada_thread;

	if (al_os_thread_self() != aat->ada_thread) {
		log_put(LOG_ERR "al_ada_timer_set from other thread %p",
		    al_os_thread_self());
	}
	ASSERT(al_os_thread_self() == aat->ada_thread);
	timer_set(&aat->timers, timer, ms);

	/*
	 * No wakeup is required since it is called from the ADA thread.
	 */
}

/*
 * Cancel ADA timer.
 * This should only be called from the ADA thread itself.
 */
void pfm_timer_cancel(struct timer *timer)
{
	struct al_ada_thread *aat = &al_ada_thread;

	if (al_os_thread_self() != aat->ada_thread) {
		log_put(LOG_ERR "al_ada_timer_cancel from other thread %p",
		    al_os_thread_self());
	}
	ASSERT(al_os_thread_self() == aat->ada_thread);
	timer_cancel(&aat->timers, timer);
}
