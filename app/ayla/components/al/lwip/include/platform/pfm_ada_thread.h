/*
 * Copyright 2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_PFM_ADA_THREAD_H__
#define __AYLA_PFM_ADA_THREAD_H__

#include <al/al_os_thread.h>
#include <ayla/callback.h>
#include <ayla/timer.h>

/**
 * @file
 * Platform main loop Interfaces
 */

/**
 * Initialize the PWB environment.
 *
 * \param thread is the thread structure that the ADA will be running.
 */
void pfm_ada_thread_init(struct al_thread *thread);

/**
 * Finalize the PWB environment.
 */
void pfm_ada_thread_final(void);

/*
 * Initialize the platform-specific parts for the ADA thread (internal to AL).
 */
void pfm_ada_thread_init_pfm(void);

/**
 * Add a socket to the FD_SET used by the select in the main loop.
 *
 * \param socket is the socket.
 * \param read is reading callback.
 * \param write is writing callback.
 * \param exception is exception callback.
 * \param arg is the opaque argument to be passed to callback.
 * \returns 0, or -1 if adding fails.
 *
 * The 3 callbacks first argument is the socket, the second argument
 * is opaque argument.
 *
 * The 3 callbacks can be NULL if the event don't be attended.
 */
int pfm_ada_add_socket(int socket,
		void (*read)(int socket, void *arg),
		void (*write)(int socket, void *arg),
		void (*exception)(int socket, void *arg),
		void *arg);

/**
 * Remove the socket from the FD_SET.
 *
 * \param socket is the socket.
 * \returns 0, or -1 if adding fails.
 */
int pfm_ada_remove_socket(int socket);

/**
 * Enable read callback.
 *
 * \param socket is the socket.
 * \param enable is 1 for enable or 0 for disable
 */
int pfm_ada_enable_read(int socket, int enable);

/**
 * Enable write callback.
 *
 * \param socket is the socket.
 * \param enable is 1 for enable or 0 for disable
 */
int pfm_ada_enable_write(int socket, int enable);

/**
 * Call callback function.
 *
 * The function can be called on any thread, it puts the callback structure into
 * main thread's callback queue, and then wakes up the thread, in the thread
 * loop, the callback function is called.
 *
 * \param cb is a callback structure.
 */
void pfm_callback_pend(struct callback *cb);

/**
 * Start the timer.
 *
 * \param timer points a structure that is already initialized by timer_init.
 * \param ms is delay in millisecond to trigger the callback.
 */
void pfm_timer_set(struct timer *timer, unsigned long ms);

/**
 * Cancel the started timer.
 *
 * \param timer points structure that is already initialized by timer_init.
 */
void pfm_timer_cancel(struct timer *timer);

#endif /* __AYLA_PFM_ADA_THREAD_H__ */
