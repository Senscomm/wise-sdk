/*
 * Copyright 2021 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_LOCAL_CONTROL_H__
#define __AYLA_LOCAL_CONTROL_H__

#include <ada/generic_session.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LC_AUTH_DATA_LENGTH	32
#define LC_RX_BUFFER_SIZE	512

enum lc_auth_state {
	LC_AUTH_STATE_NOT_AUTH = 0,
	LC_AUTH_STATE_CHALLENGE,
	LC_AUTH_STATE_AUTHED
};

struct lctrl_session {
	struct generic_session gs;	/* generic session info */
	u8 sending_prop:1;		/* indicates prop send active */
	u8 id;				/* index of session */
	u8 mask;
	u8 tx_id;			/* transmit id */
	enum lc_auth_state auth_state;	/* authentication state */
	u8 *auth_data;			/* auth data buffer */
	struct timer timer;
};

void lctrl_init(void);
void lctrl_wakeup(void);
struct generic_session *lctrl_session_alloc(void);
void lctrl_session_down(struct generic_session *gs);
enum ada_err lctrl_msg_rx(struct generic_session *gs, const u8 *buf,
    u16 length);
int lctrl_up(void);
void lctrl_down(void);
void lctrl_session_close_all(void);

#ifdef __cplusplus
}
#endif

#endif
