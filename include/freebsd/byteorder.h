/*-
 * Copyright (c) 2002 Thomas Moestl <tmm@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/sys/endian.h,v 1.6 2003/10/15 20:05:57 obrien Exp $
 */
#ifndef _FBSD_COMPAT_SYS_ENDIAN_H_
#define _FBSD_COMPAT_SYS_ENDIAN_H_

#include <stdint.h>
#include <sys/cdefs.h>
#include <hal/compiler.h>

#ifndef __BSD_VISIBLE
#define __BSD_VISIBLE		1
#endif

#include <sys/types.h>

#ifndef _BYTE_ORDER
#define _BYTE_ORDER __BYTE_ORDER
#endif

#ifndef _BIG_ENDIAN
#define _BIG_ENDIAN __BIG_ENDIAN
#endif

#ifndef _LITTLE_ENDIAN
#define _LITTLE_ENDIAN __LITTLE_ENDIAN
#endif

#ifndef __bswap16
static __inline __uint16_t
__bswap16(__uint16_t _x)
{

	return ((__uint16_t)((_x >> 8) | ((_x << 8) & 0xff00)));
}
#endif

#ifndef __bswap32
static __inline __uint32_t
__bswap32(__uint32_t _x)
{

	return ((__uint32_t)((_x >> 24) | ((_x >> 8) & 0xff00) |
	    ((_x << 8) & 0xff0000) | ((_x << 24) & 0xff000000)));
}
#endif

#ifndef __bswap64
static __inline __uint64_t
__bswap64(__uint64_t _x)
{

	return ((__uint64_t)((_x >> 56) | ((_x >> 40) & 0xff00) |
	    ((_x >> 24) & 0xff0000) | ((_x >> 8) & 0xff000000) |
	    ((_x << 8) & ((__uint64_t)0xff << 32)) |
	    ((_x << 24) & ((__uint64_t)0xff << 40)) |
	    ((_x << 40) & ((__uint64_t)0xff << 48)) | ((_x << 56))));
}
#endif

/*
 * General byte order swapping functions.
 */
#define	bswap16(x)	__bswap16(x)
#define	bswap32(x)	__bswap32(x)
#define	bswap64(x)	__bswap64(x)

#ifndef le16toh
/*
 * Host to big endian, host to little endian, big endian to host, and little
 * endian to host byte order functions as detailed in byteorder(9).
 */
#if _BYTE_ORDER == _LITTLE_ENDIAN
#define	htobe16(x)	bswap16((x))
#define	htobe32(x)	bswap32((x))
#define	htobe64(x)	bswap64((x))
#define	htole16(x)	((uint16_t)(x))
#define	htole32(x)	((uint32_t)(x))
#define	htole64(x)	((uint64_t)(x))

#define	be16toh(x)	bswap16((x))
#define	be32toh(x)	bswap32((x))
#define	be64toh(x)	bswap64((x))
#define	le16toh(x)	((uint16_t)(x))
#define	le32toh(x)	((uint32_t)(x))
#define	le64toh(x)	((uint64_t)(x))
#else /* _BYTE_ORDER != _LITTLE_ENDIAN */
#define	htobe16(x)	((uint16_t)(x))
#define	htobe32(x)	((uint32_t)(x))
#define	htobe64(x)	((uint64_t)(x))
#define	htole16(x)	bswap16((x))
#define	htole32(x)	bswap32((x))
#define	htole64(x)	bswap64((x))

#define	be16toh(x)	((uint16_t)(x))
#define	be32toh(x)	((uint32_t)(x))
#define	be64toh(x)	((uint64_t)(x))
#define	le16toh(x)	bswap16((x))
#define	le32toh(x)	bswap32((x))
#define	le64toh(x)	bswap64((x))
#endif /* _BYTE_ORDER == _LITTLE_ENDIAN */

#endif

/* Alignment-agnostic encode/decode bytestream to/from little/big endian. */

static __inline uint16_t
be16dec(const void *pp)
{
	unsigned char const *p = (unsigned char const *)pp;

	return ((p[0] << 8) | p[1]);
}

static __inline uint32_t
be32dec(const void *pp)
{
	unsigned char const *p = (unsigned char const *)pp;

	return ((p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]);
}

static __inline uint64_t
be64dec(const void *pp)
{
	unsigned char const *p = (unsigned char const *)pp;

	return (((uint64_t)be32dec(p) << 32) | be32dec(p + 4));
}

static __inline uint16_t
le16dec(const void *pp)
{
	unsigned char const *p = (unsigned char const *)pp;

	return ((p[1] << 8) | p[0]);
}

static __inline uint32_t
le32dec(const void *pp)
{
	unsigned char const *p = (unsigned char const *)pp;

	return ((p[3] << 24) | (p[2] << 16) | (p[1] << 8) | p[0]);
}

static __inline uint64_t
le64dec(const void *pp)
{
	unsigned char const *p = (unsigned char const *)pp;

	return (((uint64_t)le32dec(p + 4) << 32) | le32dec(p));
}

static __inline void
be16enc(void *pp, uint16_t u)
{
	unsigned char *p = (unsigned char *)pp;

	p[0] = (u >> 8) & 0xff;
	p[1] = u & 0xff;
}

static __inline void
be32enc(void *pp, uint32_t u)
{
	unsigned char *p = (unsigned char *)pp;

	p[0] = (u >> 24) & 0xff;
	p[1] = (u >> 16) & 0xff;
	p[2] = (u >> 8) & 0xff;
	p[3] = u & 0xff;
}

static __inline void
be64enc(void *pp, uint64_t u)
{
	unsigned char *p = (unsigned char *)pp;

	be32enc(p, u >> 32);
	be32enc(p + 4, u & 0xffffffff);
}

static __inline void
le16enc(void *pp, uint16_t u)
{
	unsigned char *p = (unsigned char *)pp;

	p[0] = u & 0xff;
	p[1] = (u >> 8) & 0xff;
}

static __inline void
le32enc(void *pp, uint32_t u)
{
	unsigned char *p = (unsigned char *)pp;

	p[0] = u & 0xff;
	p[1] = (u >> 8) & 0xff;
	p[2] = (u >> 16) & 0xff;
	p[3] = (u >> 24) & 0xff;
}

static __inline void
le64enc(void *pp, uint64_t u)
{
	unsigned char *p = (unsigned char *)pp;

	le32enc(p, u & 0xffffffff);
	le32enc(p + 4, u >> 32);
}

#ifdef CONFIG_SUPPORT_HE
static __inline void
leenc(void *pp, uint64_t u, int n)
{
	unsigned char *p = (unsigned char *)pp;

	for(; p<(unsigned char *)pp+n;) {
		*(p++) = u & 0xff;
		u >>= 8;
	}
}
#endif
#endif	/* _SYS_ENDIAN_H_ */
