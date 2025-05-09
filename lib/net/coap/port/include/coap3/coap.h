/* Modify head file implementation for wise platform.
 *
 * coap.h -- main header file for CoAP stack of libcoap
 *
 * Copyright 2024-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.

 * Copyright (C) 2010-2012,2015-2024 Olaf Bergmann <bergmann@tzi.org>
 *               2015 Carsten Schoenert <c.schoenert@t-online.de>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * This file is part of the CoAP library libcoap. Please see README for terms
 * of use.
 */

#ifndef _COAP_H_
#define _COAP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "coap3/libcoap.h"

#include "coap3/coap_forward_decls.h"
#include "coap3/coap_address.h"
#include "coap3/coap_async.h"
#include "coap3/coap_block.h"
#include "coap3/coap_cache.h"
#include "coap3/coap_debug.h"
#include "coap3/coap_dtls.h"
#include "coap3/coap_encode.h"
#include "coap3/coap_event.h"
#include "coap3/coap_io.h"
#include "coap3/coap_mem.h"
#include "coap3/coap_net.h"
#include "coap3/coap_option.h"
#include "coap3/coap_oscore.h"
#include "coap3/coap_pdu.h"
#include "coap3/coap_prng.h"
#include "coap3/coap_str.h"
#include "coap3/coap_resource.h"
#include "coap3/coap_subscribe.h"
#include "coap3/coap_time.h"
#include "coap3/coap_uri.h"
#include "coap3/coap_ws.h"

#ifdef __cplusplus
}
#endif

#endif /* _COAP_H_ */
