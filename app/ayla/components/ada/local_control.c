/*
 * Copyright 2021 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifdef AYLA_LOCAL_CONTROL_SUPPORT

/*
 * This file contains transport independent handling of local control
 * operations between mobile clients and devices. Local control can be
 * used over a variety of message transport mechanisms such as Bluetooth GATT
 * MQTT, HTTP, etc.
 */
#include <stddef.h>
#include <string.h>

#include <ayla/utypes.h>
#include <ayla/endian.h>
#include <ayla/log.h>
#include <ayla/mod_log.h>
#include <ayla/timer.h>
#include <ayla/callback.h>
#include <al/al_os_mem.h>
#include <al/al_random.h>
#include <ada/libada.h>
#include <ada/ada_conf.h>
#include <ada/local_control.h>
#include <al/al_hmac_sha256.h>

#include "ame.h"
#include "ame_json.h"
#include "client_lock.h"
#include "clientv2_msg.h"
#include "client_timer.h"

/*
 * Uncomment the following define for manual testing without requiring
 * response messages to ack upstream requests.
 */
/* #define TEST_WITHOUT_ACKS */

#define LCTRL_TX_BUF_SZ		512
#define LCTRL_KVP_STACK_SZ	15
#define LCTRL_SESSION_CT	3
#define LCTRL_RESP_TIMEOUT	5000
/*
 * Receive operation table. Ops are looked up by a linear search.
 * For efficiency, the table is arranged with more frequently occurring
 * ops near the top of the table.
 */
enum lctrl_rx_op {
	LCRO_POST_PROBE_REQ,
	LCRO_GET_PROP_REQ,
	LCRO_POST_PROP_REQ,
	LCRO_POST_DATAPOINT_RSP,
	LCRO_POST_AUTH_CLIENT_REQ,
	LCRO_GET_LOG_MSGS_REQ,
	LCRO_GET_DEVICE_INFO_REQ,
	LCRO_POST_KEEP_ALIVE_REQ,
	LCRO_COUNT	/* keep at end */
};

/*
 * Enable/disable flag. Operates at the protocol level, not the interface
 * level.
 */
static u8 lctrl_enabled;

/*
 * Local control session table is used to track active sessions.
 */
static struct lctrl_session session_table[LCTRL_SESSION_CT];

/*
 * Local control callback is use to execute most local control handling
 * on the client thread.
 */
static struct callback lctrl_callback;

/*
 * Variables used only on the client thread in a serialized fashion so
 * they can be shared across sessions.
 */
static const struct ame_encoder *lctrl_encoder;
static struct ame_encoder_state lctrl_encoder_state;
static const struct ame_decoder *lctrl_decoder;
static struct ame_decoder_state lctrl_decoder_state;
static struct ame_kvp lctrl_kvp_stack[LCTRL_KVP_STACK_SZ];
static u8 lctrl_tx_buf[LCTRL_TX_BUF_SZ];

static struct prop_request_info prop_request_info;

static void lctrl_session_free(struct lctrl_session *session);

/*
 * Wake up local control event handler on client thread.
 */
void lctrl_wakeup(void)
{
	client_callback_pend(&lctrl_callback);
}

static enum ada_err lctrl_msg_tx(
    struct generic_session *gs,
    void (*msg_encoder)(const struct ame_encoder *enc,
	struct ame_encoder_state *es, u32 id, s32 status, const void *arg),
    u32 id, s32 status, const void *arg)
{
	const struct ame_encoder *enc = lctrl_encoder;
	struct ame_encoder_state *es = &lctrl_encoder_state;
	u32 mtu = MAX_U32;

	gs->mtu_get(gs, &mtu);
	if (mtu > sizeof(lctrl_tx_buf)) {
		mtu = sizeof(lctrl_tx_buf);
	}
	ame_encoder_buffer_set(es, lctrl_tx_buf, mtu);
	msg_encoder(enc, es, id, status, arg);
	return gs->msg_tx(gs, es->buffer, es->offset);
}

