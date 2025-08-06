/*
 * Copyright 2022 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <stdlib.h>
#include <string.h>
#include <al/al_os_thread.h>
#include <al/al_os_mem.h>
#include <al/al_os_lock.h>
#include <ayla/assert.h>
#include <FreeRTOS.h>
#include <task.h>
#include <platform/pfm_ada_thread.h>

/*
 * Thread local storage is required in FreeRTOS.
 * The AL uses index 1 for the AL thread information, the SDK may use index 0.
 */
#define PFM_THREAD_TLS	1	/* index into thread local storage */
#if configNUM_THREAD_LOCAL_STORAGE_POINTERS <= PFM_THREAD_TLS
#error Not enough thread local storage.  Need at least 2 pointers.
#endif

#define TASK_PRIORITY_NORMAL	(tskIDLE_PRIORITY + 2)

static const UBaseType_t task_pri[] = {
	TASK_PRIORITY_NORMAL + 1,	/* al_os_thread_pri_high */
	TASK_PRIORITY_NORMAL + 0,	/* al_os_thread_pri_nomal */
	TASK_PRIORITY_NORMAL - 1,	/* al_os_thread_pri_low */
};

/*
 * When the thread is not created by the AL layer (e.g., for the main thread),
 * check validity via MAGIC.  Use a dummy pointer for others for lock owners.
 */
#define PFM_AL_THREAD_MAGIC	0x74687264	/* "thrd" */
#define PFM_AL_THREAD_OTHER	((struct al_thread *)1) /* not AL thread */

struct al_thread {
	u32 magic;		/* PFM_AL_THREAD_MAGIC */
	TaskHandle_t task_handle;
	void *arg;
	void (*thread_main)(struct al_thread *thread, void *arg);
	volatile u8 exit_flag:1;
};

struct al_thread *al_os_thread_self(void)
{
	struct al_thread *thread;

	thread = pvTaskGetThreadLocalStoragePointer(NULL, PFM_THREAD_TLS);
	if (!thread || thread->magic != PFM_AL_THREAD_MAGIC) {
		return PFM_AL_THREAD_OTHER;	/* thread created elsewhere */
	}
	return thread;
}

static int pfm_al_os_thread_valid(struct al_thread *thread)
{
	return thread && thread != PFM_AL_THREAD_OTHER &&
	   thread->magic == PFM_AL_THREAD_MAGIC &&
	   thread->task_handle;
}

static void al_os_thread_wrapper(void *arg)
{
	struct al_thread *thread = (struct al_thread *)arg;

	ASSERT(thread);
	vTaskSetThreadLocalStoragePointer(NULL, PFM_THREAD_TLS, thread);

	thread->thread_main(thread, thread->arg);

	al_os_mem_free(thread);
	vTaskDelete(NULL);
}

struct al_thread *al_os_thread_create(const char *name,
    void *stack, size_t stack_size, enum al_os_thread_pri pri,
    void (*thread_main)(struct al_thread *thread, void *arg), void *arg)
{
	struct al_thread *thread;

	ASSERT(name);
	ASSERT((stack_size >= PFM_STACKSIZE_MIN
	    && stack_size <= PFM_STACKSIZE_MAX));
	ASSERT(pri <= al_os_thread_pri_low);
	ASSERT(thread_main);

	ASSERT(!stack);		/* caller must not provide the stack */

	thread = al_os_mem_calloc(sizeof(*thread));
	if (!thread) {
		return NULL;
	}
	thread->magic = PFM_AL_THREAD_MAGIC;
	thread->thread_main = thread_main;
	thread->arg = arg;
	thread->exit_flag = 0;

	if (xTaskCreate(al_os_thread_wrapper, name,
	    stack_size / sizeof(StackType_t),
	    thread, task_pri[pri], &thread->task_handle) != pdPASS ||
	    !thread->task_handle) {
		al_os_mem_free(thread);
		return NULL;
	}
	return thread;
}

int al_os_thread_suspend(struct al_thread *thread)
{
	ASSERT(pfm_al_os_thread_valid(thread));
	vTaskSuspend(thread->task_handle);
	return 0;
}

int al_os_thread_resume(struct al_thread *thread)
{
	ASSERT(pfm_al_os_thread_valid(thread));
	vTaskResume(thread->task_handle);
	return 0;
}

enum al_os_thread_pri al_os_thread_get_priority(struct al_thread *thread)
{
	UBaseType_t priority;
	enum al_os_thread_pri pri;

	ASSERT(pfm_al_os_thread_valid(thread));
	priority = uxTaskPriorityGet(thread->task_handle);
	if (priority == TASK_PRIORITY_NORMAL) {
		pri = al_os_thread_pri_normal;
	} else if (priority > TASK_PRIORITY_NORMAL) {
		pri = al_os_thread_pri_high;
	} else {
		pri = al_os_thread_pri_low;
	}
	return pri;
}

void al_os_thread_set_priority(struct al_thread *thread,
    enum al_os_thread_pri pri)
{
	ASSERT(pfm_al_os_thread_valid(thread));
	ASSERT(pri <= al_os_thread_pri_low);
	vTaskPrioritySet(thread->task_handle, task_pri[pri]);
}

int al_os_thread_get_exit_flag(struct al_thread *thread)
{
	ASSERT(pfm_al_os_thread_valid(thread));
	return thread->exit_flag;
}

void al_os_thread_terminate(struct al_thread *thread)
{
	ASSERT(pfm_al_os_thread_valid(thread));
	thread->exit_flag = 1;
}

void al_os_thread_sleep(int ms)
{
	ASSERT(ms >= 0);
	vTaskDelay(ms / portTICK_RATE_MS);
}

void pfm_os_thread_set_mem_type(enum al_os_mem_type type)
{
}

enum al_os_mem_type pfm_os_thread_get_mem_type(void)
{
	return al_os_mem_type_long_cache;
}
