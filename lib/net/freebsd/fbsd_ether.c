/*-
 * Copyright (c) 1982, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)if_ethersubr.c	8.1 (Berkeley) 6/10/93
 * $FreeBSD: src/sys/net/if_ethersubr.c,v 1.193.2.12 2006/08/28 02:54:14 thompsa Exp $
 */
#include <stdio.h>
#include <stdint.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include "ethernet.h"

#if 0
/*
 * This is for reference.  We have a table-driven version
 * of the little-endian crc32 generator, which is faster
 * than the double-loop.
 */
uint32_t
ether_crc32_le(const uint8_t *buf, size_t len)
{
	size_t i;
	uint32_t crc;
	int bit;
	uint8_t data;

	crc = 0xffffffff;	/* initial value */

	for (i = 0; i < len; i++) {
		for (data = *buf++, bit = 0; bit < 8; bit++, data >>= 1)
			carry = (crc ^ data) & 1;
			crc >>= 1;
			if (carry)
				crc = (crc ^ ETHER_CRC_POLY_LE);
	}

	return (crc);
}
#else
uint32_t
ether_crc32_le(const uint8_t *buf, size_t len)
{
	static const uint32_t crctab[] = {
		0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
		0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
		0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
		0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
	};
	size_t i;
	uint32_t crc;

	crc = 0xffffffff;	/* initial value */

	for (i = 0; i < len; i++) {
		crc ^= buf[i];
		crc = (crc >> 4) ^ crctab[crc & 0xf];
		crc = (crc >> 4) ^ crctab[crc & 0xf];
	}

	return (crc);
}
#endif

uint32_t
ether_crc32_be(const uint8_t *buf, size_t len)
{
	size_t i;
	uint32_t crc, carry;
	int bit;
	uint8_t data;

	crc = 0xffffffff;	/* initial value */

	for (i = 0; i < len; i++) {
		for (data = *buf++, bit = 0; bit < 8; bit++, data >>= 1) {
			carry = ((crc & 0x80000000) ? 1 : 0) ^ (data & 0x01);
			crc <<= 1;
			if (carry)
				crc = (crc ^ ETHER_CRC_POLY_BE) | carry;
		}
	}

	return (crc);
}

char *
ether_sprintf(const u_char *ap)
{
	static char etherbuf[18];
	snprintf(etherbuf, sizeof (etherbuf),
		"%02x:%02x:%02x:%02x:%02x:%02x",
		(unsigned)ap[0], (unsigned)ap[1], (unsigned)ap[2],
		(unsigned)ap[3], (unsigned)ap[4], (unsigned)ap[5]);
	return (etherbuf);
}

#include <ctype.h>

/* Input an Ethernet address and convert to binary. */
struct ether_addr *ether_aton(const char *bufp)
{
	static struct ether_addr ether;
	unsigned char *ptr;
	char c;
	int i;
	unsigned val;

	ptr = (unsigned char *)ether.octet;

	i = 0;
	while ((*bufp != '\0') && (i < sizeof(ether))) {
		val = 0;
		c = *bufp++;
		if (isdigit((int)c))
			val = c - '0';
		else if (c >= 'a' && c <= 'f')
			val = c - 'a' + 10;
		else if (c >= 'A' && c <= 'F')
			val = c - 'A' + 10;
		else
			return NULL;

		val <<= 4;
		c = *bufp;
		if (isdigit((int)c))
			val |= c - '0';
		else if (c >= 'a' && c <= 'f')
			val |= c - 'a' + 10;
		else if (c >= 'A' && c <= 'F')
			val |= c - 'A' + 10;
		else if (c == ':' || c == 0)
			val >>= 4;
		else
			return NULL;

		if (c != 0)
			bufp++;
		*ptr++ = (unsigned char) (val & 0377);
		i++;

		/* We might get a semicolon here - not required. */
		if (*bufp == ':')
			bufp++;
	}

	/* That's it.  Any trailing junk? */
	if ((i == sizeof(ether)) && (*bufp != '\0'))
		return NULL;

	return &ether;
}

char *ether_ntoa_r(const struct ether_addr *n, char *a)
{
	int i;

	i = sprintf(a, "%02x:%02x:%02x:%02x:%02x:%02x", n->octet[0],
           n->octet[1], n->octet[2], n->octet[3], n->octet[4], n->octet[5]);
        if (i < 17)
                return (NULL);
        return (a);
}

char *
ether_ntoa(const struct ether_addr *n)
{
        static char a[18];

        return (ether_ntoa_r(n, a));
}
