/*
 * Copyright 2021 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#if defined(AYLA_LOCAL_CONTROL_SUPPORT) || defined(AYLA_TEST_SERVICE_SUPPORT)
#include <stddef.h>
#include <string.h>

#include <ada/libada.h>
#include <ayla/log.h>
#include <ada/generic_session.h>
#include <ada/ada_wifi.h>

#include "ame.h"
#include "ame_json.h"
#include "client_common.h"
#include "clientv2_msg.h"

/*
 * Uncomment the following define for manual testing without enforcing
 * local control authentication before processing messages.
 */
/* #define TEST_WITHOUT_AUTH_CHECK */

/*
 * Log message message encoding defines to help with stuffing as many
 * messages as will fit into encode buffer.
 */
#define CM_GLM_MIN_TRAILER	2	/* minimum space for trailer bytes */
#define CM_GLM_MIN_MSG_SPACE	64	/* approx. min size encoded message */

enum ada_prop_type {
	APT_INTEGER = 0,
	APT_DECIMAL = 1,
	APT_FLOAT = 2,
	APT_BOOLEAN = 3,
	APT_UTF8 = 4,
	APT_OPAQUE = 5,
	APT_LOCATION = 6
};

static char *ada_status_str[AST_COUNT] = {
	[AST_SUCCESS] = "success",
	[AST_FAILURE] = "failure",
	[AST_BAD_REQ] = "bad_request",
	[AST_UNAUTH] = "unauthorized",
	[AST_AUTH_FAIL] = "auth_failed",
	[AST_FORBIDDEN] = "forbidden",
	[AST_INVAL_OP] = "invalid_op",
	[AST_FIELD_MISSING] = "field_missing",
	[AST_INVAL_VAL] = "invalid_value",
	[AST_INVAL_STATE] = "invalid_state",
	[AST_SHORTAGE] = "rsrc_shortage",
	[AST_IN_PROG] = "op_in_progress",
	[AST_BUSY] = "rsrc_busy",
	[AST_TIMEOUT] = "timeout",
	[AST_ABORT] = "aborted",
	[AST_SYS_BUSY] = "system_busy",
};

static enum ada_status ae_to_ast[] = {
	[-AE_OK] = AST_SUCCESS,
	[-AE_BUF] = AST_SHORTAGE,
	[-AE_ALLOC] = AST_SHORTAGE,
	[-AE_ERR] = AST_FAILURE,
	[-AE_NOT_FOUND] = AST_FIELD_MISSING,
	[-AE_INVAL_VAL] = AST_INVAL_VAL,
	[-AE_INVAL_TYPE] = AST_INVAL_VAL,
	[-AE_IN_PROGRESS] = AST_IN_PROG,
	[-AE_BUSY] = AST_BUSY,
	[-AE_LEN] = AST_INVAL_VAL,
	[-AE_INVAL_STATE] = AST_INVAL_STATE,
	[-AE_TIMEOUT] = AST_TIMEOUT,
	[-AE_ABRT] = AST_ABORT,
	[-AE_RST] = AST_FAILURE,
	[-AE_CLSD] = AST_FAILURE,
	[-AE_NOTCONN] = AST_FAILURE,
	[-AE_INVAL_NAME] = AST_INVAL_VAL,
	[-AE_RDONLY] = AST_FORBIDDEN,
	[-AE_CERT_EXP] = AST_AUTH_FAIL,
	[-AE_PARSE] = AST_BAD_REQ,
	[-AE_UNEXP_END] = AST_BAD_REQ,
};

enum cm_log_level {
	CM_LOG_ERR = 0,
	CM_LOG_WARN,
	CM_LOG_INFO,
	CM_LOG_DEBUG,
	CM_LOG_DEBUG2,
	CM_LOG_METRIC,
	CM_LOG_OTHER	/* always keep it last */
};

struct cm_log_msg_info {
	u64 timestamp;
	u32 sequence;
	u8 level;
	const char *module;
	const char *message;
};

/*
 * Message token definitions.
 */
AME_KEY(k_cmd_op, "o", "operation");
AME_KEY(k_cmd_id, "i", "id");
AME_KEY(k_cmd_status, "s", "status");
AME_KEY(k_cmd_payload, "p", "payload");

