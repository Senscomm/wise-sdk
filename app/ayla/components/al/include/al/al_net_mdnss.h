/*
 * Copyright 2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_AL_COMMON_NET_MDNSS_H__
#define __AYLA_AL_COMMON_NET_MDNSS_H__

#include <ayla/utypes.h>
#include <al/al_net_if.h>

/**
 * @file
 * mDSN service interface
 */

/**
 * Start multicast DNS server.
 *
 * \param net_if is net interface. NULL means all interfaces.
 * \param hostname is a host name in the pattern DSN.local.
 */
void al_mdns_server_up(struct al_net_if *net_if, const char *hostname);

/**
 * Stop multicast DNS server.
 *
 * \param net_if is net interface. NULL means all interfaces.
 */
void al_mdns_server_down(struct al_net_if *net_if);

#endif /* __AYLA_AL_COMMON_NET_MDNSS_H__ */
