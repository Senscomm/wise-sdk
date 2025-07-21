/*
 * Copyright 2011-2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_ASSERT_H__
#define __AYLA_ASSERT_H__

#include <al/al_assert.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AYLA_ASSERT(expr)				\
	do {						\
		if (!(expr)) {				\
			al_assert_handle(__FILE__, __LINE__);	\
		}					\
	} while (0)

#ifndef ASSERT
#define ASSERT AYLA_ASSERT
#endif

#ifdef DEBUG
#define AYLA_ASSERT_DEBUG(expr) AYLA_ASSERT(expr)
#else
#define AYLA_ASSERT_DEBUG(expr) do { (void)(expr); } while (0)
#endif /* DEBUG */

#ifndef ASSERT_DEBUG
#define ASSERT_DEBUG		AYLA_ASSERT_DEBUG
#endif

#define AYLA_ASSERT_NOTREACHED()			\
	do {						\
		al_assert_handle(__FILE__, __LINE__);	\
	} while (1)

#ifndef ASSERT_NOTREACHED
#define ASSERT_NOTREACHED	AYLA_ASSERT_NOTREACHED
#endif

/*
 * Force a compile error if an expression is false or can't be evaluated.
 */
#ifdef __cplusplus
#define ASSERT_COMPILE(name, expr) \
	extern "C" char __ASSERT_##__name[(expr) ? 1 : -1]
#else
#define ASSERT_COMPILE(name, expr) \
	extern char __ASSERT_##__name[(expr) ? 1 : -1]
#endif

/*
 * Force a compile error if size of type is not as expected.
 */
#define ASSERT_SIZE(kind, name, size) \
	ASSERT_COMPILE(kind ## name, sizeof(kind name) == (size))

#ifdef __cplusplus
}
#endif

#endif /* __AYLA_ASSERT_H__ */
