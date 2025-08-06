/*
 * Copyright 2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef _NET_IPADDR_FMT_H_
#define _NET_IPADDR_FMT_H_

/**
 * Extract a string with an IPv4 address.
 *
 * \param addr is the string, e.g. "1.2.3.4".
 * \return the value in host order, e.g., 0x01020304, or 0 on error.
 */
u32 ipaddr_fmt_str_to_ipv4(const char *addr);

/**
 * Format an IP address as a string.
 *
 * \param addr the IPv4 address in host order, e.g., 0x01020304.
 * \param buf a buffer to contain the resulting formatted address.
 * \param buflen the length of the buffer,
 * which should be at least 16 bytes long.
 * \returns a pointer to the result in the buffer.
 */
char *ipaddr_fmt_ipv4_to_str(u32 addr, char *buf, int buflen);

#endif
