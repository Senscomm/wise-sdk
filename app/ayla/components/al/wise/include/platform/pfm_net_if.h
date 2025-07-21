/*
 * Copyright 2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_PFM_NET_IF_H__
#define __AYLA_PFM_NET_IF_H__

#include <al/al_utypes.h>
#include <al/al_net_addr.h>

/**
 * @file
 * Private global functions defined in al_net_if.c
 */

/**
 * Initialize net interface module and scan all network interfaces
 * in the system.
 *
 * \return total numbers of the network interfaces found. Negative on error.
 */
int pfm_net_if_init(void);

/**
 * Finalize net interface module.
 */
void pfm_net_if_final(void);

/**
 * Attach an al_net_if to a native netif.
 * It replaces the netif for the specified type.
 *
 * \param type is the network interface type.
 * \param esp_netif is the platform-specific network interface.  It may be NULL.
 * \return the al_net_if or NULL on failure.
 */
enum al_err pfm_net_if_attach(enum al_net_if_type type, void *netif);

/**
 * Return the platform-specific network interface for a type.
 * This is for platform-specific code only.
 */
void *pfm_net_if_get(enum al_net_if_type type);

#endif /* __AYLA_PFM_NET_IF_H__ */
