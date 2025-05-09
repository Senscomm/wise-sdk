/*
 * Copyright 2007 Haiku Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_COMPAT_SYS_TASKQUEUE_H_
#define _FBSD_COMPAT_SYS_TASKQUEUE_H_

#include <sys/queue.h>
#include "_task.h"

#ifdef __WISE__
#ifndef __printflike
#define __printflike(fmtarg, firstvararg) \
	__attribute__((__format__ (__printf__, fmtarg, firstvararg)))
#endif
#endif

#define PI_NET	(osPriorityNormal)

#define TASK_INIT(taskp, prio, hand, arg) task_init(taskp, prio, hand, arg)

typedef uint32_t cpu_status;

typedef int (*taskqueue_enqueue_fn)(void *context);


struct taskqueue;

struct taskqueue *taskqueue_create(const char *name, int mflags,
	taskqueue_enqueue_fn enqueue, void *context);
int _taskqueue_start_threads(struct taskqueue **tq, int count, int pri,
	const char *name, ...) __printflike(4, 5);
extern int (*taskqueue_start_threads)(struct taskqueue **taskQueue, int count,
	int priority, const char *format, ...) __printflike(4, 5);

void taskqueue_free(struct taskqueue *tq);
void _taskqueue_drain(struct taskqueue *tq, struct task *task);
extern void (*taskqueue_drain)(struct taskqueue *taskQueue, struct task *task);

void taskqueue_block(struct taskqueue *queue);
void taskqueue_unblock(struct taskqueue *queue);
int _taskqueue_enqueue(struct taskqueue *tq, struct task *task);
extern int (*taskqueue_enqueue)(struct taskqueue *queue, struct task *task);

int taskqueue_thread_enqueue(void *context);

int taskqueue_enqueue_fast(struct taskqueue *queue, struct task *task);
struct taskqueue *taskqueue_create_fast(const char *name, int mflags,
	taskqueue_enqueue_fn enqueue, void *context);

void task_init(struct task *, int prio, task_handler_t handler, void *arg);

/* Timeout task */
#include "callout.h"

struct timeout_task {
	struct taskqueue *q;
	struct task t;
	struct callout c;
	int    f;
};

int	_taskqueue_enqueue_timeout(struct taskqueue *queue,
	    struct timeout_task *timeout_task, int ticks);
extern int (*taskqueue_enqueue_timeout)(struct taskqueue *queue,
	struct timeout_task *timeout_task, int tick);

int	taskqueue_enqueue_timeout_sbt(struct taskqueue *queue,
	    struct timeout_task *timeout_task, uint64_t sbt, uint64_t pr,
	    int flags);
int	_taskqueue_cancel_timeout(struct taskqueue *queue,
	    struct timeout_task *timeout_task, u_int *pendp);
extern int (*taskqueue_cancel_timeout)(struct taskqueue *queue,
	struct timeout_task *to_task, u_int *pendp);

void	_taskqueue_drain_timeout(struct taskqueue *queue,
	    struct timeout_task *timeout_task);
extern void (*taskqueue_drain_timeout)(struct taskqueue *queue,
	struct timeout_task *to_task);

void _timeout_task_init(struct taskqueue *queue,
						struct timeout_task *timeout_task,
						int priority,
						task_handler_t func,
						void *context);
#define	TIMEOUT_TASK_INIT(queue, timeout_task, priority, func, context) \
	_timeout_task_init(queue, timeout_task, priority, func, context);

void taskqueue_stack_size(struct taskqueue **taskQueue, uint16_t stack_sz);

#endif
