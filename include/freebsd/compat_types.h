/*
 * Copyright 2007 Haiku Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_COMPAT_SYS_TYPES_H_
#define _FBSD_COMPAT_SYS_TYPES_H_

#include <sys/cdefs.h>
#include <sys/types.h>

typedef __const char* c_caddr_t;

/* 
 * Some toolchans doesn't support these at all
 * because they are obsolete.
 * But we still need them in network stack.
 */
#if ___int8_t_defined
typedef __uint8_t	u_int8_t;
#endif
#if ___int16_t_defined
typedef __uint16_t	u_int16_t;
#endif 
#if ___int32_t_defined
typedef __uint32_t	u_int32_t;
#endif

#if ___int64_t_defined
typedef __uint64_t	u_int64_t;

typedef	__uint64_t	u_quad_t;
typedef	__int64_t	quad_t;
typedef	quad_t *	qaddr_t;
#endif

#endif
