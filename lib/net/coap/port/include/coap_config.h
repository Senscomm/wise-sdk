/*
 * libcoap configure implementation for wise platform.
 *
 * coap_config.h -- main header file for CoAP stack of libcoap
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

#ifndef _COAP_CONFIG_H_
#define _COA_CONFIG_H_

/* Always enabled in wise */
#ifndef WITH_POSIX
#define WITH_POSIX
#endif

#include <mbedtls/scm2010_config.h> /* Ugly ! */

#include "coap_config_posix.h"

#define HAVE_STDIO_H
#define HAVE_ASSERT_H
#define HAVE_INTTYPES_H

#define PACKAGE_STRING PACKAGE_NAME PACKAGE_VERSION

/* it's just provided by libc. i hope we don't get too many of those, as
 * actually we'd need autotools again to find out what environment we're
 * building in */
#define HAVE_STRNLEN 1

#define HAVE_LIMITS_H
#define HAVE_UNISTD_H

#define COAP_RESOURCES_NOHASH

/* Note: If neither of COAP_CLIENT_SUPPORT or COAP_SERVER_SUPPORT is set,
   then libcoap sets both for backward compatibility */
#ifdef CONFIG_COAP_CLIENT_SUPPORT
#define COAP_CLIENT_SUPPORT 1
#else  /* CONFIG_COAP_CLIENT_SUPPORT */
#define COAP_CLIENT_SUPPORT 0
#endif /* CONFIG_COAP_CLIENT_SUPPORT */

#ifdef CONFIG_COAP_SERVER_SUPPORT
#define COAP_SERVER_SUPPORT 1
#else  /* CONFIG_COAP_SERVER_SUPPORT */
#define COAP_SERVER_SUPPORT 0
#endif /* CONFIG_COAP_SERVER_SUPPORT */

#ifdef CONFIG_LWIP_IPV4
#define COAP_IPV4_SUPPORT 1
#else  /* CONFIG_LWIP_IPV4 */
#define COAP_IPV4_SUPPORT 0
#endif /* CONFIG_LWIP_IPV4 */

#ifdef CONFIG_LWIP_IPV6
#define COAP_IPV6_SUPPORT 1
#else  /* CONFIG_LWIP_IPV6 */
#define COAP_IPV6_SUPPORT 0
#endif /* CONFIG_LWIP_IPV6 */

#ifdef CONFIG_COAP_TCP_SUPPORT
#define COAP_DISABLE_TCP 0
#else  /* CONFIG_COAP_TCP_SUPPORT */
#define COAP_DISABLE_TCP 1
#endif /* CONFIG_COAP_TCP_SUPPORT */

#ifdef CONFIG_COAP_OSCORE_SUPPORT
#define COAP_OSCORE_SUPPORT 1
#else  /* CONFIG_COAP_OSCORE_SUPPORT */
#define COAP_OSCORE_SUPPORT 0
#endif /* CONFIG_COAP_OSCORE_SUPPORT */

#ifdef CONFIG_COAP_WEBSOCKETS
#define COAP_WS_SUPPORT 1
#else  /* CONFIG_COAP_WEBSOCKETS */
#define COAP_WS_SUPPORT 0
#endif /* CONFIG_COAP_WEBSOCKETS */

#ifdef CONFIG_COAP_OBSERVE_PERSIST
#define COAP_WITH_OBSERVE_PERSIST 1
#else  /* CONFIG_COAP_OBSERVE_PERSIST */
#define COAP_WITH_OBSERVE_PERSIST 0
#endif /* CONFIG_COAP_OBSERVE_PERSIST */

#ifdef CONFIG_COAP_Q_BLOCK
#define COAP_Q_BLOCK_SUPPORT 1
#else  /* CONFIG_COAP_Q_BLOCK */
#define COAP_Q_BLOCK_SUPPORT 0
#endif /* CONFIG_COAP_Q_BLOCK */

#ifdef CONFIG_COAP_ASYNC_SUPPORT
#define COAP_ASYNC_SUPPORT 1
#else  /* CONFIG_COAP_ASYNC_SUPPORT */
#define COAP_ASYNC_SUPPORT 0
#endif /* CONFIG_COAP_ASYNC_SUPPORT */

#undef COAP_MAX_LOGGING_LEVEL
#ifdef CONFIG_COAP_DEBUGGING
#define COAP_MAX_LOGGING_LEVEL CONFIG_COAP_LOG_MAX_LEVEL
#else  /* CONFIG_COAP_DEBUGGING */
#define COAP_MAX_LOGGING_LEVEL 0
#endif /* CONFIG_COAP_DEBUGGING */

#endif /* _COAP_CONFIG_H_ */