/*
 * Client authentication message token definitions.
 */
AME_KEY(k_auth_client_type, "t", "type");
AME_KEY(k_auth_client_id, "i", "id");
AME_KEY(k_auth_client_challenge, "c", "challenge");
AME_KEY(k_auth_client_rsp, "r", "response");

/*
 * Property message token definitions.
 */
AME_KEY(k_prop_name, "n", "name");
AME_KEY(k_prop_type, "t", "type");
AME_KEY(k_prop_echo, "e", "echo");
AME_KEY(k_prop_value, "v", "value");
AME_KEY(k_prop_meta, "m", "meta");

/*
 * Log message token definitions.
 */
AME_KEY(k_log_tstamp, "t", "timestamp");
AME_KEY(k_log_sequence, "s", "sequence");
AME_KEY(k_log_level, "l", "level");
AME_KEY(k_log_category, "c", "category");
AME_KEY(k_log_msg, "m", "message");

/*
 * Log message request specific token definitions.
 */
AME_KEY(k_log_limit, "l", "limit");	/* max number of msgs to get */
AME_KEY(k_log_buf, "b", "buffer");	/* log = 0, snapshot > 0  */

/*
 * Test service specific token definitions.
 */
AME_KEY(k_test_setup_host, "h", "host");/* test host nickname */
AME_KEY(k_test_setup_time, "t", "time");/* setup time  */
AME_KEY(k_test_setup_id, "i", "id");	/* test host nickname */
AME_KEY(k_test_setup_key, "k", "key");	/* setup key  */
AME_KEY(k_test_setup_data, "d", "data");/* setup data  */
AME_KEY(k_test_setup_iv, "v", "iv");	/* iv for setup/config data encrption */

/*
 * Device info specific token definitions.
 */
AME_KEY(k_device_info_dsn, "d", "dsn");		/* device DSN */
AME_KEY(k_device_info_part, "p", "part");	/* Ayla part number  */
AME_KEY(k_device_info_oem, "o", "oem");		/* OEM ID */
AME_KEY(k_device_info_model, "m", "model");	/* OEM model  */
AME_KEY(k_device_info_feat, "f", "features");	/* setup data  */
AME_KEY(k_keep_alive_period, "p", "period");    /* Keep alive */

const char *clientv2_msg_status_to_str(enum ada_status status)
{
	if (status < 0 || status >= AST_COUNT || !ada_status_str[status]) {
		return "unknown";
	}
	return ada_status_str[status];
}

enum ada_status clientv2_msg_err_to_status(enum ada_err err)
{
	err = -err;
	if (err < 0 || err > sizeof(ae_to_ast)) {
		return AST_FAILURE;
	}
	return ae_to_ast[err];
}

enum ada_err clientv2_msg_status_to_err(enum ada_status status)
{
	int i;

	for (i = 0; i < ARRAY_LEN(ae_to_ast); i++) {
		if (ae_to_ast[i] == status) {
			return -i;
		}
	}
	return AE_ERR;
}

void clientv2_msg_dec_err_log(const char *op_name, const struct ame_key *key,
    enum ada_err err)
{
	client_log(LOG_ERR "op %s get %s failed err %s(%d)", op_name,
	    key->display_name, ada_err_string(err), err);
}

void clientv2_msg_enc_hdr(const struct ame_encoder *enc,
    struct ame_encoder_state *es, const char *op, u32 id, s32 status,
    int has_payload)
{
	u8 suffix = 0;

	enc->enc_utf8(es, EF_PREFIX_O, &k_cmd_op, op);
	if (!has_payload && !status) {
		suffix = EF_SUFFIX_E;
	}
	enc->enc_u32(es, EF_PREFIX_C | suffix, &k_cmd_id, id);
	if (status) {
		if (!has_payload) {
			suffix = EF_SUFFIX_E;
		}
		enc->enc_s32(es, EF_PREFIX_C | suffix, &k_cmd_status, status);
	}
	if (has_payload) {
		enc->enc_key(es, EF_PREFIX_C, &k_cmd_payload);
	}
}

void clientv2_msg_enc_basic(const struct ame_encoder *enc,
    struct ame_encoder_state *es, u32 id, s32 status, const void *arg)
{
	clientv2_msg_enc_hdr(enc, es, (const char *)arg, id, status, 0);
}

