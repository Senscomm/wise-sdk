/*
 * Copyright 2010, Axel Dörfler, axeld@pinc-software.de.
 * Copyright 2009, Colin Günther, coling@gmx.de.
 * Copyright 2007, Hugo Santos. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_COMPAT_SYS_CALLOUT_H_
#define _FBSD_COMPAT_SYS_CALLOUT_H_


#include "cmsis_os.h"

struct callout {
	osTimerId_t			tmid;
	void *				c_arg;
	void				(*c_func)(void *);
	osMutexId_t			mtx;
};

void _callout_init(struct callout *c);
extern void (*callout_init)(struct callout *c);

void _callout_delete(struct callout *c);
extern void (*callout_delete)(struct callout *c);

/* Time values are in ticks, see compat/sys/kernel.h for its definition */
int	callout_schedule(struct callout *c, int ticks);
int	_callout_reset(struct callout *c, int ticks, void (*func)(void *), void *arg);
extern int (*callout_reset)(struct callout *c, int ticks, void (*func)(void *), void *arg);

void _callout_delete(struct callout *c);
extern void (*callout_delete)(struct callout *c);

int callout_pending(struct callout *c);

#define	callout_drain(c)	_callout_stop_safe(c)
#define	callout_stop(c)		_callout_stop_safe(c)
int	__callout_stop_safe(struct callout *c);
extern int (*_callout_stop_safe)(struct callout *c);


#define	CALLOUT_RETURNUNLOCKED	0x0010 /* handler returns with mtx unlocked */

void	_callout_init_lock(struct callout *, osMutexId_t, int);
#define	callout_init_mtx(c, mtx, flags)									\
	_callout_init_lock((c), ((mtx) != NULL) ? (mtx)->mid : NULL, (flags))

#endif	/* _FBSD_COMPAT_SYS_CALLOUT_H_ */