static enum ada_status lctrl_prop_recv(struct lctrl_session *session,
    const struct cm_rx_op *op, struct ame_kvp *payload, char *name,
    size_t length, u8 is_schedule)
{
	const struct ame_decoder *dec = lctrl_decoder;

	return clientv2_msg_prop_recv(session->mask, dec, op->name,
	    payload, name, length, is_schedule);
}

static enum ada_err lctrl_get_prop_cb(struct prop *prop, void *arg,
    enum ada_err err)
{
	enum ada_status st;
	struct prop_request_info *req_info = arg;

	if (err || !prop || prop->type == ATLV_SCHED) {
		if (err) {
			st = clientv2_msg_err_to_status(err);
		} else {
			st = AST_INVAL_VAL;
		}
		lctrl_msg_tx(req_info->ctxt,
		    clientv2_msg_enc_prop_name, req_info->id, st, req_info);
	} else {
		lctrl_msg_tx(req_info->ctxt,
		    clientv2_msg_enc_get_prop_rsp, req_info->id, AST_SUCCESS,
		    prop);
	}
	req_info->in_use = 0;

	return AE_OK;
}

static enum ada_err lctrl_hdl_get_prop_req(const struct cm_rx_op *op,
    struct generic_session *gs, struct ame_kvp *payload, u32 id,
    enum ada_status status)
{
	char *name;
	enum ada_err err;
	enum ada_status st;
	const struct ame_decoder *dec = lctrl_decoder;
	struct prop_request_info *req_info = &prop_request_info;
	struct lctrl_session *session =
	    CONTAINER_OF(struct lctrl_session, gs, gs);

	if (req_info->in_use) {
		/* busy, ignore request and handle later */
		return AE_OK;
	}

	req_info->in_use = 1;
	req_info->id = id;
	req_info->ctxt = session;
	req_info->resp_name = op->resp_name;
	name = req_info->name;
	err = clientv2_msg_dec_get_prop(op->name, dec, payload, name,
	    sizeof(req_info->name));
	if (err) {
		goto error_exit;
	}

	if (name[0] == '\0') {
		st = AST_FAILURE;	/* GET ALL not supported */
		goto done;
	}

	err = ada_prop_mgr_get(name, lctrl_get_prop_cb, req_info);
	if (!err) {
		return AE_OK;
	}

error_exit:
	st = clientv2_msg_err_to_status(err);
done:
	lctrl_msg_tx(gs, clientv2_msg_enc_prop_name, id, st, req_info);
	req_info->in_use = 0;
	return AE_OK;
}

static enum ada_err lctrl_hdl_post_probe_req(const struct cm_rx_op *op,
    struct generic_session *gs, struct ame_kvp *payload, u32 id,
    enum ada_status status)
{
	lctrl_msg_tx(gs, clientv2_msg_enc_basic, id, AST_SUCCESS,
	    op->resp_name);

	return AE_OK;
}

static enum ada_err lctrl_hdl_post_auth_client_req(const struct cm_rx_op *op,
    struct generic_session *gs, struct ame_kvp *payload, u32 id,
    enum ada_status status)
{
	struct ada_lan_conf *lcf = &ada_lan_conf;
	const struct ame_decoder *dec = lctrl_decoder;
	enum ada_err err;
	enum ada_status st = AST_SUCCESS;
	struct cm_auth_client_info auth_info;
	u8 buf[CM_ACM_CHLNG_RSP_LENGTH];
	struct al_hmac_ctx *ctx;
	const char seed[] = "response";
	struct lctrl_session *session =
	    CONTAINER_OF(struct lctrl_session, gs, gs);

	auth_info.buf = buf;
	auth_info.length = sizeof(buf);
	err = clientv2_msg_dec_auth_client_req(op->name, dec, payload,
	    &auth_info);
	if (err) {
		st = clientv2_msg_err_to_status(err);
		goto done;
	}

