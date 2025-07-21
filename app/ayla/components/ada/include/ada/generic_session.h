/*
 * Copyright 2022 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_GENERIC_SESSION_H__
#define __AYLA_GENERIC_SESSION_H__

#include <al/al_os_mem.h>

#ifdef __cplusplus
extern "C" {
#endif

struct generic_session {
	u8 active:1;			/* indicates session is in use */
	u8 fragment:1;			/* indicates fragmentation allowed */
	u8 authed:1;			/* indicates peer authenticated */
	void *ctxt;			/* lower layer session context */
	size_t rx_length;
	u8 *rx_buffer;			/* receive buffer */
	u8 *rx_defrag_buf;		/* pointer to rx defrag buffer */
	size_t rx_defrag_offset;	/* offset into rx buffer */
	enum ada_err (*mtu_get)(struct generic_session *gs, u32 *mtu);
	enum ada_err (*msg_tx)(struct generic_session *gs, u8 *msg,
	    u16 length);
	enum ada_err (*close)(struct generic_session *gs);
	enum ada_err (*keep_alive)(struct generic_session *gs, u32 period);
};

static inline void generic_session_close(struct generic_session *gs)
{
	gs->active = 0;
	al_os_mem_free(gs->rx_defrag_buf);
	gs->rx_defrag_buf = NULL;
	gs->rx_defrag_offset = 0;
}

#ifdef __cplusplus
}
#endif

#endif
