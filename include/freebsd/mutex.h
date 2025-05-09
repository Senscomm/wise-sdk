/*
 * Copyright 2009, Colin Günther, coling@gmx.de.
 * Copyright 2007, Hugo Santos. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_COMPAT_SYS_MUTEX_H_
#define _FBSD_COMPAT_SYS_MUTEX_H_

#include "cmsis_os.h"
#include "hal/compiler.h"

struct mtx {
	osMutexId_t		mid;
};

typedef struct mtx mutex;

#define MA_OWNED		0x1
#define MA_NOTOWNED		0x2
#define MA_RECURSED		0x4
#define MA_NOTRECURSED	0x8

#define mtx_assert(mtx, what)

#define MTX_DEF				0x0000
#define MTX_RECURSE			0x0004
#define MTX_QUIET			0x40000
#define MTX_DUPOK			0x400000

void mtx_init(struct mtx*, const char*, const char*, int);
void mtx_destroy(struct mtx*);


#ifndef CONFIG_BUILD_ROM
__ilm__
#endif
static inline void
mtx_lock(struct mtx* mutex)
{
	osMutexAcquire(mutex->mid, osWaitForever);
}


#ifndef CONFIG_BUILD_ROM
__ilm__
#endif
static inline void
mtx_unlock(struct mtx* mutex)
{
	osMutexRelease(mutex->mid);
}


static inline int
mtx_owned(struct mtx* mutex)
{
	return osMutexGetOwner(mutex->mid) == osThreadGetId();
}


#endif	/* _FBSD_COMPAT_SYS_MUTEX_H_ */
