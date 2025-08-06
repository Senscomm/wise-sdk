/*
 * Copyright 2017 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_SNPRINTF_H__
#define __AYLA_SNPRINTF_H__

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Custom versions of vsnprintf() and snprintf().
 * This is to deal with SDKs that have different formatting support.
 */
int libayla_vsnprintf(char *bp, size_t size, const char *fmt, va_list ap);

int libayla_snprintf(char *bp, size_t size, const char *fmt, ...)
	ADA_ATTRIB_FORMAT(3, 4);

/*
 * Use the above definitions instead of the standard ones if this is included.
 */
#undef snprintf
#define snprintf libayla_snprintf
#undef vsnprintf
#define vsnprintf libayla_vsnprintf

#ifdef __cplusplus
}
#endif

#endif /* __AYLA_SNPRINTF_H__ */