void clientv2_msg_enc_auth_client_rsp(const struct ame_encoder *enc,
    struct ame_encoder_state *es, u32 id, s32 status, const void *arg)
{
	struct cm_auth_client_info *auth_info =
	    (struct cm_auth_client_info *)arg;

	clientv2_msg_enc_hdr(enc, es, OP_POST_AUTH_CLIENT_RSP, id, status, 1);
	enc->enc_u8(es, EF_PREFIX_O, &k_auth_client_type, auth_info->type);

	if (status) {
		enc->enc_suffix(es, EF_SUFFIX_E);
		goto done;
	}

	switch (auth_info->type) {
	case CMCA_TYPE_START:
		ASSERT(auth_info->length == CM_ACM_CHLNG_RSP_LENGTH);
		enc->enc_opaque(es, EF_PREFIX_C | EF_SUFFIX_E,
		    &k_auth_client_challenge, auth_info->buf,
		    auth_info->length);
		break;
	case CMCA_TYPE_RESPONSE:
		enc->enc_suffix(es, EF_SUFFIX_E);
		break;
	default:
		ASSERT_NOTREACHED();
	};

done:
	enc->enc_suffix(es, EF_SUFFIX_E);
}

void clientv2_msg_enc_prop_name(const struct ame_encoder *enc,
    struct ame_encoder_state *es, u32 id, s32 status, const void *arg)
{
	const struct prop_request_info *req_info = arg;
	int has_payload = req_info->name[0] != '\0';

	clientv2_msg_enc_hdr(enc, es, req_info->resp_name, id, status,
	    has_payload);
	if (has_payload) {
		enc->enc_utf8(es, EF_PREFIX_O | EF_SUFFIX_E, &k_prop_name,
		    req_info->name);
		enc->enc_suffix(es, EF_SUFFIX_E);
	}
}

void clientv2_msg_enc_prop(const struct ame_encoder *enc,
    struct ame_encoder_state *es, const struct prop *prop)
{
	enum ada_prop_type ada_type;
	u32 prefix;
	struct ame_key meta_key;
	const struct prop_dp_meta *meta = prop->dp_meta;
	const struct prop_dp_meta *meta_ptr = meta;

	enc->enc_utf8(es, EF_PREFIX_O, &k_prop_name, prop->name);

	switch (prop->type) {
	case ATLV_INT:
		ada_type = APT_INTEGER;
		enc->enc_s32(es, EF_PREFIX_C, &k_prop_value,
		    *(s32 *)prop->val);
		break;
	case ATLV_UINT:
		ada_type = APT_INTEGER;
		enc->enc_u32(es, EF_PREFIX_C, &k_prop_value,
		    *(u32 *)prop->val);
		break;
	case ATLV_BOOL:
		ada_type = APT_BOOLEAN;
		if (*(u8 *)prop->val) {
			enc->enc_u8(es, EF_PREFIX_C, &k_prop_value, 1);
		} else {
			enc->enc_u8(es, EF_PREFIX_C, &k_prop_value, 0);
		}
		break;
	case ATLV_UTF8:
		ada_type = APT_UTF8;
		enc->enc_utf8(es, EF_PREFIX_C, &k_prop_value,
		    (char *)prop->val);
		break;
	case ATLV_CENTS:
		ada_type = APT_DECIMAL;
		enc->enc_d32(es, EF_PREFIX_C, &k_prop_value,
		    *(s32 *)prop->val, 2);
		break;
	case ATLV_LOC:
		ada_type = APT_LOCATION;
		/* no value */
		break;
	case ATLV_BIN:
	case ATLV_SCHED:
	default:
		ada_type = APT_OPAQUE;
		enc->enc_opaque(es, EF_PREFIX_C, &k_prop_value, prop->val,
		    prop->len);
		break;
	}

	if (prop->echo) {
		enc->enc_u8(es, EF_PREFIX_C, &k_prop_echo, 1);
	}

