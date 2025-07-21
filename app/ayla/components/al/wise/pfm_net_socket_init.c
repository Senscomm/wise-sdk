/*
 * Copyright 2022 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <ayla/log.h>
#include <platform/pfm_net_socket.h>

/*
 * Initialize the socket layer and start LwIP.
 */
void pfm_net_socket_init(void)
{
	pfm_net_dns_init();

	/* There is nothing more to do here because
	 * lWIP has already been initialized, i.e. tcpip_init() done,
	 * in net_init().
	 */
}
