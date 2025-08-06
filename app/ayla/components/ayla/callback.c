/*
 * Copyright 2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <string.h>
#include <ayla/utypes.h>
#include <ayla/callback.h>
#include <ayla/assert.h>

void callback_init(struct callback *cb, void (*func)(void *), void *arg)
{
	if (cb == NULL || func == NULL) {
		return;
	}
	memset(cb, 0, sizeof(*cb));
	cb->func = func;
	cb->arg = arg;
}

void callback_enqueue(struct callback_queue *queue, struct callback *cb)
{
	if (queue == NULL || cb == NULL || cb->pending) {
		return;
	}
	AYLA_ASSERT(cb->func);

	cb->pending = 1;
	if (queue->tail) {
		queue->tail->next = cb;
		queue->tail = cb;
	} else {
		queue->head = cb;
		queue->tail = cb;
	}
	cb->next = NULL;
}

struct callback *callback_dequeue(struct callback_queue *queue)
{
	struct callback *cb;

	if (queue == NULL) {
		return NULL;
	}

	cb = queue->head;
	if (cb == NULL) {
		return NULL;
	}
	queue->head = cb->next;
	if (queue->head == NULL) {
		queue->tail = NULL;
	}

	return cb;
}

void callback_remove(struct callback_queue *queue, struct callback *cb)
{
	struct callback **t;
	struct callback *pre;

	if (queue == NULL || cb == NULL || !cb->pending) {
		return;
	}
	t = &queue->head;
	pre = NULL;
	while (*t != NULL) {
		if (*t == cb) {
			if (*t == queue->tail) {
				queue->tail = pre;
			}
			*t = cb->next;
			cb->pending = 0;
			return;
		}
		pre = *t;
		t = &(*t)->next;
	}
}

#ifdef TODO
/*
 * Call all pending callbacks on the queue.
 * Note: each callback is only called once, even if it is repended.
 */
void callback_queue_call(struct callback_qeuue *queue)
{
	struct callback_queue work_queue;
	struct callback *ac;

	if (!queue || !queue->head) {
		return;
	}

	/*
	 * move pending callbacks to a temporary queue.
	 */
	work_queue = *queue;	/* struct copy */
	queue->head = NULL;
	queue->tail = NULL;

	while (1) {
		ac = callback_dequeue(&work_queue);
		if (!ac) {
			break;
		}
		AYLA_ASSERT(ac->pending);
		ac->pending = 0;
		ac->func(ac->arg);
	}
}
#endif