	switch (auth_info.type) {
	case CMCA_TYPE_START:
		client_log(LOG_INFO "auth start session %d id %s",
		    session->id, (const char *)buf);
		if (!CLIENT_LANIP_HAS_KEY(lcf)) {
			client_log(LOG_ERR "no LAN IP key");
			st = AST_INVAL_STATE;
			goto done;
		}
		if (!session->auth_data) {
			session->auth_data =
			    al_os_mem_alloc(LC_AUTH_DATA_LENGTH);
			if (!session->auth_data) {
				client_log(LOG_ERR "auth_data alloc failed");
				st = AST_SHORTAGE;
				goto done;
			}
		}
		session->auth_state = LC_AUTH_STATE_CHALLENGE;
		gs->authed = 0;
		al_random_fill(session->auth_data, CM_ACM_CHLNG_RSP_LENGTH);
		auth_info.buf = session->auth_data;
		auth_info.length = CM_ACM_CHLNG_RSP_LENGTH;
		break;
	case CMCA_TYPE_RESPONSE:
		if (session->auth_state != LC_AUTH_STATE_CHALLENGE ||
		    !session->auth_data) {
			st = AST_INVAL_STATE;
			goto done;
		}

		/*
		 * Expects:
		 * resp = SHA256(lanip_key + "response" + challenge)
		 */
		ctx = al_hmac_sha256_new();
		if (!ctx) {
			st = AST_SHORTAGE;
			goto done;
		}
		al_hmac_sha256_init(ctx, lcf->lanip_key,
		    strlen(lcf->lanip_key));
		al_hmac_sha256_update(ctx, seed, sizeof(seed) - 1);
		al_hmac_sha256_update(ctx, session->auth_data,
		    CM_ACM_CHLNG_RSP_LENGTH);
		al_hmac_sha256_final(ctx, session->auth_data);
		al_hmac_sha256_free(ctx);

		if (memcmp(session->auth_data, buf, CM_ACM_CHLNG_RSP_LENGTH)) {
			st = AST_AUTH_FAIL;
			session->auth_state = LC_AUTH_STATE_NOT_AUTH;
			gs->authed = 0;
			client_log(LOG_WARN "session %d auth response check "
			    "failed using lanip key id %u",
			    session->id, lcf->lanip_key_id);
			goto done;
		}

		session->auth_state = LC_AUTH_STATE_AUTHED;
		gs->authed = 1;
		client_log(LOG_INFO "auth success session %d", session->id);
		break;
	default:
		ASSERT_NOTREACHED();
	}

done:
	if (session->auth_state != LC_AUTH_STATE_CHALLENGE) {
		al_os_mem_free(session->auth_data);
		session->auth_data = NULL;
	}
	lctrl_msg_tx(&session->gs, clientv2_msg_enc_auth_client_rsp, id, st,
	    &auth_info);

	return AE_OK;
}

static enum ada_err lctrl_hdl_post_prop_req(const struct cm_rx_op *op,
    struct generic_session *gs, struct ame_kvp *payload, u32 id,
    enum ada_status status)
{
	enum ada_status st;
	struct prop_request_info req_info;
	struct lctrl_session *session =
	    CONTAINER_OF(struct lctrl_session, gs, gs);

	req_info.resp_name = op->resp_name;

	st = lctrl_prop_recv(session, op, payload, req_info.name,
	    sizeof(req_info.name), 0);

	lctrl_msg_tx(gs, clientv2_msg_enc_prop_name, id, st,
	    &req_info);

	return AE_OK;
}

static enum ada_err lctrl_hdl_post_dp_rsp(const struct cm_rx_op *op,
    struct generic_session *gs, struct ame_kvp *payload, u32 id,
    enum ada_status status)
{
	struct lctrl_session *session =
	    CONTAINER_OF(struct lctrl_session, gs, gs);

