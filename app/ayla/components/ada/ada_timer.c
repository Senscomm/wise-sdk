/*
 * Copyright 2022 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <ayla/utypes.h>
#include <ayla/timer.h>
#include <al/al_os_mem.h>
#include <ada/ada_timer.h>
#include "client_lock.h"
#include "client_timer.h"

/*
 * Convenience timer for other components to use where the handler
 * is called without holding the client lock.
 */

static void ada_timer_handler(struct timer *timer)
{
	struct ada_timer *atimer;

	atimer = CONTAINER_OF(struct ada_timer, timer, timer);
	client_unlock();
	atimer->handler(atimer->arg);
	client_lock();
}

void ada_timer_init(struct ada_timer *atimer,
		void (*handler)(void *), void *arg)
{
	atimer->handler = handler;
	atimer->arg = arg;
	timer_init(&atimer->timer, ada_timer_handler);
}

struct ada_timer *ada_timer_create(void (*handler)(void *), void *arg)
{
	struct ada_timer *atimer;

	atimer = al_os_mem_calloc(sizeof(*atimer));
	if (!atimer) {
		return NULL;
	}
	ada_timer_init(atimer, handler, arg);
	return atimer;
}

void ada_timer_free(struct ada_timer *atimer)
{
	if (atimer) {
		ada_timer_cancel(atimer);
		al_os_mem_free(atimer);
	}
}

void ada_timer_set(struct ada_timer *atimer, unsigned long ms)
{
	client_lock();
	client_timer_set(&atimer->timer, ms);
	client_unlock();
}

void ada_timer_cancel(struct ada_timer *atimer)
{
	client_lock();
	client_timer_cancel(&atimer->timer);
	client_unlock();
}