	if (meta && meta->key[0]) {
		enc->enc_key(es, EF_PREFIX_C, &k_prop_meta);
		prefix = EF_PREFIX_O;
		while (meta_ptr < meta + PROP_MAX_DPMETA && meta_ptr->key[0]) {
			meta_key.tag = meta_ptr->key;
			meta_key.display_name = meta_ptr->key;
			enc->enc_utf8(es, prefix, &meta_key, meta_ptr->value);
			prefix = EF_PREFIX_C;
			meta_ptr++;
		}
		enc->enc_suffix(es, EF_SUFFIX_E);
	}
	enc->enc_u8(es, EF_PREFIX_C | EF_SUFFIX_E, &k_prop_type, ada_type);
}

void clientv2_msg_enc_get_prop_rsp(const struct ame_encoder *enc,
    struct ame_encoder_state *es, u32 id, s32 status, const void *arg)
{
	const struct prop *prop = arg;

	clientv2_msg_enc_hdr(enc, es, OP_GET_PROP_RSP, id, status, 1);
	clientv2_msg_enc_prop(enc, es, prop);
	enc->enc_suffix(es, EF_SUFFIX_E);
}

void clientv2_msg_enc_post_dp_req(const struct ame_encoder *enc,
    struct ame_encoder_state *es, u32 id, s32 sarg, const void *parg)
{
	const struct prop *prop = parg;

	clientv2_msg_enc_hdr(enc, es, OP_POST_DATAPOINT_REQ, id, 0, 1);
	clientv2_msg_enc_prop(enc, es, prop);
	enc->enc_suffix(es, EF_SUFFIX_E);
}

static enum cm_log_level clientv2_msg_log_sev_to_level(enum log_sev severity)
{
	switch (severity) {
	case LOG_SEV_ERR:
		return CM_LOG_ERR;
	case LOG_SEV_WARN:
		return CM_LOG_WARN;
	case LOG_SEV_INFO:
		return CM_LOG_INFO;
	case LOG_SEV_DEBUG:
		return CM_LOG_DEBUG;
	case LOG_SEV_DEBUG2:
		return CM_LOG_DEBUG2;
	case LOG_SEV_METRIC:
		return CM_LOG_METRIC;
	default:
		break;
	}
	return CM_LOG_OTHER;
}

static void clientv2_msg_enc_log_msg(const struct ame_encoder *enc,
    struct ame_encoder_state *es, u32 prefix,
    struct cm_log_msg_info *log_info)
{
	enc->enc_u64(es, prefix | EF_PREFIX_O, &k_log_tstamp,
	    log_info->timestamp);
	enc->enc_u32(es, EF_PREFIX_C, &k_log_sequence, log_info->sequence);
	enc->enc_u8(es, EF_PREFIX_C, &k_log_level, log_info->level);
	enc->enc_utf8(es, EF_PREFIX_C, &k_log_category, log_info->module);
	enc->enc_utf8(es, EF_PREFIX_C | EF_SUFFIX_E, &k_log_msg,
	    log_info->message);
}

void clientv2_msg_enc_get_log_msgs_resp(const struct ame_encoder *enc,
    struct ame_encoder_state *es, u32 id, s32 status, const void *parg)
{
	struct cm_log_req_info *log_req_info =
	    (struct cm_log_req_info *)parg;
	struct cm_log_msg_info log_info;
	struct log_buf_ctxt *ctxt;
	u32 prefix = 0;
	u8 buf[LOG_ENTRY_MAX_SZ];
	size_t length = sizeof(buf);
	struct log_msg_head *head = (struct log_msg_head *)buf;
	size_t old_offset;
	u16 count = 0;

	clientv2_msg_enc_hdr(enc, es, OP_GET_LOG_MSGS_RSP, id, 0, 1);
	enc->enc_prefix(es, EF_PREFIX_A);

	ctxt = log_buf_open(log_req_info->buf_id);
	if (!ctxt) {
		client_log(LOG_WARN "failed to open log buffer %u",
		    log_req_info->buf_id);
		goto done;
	}

	if (log_req_info->sequence) {
		log_buf_seq_set(ctxt, log_req_info->sequence);
	}

