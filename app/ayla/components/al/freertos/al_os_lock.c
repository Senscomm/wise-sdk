/*
 * Copyright 2021 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <FreeRTOS.h>
#include <semphr.h>
#include <al/al_utypes.h>
#include <ayla/assert.h>
#include <al/al_os_lock.h>

struct al_lock *al_os_lock_create(void)
{
	SemaphoreHandle_t lock;

	lock = xSemaphoreCreateMutex();
	ASSERT(lock);
	return (struct al_lock *)lock;
}

void al_os_lock_lock(struct al_lock *lockp)
{
	SemaphoreHandle_t lock = (SemaphoreHandle_t)lockp;

	ASSERT(lock);
	if (xSemaphoreTake(lock, portMAX_DELAY) != pdTRUE) {
		ASSERT_NOTREACHED();
	}
}

void al_os_lock_unlock(struct al_lock *lockp)
{
	SemaphoreHandle_t lock = (SemaphoreHandle_t)lockp;

	ASSERT(lock);
	if (xSemaphoreGive(lock) != pdTRUE) {
		ASSERT_NOTREACHED();
	}
}

void al_os_lock_destroy(struct al_lock *lockp)
{
	SemaphoreHandle_t lock = (SemaphoreHandle_t)lockp;

	if (lock) {
		vSemaphoreDelete(lock);
	}
}
