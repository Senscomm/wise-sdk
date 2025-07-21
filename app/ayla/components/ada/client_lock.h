/*
 * Copyright 2015 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_CLIENT_LOCK_H__
#define __AYLA_CLIENT_LOCK_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CLIENT_MT

/*
 * Multi-threaded version, requiring locks.
 */
#define client_lock() client_lock_int(__func__, __LINE__);
#define client_unlock() client_unlock_int(__func__, __LINE__);

void client_lock_int(const char *, int);
void client_unlock_int(const char *, int);
void client_lock_stamp(const char *func, int line);
extern u8 client_locked;		/* for ASSERTs only */

#else

/*
 * Single-threaded version, always running in TCP/IP thread.
 */
static inline void client_lock(void)
{
}

static inline void client_unlock(void)
{
}

static inline void client_lock_stamp(const char *func, int line)
{
}

#define client_locked 1			/* satisfy ASSERTs */

#endif /* CLIENT_MT */

#ifdef __cplusplus
}
#endif

#endif /* __AYLA_CLIENT_LOCK_H__ */