	if (!session->sending_prop) {
		return AE_OK;
	}
#ifndef TEST_WITHOUT_ACKS
	session->sending_prop = 0;
	client_timer_cancel(&session->timer);
	client_prop_finish_send(status == AST_SUCCESS, session->mask);
#endif

	return AE_OK;
}

static enum ada_err lctrl_hdl_get_log_msgs_req(const struct cm_rx_op *op,
    struct generic_session *gs, struct ame_kvp *payload, u32 id,
    enum ada_status status)
{
	enum ada_err err;
	const struct ame_decoder *dec = lctrl_decoder;
	enum ada_status st = AST_SUCCESS;
	struct cm_log_req_info log_req_info;

	err = clientv2_msg_dec_get_log_msgs_req(op->name, dec, payload,
	    &log_req_info);
	if (err) {
		st = clientv2_msg_err_to_status(err);
		lctrl_msg_tx(gs, clientv2_msg_enc_basic, id, st,
		    op->resp_name);
		return AE_OK;
	}

	lctrl_msg_tx(gs, clientv2_msg_enc_get_log_msgs_resp, id, st,
	    &log_req_info);

	return AE_OK;
}

static enum ada_err lctrl_hdl_get_device_info_req(const struct cm_rx_op *op,
    struct generic_session *gs, struct ame_kvp *payload, u32 id,
    enum ada_status status)
{
	return lctrl_msg_tx(gs, clientv2_msg_enc_get_device_info_rsp, id,
	    AST_SUCCESS, NULL);
}

static enum ada_err lctrl_hdl_post_keep_alive_req(const struct cm_rx_op *op,
    struct generic_session *gs, struct ame_kvp *payload, u32 id,
    enum ada_status status)
{
	u32 period = 30;
	enum ada_err err;
	const struct ame_decoder *dec = lctrl_decoder;
	enum ada_status st = AST_SUCCESS;

	err = clientv2_msg_dec_post_keep_alive_req(op->name, dec, payload,
	    &period);
	if (err) {
		client_log(LOG_ERR "%s: id %lu, pd %lu, err %d, ka %p",
		    __func__, id, period, err, gs->keep_alive);
		st = clientv2_msg_err_to_status(err);
		lctrl_msg_tx(gs, clientv2_msg_enc_basic, id, st,
		    op->resp_name);
		return AE_OK;
	}

	if (gs->keep_alive) {
		gs->keep_alive(gs, period);
	}

	lctrl_msg_tx(gs, clientv2_msg_enc_post_keep_alive_rsp, id, st, NULL);

	return AE_OK;
}

static const struct cm_rx_op rx_op_table[LCRO_COUNT] = {
	[LCRO_POST_PROBE_REQ] = {
		.name = OP_POST_PROBE_REQ,
		.resp_name = OP_POST_PROBE_RSP,
		.handler = lctrl_hdl_post_probe_req,
	},
	[LCRO_GET_PROP_REQ] = {
		.name = OP_GET_PROP_REQ,
		.resp_name = OP_GET_PROP_RSP,
		.handler = lctrl_hdl_get_prop_req,
		.payload_required = 1,
	},
	[LCRO_POST_PROP_REQ] = {
		.name = OP_POST_PROP_REQ,
		.resp_name = OP_POST_PROP_RSP,
		.handler = lctrl_hdl_post_prop_req,
		.payload_required = 1,
	},
	[LCRO_POST_DATAPOINT_RSP] = {
		.name = OP_POST_DATAPOINT_RSP,
		.handler = lctrl_hdl_post_dp_rsp,
		.payload_required = 1,
	},
	[LCRO_POST_AUTH_CLIENT_REQ] = {
		.name = OP_POST_AUTH_CLIENT_REQ,
		.resp_name = OP_POST_AUTH_CLIENT_RSP,
		.handler = lctrl_hdl_post_auth_client_req,
		.payload_required = 1,
		.allow_unauth = 1,
	},
	[LCRO_GET_LOG_MSGS_REQ] = {
		.name = OP_GET_LOG_MSGS_REQ,
		.resp_name = OP_GET_LOG_MSGS_RSP,
		.handler = lctrl_hdl_get_log_msgs_req,
	},
	[LCRO_GET_DEVICE_INFO_REQ] = {
		.name = OP_GET_DEVICE_INFO_REQ,
		.resp_name = OP_GET_DEVICE_INFO_RSP,
		.handler = lctrl_hdl_get_device_info_req,
	},
	[LCRO_POST_KEEP_ALIVE_REQ] = {
		.name = OP_POST_KEEP_ALIVE_REQ,
		.resp_name = OP_POST_KEEP_ALIVE_RSP,
		.handler = lctrl_hdl_post_keep_alive_req,
	},
};