	/*
	 * Fill the response buffer until: there are no additional messages,
	 * there isn't room for another message or the limit on maximum number
	 * of messages requested has been reached.
	 */
	while (count < log_req_info->limit &&
	    es->offset < es->size - CM_GLM_MIN_MSG_SPACE) {
		old_offset = es->offset;
		length = log_buf_get_next(ctxt, buf, sizeof(buf));
		if (length <= 0) {
			goto done_close;
		}

		log_info.level = clientv2_msg_log_sev_to_level(head->sev);
		log_info.module = log_mod_get_name(head->mod_nr);
		log_info.message = (char *)(head + 1);
		log_info.timestamp = (u64)head->time * 1000 + head->msec;
		log_info.sequence = log_buf_seq_get(ctxt);

		clientv2_msg_enc_log_msg(enc, es, prefix, &log_info);
		prefix = EF_PREFIX_C;

		if (es->offset > es->size - CM_GLM_MIN_TRAILER) {
			/* overflow, trim off last message */
			es->offset = old_offset;
			break;
		}
		count++;
	}
done_close:
	log_buf_close(ctxt);
done:
	enc->enc_suffix(es, EF_SUFFIX_Z);
	enc->enc_suffix(es, EF_SUFFIX_E);
}

void clientv2_msg_enc_get_test_setup_rsp(const struct ame_encoder *enc,
    struct ame_encoder_state *es, u32 id, s32 status, const void *parg)
{
	const struct ts_setup_info *setup_rsp_info = parg;

	clientv2_msg_enc_hdr(enc, es, OP_GET_TEST_SETUP_RSP, id, 0, 1);

	enc->enc_utf8(es, EF_PREFIX_O, &k_test_setup_id,
	    setup_rsp_info->unique_id);
	enc->enc_opaque(es, EF_PREFIX_C, &k_test_setup_key,
	    setup_rsp_info->enc_setup_key, setup_rsp_info->enc_setup_key_len);
	enc->enc_opaque(es, EF_PREFIX_C, &k_test_setup_iv,
	    setup_rsp_info->iv, sizeof(setup_rsp_info->iv));
	enc->enc_opaque(es, EF_PREFIX_C | EF_SUFFIX_E, &k_test_setup_data,
	    setup_rsp_info->data, setup_rsp_info->data_len);
	enc->enc_suffix(es, EF_SUFFIX_E);
}

void clientv2_msg_enc_get_device_info_rsp(const struct ame_encoder *enc,
    struct ame_encoder_state *es, u32 id, s32 status, const void *parg)
{
	enum ada_wifi_features features;
	u32 prefix = 0;

	clientv2_msg_enc_hdr(enc, es, OP_GET_DEVICE_INFO_RSP, id, 0, 1);

	enc->enc_utf8(es, EF_PREFIX_O, &k_device_info_dsn, conf_sys_dev_id);
	enc->enc_utf8(es, EF_PREFIX_C, &k_device_info_part, conf_sys_model);
	enc->enc_utf8(es, EF_PREFIX_C, &k_device_info_oem, oem);
	enc->enc_utf8(es, EF_PREFIX_C, &k_device_info_model, oem_model);
	enc->enc_key(es, EF_PREFIX_C, &k_device_info_feat);
	enc->enc_prefix(es, EF_PREFIX_A);
	features = adap_wifi_features_get();
	if (features & AWF_SIMUL_AP_STA) {
		enc->enc_utf8(es, prefix, NULL, FEAT_STR_AP_STA);
		prefix = EF_PREFIX_C;
	}
	if (features & AWF_WPS) {
		enc->enc_utf8(es, prefix, NULL, FEAT_STR_WPS);
		prefix = EF_PREFIX_C;
	}
	if (features & AWF_WPS_APREG) {
		enc->enc_utf8(es, prefix, NULL, FEAT_STR_WPS_APREG);
		prefix = EF_PREFIX_C;
	}
	if (conf_sys_test_mode_get()) {
		enc->enc_utf8(es, prefix, NULL, FEAT_STR_TEST_MODE);
		prefix = EF_PREFIX_C;
	}
	enc->enc_utf8(es, prefix | EF_SUFFIX_Z, NULL, FEAT_STR_RSA_KE);
	enc->enc_suffix(es, EF_SUFFIX_E);
	enc->enc_suffix(es, EF_SUFFIX_E);
}

void clientv2_msg_enc_post_keep_alive_rsp(const struct ame_encoder *enc,
    struct ame_encoder_state *es, u32 id, s32 status, const void *parg)
{
	clientv2_msg_enc_hdr(enc, es, OP_POST_KEEP_ALIVE_RSP, id, 0, 0);
}

