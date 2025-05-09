/*
 * Copyright 2009, Colin Günther, coling@gmx.de.
 * Copyright 2007, Hugo Santos. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_COMPAT_SYS_SYSTM_H_
#define _FBSD_COMPAT_SYS_SYSTM_H_


#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "atomic.h"

#include "callout.h"
#include <sys/cdefs.h>
#include <sys/queue.h>

#include "libkern.h"


#define ovbcopy(f, t, l) bcopy((f), (t), (l))

#ifdef __WISE__
#include <assert.h>

#ifndef CONFIG_BUILD_ROM
#define KASSERT(cond, msg) 	assert(cond)
#else
/* for increasing ROM Space, disable the KASSERT log*/
#define KASSERT(cond, msg) do {			\
	if (!(cond)) ;\
} while (0)

#endif

#else
#define KASSERT(cond,msg) do {			\
	if (!(cond))				\
		printf msg;			\
} while (0)
#endif

#ifndef CTASSERT /* Allow lint to override */
#define CTASSERT(x)		_CTASSERT(x, __LINE__)
#define _CTASSERT(x, y)		__CTASSERT(x, y)
#define __CTASSERT(x, y)	typedef char __assert ## y[(x) ? 1 : -1]
#endif

/* min and max comparisons */
#ifndef __cplusplus
#	ifndef min
#		define min(a,b) ((a)>(b)?(b):(a))
#	endif
#	ifndef max
#		define max(a,b) ((a)>(b)?(a):(b))
#	endif
#endif


static inline int
copyin(const void * __restrict udaddr, void * __restrict kaddr,
	size_t len)
{
	memcpy(kaddr, udaddr, len);
	return 0;
}


static inline int
copyout(const void * __restrict kaddr, void * __restrict udaddr,
	size_t len)
{
	memcpy(udaddr, kaddr, len);
	return 0;
}

#endif	/* _FBSD_COMPAT_SYS_SYSTM_H_ */
