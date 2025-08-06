/*
 * Copyright 2014 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_NET_H__
#define __AYLA_NET_H__

/*
 * This file is deprecated but kept for host-app source compatibility.
 * Note that not all features of net_callbacks, such as net_callback_wait(),
 * are available.
 * Do not use in new code.
 */
#include <ayla/callback.h>
#include <ada/client.h>

#define net_callback		callback
#define net_callback_queue	callback_queue
#define net_callback_init	callback_init
#define net_callback_pend	ada_callback_pend
#define net_callback_enqueue	callback_enqueue
#define net_callback_dequeue	callback_dequeue

#endif /* __AYLA_NET_H__ */
