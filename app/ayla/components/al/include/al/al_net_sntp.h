/*
 * Copyright 2022 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_AL_NET_SNTP_H__
#define __AYLA_AL_NET_SNTP_H__

/**
 * Simple Network Time Protocol Interfaces.
 */

/**
 * Set the servers to be used for SNTP.
 * The number of servers supported is platform-dependent.
 *
 * \param index the index of the server name, zero-based.
 * \param name the server host name.  The implementation may continue to
 * reference this name pointer after returning, so it must remain stable.
 * \returns an error if the server name wasn't used.
 */
enum al_err al_net_sntp_server_set(unsigned int index, const char *name);

/**
 * Start SNTP service.
 *
 * \returns zero on success, or an error code if the service did not start.
 */
enum al_err al_net_sntp_start(void);

/**
 * Stop SNTP service.
 */
void al_net_sntp_stop(void);

/**
 * Set callback indicating that the time has been set by SNTP.
 *
 * The callback function may be called from any thread.
 *
 * \param handler callback function.
 */
void al_net_sntp_set_callback(void (*handler)(void));

#endif /* __AYLA_AL_NET_SNTP_H__ */