enum ada_err clientv2_msg_dec_post_keep_alive_req(const char *opname,
    const struct ame_decoder *dec, struct ame_kvp *payload, u32 *period)
{
	enum ada_err err;

	if (payload) {
		err = dec->get_u32(payload, &k_keep_alive_period, period);
		if (err && err != AE_NOT_FOUND) {
			client_log(LOG_ERR "%s: get period err %s(%d)",
			    __func__, ada_err_string(err), err);
			clientv2_msg_dec_err_log(opname,
			    &k_keep_alive_period, err);
			return err;
		}
	}
	return AE_OK;
}

enum ada_err clientv2_msg_dec_operation(const struct ame_decoder *dec,
    struct ame_kvp *operation, char *opname, size_t op_length, u32 *id,
    s16 *status, struct ame_kvp **payload)
{
	enum ada_err err;

	err = dec->get_utf8(operation, &k_cmd_op, opname, &op_length);
	if (err) {
		client_log(LOG_ERR "get op failed err %s(%d)",
		    ada_err_string(err), err);
		return err;
	}

	err = dec->get_u32(operation, &k_cmd_id, id);
	if (err) {
		clientv2_msg_dec_err_log(opname, &k_cmd_id, err);
		return err;
	}

	*status = AST_SUCCESS;
	err = dec->get_s16(operation, &k_cmd_status, status);
	if (err && err != AE_NOT_FOUND) {
		clientv2_msg_dec_err_log(opname, &k_cmd_status, err);
		return err;
	}

	*payload = NULL;
	err = ame_get_child(operation, &k_cmd_payload, payload);
	if (err && err != AE_NOT_FOUND) {
		clientv2_msg_dec_err_log(opname, &k_cmd_payload, err);
		return err;
	}
	if (*payload && (*payload)->type == AME_TYPE_NULL) {
		*payload = NULL;
	}

	return AE_OK;
}

enum ada_err clientv2_msg_dec_auth_client_req(const char *opname,
    const struct ame_decoder *dec, struct ame_kvp *payload,
    struct cm_auth_client_info *auth_info)
{
	enum ada_err err;
	u8 type;

	err = dec->get_u8(payload, &k_auth_client_type, &type);
	if (err) {
type_err:
		clientv2_msg_dec_err_log(opname, &k_auth_client_type, err);
		return err;
	}

	switch (type) {
	case CMCA_TYPE_START:
		err = dec->get_utf8(payload, &k_auth_client_id,
		    (char *)auth_info->buf, &auth_info->length);
		if (err) {
			clientv2_msg_dec_err_log(opname, &k_auth_client_id,
			    err);
			return err;
		}
		break;
	case CMCA_TYPE_RESPONSE:
		err = dec->get_opaque(payload, &k_auth_client_rsp,
		    auth_info->buf, &auth_info->length);
		if (!err && auth_info->length != CM_ACM_CHLNG_RSP_LENGTH) {
			err = AE_INVAL_VAL;
		}
		if (err) {
			clientv2_msg_dec_err_log(opname, &k_auth_client_rsp,
			    err);
			return err;
		}
		break;
	default:
		err = AE_INVAL_VAL;
		goto type_err;
	}

	auth_info->type = type;

	return AE_OK;
}

enum ada_err clientv2_msg_dec_get_prop(const char *opname,
    const struct ame_decoder *dec, struct ame_kvp *payload, char *name,
    size_t length)
{
	enum ada_err err;

	name[0] = '\0';
	if (payload) {
		err = dec->get_utf8(payload, &k_prop_name, name, &length);
		if (err && err != AE_NOT_FOUND) {
			clientv2_msg_dec_err_log(opname, &k_prop_name, err);
			return err;
		}
	}
	return AE_OK;
}

enum ada_err clientv2_msg_dec_get_log_msgs_req(const char *opname,
    const struct ame_decoder *dec, struct ame_kvp *payload,
    struct cm_log_req_info *log_req_info)
{
	enum ada_err err;
	u32 sequence = 0;	/* starting sequence for get next */
	u16 limit = MAX_U16;	/* max number of messages to get */
	u8 buf_id = 0;		/* log buffer id */

