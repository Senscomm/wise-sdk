/*
 * Copyright 2022 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_ADA_LAN_STATE_H__
#define __AYLA_ADA_LAN_STATE_H__

/*
 * State delivered in prop_mgr event PME_LAN_STATE_CHANGE.
 */
enum ada_lan_state {
	ALS_INACTIVE = 0,	/* inactive */
	ALS_ACTIVE,		/* started */
	ALS_KEY_EXCH_FAILED,	/* key exchange failed */
};

struct ada_lan_session {
	u8	index;		/* index of LAN session */
	enum ada_lan_state state;
	u32	remote_ipv4_addr; /* IP address on the local LAN */
	u16	remote_port;	/* port for outgoing requests */
};

#endif /* __AYLA_ADA_LAN_STATE_H__ */
