/*
 * Copyright 2022 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <stddef.h>
#include <string.h>

#include <ayla/utypes.h>
#include <ayla/log.h>
#include <ayla/callback.h>
#include <ada/libada.h>
#include <ada/generic_session.h>
#include <al/al_os_mem.h>

#include "ame.h"
#include "ame_json.h"
#include "client_timer.h"
#include "clientv2_msg.h"
#include "test_svc_int.h"

#define OM_TX_BUF_SZ		1024
#define OM_KVP_STACK_SZ		15

enum om_rx_op {
	OMRO_GET_DEVICE_INFO_REQ,
	OMRO_GET_TEST_SETUP_REQ,
	OMRO_PUT_TEST_TICKET_REQ,
	OMRO_COUNT	/* keep at end */
};

static u8 om_tx_buf[OM_TX_BUF_SZ];
static struct generic_session om_session;

/*
 * Onboard message callback is use to execute message handling on the client
 * thread.
 */
static struct callback om_callback;

/*
 * Wake up local control event handler on client thread.
 */
void om_wakeup(void)
{
	client_callback_pend(&om_callback);
}

static enum ada_err om_tx(struct generic_session *gs,
    void (*msg_encoder)(const struct ame_encoder *enc,
	struct ame_encoder_state *es, u32 id, s32 status, const void *arg),
    u32 id, s32 status, const void *arg)
{
	struct ame_encoder_state encoder_state;
	struct ame_encoder_state *es = &encoder_state;

	ame_encoder_buffer_set(es, om_tx_buf, sizeof(om_tx_buf));
	msg_encoder(&ame_json_enc, es, id, status, arg);
	return gs->msg_tx(gs, es->buffer, es->offset);
}

static enum ada_err om_hdl_get_device_info_req(const struct cm_rx_op *op,
    struct generic_session *gs, struct ame_kvp *payload, u32 id,
    enum ada_status status)
{
	return om_tx(gs, clientv2_msg_enc_get_device_info_rsp, id, AST_SUCCESS,
	    NULL);
}

static enum ada_err om_hdl_get_test_setup_req(const struct cm_rx_op *op,
    struct generic_session *gs, struct ame_kvp *payload, u32 id,
    enum ada_status status)
{
	enum ada_status st = AST_SUCCESS;
	enum ada_err err = AE_OK;
	const struct ame_decoder *dec = &ame_json_dec;
	struct ts_setup_info *setup_info = NULL;

	setup_info = (struct ts_setup_info *)
	    al_os_mem_alloc(sizeof(struct ts_setup_info));
	if (!setup_info) {
		return AE_ALLOC;
	}

	err = clientv2_msg_dec_get_test_setup_req(op->name, dec, payload,
	    setup_info);
	if (err) {
		goto error_exit;
	}

	err = test_svc_setup_data_get(setup_info);
	if (err) {
		goto error_exit;
	}

	om_tx(gs, clientv2_msg_enc_get_test_setup_rsp, id, st, setup_info);
exit:
	al_os_mem_free(setup_info);
	return err;

error_exit:
	om_tx(gs, clientv2_msg_enc_basic, id, clientv2_msg_err_to_status(err),
	    op->resp_name);
	goto exit;
}

static enum ada_err om_hdl_put_test_ticket_req(const struct cm_rx_op *op,
    struct generic_session *gs, struct ame_kvp *payload, u32 id,
    enum ada_status status)
{
	enum ada_err err;

	err = test_svc_ticket_apply(payload->value, payload->value_length);

	om_tx(gs, clientv2_msg_enc_basic, id, clientv2_msg_err_to_status(err),
	    op->resp_name);

	return err;
}

static const struct cm_rx_op om_rx_op_table[OMRO_COUNT] = {
	[OMRO_GET_DEVICE_INFO_REQ] = {
		.name = OP_GET_DEVICE_INFO_REQ,
		.resp_name = OP_GET_DEVICE_INFO_RSP,
		.handler = om_hdl_get_device_info_req,
		.allow_unauth = 1,
	},
	[OMRO_GET_TEST_SETUP_REQ] = {
		.name = OP_GET_TEST_SETUP_REQ,
		.resp_name = OP_GET_TEST_SETUP_RSP,
		.handler = om_hdl_get_test_setup_req,
		.payload_required = 1,
		.allow_unauth = 1,
	},
	[OMRO_PUT_TEST_TICKET_REQ] = {
		.name = OP_PUT_TEST_TICKET_REQ,
		.resp_name = OP_PUT_TEST_TICKET_RSP,
		.handler = om_hdl_put_test_ticket_req,
		.payload_required = 1,
		.allow_unauth = 1,
	},
};

struct generic_session *om_session_alloc(void)
{
	struct generic_session *gs = &om_session;

	if (gs->active) {
		client_log(LOG_ERR "%s session in use", __func__);
		return NULL;
	}
	gs->active = 1;
	gs->fragment = 1;

	return gs;
}

void om_session_down(struct generic_session *gs)
{
	ASSERT(gs == &om_session);
	generic_session_close(gs);
}

enum ada_err om_rx(struct generic_session *gs, const u8 *buf, u16 length)
{
	if (!gs->active) {
		client_log(LOG_ERR "%s session not active", __func__);
		return AE_INVAL_STATE;
	}
	if (gs->rx_buffer) {
		return AE_BUSY;
	}
	gs->rx_buffer = al_os_mem_alloc(length);
	if (!gs->rx_buffer) {
		return AE_ALLOC;
	}
	memcpy(gs->rx_buffer, buf, length);
	gs->rx_length = length;

	om_wakeup();

	return AE_OK;
}

/*
 * Handler for all local control event processing. This always runs on the
 * client thread.
 */
static void om_event_handler(void *arg)
{
	struct generic_session *gs = &om_session;
	static struct ame_decoder_state *ds;
	static u8 inited;

	if (!inited) {
		ds = ame_decoder_state_alloc(OM_KVP_STACK_SZ);
		ASSERT(ds);
		inited = 1;
	}

	if (!gs->active || !gs->rx_buffer) {
		return;
	}

	ame_decoder_buffer_set(ds, gs->rx_buffer, gs->rx_length);
	clientv2_msg_process(gs, &ame_json_dec, ds, om_rx_op_table,
	    ARRAY_LEN(om_rx_op_table), om_tx);

	al_os_mem_free(gs->rx_buffer);
	gs->rx_buffer = NULL;
	gs->rx_length = 0;
}

void om_init(void)
{
	callback_init(&om_callback, om_event_handler, NULL);
}
