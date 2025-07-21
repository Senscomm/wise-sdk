/*
 * Copyright 2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_AL_COMMON_NET_IF_H__
#define __AYLA_AL_COMMON_NET_IF_H__

#include <al/al_utypes.h>
#include <al/al_net_addr.h>

/**
 * @file
 * Platform Network Interface APIs
 */

/**
 * Opaque network interface structure.
 */
struct al_net_if;

/**
 * Network interface type.
 */
enum al_net_if_type {
	AL_NET_IF_DEF,	/**< Default interface */
	AL_NET_IF_STA,	/**< Wifi station interface */
	AL_NET_IF_AP,	/**< Wifi AP interface */
	AL_NET_IF_MAX,	/**< Wifi AP interface limit */
};

/**
 * Network technology enumeration.
 */
enum al_net_if_tech {
	AL_NET_IF_TECH_UNKNOWN,	/**< Unknown technology */
	AL_NET_IF_TECH_WIFI,	/**< Wifi technology */
	AL_NET_IF_TECH_ETHERNET,/**< Ethernet technology */
	AL_NET_IF_TECH_CELLULAR,/**< Cellular technology */
};

/**
 * Get network interface.
 *
 * This function requires that the implementation be able to
 * return the default interface.
 *
 * \param type specifies the network interface.
 *
 * \return the network interface or NULL if no interface.
 */
struct al_net_if *al_net_if_get(enum al_net_if_type type);

/**
 * Get technology type of the network interface.
 *
 * \param nif is a pointer to network interface structure.
 *
 * \return enumerated value indicating type.
 */
enum al_net_if_tech al_net_if_get_technology(struct al_net_if *nif);

/**
 * Get the IPv4 address that is associated with the network interface.
 *
 * \param nif is a pointer to network interface structure.
 *
 * \return the IPv4 address in host byte order.
 */
u32 al_net_if_get_ipv4(struct al_net_if *nif);

/**
 * Get netmask that is associated with the network interface.
 *
 * \param nif is a pointer to network interface structure.
 *
 * \return the metmask in host byte order.
 */
u32 al_net_if_get_netmask(struct al_net_if *nif);

/**
 * Get MAC address that is associated with the network interface.
 *
 * \param nif is a pointer to network interface structure.
 * \param mac_addr points a buffer to retrieve the MAC address.
 *
 * \return zero on success, -1 on error, if the network interface
 * technology is cellular, the function will always return -1.
 */
int al_net_if_get_mac_addr(struct al_net_if *nif, u8 mac_addr[6]);

/**
 * Get network address that is associated with the network interface.
 *
 * \param nif is a pointer to network interface structure.
 *
 * \return the network address structure pointer.
 */
struct al_net_addr *al_net_if_get_addr(struct al_net_if *nif);

/**
 * Get default gateway
 *
 * \param nif is a pointer to network interface structure.
 *
 * \return the default gateway address pointer, or NULL on failure.
 */
struct al_net_addr *al_net_if_get_default_gw(struct al_net_if *nif);

/**
 * Check if the specified IPV4 address is on local network or not.
 * All the network interfaces are queried in the function.
 *
 * \param addr: IPV4 address.
 *
 * The following are local network addresses.
 *	127.0.0.1 (loopback address)
 *	169.254.0.1~169.254.255.254 (dhcp address)
 * The following are not local network addresses.
 *	0.0.0.0
 *	255.255.255.255
 *	(addr & ~mask) == 0, sub-network address
 *	(addr & ~mask) == ~mask, sub-network broadcast address
 *	224.0.0.0 to 239.255.255.255, multicast address
 *
 * \return non-zero if IPv4 address is on the local network.
 */
int al_net_if_ipv4_is_local(u32 addr);

#endif /* __AYLA_AL_COMMON_NET_IF_H__ */
