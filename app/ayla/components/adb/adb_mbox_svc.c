/*
 * Copyright 2021-2022 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <stddef.h>
#include <string.h>

#include <ada/libada.h>
#include <ayla/log.h>
#include <ayla/timer.h>
#include <al/al_os_mem.h>
#include <ada/generic_session.h>
#include <adb/adb.h>
#include <al/al_bt.h>
#include <adb/adb_mbox_svc.h>

#if defined(AYLA_BLUETOOTH_SUPPORT) && \
    (defined(AYLA_LOCAL_CONTROL_SUPPORT) || defined(AYLA_TEST_SERVICE_SUPPORT))

#define ADB_MBOX_OUTBOX_INDEX	2
#define ADB_MBOX_MAX_MSG_LEN	2000

struct adb_mbox_state {
	enum ada_err (*msg_rx)(struct generic_session *gs, const u8 *buf,
	    u16 length);
	struct generic_session *(*session_alloc)(void);
	void (*session_down)(struct generic_session *gs);
};

static struct adb_mbox_state adb_mbox_state;

static const struct adb_attr adb_mbox_svc_table[];

static enum ada_err adb_mbox_svc_mtu_get(struct generic_session *gs,
    u32 *mtu)
{
	u16 conn = (u16)(u32)gs->ctxt;
	u16 mtu16;

	if (al_bt_conn_mtu_get(conn, &mtu16)) {
		return AE_ERR;
	}

	*mtu = mtu16;
	return AE_OK;
}

/*
 * Outbox message transmit function. This transmits the message
 * by sending an indication on the outbox. The peer device must subscribe
 * for indications to the outbox characteristic to receive messages.
 */
static enum ada_err adb_mbox_svc_msg_tx(struct generic_session *gs, u8 *msg,
    u16 length)
{
	enum ada_err err = AE_OK;
	u16 conn = (u16)(u32)gs->ctxt;
	u8 *buf;
	u16 offset = 0;
	u8 final = 0;
	u16 write_len;
	u32 mtu = ADB_GATT_MTU_DEFAULT;

	adb_mbox_svc_mtu_get(gs, &mtu);
	mtu -= ADB_NOTIFY_OVERHEAD;
	if (!gs->fragment || length <= mtu) {
		/* No fragmentation */
		if (al_bt_notify_conn(conn,
		    &adb_mbox_svc_table[ADB_MBOX_OUTBOX_INDEX],
		    msg, length)) {
			return AE_ERR;
		}
		return AE_OK;
	}

	/* Fragment to fit MTU */
	buf = al_os_mem_alloc(mtu);
	if (!buf) {
		return AE_ALLOC;
	}
	mtu -= 4;	/* 4 characters used for length */
	while (offset < length) {
		write_len = length - offset;
		if (write_len > mtu) {
			write_len = mtu;
		} else {
			final = 1;
		}
		snprintf((char *)buf, 5, "%04x", offset | (final ? 0x8000 : 0));
		memcpy(buf + 4, msg + offset, write_len);
		if (al_bt_notify_conn(conn,
		    &adb_mbox_svc_table[ADB_MBOX_OUTBOX_INDEX],
		    buf, write_len + 4)) {
			err = AE_ERR;
			goto exit;
		}
		offset += write_len;
	}

exit:
	al_os_mem_free(buf);
	return err;
}

/*
 * Close the underlying connection but leave it to the caller to free the
 * session.
 */
static enum ada_err adb_mbox_svc_session_close(struct generic_session *gs)
{
	u16 conn = (u16)(u32)gs->ctxt;

	al_bt_connection_terminate(conn);

	return AE_OK;
}

/*
 * Get the mailbox session assigned to the connection or assign a
 * session if any are available.
 */
static struct generic_session *adb_mbox_svc_session_get(u16 conn)
{
	struct adb_mbox_state *ms = &adb_mbox_state;
	struct generic_session *gs = al_bt_connection_ctxt_get(conn);

	if (gs) {
		return gs;
	}

	if (!ms->session_alloc) {
		return NULL;
	}
	gs = ms->session_alloc();
	if (!gs) {
		return NULL;
	}

	gs->ctxt = (void *)(u32)conn;
	gs->mtu_get = adb_mbox_svc_mtu_get;
	gs->msg_tx = adb_mbox_svc_msg_tx;
	gs->close = adb_mbox_svc_session_close;
	al_bt_connection_ctxt_set(conn, gs);

