/*
 * Copyright 2023 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include "lwip/opt.h"
#include "lwip/stats.h"
#include "lwip/mem.h"
#include "lwip/udp.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "lwip/netifapi.h"
#include "lwip/def.h"
#include "lwip/dhcp.h"
#include "lwip/autoip.h"
#include "lwip/dns.h"
#include "lwip/etharp.h"
#include "lwip/prot/dhcp.h"
#include "lwip/prot/iana.h"
#include <lwip/netdb.h>
#include <lwip/tcpip.h>
#include <lwip/dns.h>
#include <sntp/sntp.h>
#include <al/al_err.h>
#include <al/al_net_sntp.h>
#include <al/al_clock.h>
#include <platform/pfm_clock.h>
#include <platform/pfm_ada_thread.h>

#define PFM_NET_SNTP_MAX_ADJ	(5 * 60)	/* seconds max difference */

static void (*pfm_sntp_callback_fn)(void);
static struct callback pfm_net_sntp_ada_callback;
static u32 pfm_net_sntp_time;

/*
 * Callback in ADA thread after time is set.
 */
static void pfm_net_sntp_clock_set_cb(void *arg)
{
	s32 diff;

	pfm_clock_source_set(AL_CS_SNTP);

	/*
	 * ESP-IDF uses adjtime() which allows a wide difference, but we need
	 * a smaller difference for authentication purposes.
	 * If that difference is exceeded, set the time immediately.
	 */
	diff = al_clock_get(NULL) - pfm_net_sntp_time;
	if (ABS(diff) > PFM_NET_SNTP_MAX_ADJ) {
		al_clock_set(pfm_net_sntp_time, AL_CS_SNTP);
	}

	if (pfm_sntp_callback_fn) {
		pfm_sntp_callback_fn();
	}
}

/*
 * Callback in LwIP thread after time is set.
 * Generate callback to ADA thread so that the LwIP thread doesn't block on
 * the client_lock, which is sometimes held while calling LwIP socket functions.
 */
static void pfm_net_sntp_clock_set(uint32_t sec, uint32_t us)
{
	pfm_net_sntp_time = sec;
	pfm_callback_pend(&pfm_net_sntp_ada_callback);
}

enum al_err al_net_sntp_server_set(unsigned int index, const char *name)
{
	sntp_setservername(index, name);

	return AL_ERR_OK;
}

enum al_err al_net_sntp_start(void)
{
	if (!pfm_net_sntp_ada_callback.func) {
		callback_init(&pfm_net_sntp_ada_callback,
		    pfm_net_sntp_clock_set_cb, NULL);
	}

	sntp_stop();
	sntp_setoperatingmode(SNTP_OPMODE_POLL);
	sntp_set_time_sync_notification_cb(pfm_net_sntp_clock_set);

	sntp_init();
	if (!sntp_enabled()) {
		return AL_ERR_ERR;
	}
	return AL_ERR_OK;
}

void al_net_sntp_stop(void)
{
	sntp_stop();
}

void al_net_sntp_set_callback(void (*handler)(void))
{
	pfm_sntp_callback_fn = handler;
}
