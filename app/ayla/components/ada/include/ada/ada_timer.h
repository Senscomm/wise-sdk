/*
 * Copyright 2022 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_ADA_TIMER_H__
#define __AYLA_ADA_TIMER_H__

#include <ayla/timer.h>

/*
 * Alternative timer to be used with ada_timer_* APIs.
 * This is used when client_lock is not desired during timer operations.
 */
struct ada_timer {
	struct timer timer;
	void (*handler)(void *arg);
	void (*arg);
};

/*
 * Allocate an ADA timer.
 */
struct ada_timer *ada_timer_create(void (*handler)(void *arg), void *arg);

/*
 * Free an ADA timer.
 * The timer is cancelled if it is pending.
 * The argument may be NULL.
 */
void ada_timer_free(struct ada_timer *);

/*
 * Initialize an ADA timer that was pre-allocated.
 */
void ada_timer_init(struct ada_timer *, void (*handler)(void *arg), void *arg);

/*
 * Set a timer for the client thread.
 * For internal use only.
 * The client lock must not be held when this is called.
 * The client lock will *not* be held during the handler.
 */
void ada_timer_set(struct ada_timer *, unsigned long ms);

/*
 * Cancel a timer set by ada_timer_set().
 * The client lock must not be held when this is called.
 */
void ada_timer_cancel(struct ada_timer *);

#endif /* __AYLA_ADA_TIMER_H__ */