	return gs;
}

/*
 * End the mailbox session if one is active for the specified handle.
 * Otherwise, do nothing.
 */
static void adb_mbox_svc_session_end(u16 conn)
{
	struct adb_mbox_state *ms = &adb_mbox_state;
	struct generic_session *gs = al_bt_connection_ctxt_get(conn);

	if (gs) {
		adb_log(LOG_DEBUG
		    "shutting down mailbox session on connection %u",
		    conn);
		al_bt_connection_ctxt_set(conn, NULL);
		if (ms->session_down) {
			ms->session_down(gs);
		}
	}
}

/*
 * Connection event handler callback.
 */
static void adb_mbox_svc_conn_event_cb(enum adb_conn_event event, u16 conn,
    void *arg)
{
	switch (event) {
	case ADB_CONN_DOWN:
		adb_mbox_svc_session_end(conn);
		break;
	default:
		break;
	}
}

static enum adb_att_err adb_mbox_svc_subscribe_cb(u16 conn, u8 notify,
    u8 indicate)
{
	struct generic_session *gs;

	if (indicate) {
		gs = adb_mbox_svc_session_get(conn);
		if (!gs) {
			adb_log(LOG_WARN
			    "unable to start mailbox session");
		}
	} else {
		adb_mbox_svc_session_end(conn);
	}
	return ADB_ATT_SUCCESS;
}

static inline int rx_fragment_offset(const u8 *buf)
{
	u8 c;
	int i;
	int frag_offset = 0;

	for (i = 0; i < 4; ++i) {
		c = *buf++;
		if (c >= '0' && c <= '9') {
			c -= '0';
		} else if (c >= 'a' && c <= 'f') {
			c = c - 'a' + 10;
		} else if (c >= 'A' && c <= 'F') {
			c = c - 'A' + 10;
		} else {
			return -1;
		}
		frag_offset <<= 4;
		frag_offset += c;
	}

	return frag_offset;
}

/*
 * Mailbox inbox characteristic write handler. This characteristic is
 * virtualized per connection. Each client (session) has its
 * own message inbox. If there is already a message in the inbox (receiver
 * returns AE_BUSY), the write is failed due to insufficient resources.
 */
static enum adb_att_err adb_mbox_svc_write_cb(u16 conn,
    const struct adb_attr *attr, u8 *buf, u16 length)
{
	struct adb_mbox_state *ms = &adb_mbox_state;
	struct generic_session *gs = al_bt_connection_ctxt_get(conn);
	enum ada_err err = AE_ERR;
	enum adb_att_err attr_err;
	int frag_offset = -1;
	u8 final;

	if (!gs || !gs->active) {
		adb_log(LOG_WARN "mailbox session not found");
		return ADB_ATT_UNLIKELY;
	}

	if (gs->fragment && length >= 4) {
		/* connection supports fragmentation, check for frag length */
		frag_offset = rx_fragment_offset(buf);
	}
	if (frag_offset >= 0) {
		/* write is a fragment */
		final = (frag_offset & 0x8000) != 0;
		frag_offset &= 0x7fff;
		buf += 4;
		length -= 4;

		if (frag_offset == 0) {
			gs->rx_defrag_offset = 0;
			if (!gs->rx_defrag_buf) {
				gs->rx_defrag_buf =
				    al_os_mem_alloc(ADB_MBOX_MAX_MSG_LEN);
				if (!gs->rx_defrag_buf) {
					return ADB_ATT_INSUFFICIENT_RES;
				}
			}
		}

		if (frag_offset != gs->rx_defrag_offset ||
		    frag_offset > ADB_MBOX_MAX_MSG_LEN) {
			attr_err = ADB_ATT_INVALID_OFFSET;
			goto free_and_exit;
		}

		if (frag_offset + length > ADB_MBOX_MAX_MSG_LEN) {
			attr_err = ADB_ATT_INVALID_ATTR_VALUE_LEN;
			goto free_and_exit;
		}

		memcpy(gs->rx_defrag_buf + gs->rx_defrag_offset, buf, length);
		gs->rx_defrag_offset += length;

		if (!final) {
			return ADB_ATT_SUCCESS;
		}

		/* last fragment, forward to message rx handler */
		buf = gs->rx_defrag_buf;
		length = gs->rx_defrag_offset;
	}

	if (ms->msg_rx) {
		err = ms->msg_rx(gs, buf, length);
	}

	switch (err) {
	case AE_OK:
		attr_err = ADB_ATT_SUCCESS;
		break;
	case AE_BUSY:
		attr_err = ADB_ATT_INSUFFICIENT_RES;
		break;
	default:
		attr_err = ADB_ATT_UNLIKELY;
		break;
	}

free_and_exit:
	al_os_mem_free(gs->rx_defrag_buf);
	gs->rx_defrag_buf = NULL;
	gs->rx_defrag_offset = 0;
	return attr_err;
}

