/*
 * Copyright 2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <stdio.h>
#include <string.h>

#include <ayla/utypes.h>
#include <ayla/ipaddr_fmt.h>

u32 ipaddr_fmt_str_to_ipv4(const char *cp)
{
	unsigned int data[4];
	int rc;

	rc = sscanf(cp, "%u.%u.%u.%u", data + 3, data + 2, data + 1, data);
	if (rc != 4) {
		return 0;
	}
	if (data[0] > 255 || data[1] > 255 || data[2] > 255 || data[3] > 255) {
		return 0;
	}

	return  data[3] << 24 | data[2] << 16 | data[1] << 8 | data[0];
}

char *ipaddr_fmt_ipv4_to_str(u32 addr, char *buf, int buflen)
{
	snprintf(buf, buflen, "%u.%u.%u.%u", (u8)(addr >> 24),
	    (u8)(addr >> 16), (u8)(addr >> 8) & 0xff, (u8)addr);
	return buf;
}
