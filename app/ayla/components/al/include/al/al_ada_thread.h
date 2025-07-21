/*
 * Copyright 2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_AL_ADA_THREAD_H__
#define __AYLA_AL_ADA_THREAD_H__

#include <ayla/utypes.h>

/**
 * Watchdog timer interval, in milliseconds, for ADA thread.
 * The ADA thread must call al_ada_watchdog_timer_reset() at least this often.
 */
#define AL_ADA_WATCHDOG_TIMER	10000

/**
 * @file
 * ADA Thread Interfaces
 *
 * The ADA thread may be combined with another thread, such as the
 * LwIP TCP/IP thread, if desired.
 */

struct al_thread;

/**
 * Initialize the ADA thread.
 *
 * This must be called from the ADA thread itself.
 * This should enable the watchdog timer as well, see AL_ADA_WATCHDOG_TIMER.
 */
void al_ada_thread_init(struct al_thread *thread);

/**
 * Wakes up the al_ada_poll().
 */
void al_ada_wakeup(void);

/**
 * It blocks the thread for the max_wait ms, and do the platform polling.
 *
 * It immediately returns when al_ada_wakeup is called.
 */
void al_ada_poll(u32 max_wait);

/**
 * Indicate that the client thread is healthy.  Reset the watchdog timer.
 */
void al_ada_watchdog_timer_reset(void);

#endif /* __AYLA_AL_ADA_THREAD_H__ */