static const AL_BT_UUID128(inbox_uuid,
	/* 01000001-FE28-435B-991A-F1B21BB9BCD0 */
	0xd0, 0xbc, 0xb9, 0x1b, 0xb2, 0xf1, 0x1a, 0x99,
	0x5b, 0x43, 0x28, 0xfe, 0x01, 0x00, 0x00, 0x01);

static const AL_BT_UUID128(outbox_uuid,
	/* 01000002-FE28-435B-991A-F1B21BB9BCD0 */
	0xd0, 0xbc, 0xb9, 0x1b, 0xb2, 0xf1, 0x1a, 0x99,
	0x5b, 0x43, 0x28, 0xfe, 0x02, 0x00, 0x00, 0x01);

static const AL_BT_UUID128(mailbox_svc_uuid,
	/* 01000000-FE28-435B-991A-F1B21BB9BCD0 */
	0xd0, 0xbc, 0xb9, 0x1b, 0xb2, 0xf1, 0x1a, 0x99,
	0x5b, 0x43, 0x28, 0xfe, 0x00, 0x00, 0x00, 0x01);

static struct adb_service_info mailbox_svc = {
	.is_primary = 1,
};

static struct adb_chr_info inbox_info;
static struct adb_chr_info outbox_info;

static const struct adb_attr adb_mbox_svc_table[] = {
	ADB_SERVICE("mailbox_svc", &mailbox_svc_uuid, &mailbox_svc),
	ADB_CHR("inbox", &inbox_uuid,
	    AL_BT_AF_WRITE | AL_BT_AF_WRITE_ENC,
	    NULL, adb_mbox_svc_write_cb, &inbox_info),
	ADB_CHR_SUB("outbox", &outbox_uuid,
	    AL_BT_AF_READ_ENC | AL_BT_AF_INDICATE,
	    NULL, NULL, adb_mbox_svc_subscribe_cb, &outbox_info),
	ADB_SERVICE_END()
};

const void *adb_mbox_svc_uuid_get(void)
{
	return &mailbox_svc_uuid;
}

int adb_mbox_svc_register(const struct adb_attr **service)
{
	if (service) {
		*service = adb_mbox_svc_table;
	}

	adb_conn_event_register(adb_mbox_svc_conn_event_cb, NULL);

	return al_bt_register_service(adb_mbox_svc_table);
}

/*
 * Shutdown all active mailboxes.
 */
static void adb_mbox_shutdown_all(void)
{
	u16 cookie = 0;
	u16 handle;
	int rc;

	while (1) {
		rc = al_bt_connection_next(&handle, &cookie);
		if (rc) {
			break;
		}
		adb_log(LOG_DEBUG
		    "shutting down mailbox session on connection %u",
		    handle);
		adb_mbox_svc_session_end(handle);
	}
}

void adb_mbox_svc_callbacks_set(
    struct generic_session *(*session_alloc_cb)(void),
    void (*session_down_cb)(struct generic_session *gs),
    enum ada_err (*msg_rx_cb)(struct generic_session *gs, const u8 *buf,
    u16 length))
{
	struct adb_mbox_state *ms = &adb_mbox_state;

	/*
	 * If switching to a different session_down handler and there is an
	 * existing handler registered, shutdown any active sessions.
	 */
	if ((session_down_cb != ms->session_down) && ms->session_down) {
		adb_log(LOG_DEBUG "%s: shutdown last sessions", __func__);
		adb_mbox_shutdown_all();
	}

	ms->session_alloc = session_alloc_cb;
	ms->session_down = session_down_cb;
	ms->msg_rx = msg_rx_cb;
}
#endif