void lctrl_process_rx(struct lctrl_session *session)
{
	const struct ame_decoder *dec = lctrl_decoder;
	struct ame_decoder_state *ds = &lctrl_decoder_state;

	ame_decoder_buffer_set(ds, session->gs.rx_buffer,
	    session->gs.rx_length);
	clientv2_msg_process(&session->gs, dec, ds, rx_op_table,
	    ARRAY_LEN(rx_op_table), lctrl_msg_tx);
	session->gs.rx_length = 0;
}

/*
 * Local control receive handler. The arriving message is copied into the
 * session's rx_buffer (inbox) if it is empty. The local control event
 * handler is then woken up to process the message on the client thread.
 */
enum ada_err lctrl_msg_rx(struct generic_session *gs, const u8 *buf, u16 length)
{
	if (!gs->active) {
		return AE_INVAL_STATE;
	}
	if (gs->rx_length) {
		return AE_BUSY;
	}
	if (length > LC_RX_BUFFER_SIZE) {
		return AE_LEN;
	}

	if (!gs->rx_buffer) {
		/* buffer hasn't been allocated */
		return AE_INVAL_STATE;
	}
	memcpy(gs->rx_buffer, buf, length);
	gs->rx_length = length;

	lctrl_wakeup();

	return AE_OK;
}

static int lctrl_poll(struct lctrl_session *session)
{
	struct prop *prop;
	enum ada_err err;

	/*
	 * Check for received message to process.
	 */
	if (session->gs.rx_length) {
		lctrl_process_rx(session);
		return 1;
	}

	if (session->sending_prop) {
		return 0;
	}

	/*
	 * Check for property to send.
	 */
	prop = client_prop_get_to_send(session->mask);
	if (!prop) {
		return 0;
	}
	if (prop->name[0] == '\0') {
		client_prop_finish_send(1, session->mask);
		return 1;
	}
	if (prop->type == ATLV_SCHED) {
		/* don't send schedules to local control clients */
		client_prop_finish_send(1, session->mask);
		return 0;
	}
	err = lctrl_msg_tx(&session->gs, clientv2_msg_enc_post_dp_req,
	    session->tx_id++, 0, prop);
	if (err) {
		client_prop_finish_send(0, session->mask);
	} else {
#ifdef TEST_WITHOUT_ACKS
		/* For manual testing, don't require an ack. */
		client_prop_finish_send(1, session->mask);
#else
		session->sending_prop = 1;
		client_timer_set(&session->timer, LCTRL_RESP_TIMEOUT);
#endif
	}
	return 1;
}

static void lctrl_poll_all(void)
{
	struct lctrl_session *session;
	int activity = 0;

	if (!client_lctrl_is_enabled()) {
		return;
	}

	for (session = session_table; session < ARRAY_END(session_table);
	    session++) {
		if (!session->gs.active) {
			continue;
		}
		if (lctrl_poll(session)) {
			activity = 1;
		}
	}
	if (activity) {
		/* schedule to run again since there was activity */
		lctrl_wakeup();
	}
}

/*
 * Handler for all local control event processing. This always runs on the
 * client thread.
 */
