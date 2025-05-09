/*
 * libcoap configure implementation for wise platform.
 *
 * Uses libcoap software implementation for failover when concurrent
 * configure operations are in use.
 *
 * coap_config_posix.h -- main header file for CoAP stack of libcoap
 *
 * Copyright 2024-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * Copyright (C) 2010-2012,2015-2024 Olaf Bergmann <bergmann@tzi.org>
 *               2015 Carsten Schoenert <c.schoenert@t-online.de>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * This file is part of the CoAP library libcoap. Please see README for terms
 * of use.
 */

#ifndef _COAP_CONFIG_POSIX_H_
#define _COAP_CONFIG_POSIX_H_

#ifdef WITH_POSIX

#include <sys/socket.h>
#include <net/if.h>

#define HAVE_ERRNO_H
#define HAVE_SYS_SOCKET_H
#define HAVE_SYS_IOCTL_H
#define HAVE_MALLOC
#define HAVE_ARPA_INET_H
#define HAVE_TIME_H
#define HAVE_NETDB_H
#define HAVE_NETINET_IN_H
/* #define HAVE_STRUCT_CMSGHDR */
#define HAVE_IF_NAMETOINDEX

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#define ipi_spec_dst ipi_addr
struct in6_pktinfo {
    struct in6_addr ipi6_addr;        /* src/dst IPv6 address */
    unsigned int ipi6_ifindex;        /* send/recv interface index */
};
#define IN6_IS_ADDR_V4MAPPED(a) \
        ((((__const uint32_t *) (a))[0] == 0)                                 \
         && (((__const uint32_t *) (a))[1] == 0)                              \
         && (((__const uint32_t *) (a))[2] == htonl (0xffff)))

#define IN6_IS_ADDR_MULTICAST(a) ((((__const uint32_t *)(a))[0] & htonl(0xff000000UL)) == htonl(0xff000000UL))

#define PACKAGE_NAME "libcoap-posix"
#define PACKAGE_VERSION "4.3.4"

#if defined(CONFIG_MBEDTLS_TLS_CLIENT)\
|| defined(CONFIG_MBEDTLS_TLS_SERVER)
#define CONFIG_MBEDTLS_TLS_ENABLED
#else
#undef CONFIG_MBEDTLS_TLS_ENABLED
#endif

#ifdef CONFIG_COAP_WITH_MBEDTLS
#ifdef CONFIG_MBEDTLS_TLS_ENABLED
#define COAP_WITH_LIBMBEDTLS
#endif /* CONFIG_MBEDTLS_TLS_ENABLED */
#endif /* CONFIG_COAP_WITH_MBEDTLS */

#define COAP_DEFAULT_MAX_PDU_RX_SIZE CONFIG_TCP_MSL

#define COAP_CONSTRAINED_STACK 1

#endif /* WITH_POSIX */
#endif /* _COAP_CONFIG_POSIX_H_ */