	if (payload) {
		err = dec->get_u32(payload, &k_log_sequence, &sequence);
		if (err && err != AE_NOT_FOUND) {
			clientv2_msg_dec_err_log(opname, &k_log_sequence, err);
			return err;
		}
		err = dec->get_u16(payload, &k_log_limit, &limit);
		if (err && err != AE_NOT_FOUND) {
			clientv2_msg_dec_err_log(opname, &k_log_limit, err);
			return err;
		}
		err = dec->get_u8(payload, &k_log_buf, &buf_id);
		if (err && err != AE_NOT_FOUND) {
			clientv2_msg_dec_err_log(opname, &k_log_buf, err);
			return err;
		}
	}

	log_req_info->sequence = sequence;
	log_req_info->limit = limit;
	log_req_info->buf_id = buf_id;

	return AE_OK;
}

enum ada_err clientv2_msg_dec_get_test_setup_req(const char *opname,
    const struct ame_decoder *dec, struct ame_kvp *payload,
    struct ts_setup_info *setup_info)
{
	enum ada_err err;
	const struct ame_key *ame_key;
	size_t length;

	length = sizeof(setup_info->svc_name);
	err = dec->get_utf8(payload, &k_test_setup_host, setup_info->svc_name,
	    &length);
	if (err) {
		ame_key = &k_test_setup_host;
		goto decode_error_exit;
	}
	err = dec->get_u32(payload, &k_test_setup_time,
	    &setup_info->setup_time);
	if (err) {
		ame_key = &k_test_setup_time;
		goto decode_error_exit;
	}
	return AE_OK;

decode_error_exit:
	clientv2_msg_dec_err_log(opname, ame_key, err);
	return err;
}

enum ada_status clientv2_msg_prop_recv(u8 src, const struct ame_decoder *dec,
    const char *opname, struct ame_kvp *payload, char *name,
    size_t length, u8 is_schedule)
{
	enum ada_err err;
	u8 type;
	s32 s32value;
	u32 u32value;
	char str_value[PROP_VAL_LEN+1];
	enum ayla_tlv_type tlv_type = ATLV_INVALID;
	const void *val_ptr;
	size_t val_len;
	size_t offset = 0;

	name[0] = '\0';
	err = dec->get_utf8(payload, &k_prop_name, name, &length);
	if (err) {
		clientv2_msg_dec_err_log(opname, &k_prop_name, err);
		goto error_exit;
	}
	if (is_schedule) {
		type = APT_OPAQUE;
	} else {
		err = dec->get_u8(payload, &k_prop_type, &type);
		if (err) {
			clientv2_msg_dec_err_log(opname, &k_prop_type, err);
			goto error_exit;
		}
	}

	switch (type) {
	case APT_INTEGER:
		tlv_type = ATLV_INT;
		err = dec->get_s32(payload, &k_prop_value, &s32value);
		val_ptr = &s32value;
		val_len = sizeof(s32value);
		client_log(LOG_DEBUG "prop %s value %ld", name, s32value);
		break;
	case APT_BOOLEAN:
		tlv_type = ATLV_BOOL;
		err = dec->get_u32(payload, &k_prop_value, &u32value);
		val_ptr = &u32value;
		val_len = sizeof(u32value);
		client_log(LOG_DEBUG "prop %s value %lu", name, u32value);
		break;
	case APT_DECIMAL:
		tlv_type = ATLV_CENTS;
		err = dec->get_d32(payload, &k_prop_value, &s32value, 2);
		val_ptr = &s32value;
		val_len = sizeof(s32value);
		client_log(LOG_DEBUG "prop %s value %ld", name, s32value);
		break;
	case APT_UTF8:
		tlv_type = ATLV_UTF8;
		val_len = sizeof(str_value);
		err = dec->get_utf8(payload, &k_prop_value, str_value,
		    &val_len);
		val_ptr = &str_value;
		client_log(LOG_DEBUG "prop %s value %s", name, str_value);
		break;
	case APT_OPAQUE:
		tlv_type = ATLV_BIN;
		val_len = sizeof(str_value);
		err = dec->get_opaque(payload, &k_prop_value, str_value,
		    &val_len);
		val_ptr = &str_value;
		client_log(LOG_DEBUG "prop %s opaque data length %d", name,
		    val_len);
		break;
	case APT_LOCATION:
	case APT_FLOAT:
	default:
		client_log(LOG_ERR
		    "prop type not supported name %s type %d", name, type);
		return AST_INVAL_VAL;
	}

	if (err) {
		clientv2_msg_dec_err_log(opname, &k_prop_value, err);
		goto error_exit;
	}

	client_log(LOG_DEBUG "%s set prop %s", __func__, name);

	err = ada_prop_mgr_meta_set(name, tlv_type, val_ptr, val_len,
	    &offset, src, NULL, NULL, NULL);
	if (err) {
		client_log(LOG_ERR "prop %s set failed err %s(%d)",
		    name, ada_err_string(err), err);
		goto error_exit;
	}
	return AST_SUCCESS;

error_exit:
	if (err == AE_NOT_FOUND) {
		/*
		 * In this case, not found means the value of the
		 * name field was invalid
		 */
		err = AE_INVAL_VAL;
	}
	return clientv2_msg_err_to_status(err);
}