static void lctrl_event_handler(void *arg)
{
	lctrl_poll_all();
}

void lctrl_session_down(struct generic_session *gs)
{
	struct lctrl_session *session =
	    CONTAINER_OF(struct lctrl_session, gs, gs);

	lctrl_session_free(session);
	client_lock();
	client_dest_down(session->mask);
	client_unlock();
}

static void lctrl_session_close(struct generic_session *gs)
{
	ASSERT(gs->close);
	gs->close(gs);
	lctrl_session_down(gs);
}

void lctrl_session_close_all(void)
{
	struct lctrl_session *session = &session_table[0];
	struct generic_session *gs;
	u8 idx;

	ASSERT(client_locked);		/* required for client_dest_down */

	for (session = session_table, idx = 0;
	    session < ARRAY_END(session_table);
	    session++, idx++) {
		gs = &session->gs;
		if (gs->active) {
			gs->close(gs);
			lctrl_session_free(session);
			client_dest_down(session->mask);
		}
	}
}

int lctrl_up(void)
{
	if (!client_lctrl_is_enabled()) {
		/*
		 * Log a warning while allowing local control to be enabled
		 * by the app. Services will not be offered to clients until
		 * they are enabled in config.
		 */
		client_log(LOG_WARN "local control disabled by config");
	}
	lctrl_enabled = 1;

	return 0;
}

void lctrl_down(void)
{
	if (!lctrl_enabled) {
		return;
	}
	lctrl_enabled = 0;

	lctrl_session_close_all();
}

static void lctrl_session_timeout(struct timer *timer)
{
	struct lctrl_session *session =
	    CONTAINER_OF(struct lctrl_session, timer, timer);

	client_log(LOG_DEBUG "%s session %u", __func__, session->id);

	if (session->sending_prop) {
		client_prop_finish_send(0, session->mask);
	}

	client_unlock();
	lctrl_session_close(&session->gs);
	client_lock();
}

static void lctrl_session_init(struct lctrl_session *session, u8 *buf)
{
	session->mask = NODES_LC_BASE << (session - session_table);
	session->gs.rx_buffer = buf;
	session->gs.rx_length = 0;
	session->tx_id = 0;
	session->sending_prop = 0;
	timer_handler_init(&session->timer, lctrl_session_timeout);
}

struct generic_session *lctrl_session_alloc(void)
{
	struct lctrl_session *session = &session_table[0];
	u8 idx;
	u8 *buf;

	if (!lctrl_enabled) {
		client_log(LOG_WARN "local control disabled, session refused");
		return NULL;
	}

	for (session = session_table, idx = 0;
	    session < ARRAY_END(session_table);
	    session++, idx++) {
		if (!session->gs.active) {
			buf = al_os_mem_alloc(LC_RX_BUFFER_SIZE);
			if (!buf) {
				client_log(LOG_ERR
				    "local control rx buffer alloc failed");
				return NULL;
			}
			lctrl_session_init(session, buf);
			session->gs.active = 1;
			session->id = idx + 1;
			client_log(LOG_INFO "local control session %u up",
			    session->id);
			client_lock();
			client_dest_up(session->mask);
			client_unlock();
			return &session->gs;
		}
	}
	return NULL;
}

static void lctrl_session_free(struct lctrl_session *session)
{
	generic_session_close(&session->gs);
	al_os_mem_free(session->gs.rx_buffer);
	session->gs.rx_buffer = NULL;
	client_log(LOG_INFO "local control session %u down", session->id);
}

void lctrl_init(void)
{
	lctrl_encoder = &ame_json_enc;
	lctrl_decoder = &ame_json_dec;
	ame_decoder_stack_set(&lctrl_decoder_state, lctrl_kvp_stack,
	    ARRAY_LEN(lctrl_kvp_stack));
	callback_init(&lctrl_callback, lctrl_event_handler, NULL);
}
#endif /* AYLA_LOCAL_CONTROL_SUPPORT */
