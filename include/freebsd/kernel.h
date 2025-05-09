/*
 * Copyright 2007-2009 Haiku Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_COMPAT_SYS_KERNEL_H_
#define _FBSD_COMPAT_SYS_KERNEL_H_

#include <stdint.h>
#include <stddef.h>

#include "errors.h"
#include "linker_set.h"
#include <sys/queue.h>
#include "cmsis_os.h"

#define _KERNEL
/*
 *
 * The rate at which FreeBSD can generate callouts (kind of timeout mechanism).
 * For FreeBSD 8 this is typically 1000 times per second (100 for ARM).
 * This value is defined in a file called subr_param.c
 *
 * WHile Haiku can have a much higher granularity, it is not a good idea to have
 * this since FreeBSD tries to do certain tasks based on ticks, for instance
 * autonegotiation and wlan scanning.
 * Suffixing LL prevents integer overflows during calculations.
 * as it defines a long long constant.*/
#define hz	1000LL

#define ticks_to_usecs(t) (1000000*((int64_t)t) / hz)

typedef void (*system_init_func_t)(void *);

struct __system_init {
	system_init_func_t func;
};

/* TODO implement SYSINIT/SYSUNINIT subsystem */
#define SYSINIT(uniquifier, subsystem, order, func, ident) \
	struct __system_init __init_##uniquifier = { (system_init_func_t) func }

#define SYSUNINIT(uniquifier, subsystem, order, func, ident) \
	struct __system_init __uninit_##uniquifier = { (system_init_func_t) func }

extern int32_t ticks;

#endif