enum ada_err clientv2_msg_process(struct generic_session *gs,
    const struct ame_decoder *dec, struct ame_decoder_state *ds,
    const struct cm_rx_op *rx_op_table, size_t rx_op_table_len,
    enum ada_err (*msg_tx_func)(struct generic_session *gs,
	void (*msg_encoder)(const struct ame_encoder *enc,
	struct ame_encoder_state *es, u32 id, s32 status, const void *arg),
    u32 id, s32 status, const void *arg))
{
	struct ame_kvp *operation = NULL;
	char opname[6];
	u32 id = 0; /* use zero in response if request couldn't be parsed */
	s16 status;
	s16 log_status;
	s16 resp_status = AST_BAD_REQ;
	struct ame_kvp *payload;
	int i;
	const struct cm_rx_op *op = rx_op_table;
	enum ada_err err;

	err = dec->parse(ds, AME_PARSE_FULL, &operation);
	if (err) {
		client_log(LOG_ERR "message parse failed err %d", err);
		goto bad_request;
	}
	if (!operation) {
		client_log(LOG_ERR "message contains no objects");
		goto bad_request;
	}
	if (operation->type != AME_TYPE_OBJECT) {
		client_log(LOG_ERR "message type is not object");
		goto bad_request;
	}

	err = clientv2_msg_dec_operation(dec, operation, opname, sizeof(opname),
	    &id, &status, &payload);
	if (err) {
		goto bad_request;
	}

	for (i = 0; i < rx_op_table_len; ++i) {
		if (!strcmp(op->name, opname)) {
			break;
		}
		op++;
	}

	if (i >= rx_op_table_len) {
		log_status = status ? status : AST_INVAL_OP;
		client_log(LOG_WARN "op %s status %s(%d)", opname,
		    clientv2_msg_status_to_str(log_status), log_status);
		resp_status = AST_INVAL_OP;
error_response:
		if (status || !strcmp(opname, OP_UNKNOWN)) {
			/* Assume it's an error response */
		} else if (opname[0] >= 'A' && opname[0] <= 'Z') {
			/* convert to lowercase */
			opname[0] += 'a' - 'A';
		} else {
bad_request:
			strncpy(opname, OP_UNKNOWN, sizeof(opname));
		}

		/*
		 * The response to receiving an invalid message or garbage is
		 * a well-formed response that tries to give clues about the
		 * message that was received if anything was parsed out of it.
		 * For example, if the id was parsed out of the received
		 * message, its value will be used in the response.
		 */
		msg_tx_func(gs, clientv2_msg_enc_basic, id,
		    resp_status, opname);
		return AE_ERR;
	}

	if (op->payload_required && !payload) {
		client_log(LOG_WARN "op %s missing payload", opname);
		resp_status = AST_FIELD_MISSING;
		goto error_response;
	}
#ifndef TEST_WITHOUT_AUTH_CHECK
	if (!gs->authed && !op->allow_unauth) {
		client_log(LOG_WARN "op %s requires authentication", opname);
		resp_status = AST_UNAUTH;
		goto error_response;
	}
#endif
	if (op->handler) {
		err = op->handler(op, gs, payload, id, (enum ada_status)status);
	}

	return err;
}
#endif /* AYLA_LOCAL_CONTROL_SUPPORT */
