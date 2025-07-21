/*
 * Copyright 2017 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_AL_COMMON_OS_LOCK_H__
#define __AYLA_AL_COMMON_OS_LOCK_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * Platform OS mutexes Interfaces
 */

/**
 * Opaque structure representing a locker.
 */
struct al_lock;

/**
 * Create a lock
 *
 * \return pointer to the locker structure or NULL on failure
 */
struct al_lock *al_os_lock_create(void);

/**
 * Lock the lock
 *
 * The function blocks the current thread when the lock is locked.
 *
 * \param lock is a pointer to the lock.
 */
void al_os_lock_lock(struct al_lock *lock);

/**
 * Unlock the lock
 *
 * \param lock is a pointer to the lock.
 */
void al_os_lock_unlock(struct al_lock *lock);

/**
 * Destroy the lock
 *
 * \param lock is a pointer to the lock.
 */
void al_os_lock_destroy(struct al_lock *lock);

#ifdef __cplusplus
}
#endif

#endif /* __AYLA_AL_COMMON_OS_LOCK_H__ */
