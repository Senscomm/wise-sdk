/*
 * Copyright 2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_CALLBACK_H__
#define __AYLA_CALLBACK_H__

#include <ayla/utypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The callback structure.
 */
struct callback {
	u8 pending;		/* The pending flag */
	u8 impl_priv;		/* reserved for the infrastructure */
	void (*func)(void *);	/* The callback function */
	void *arg;		/* The argument is passed to callback
				    function */

	struct callback *next;
};

/*
 * The callback queue structure.
 */
struct callback_queue {
	struct callback *head;
	struct callback *tail;
};

/**
 * Initialize the callback structure.
 *
 * \param cb is a callback structure.
 * \param func is a callback function.
 * \param arg is a argument to pass to the callback function.
 */
void callback_init(struct callback *cb, void (*func)(void *), void *arg);

/**
 * Enqueue a task to the queue.
 *
 * \param queue is a callback queue.
 * \param cb is a callback structure.
 */
void callback_enqueue(struct callback_queue *queue, struct callback *cb);

/**
 * Dequeue a task from a callback queue
 *
 * \param queue is a callback queue.
 *
 * \return points a callback or NULL if the queue is empty.
 */
struct callback *callback_dequeue(struct callback_queue *queue);

/**
 * Remove a task from a callback queue
 *
 * \param queue is a callback queue.
 * \param cb is a callback structure.
 */
void callback_remove(struct callback_queue *queue, struct callback *cb);

#ifdef __cplusplus
}
#endif

#endif /* __AYLA_CALLBACK_H__ */
