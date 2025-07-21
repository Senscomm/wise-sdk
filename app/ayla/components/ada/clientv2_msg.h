/*
 * Copyright 2021 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_CLIENTV2_MSG_H__
#define __AYLA_CLIENTV2_MSG_H__

#include "test_svc_int.h"

/*
 * Message op names
 */
#define OP_GET_PROP_REQ		"Gpr"
#define OP_GET_PROP_RSP		"gpr"
#define OP_POST_AUTH_CLIENT_REQ	"Oac"
#define OP_POST_AUTH_CLIENT_RSP	"oac"
#define OP_POST_PROP_REQ	"Opr"
#define OP_POST_PROP_RSP	"opr"
#define OP_POST_DATAPOINT_REQ	"Odp"
#define OP_POST_DATAPOINT_RSP	"odp"
#define OP_POST_PROBE_REQ	"Opb"
#define OP_POST_PROBE_RSP	"opb"
#define OP_GET_LOG_MSGS_REQ	"Glm"
#define OP_GET_LOG_MSGS_RSP	"glm"
#define OP_GET_TEST_SETUP_REQ	"Gts"
#define OP_GET_TEST_SETUP_RSP	"gts"
#define OP_PUT_TEST_TICKET_REQ	"Ptt"
#define OP_PUT_TEST_TICKET_RSP	"ptt"
#define OP_GET_DEVICE_INFO_REQ	"Gdi"
#define OP_GET_DEVICE_INFO_RSP	"gdi"
#define OP_POST_KEEP_ALIVE_REQ	"Oka"
#define OP_POST_KEEP_ALIVE_RSP	"oka"

#define OP_UNKNOWN		"ukn"

#define CM_ACM_CHLNG_RSP_LENGTH	32	/* length of challenge or response */

enum ada_status {
	AST_UNKNOWN = -1,		/* status of op is unknown */
	AST_SUCCESS = 0,		/* success */
	AST_FAILURE = 1,		/* general failure */
	AST_BAD_REQ = 2,		/* bad request */
	AST_UNAUTH = 3,			/* unauthorized - recoverable issue */
	AST_AUTH_FAIL = 4,		/* auth failed - unrecoverable */
	AST_FORBIDDEN = 5,		/* access to resource denied */
	AST_INVAL_OP = 6,		/* invalid operation */
	AST_FIELD_MISSING = 7,		/* required field missing */
	AST_INVAL_VAL = 8,		/* invalid value */
	AST_INVAL_STATE = 9,		/* invalid state */
	AST_SHORTAGE = 10,		/* resource shortage */
	AST_IN_PROG = 11,		/* operation in progress */
	AST_BUSY = 12,			/* resource is busy */
	AST_TIMEOUT = 13,		/* operation timed out */
	AST_ABORT = 14,			/* operation aborted */
	AST_SYS_BUSY = 15,		/* system is overloaded */
	AST_COUNT			/* number of status values */
};

enum cm_auth_client_type {
	CMCA_TYPE_START = 1,
	CMCA_TYPE_RESPONSE,
};

struct cm_auth_client_info {
	enum cm_auth_client_type type;
	u8 *buf;
	size_t length;
};

struct prop_request_info {
	u32 id;
	void *ctxt;
	const char *resp_name;
	char name[(PROP_LOC_LEN > PROP_NAME_LEN) ?
	    PROP_LOC_LEN : PROP_NAME_LEN];
	u8 in_use;
};

struct cm_log_req_info {
	u32 sequence;
	u16 limit;
	u8 buf_id;
};

/*
 * Definition of an entry in the receive message handler table. The opname
 * in the message is used to look up the op entry in the table.
 */
struct cm_rx_op {
	const char *name;
	const char *resp_name;
	enum ada_err (*handler)(const struct cm_rx_op *op,
	    struct generic_session *gs, struct ame_kvp *kvp_payload, u32 id,
	    enum ada_status status);
	    /* handler, if default handling won't do */
	u8 payload_required:1;	/* requires a payload */
	u8 allow_unauth:1;	/* message may be processed without LC auth */
};

/*
 * Convert a V2 message status value to a string.
 */
const char *clientv2_msg_status_to_str(enum ada_status status);

/*
 * Convert and ada_err to a V2 status.
 */
enum ada_status clientv2_msg_err_to_status(enum ada_err err);

/*
 * Convert a V2 status to an ada_err.
 */
enum ada_err clientv2_msg_status_to_err(enum ada_status status);

/*
 * Log a decoding error with the name of the op and the key that was
 * being decoded.
 */
void clientv2_msg_dec_err_log(const char *op_name, const struct ame_key *key,
    enum ada_err err);

/*
 * Encode the header of a V2 message.
 */
