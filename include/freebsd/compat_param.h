/*
 * Copyright 2009, Colin Günther, coling@gmx.de.
 * Copyright 2007, Hugo Santos. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_COMPAT_SYS_PARAM_H_
#define _FBSD_COMPAT_SYS_PARAM_H_


#include <sys/param.h>
#include <sys/types.h>
#include <sys/cdefs.h>
#include <sys/time.h>

#include "byteorder.h"
#include "errno.h"

/* The version this compatibility layer is based on */
#define __FreeBSD_version 800107

#define MAXBSIZE	0x10000

#define PAGE_SHIFT	12
#define PAGE_MASK	(PAGE_SIZE - 1)

#define trunc_page(x)	((x) & ~PAGE_MASK)

#define ptoa(x)			((unsigned long)((x) << PAGE_SHIFT))
#define atop(x)			((unsigned long)((x) >> PAGE_SHIFT))

/* MAJOR FIXME */
#define Maxmem			(32768)

#ifndef CONFIG_FREEBSD_MBUF_CACHE_SIZE
#define CONFIG_FREEBSD_MBUF_CACHE_SIZE 1024
#endif
#ifndef CONFIG_FREEBSD_MBUF_CHUNK_SIZE
#define CONFIG_FREEBSD_MBUF_CHUNK_SIZE 1024
#endif

#ifndef __MSIZE__
#ifndef __WISE__
#define __MSIZE__ 256
#else
#define __MSIZE__ CONFIG_FREEBSD_MBUF_CACHE_SIZE
#endif
#endif /* __MSIZE__ */


#ifndef __WISE__
#ifndef	MCLSHIFT
#define MCLSHIFT	11		/* convert bytes to mbuf clusters */
#endif	/* MCLSHIFT */

#define __MCLBYTES__	(1 << MCLSHIFT)	/* size of an mbuf cluster */

#else

#define __MCLBYTES__	(CONFIG_FREEBSD_MBUF_CHUNK_SIZE)
#endif


#define	MJUMPAGESIZE		PAGE_SIZE
#define	MJUM9BYTES		(9 * 1024)
#define	MJUM16BYTES		(16 * 1024)

#define ALIGN_BYTES		(sizeof(unsigned long) - 1)
#define ALIGN(x)		((((unsigned long)x) + ALIGN_BYTES) & ~ALIGN_BYTES)

/* FIXME: check if this is necessary */
#define __NO_STRICT_ALIGNMENT
/*#define	ALIGNED_POINTER(p, t)	((((u_long)(p)) & (sizeof(t) - 1)) == 0)*/

/* Macros for counting and rounding. */
#ifndef howmany
#define	howmany(x, y)	(((x)+((y)-1))/(y))
#endif

#define	nitems(x)	(sizeof((x)) / sizeof((x)[0]))
#ifndef roundup
#define roundup(x, y)	((((x)+((y)-1))/(y))*(y))  /* to any y */
#endif
#ifndef roundup2
#define roundup2(x, y)	(((x) + ((y) - 1)) & (~((y) - 1)))
#endif
#ifndef rounddown
#define rounddown(x, y)  (((x) / (y)) * (y))
#endif

#define	PRIMASK	0x0ff
#define	PCATCH	0x100
#define	PDROP	0x200
#define	PBDRY	0x400


#ifndef NBBY
#define	NBBY	8		/* number of bits in a byte */
#endif

/* Bit map related macros. */
#undef setbit
#undef clrbit
#undef isset
#undef isclr

#define	setbit(a,i)	(((unsigned char *)(a))[(i)/NBBY] |= 1<<((i)%NBBY))
#define	clrbit(a,i)	(((unsigned char *)(a))[(i)/NBBY] &= ~(1<<((i)%NBBY)))
#define	isset(a,i)	(((unsigned char *)(a))[(i)/NBBY] & (1<<((i)%NBBY)))
#define	isclr(a,i)	((((unsigned char *)(a))[(i)/NBBY] & (1<<((i)%NBBY))) == 0)

#endif	/* _FBSD_COMPAT_SYS_PARAM_H_ */
