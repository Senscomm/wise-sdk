/*
 * Copyright 2021 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <al/al_utypes.h>
#include <ayla/assert.h>
#include <al/al_os_lock.h>
#include <cmsis_os.h>

struct al_lock *al_os_lock_create(void)
{
	osMutexId_t lock;

	lock = osMutexNew(NULL);
	ASSERT(lock);
	return (struct al_lock *)lock;
}

void al_os_lock_lock(struct al_lock *lockp)
{
	osMutexId_t lock = (osMutexId_t)lockp;

	ASSERT(lock);
	if (osMutexAcquire(lock, osWaitForever) != osOK) {
		ASSERT_NOTREACHED();
	}
}

void al_os_lock_unlock(struct al_lock *lockp)
{
	osMutexId_t lock = (osMutexId_t)lockp;

	ASSERT(lock);
	if (osMutexRelease(lock) != osOK) {
		ASSERT_NOTREACHED();
	}
}

void al_os_lock_destroy(struct al_lock *lockp)
{
	osMutexId_t lock = (osMutexId_t)lockp;

	if (lock) {
		osMutexDelete(lock);
	}
}