void clientv2_msg_enc_hdr(const struct ame_encoder *enc,
    struct ame_encoder_state *es, const char *op, u32 id, s32 status,
    int has_payload);

/*
 * Encode a basic V2 message that has no payload.
 */
void clientv2_msg_enc_basic(const struct ame_encoder *enc,
    struct ame_encoder_state *es, u32 id, s32 status, const void *arg);

/*
 * Encode an auth client response message.
 */
void clientv2_msg_enc_auth_client_rsp(const struct ame_encoder *enc,
    struct ame_encoder_state *es, u32 id, s32 status, const void *arg);

/*
 * Encode a message with a property name in the payload.
 */
void clientv2_msg_enc_prop_name(const struct ame_encoder *enc,
    struct ame_encoder_state *es, u32 id, s32 status, const void *arg);

/*
 * Encode a message with a property name in the payload.
 */
void clientv2_msg_enc_sched_name(const struct ame_encoder *enc,
    struct ame_encoder_state *es, u32 id, s32 status, const void *arg);

/*
 * Encode a message with a property name and value.
 */
void clientv2_msg_enc_prop(const struct ame_encoder *enc,
    struct ame_encoder_state *es, const struct prop *prop);

/*
 * Encode a get property response message.
 */
void clientv2_msg_enc_get_prop_rsp(const struct ame_encoder *enc,
    struct ame_encoder_state *es, u32 id, s32 status, const void *arg);

/*
 * Encode a post datapoint request message.
 */
void clientv2_msg_enc_post_dp_req(const struct ame_encoder *enc,
    struct ame_encoder_state *es, u32 id, s32 sarg, const void *parg);

/*
 * Encode a get log messages response message.
 */
void clientv2_msg_enc_get_log_msgs_resp(const struct ame_encoder *enc,
    struct ame_encoder_state *es, u32 id, s32 status, const void *parg);

/*
 * Encode a get test setup response message.
 */
void clientv2_msg_enc_get_test_setup_rsp(const struct ame_encoder *enc,
    struct ame_encoder_state *es, u32 id, s32 status, const void *parg);

/*
 * Encode a device info response message.
 */
void clientv2_msg_enc_get_device_info_rsp(const struct ame_encoder *enc,
    struct ame_encoder_state *es, u32 id, s32 status, const void *parg);

/*
 * Encode a keep alive response message.
 */
void clientv2_msg_enc_post_keep_alive_rsp(const struct ame_encoder *enc,
    struct ame_encoder_state *es, u32 id, s32 status, const void *parg);

/*
 * Decode the keep alive message.
 */
enum ada_err clientv2_msg_dec_post_keep_alive_req(const char *opname,
    const struct ame_decoder *dec, struct ame_kvp *payload, u32 *period);

/*
 * Decode the top level fields of a V2 operation message.
 */
enum ada_err clientv2_msg_dec_operation(const struct ame_decoder *dec,
    struct ame_kvp *operation, char *opname, size_t op_length, u32 *id,
    s16 *status, struct ame_kvp **payload);

/*
 * Decode a client authentication request message.
 */
enum ada_err clientv2_msg_dec_auth_client_req(const char *opname,
    const struct ame_decoder *dec, struct ame_kvp *payload,
    struct cm_auth_client_info *auth_info);

/*
 * Decode a get property message.
 */
enum ada_err clientv2_msg_dec_get_prop(const char *opname,
    const struct ame_decoder *dec, struct ame_kvp *payload, char *name,
    size_t length);

/*
 * Decode a get log messages request message.
 */
enum ada_err clientv2_msg_dec_get_log_msgs_req(const char *opname,
    const struct ame_decoder *dec, struct ame_kvp *payload,
    struct cm_log_req_info *log_req_info);

/*
 * Decode a get test server setup info request message.
 */
enum ada_err clientv2_msg_dec_get_test_setup_req(const char *opname,
    const struct ame_decoder *dec, struct ame_kvp *payload,
    struct ts_setup_info *setup_info);

/*
 * Decode a post property or schedule message.
 */
enum ada_status clientv2_msg_prop_recv(u8 src, const struct ame_decoder *dec,
    const char *opname, struct ame_kvp *payload, char *name,
    size_t length, u8 is_schedule);

/*
 * Process a received message.
 */
enum ada_err clientv2_msg_process(struct generic_session *gs,
    const struct ame_decoder *dec, struct ame_decoder_state *ds,
    const struct cm_rx_op *rx_op_table, size_t rx_op_table_len,
    enum ada_err (*msg_tx_func)(struct generic_session *gs,
	void (*msg_encoder)(const struct ame_encoder *enc,
	struct ame_encoder_state *es, u32 id, s32 status, const void *arg),
    u32 id, s32 status, const void *arg));

#endif
