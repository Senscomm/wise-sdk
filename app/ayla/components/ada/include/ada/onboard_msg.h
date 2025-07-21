/*
 * Copyright 2022 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_ONBOARD_MSG_H__
#define __AYLA_ONBOARD_MSG_H__

struct generic_session *om_session_alloc(void);
void om_session_down(struct generic_session *gs);
enum ada_err om_rx(struct generic_session *gs, const u8 *buf, u16 length);

#endif
