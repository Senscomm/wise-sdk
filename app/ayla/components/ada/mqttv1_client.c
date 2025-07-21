/*
 * Copyright 2020 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <ayla/log.h>
#include <ayla/mod_log.h>
#include <ayla/http.h>
#include <ayla/timer.h>
#include <ayla/clock.h>
#include <ayla/callback.h>
#include <ayla/conf.h>
#include <ada/ada_conf.h>
#include <ada/client.h>
#include <ada/err.h>
#include <al/al_os_mem.h>
#include "ame.h"
#include "ame_json.h"
#include "client_timer.h"
#include "client_lock.h"

#include "mqtt_client.h"
#include "mqttv1_client.h"

#define MQTTV1_BUF_LEN 640
#define MQTTV1_JS_KVP_STACK_SIZE 16
#define MQTTV1_JS_VAL_BUF_LEN TLV_MAX_STR_LEN	/* currently 1024 */

#define MQTTV1_DEBUG

#define mqtt_log(level, fmt, arg...) \
	mqtt_log_int(level "MQTT V1: " fmt, ##arg)

/*
 * Macro for state changes, with optional debug logging.
 */
#ifdef MQTTV1_DEBUG
#define MQTTV1_CH_STATE(cm, field, target) \
	do { \
		_mqttv1_client_ch_stat(__func__, #field, \
		    (cm)->field, (target)); \
		(cm)->field = (target); \
	} while (0)
#else
#define MQTTV1_CH_STATE(cm, field, target) \
	do { \
		(cm)->field = (target); \
	} while (0)
#endif

#define MQTTV1_BROKER_DEFAULT_HOST "mqtt-dev.aylanetworks.com"
#define MQTTV1_BROKER_DEFAULT_PORT 8883
#define MQTTV1_SSL_ENABLE 1

#define MQTTV1_BROKER_HOST_LEN 80
#define MQTTV1_CLIENT_ID_MAX_LEN 32
#define MQTTV1_USERNAME_MAX_LEN 32
#define MQTTV1_PASSWORD_MAX_LEN 256
#define MQTTV1_TOPIC_MAX_LEN 32

#define MQTTV1_PROOF_USERNAME_PREFIX "V1/PROOF"
#define MQTTV1_AUTH_TOKEN_USERNAME "AUTH_TOKEN"

#define MQTTV1_UPSTREAM_TOPIC_FMT "UP/%s"
#define MQTTV1_DOWNSTREAM_TOPIC_FMT "DOWN/%s"

/* threshold till wait time increase */
#define MQTTV1_RECONN_THRESH 5

#define MQTTV1_RECONN_WAIT1 10000
#define MQTTV1_RECONN_WAIT2 60000
#define MQTTV1_RECONN_WAIT3 300000

enum mc_account_type {
	MC_AC_NONE,
	MC_AC_PROOF,
	MC_AC_AUTH_TOKEN,
};

/* Payload content type */
enum mc_ctype {
	MC_CTYPE_NONE,
	MC_CTYPE_APP_XML,	/* application/xml */
	MC_CTYPE_APP_JSON,	/* application/json */
	MC_CTYPE_APP_STREAM,	/* application/octet-stream */
};

enum mc_url_type {
	MC_URL_NONE,
	MC_URL_GET_DSNS,	/* get DSN */
};

enum mc_conn_state {
	MC_CS_DISCONNECTED,
	MC_CS_CONNECTING,
	MC_CS_CONNECTED,
	MC_CS_DISCONNECTING,
};

enum mc_sub_state {
	MC_SS_UNSUB,
	MC_SS_SUBING,
	MC_SS_SUBED,
};

enum mc_pub_state {
	MC_PS_IDLE,
	MC_PS_PUBING,
};

enum mc_msg_state {
	MC_MSG_NONE,
	MC_MSG_IN_PROGRESS,
	MC_MSG_DONE,
};

/* AME JSON decoder state */
struct mqttv1_js_parser {
	struct ame_decoder_state ds;
	struct ame_kvp key_stack[MQTTV1_JS_KVP_STACK_SIZE];
	char val_buf[MQTTV1_JS_VAL_BUF_LEN];
};

/* Message type */
enum mc_msg_type {
	MC_MSG_TYPE_NONE = 0,
	MC_MSG_TYPE_DSN_RESP = 1,
	MC_MSG_TYPE_NOTIFY = 2,
};

struct mqttv1_msg {
	enum mc_msg_state msg_state;
	enum mc_msg_type type;
	enum al_err parse_err;

	u32 offset;
	u32 total_len;
	u8 have_status;
	u8 have_req_id;
	u8 have_resp_id;
	u8 started;

	u32 status;
	u32 req_id;
	u32 resp_id;
};

struct mqttv1_pub {
	enum mqtt_client_qos qos;
	enum mc_pub_state state;
	u8 suffix_len;
	const char *suffix;
	u32 offset;
	u32 total_len;
	void (*pub_completed)(struct mqtt_client *mc,
	    void *arg, enum al_err err);
};

struct mqttv1_sub {
	enum mqtt_client_qos qos;
	enum mc_sub_state state;

	size_t (*sub_handle)(struct mqtt_client *mc, void *arg,
	    const void *payload, size_t len, size_t total_len);
	void (*suback_cb)(struct mqtt_client *mc, void *arg,
	    enum al_err err);
};

struct mqttv1_client_manager {
	struct mqtt_client *mc; /* pointer to the client */
	enum mc_conn_state conn_state; /* state of the client */

	/* Broker configuration */
	char host[MQTTV1_BROKER_HOST_LEN];  /* host name or IP address */
	u16 port;
	u16 keepalive;
	int ssl_enable;

	const char *client_id;
	enum mc_account_type ac_type;

	void (*connack_cb)(struct mqtt_client *mc,
	    enum mqtt_client_conn_err err, void *arg);
	void (*err_cb)(struct mqtt_client *mc, enum al_err err, void *arg);
	void (*sent_cb)(void *arg);
	void (*disconnected_cb)(struct mqtt_client *mc, void *arg);

	void (*notify_cb)(void *arg);
	void (*conn_state_cb)(enum mqttv1_conn_state st, void *arg);

	void *arg;

	u32 req_id;
	u8 req_pending;
	u8 stay_connected;
	char req_buf[MQTTV1_BUF_LEN];
	enum mc_ctype req_ctype;
	enum mc_url_type req_url_type;
	void (*req_err_cb)(enum al_err err, void *arg);

	void (*resp_start_cb)(u32 http_status, int hcnt,
	    const struct http_hdr *hdrs, void *arg);
	size_t (*resp_payload_cb)(const void *part, size_t len, void *arg);

	struct mqttv1_pub pub;
	struct mqttv1_sub sub;
	struct mqttv1_msg msg;
	struct mqttv1_js_parser parser;

	struct timer reconn_timer;
	u32 reconn_ms;
	u8 reconn_tries;
	u8 paused;
	char paused_buf[MQTT_CLIENT_SUB_BUF_LEN];
	size_t paused_len;
	struct callback continue_callback;
};

static int mqttv1_js_handle_req_id(const struct ame_decoder *dec,
		void *arg, struct ame_kvp *kvp);
static int mqttv1_js_handle_resp_id(const struct ame_decoder *dec,
		void *arg, struct ame_kvp *kvp);
static int mqttv1_js_handle_status(const struct ame_decoder *dec,
		void *arg, struct ame_kvp *kvp);
static int mqttv1_js_handle_header(const struct ame_decoder *dec,
		void *arg, struct ame_kvp *kvp);
static int mqttv1_js_handle_payload(const struct ame_decoder *dec,
		void *arg, struct ame_kvp *kvp);

static const struct ame_tag mqttv1_ame_inner_tags[] = {
	AME_TAG("req_id", NULL, mqttv1_js_handle_req_id),
	AME_TAG("resp_id", NULL, mqttv1_js_handle_resp_id),
	AME_TAG("status", NULL, mqttv1_js_handle_status),
	AME_TAG_FULL("header", mqttv1_js_handle_header),
	AME_TAG_PARTIAL("payload", mqttv1_js_handle_payload),
	AME_TAG_END
};

static const struct ame_tag mqttv1_ame_tags[] = {
	AME_TAG("", mqttv1_ame_inner_tags, NULL), /* unnamed outer object */
	AME_TAG_END
};

static struct mqttv1_client_manager gv_client_manager;

static void mqttv1_reconn_disconnected_cb(struct mqtt_client *mc, void *arg);

static enum mod_log_id mqttv1_client_log_mod_nr = MOD_LOG_MQTT;

static void mqtt_log_int(const char *fmt, ...) ADA_ATTRIB_FORMAT(1, 2);

static void mqtt_log_int(const char *fmt, ...)
{
	__builtin_va_list args;

	__builtin_va_start(args, fmt);
	log_put_va(mqttv1_client_log_mod_nr, fmt, args);
	__builtin_va_end(args);
}

void mqttv1_client_log_set(enum mod_log_id mod_nr)
{
	mqttv1_client_log_mod_nr = mod_nr;
}

#ifdef MQTTV1_DEBUG
static void _mqttv1_client_ch_stat(const char *func, const char *name,
		int old, int target)
{
	mqtt_log(LOG_DEBUG, "%s: %s [%d] -> [%d]", func,
	    name, old, target);
}
#endif

/* Publish the beginning of the request to the topic "UP/{DSN}". */
static int mqttv1_publish_req_start(struct mqttv1_client_manager *cm)
{
	int ret;
	enum al_err err;
	struct mqttv1_pub *pub = &cm->pub;
	char up_topic[MQTTV1_TOPIC_MAX_LEN];

	snprintf(up_topic, sizeof(up_topic), MQTTV1_UPSTREAM_TOPIC_FMT,
	    cm->client_id);
	err = mqtt_client_publish_topic_header(cm->mc, up_topic, pub->qos,
	    pub->total_len, pub->pub_completed);
	if (err != AL_ERR_OK) {
		mqtt_log(LOG_ERR, "publish topic \"%s\" error: %s",
		     up_topic, al_err_string(err));
		return -1;
	}
	MQTTV1_CH_STATE(cm, pub.state, MC_PS_PUBING);
	if (!pub->offset) {
		return 0;
	}
	log_bytes(mqttv1_client_log_mod_nr, LOG_SEV_DEBUG2,
	    cm->req_buf, pub->offset,
	    "pub_msg start");
	ret = mqtt_client_publish_payload(cm->mc, cm->req_buf,
	    pub->offset);
	if (ret) {
		return -1;
	}
	return 0;
}

static void mqttv1_reconn_reset(struct mqttv1_client_manager *cm)
{
	cm->reconn_ms = 0;
	cm->reconn_tries = 0;
	client_timer_cancel(&cm->reconn_timer);
}

static void mqttv1_msg_reset(struct mqttv1_msg *msg)
{
	memset(msg, 0, sizeof(*msg));
}

static void mqttv1_disconnect_reset(struct mqttv1_client_manager *cm)
{
	MQTTV1_CH_STATE(cm, conn_state, MC_CS_DISCONNECTED);
	MQTTV1_CH_STATE(cm, sub.state, MC_SS_UNSUB);
	MQTTV1_CH_STATE(cm, pub.state, MC_PS_IDLE);
	mqttv1_msg_reset(&cm->msg);
	mqttv1_reconn_reset(cm);
}

static u32 mqttv1_get_next_reconn_timeout(struct mqttv1_client_manager *cm)
{
	if (cm->reconn_tries == 0) {
		cm->reconn_ms = MQTTV1_RECONN_WAIT1;
	} else if (cm->reconn_tries < MQTTV1_RECONN_THRESH) {
		cm->reconn_ms = MQTTV1_RECONN_WAIT2;
	} else {
		cm->reconn_ms = MQTTV1_RECONN_WAIT3;
	}
	if (cm->reconn_tries < 255) {
		cm->reconn_tries++;
	}
	mqtt_log(LOG_DEBUG, "reconnect in %lu ms", cm->reconn_ms);
	return cm->reconn_ms;
}

/* Subscribe the topic "DOWN/{DSN}". */
static int mqttv1_sub_down_topic(struct mqttv1_client_manager *cm)
{
	int ret;
	struct mqttv1_sub *sub = &cm->sub;
	char down_topic[MQTTV1_TOPIC_MAX_LEN];

	snprintf(down_topic, sizeof(down_topic),
	    MQTTV1_DOWNSTREAM_TOPIC_FMT, cm->client_id);
	ret = mqtt_client_subscribe_topic(cm->mc, down_topic, sub->qos,
	    sub->sub_handle, sub->suback_cb);
	if (ret) {
		mqtt_log(LOG_ERR, "subcribe topic \"%s\" error.", down_topic);
	} else {
		MQTTV1_CH_STATE(cm, sub.state, MC_SS_SUBING);
	}
	return ret;
}

static void mqttv1_conn_authfail(struct mqttv1_client_manager *cm)
{
	mqttv1_disconnect_reset(cm);
	/*
	 * Notify the client so it will retry with RSA authentication.
	 */
	if (cm->req_err_cb) {
		cm->req_err_cb(AL_ERR_AUTH_FAIL, cm->arg);
	}
	/*
	 * There should be no need to retry as the client should
	 * initiate a new connection immediately. However, if the
	 * client is unable to make progress, a periodic retry
	 * helps debugging, even if it fails.
	 */
	if (cm->stay_connected) {
		client_timer_set(&cm->reconn_timer,
		    mqttv1_get_next_reconn_timeout(cm));
	}
	if (cm->conn_state_cb) {
		cm->conn_state_cb(MCS_DISCONNECTED, cm->arg);
	}
}

static void mqttv1_disconnected(struct mqttv1_client_manager *cm)
{
	mqttv1_disconnect_reset(cm);
	if (cm->req_pending) {
		cm->req_pending = 0;
		if (cm->req_err_cb) {
			cm->req_err_cb(AL_ERR_CLSD, cm->arg);
		}
	}
	if (cm->stay_connected) {
		client_timer_set(&cm->reconn_timer,
		    mqttv1_get_next_reconn_timeout(cm));
	}
	if (cm->conn_state_cb) {
		cm->conn_state_cb(MCS_DISCONNECTED, cm->arg);
	}
}

static void mqttv1_default_connack_cb(struct mqtt_client *mc,
	    enum mqtt_client_conn_err err, void *arg)
{
	int ret;
	struct mqttv1_client_manager *cm = (struct mqttv1_client_manager *)arg;
	struct mqttv1_pub *pub = &cm->pub;
	struct mqttv1_sub *sub = &cm->sub;

	switch (err) {
	case MQTT_CONN_ERR_OK:
		mqtt_log(LOG_DEBUG, "client connected.");
		MQTTV1_CH_STATE(cm, conn_state, MC_CS_CONNECTED);
		if (sub->state == MC_SS_UNSUB) {
			ret = mqttv1_sub_down_topic(cm);
			if (ret) {
				mqtt_log(LOG_ERR, "sub_down failed %d", ret);
				break;
			}
		}
		if (cm->req_pending) {
			if (pub->state == MC_PS_IDLE &&
			    cm->req_url_type != MC_URL_GET_DSNS) {
				ret = mqttv1_publish_req_start(cm);
				if (ret) {
					mqtt_log(LOG_ERR, "pub failed %d", ret);
					break;
				}
			}
		} else {
			mqttv1_reconn_reset(cm);
		}
		if (cm->conn_state_cb) {
			cm->conn_state_cb(MCS_CONNECTED, cm->arg);
		}
		break;
	case MQTT_CONN_ERR_NOT_AUTH:
		mqtt_log(LOG_ERR, "client connect error: %d", err);
		mqttv1_conn_authfail(cm);
		break;
	default:
		mqtt_log(LOG_ERR, "client connect error: %d", err);
		mqttv1_disconnected(cm);
		break;
	}
}

static void mqttv1_default_err_cb(struct mqtt_client *mc,
		enum al_err err, void *arg)
{
	struct mqttv1_client_manager *cm = (struct mqttv1_client_manager *)arg;

	ASSERT(client_locked);
	mqtt_log(LOG_ERR, "client error: %s", al_err_string(err));
	mqttv1_disconnected(cm);
}

static void mqttv1_default_disconnected_cb(struct mqtt_client *mc, void *arg)
{
	struct mqttv1_client_manager *cm = (struct mqttv1_client_manager *)arg;

	mqtt_log(LOG_DEBUG, "client disconnected.");
	mqttv1_disconnected(cm);
}

static void mqttv1_default_pub_completed(struct mqtt_client *mc,
		void *arg, enum al_err err)
{
	struct mqttv1_client_manager *cm = (struct mqttv1_client_manager *)arg;
	struct mqttv1_pub *pub = &cm->pub;

	switch (err) {
	case AL_ERR_OK:
		mqtt_log(LOG_DEBUG2, "publish success.");
		break;
	default:
		mqtt_log(LOG_ERR, "publish error: %s",
		    al_err_string(err));
		if (cm->req_pending) {
			cm->req_pending = 0;
			if (cm->req_err_cb) {
				cm->req_err_cb(err, cm->arg);
			}
		}
		break;
	}
	MQTTV1_CH_STATE(cm, pub.state, MC_PS_IDLE);
	pub->offset = 0;
	pub->total_len = 0;
}

static void mqttv1_parser_reset(struct mqttv1_client_manager *cm)
{
	struct mqttv1_js_parser *pp = &cm->parser;
	struct ame_decoder_state *ds = &pp->ds;

	memset(pp, 0, sizeof(*pp));
	ame_decoder_reset(ds);
	ame_decoder_stack_set(ds, pp->key_stack, ARRAY_LEN(pp->key_stack));
	ame_decoder_set_ext_mode(ds, 1,
	    (void *)pp->val_buf, sizeof(pp->val_buf));
	ame_decoder_tree_set(ds, &ame_json_dec, mqttv1_ame_tags, cm);
}

static int mqttv1_js_handle_req_id(const struct ame_decoder *dec,
		void *arg, struct ame_kvp *kvp)
{
	struct mqttv1_client_manager *cm = (struct mqttv1_client_manager *)arg;
	struct mqttv1_msg *msg = &cm->msg;

	if (dec->get_u32(kvp, NULL, &msg->req_id)) {
		mqtt_log(LOG_ERR, "req_id parse failed");
		return 0;
	}
	msg->have_req_id = 1;
	mqtt_log(LOG_DEBUG2, "get req_id: %lu", msg->req_id);
	msg->type = (enum mc_msg_type)(msg->req_id >> 24);
	switch (msg->type) {
	case MC_MSG_TYPE_NOTIFY:
		mqtt_log(LOG_DEBUG, "notify");
		if (cm->notify_cb) {
			cm->notify_cb(cm->arg);
		}
		break;
	case MC_MSG_TYPE_DSN_RESP:
		break;
	default:
	case MC_MSG_TYPE_NONE:
		mqtt_log(LOG_ERR, "%s: unexpected msg type %u",
		    __func__, msg->type);
		break;
	}
	return 0;
}

static int mqttv1_js_handle_resp_id(const struct ame_decoder *dec,
		void *arg, struct ame_kvp *kvp)
{
	struct mqttv1_client_manager *cm = (struct mqttv1_client_manager *)arg;
	struct mqttv1_msg *msg = &cm->msg;

	if (dec->get_u32(kvp, NULL, &msg->resp_id)) {
		mqtt_log(LOG_ERR, "resp_id parse failed");
		return 0;
	}
	msg->have_resp_id = 1;
	mqtt_log(LOG_DEBUG2, "get resp_id: %lu", msg->resp_id);
	return 0;
}

static int mqttv1_js_handle_status(const struct ame_decoder *dec,
		void *arg, struct ame_kvp *kvp)
{
	struct mqttv1_client_manager *cm = (struct mqttv1_client_manager *)arg;
	struct mqttv1_msg *msg = &cm->msg;

	if (dec->get_u32(kvp, NULL, &msg->status)) {
		mqtt_log(LOG_ERR, "status parse failed");
		return 0;
	}
	msg->have_status = 1;
	mqtt_log(LOG_DEBUG2, "get status: %lu", msg->status);
	return 0;
}

static int mqttv1_js_handle_header(const struct ame_decoder *dec,
		void *arg, struct ame_kvp *kvp)
{
	struct mqttv1_client_manager *cm = (struct mqttv1_client_manager *)arg;
	struct mqttv1_msg *msg = &cm->msg;
	struct ame_kvp *node;
	struct http_hdr *hdrs;
	int hcnt = 0;
	enum ada_err err;
	char *cp;
	size_t len;
	size_t rlen;

	if (!cm->req_pending) {
		return 0;
	}
	if (kvp->type != AME_TYPE_OBJECT) {
		mqtt_log(LOG_WARN, "invalid header");
		return 0;
	}
	if (msg->started) {
		mqtt_log(LOG_WARN, "unexpected header after start");
		return 0;
	}

	/*
	 * If response didn't have a request ID or response ID, don't ASSERT.
	 */
	if (!msg->have_req_id && !msg->have_resp_id) {
		mqtt_log(LOG_WARN, "no request or response ID");
	}
	if (!msg->have_status) {
		mqtt_log(LOG_WARN, "no status");
	}

	/*
	 * Total temporary space needed for headers.
	 */
	len = 0;
	for (node = kvp->child; node; node = node->next) {
		if (node->type != AME_TYPE_UTF8 || !node->key || !node->value) {
			continue;
		}
		hcnt++;
		len += sizeof(struct http_hdr) +
		    node->key_length + 1 + node->value_length + 1;
	}
	hdrs = al_os_mem_alloc(len);
	if (!hdrs) {
		mqtt_log(LOG_ERR, "%s: hdr alloc failed", __func__);
		return 0;
	}
	cp = (char *)(hdrs + hcnt);
	rlen = len - hcnt * sizeof(struct http_hdr);
	hcnt = 0;

	/*
	 * Build header array from child objects.
	 */
	for (node = kvp->child; node; node = node->next) {
		if (node->type != AME_TYPE_UTF8 || !node->key || !node->value) {
			continue;	/* since skipped in length calc */
		}

		len = rlen;
		err = dec->get_utf8(node, NULL, cp, &len);
		if (err) {
			mqtt_log(LOG_ERR, "%s: hdr %u parse err %d - skipped",
			    __func__, hcnt, err);
			continue;
		}
		if (len >= rlen) {
			mqtt_log(LOG_ERR,
			    "%s: hdr %u val too long %zu rlen %zu",
			    __func__, hcnt, len, rlen);
			break;
		}
		hdrs[hcnt].val = cp;
		cp += len;
		*cp++ = '\0';
		rlen -= len + 1;

		len = node->key_length;
		if (len >= rlen) {
			mqtt_log(LOG_ERR,
			    "%s: hdr %u name too long %zu rlen %zu",
			    __func__, hcnt, len, rlen);
			break;
		}
		hdrs[hcnt].name = cp;
		memcpy(cp, node->key, len);
		cp += len;
		*cp++ = '\0';
		rlen -= len + 1;
		hcnt++;
	}

	if (msg->type == MC_MSG_TYPE_NONE &&
	    (!msg->have_resp_id || msg->resp_id != cm->req_id)) {
		mqtt_log(LOG_ERR, "req id != resp id");
		goto free;
	}

	if (cm->resp_start_cb) {
		msg->started = 1;
		cm->resp_start_cb(msg->status, hcnt, hdrs, cm->arg);
	}
free:
	al_os_mem_free(hdrs);
	return 0;
}

static void mqttv1_response_recv(const void *buf, size_t len)
{
	struct mqttv1_client_manager *cm = &gv_client_manager;
	struct mqttv1_msg *msg = &cm->msg;
	size_t consumed;

	if (!cm->req_pending) {
		cm->paused_len = 0;
		return;
	}
	if (!msg->started) {
		mqtt_log(LOG_ERR, "%s: payload before header", __func__);
		cm->paused_len = 0;
		return;
	}
	if (!cm->resp_payload_cb) {
		cm->paused_len = 0;
		return;		/* no payload expected */
	}
	consumed = cm->resp_payload_cb(buf, len, cm->arg);
	if (consumed < len) {
		cm->paused_len = len - consumed;
		ASSERT(cm->paused_len <= sizeof(cm->paused_buf));
		/* Note: buf pointer may be in paused_buf, so use memmove() */
		memmove(cm->paused_buf, (char *)buf + consumed, cm->paused_len);
		mqttv1_client_pause();
	} else {
		cm->paused_len = 0;
	}
}

static int mqttv1_js_handle_payload(const struct ame_decoder *dec,
		void *arg, struct ame_kvp *kvp)
{
	struct mqttv1_client_manager *cm = &gv_client_manager;

	if (!kvp) {
		if (!cm->resp_payload_cb) {
			return 0;
		}
		cm->resp_payload_cb(NULL, 0, cm->arg);
		cm->resp_payload_cb = NULL;
		return 0;
	}
	if (kvp->type != AME_TYPE_OBJECT && kvp->type != AME_TYPE_ARRAY) {
		mqtt_log(LOG_ERR, "%s: unexpected payload type %u",
		    __func__, kvp->type);
		return 0;
	}
	mqttv1_response_recv(kvp->value, kvp->value_length);
	if (cm->paused_len) {
		return AE_BUF;
	}
	return 0;
}

void mqttv1_client_pause(void)
{
	struct mqttv1_client_manager *cm = &gv_client_manager;

	cm->paused = 1;
	mqtt_client_pause(cm->mc);
}

/*
 * After pause and continue, deliver the remaining value from the payload.
 */
static void mqttv1_client_continue_cb(void *arg)
{
	struct mqttv1_client_manager *cm = arg;

	if (!cm->paused) {
		return;
	}
	if (cm->paused_len) {
		mqtt_log(LOG_DEBUG2, "%s: paused_len %zu",
		    __func__, cm->paused_len);
		mqttv1_response_recv(cm->paused_buf, cm->paused_len);
		if (cm->paused_len) {
			return;
		}
	}
	mqtt_log(LOG_DEBUG2, "%s: continuing", __func__);
	cm->paused = 0;
	mqtt_client_continue(cm->mc);
}

void mqttv1_client_continue(void)
{
	struct mqttv1_client_manager *cm = &gv_client_manager;

	if (!cm->paused) {
		return;
	}
	client_callback_pend(&cm->continue_callback);
}

static size_t mqttv1_default_sub_handle(struct mqtt_client *mc, void *arg,
	    const void *payload, size_t len, size_t total_len)
{
	struct mqttv1_client_manager *cm = (struct mqttv1_client_manager *)arg;
	struct mqttv1_msg *msg = &cm->msg;
	enum ame_parse_op parse_op;
	struct ame_kvp *kvp;
	enum ada_err err;
	size_t consumed = len;

	ASSERT(!cm->paused);

	switch (msg->msg_state) {
	case MC_MSG_NONE:
		mqtt_log(LOG_DEBUG2, "new message(total_len=%zu)",
		    total_len);
		msg->total_len = total_len;
		msg->msg_state = MC_MSG_IN_PROGRESS;
		FALL_THROUGH;
		/* fall through */
	case MC_MSG_IN_PROGRESS:
		ASSERT(msg->total_len == total_len);
		ASSERT(msg->offset + len <= msg->total_len);

		if (msg->offset) {
			parse_op = AME_PARSE_NEXT;
		} else {
			mqttv1_parser_reset(cm);
			parse_op = AME_PARSE_FIRST;
		}

		mqtt_log(LOG_DEBUG2, "receive message(%lu-%lu).",
		    msg->offset, msg->offset + len - 1);
		log_bytes(mqttv1_client_log_mod_nr,
		    LOG_SEV_DEBUG2, payload, len,
		    "sub msg");

		msg->offset += len;
		if (msg->parse_err == AL_ERR_OK) {
			ame_decoder_buffer_set(&cm->parser.ds, payload, len);
			err = ame_json_dec.parse(&cm->parser.ds,
			    parse_op, &kvp);
			if (err == AE_BUF) {
				consumed = cm->parser.ds.offset;
				ASSERT(cm->paused);
				ASSERT(consumed <= len);
				msg->offset -= len - consumed;
				ASSERT(msg->offset < total_len);
				break;
			}
			msg->parse_err = ada_err_to_al_err(err);
			if (err == AE_UNEXP_END && msg->offset != total_len) {
				msg->parse_err = AL_ERR_OK;
			}
			if (msg->parse_err != AL_ERR_OK) {
				mqtt_log(LOG_ERR, "json parse error: %s",
				    al_err_string(msg->parse_err));
				if (cm->req_pending) {
					cm->req_pending = 0;
					if (cm->req_err_cb) {
						cm->req_err_cb(msg->parse_err,
						    cm->arg);
					}
				}
			}
		}
		if (msg->offset == total_len) {
			mqtt_log(LOG_DEBUG2,
			    "receive message done err %d pending %d type %d",
			    msg->parse_err, cm->req_pending, msg->type);
			msg->msg_state = MC_MSG_DONE;
			if (msg->parse_err == AL_ERR_OK && cm->req_pending &&
			    msg->type != MC_MSG_TYPE_NOTIFY) {
				cm->req_pending = 0;
				if (cm->resp_payload_cb) {
					cm->resp_payload_cb(NULL, 0, cm->arg);
				}
			}
			mqttv1_msg_reset(msg);
		}
		break;
	default:
		ASSERT_NOTREACHED();
		break;
	}
	return consumed;
}

static void mqttv1_default_suback_cb(struct mqtt_client *mc, void *arg,
	    enum al_err err)
{
	struct mqttv1_client_manager *cm = (struct mqttv1_client_manager *)arg;

	switch (err) {
	case AL_ERR_OK:
		mqtt_log(LOG_DEBUG2, "subscribe success.");
		MQTTV1_CH_STATE(cm, sub.state, MC_SS_SUBED);
		break;
	default:
		mqtt_log(LOG_ERR, "subscribe error: %s",
		    al_err_string(err));
		MQTTV1_CH_STATE(cm, sub.state, MC_SS_UNSUB);
		if (cm->req_pending) {
			cm->req_pending = 0;
			if (cm->req_err_cb) {
				cm->req_err_cb(err, cm->arg);
			}
		}
		break;
	}
}

/*
 * Send publish suffix, if any, if all payload has been sent.
 * Caller will call again if it cannot be sent.
 */
static void mqttv1_client_send_suffix(void)
{
	enum al_err ret;
	struct mqttv1_client_manager *cm = &gv_client_manager;
	struct mqttv1_pub *pub = &cm->pub;

	if (!pub->suffix) {
		return;
	}
	if (pub->offset + pub->suffix_len == pub->total_len) {
		log_bytes(mqttv1_client_log_mod_nr, LOG_SEV_DEBUG2,
		    pub->suffix, pub->suffix_len,
		    "pub_suf");
		ret = mqtt_client_publish_payload(cm->mc,
		    pub->suffix, pub->suffix_len);
		if (ret) {
			if (ret == AL_ERR_BUF) {
				return;
			}
			mqtt_log(LOG_ERR, "publish suffix rc %d", ret);
			return;
		}
		pub->offset += pub->suffix_len;
	}
	if (pub->offset > pub->total_len) {
		mqtt_log(LOG_ERR, "%s: publish offset %lu gt total_len %lu",
		    __func__, pub->offset, pub->total_len);
	}
}

static void mqttv1_client_sent_cb(void *arg)
{
	struct mqttv1_client_manager *cm = &gv_client_manager;
	struct mqttv1_pub *pub = &cm->pub;

	if (pub->offset + pub->suffix_len < pub->total_len && cm->sent_cb) {
		cm->sent_cb(cm->arg);
	}
	mqttv1_client_send_suffix();
}

/**
 * Connect to broker.
 *
 * \note If returns AL_ERR_OK, it means that the client is connected.
 */
static enum al_err mqttv1_connect(struct mqttv1_client_manager *cm)
{
	enum al_err ret = AL_ERR_ERR;
	int rc;

	ASSERT(client_locked);
	switch (cm->conn_state) {
	case MC_CS_DISCONNECTED:
		mqtt_client_set_sent_cb(cm->mc, mqttv1_client_sent_cb);
		rc = mqtt_client_connect(cm->mc, cm->host, cm->port,
		    cm->ssl_enable, cm->client_id, cm->connack_cb,
		    cm->err_cb);
		if (rc) {
			return AL_ERR_ERR;	/* unspecified error */
		}
		MQTTV1_CH_STATE(cm, conn_state, MC_CS_CONNECTING);
		ret = AL_ERR_IN_PROGRESS;
		break;
	case MC_CS_CONNECTING:
		ret = AL_ERR_IN_PROGRESS;
		break;
	case MC_CS_CONNECTED:
		ret = AL_ERR_OK;
		break;
	case MC_CS_DISCONNECTING:
		if (cm->disconnected_cb == mqttv1_reconn_disconnected_cb) {
			ret = AL_ERR_IN_PROGRESS;
		} else {
			ret = AL_ERR_INVAL_STATE;
		}
		break;
	}
	return ret;
}

static void mqttv1_reconn_timer_handler(struct timer *timer)
{
	enum al_err err;
	struct mqttv1_client_manager *cm = &gv_client_manager;

	if (!cm->stay_connected) {
		return;
	}

	mqtt_log(LOG_DEBUG2, "reconnect...");
	err = mqttv1_connect(cm);
	switch (err) {
	case AL_ERR_OK:
	case AL_ERR_IN_PROGRESS:
		break;
	default:
		client_timer_set(&cm->reconn_timer,
		    mqttv1_get_next_reconn_timeout(cm));
		break;
	}
}

void mqttv1_client_init(void)
{
	struct mqttv1_client_manager *cm = &gv_client_manager;
	struct mqttv1_pub *pub = &cm->pub;
	struct mqttv1_sub *sub = &cm->sub;

	cm->mc = mqtt_client_new();
	if (!cm->mc)  {
		mqtt_log(LOG_ERR, "%s: client alloc failed", __func__);
		ASSERT_NOTREACHED();	/* We need this allocation.  No retry */
	}
	mqtt_client_set_arg(cm->mc, cm);

	cm->port = MQTTV1_BROKER_DEFAULT_PORT;
	cm->ssl_enable = MQTTV1_SSL_ENABLE;
	cm->keepalive = MQTT_CLIENT_DEFAULT_KEEPALIVE;
	cm->connack_cb = mqttv1_default_connack_cb;
	cm->err_cb = mqttv1_default_err_cb;
	cm->disconnected_cb = mqttv1_default_disconnected_cb;

	strncpy(cm->host, MQTTV1_BROKER_DEFAULT_HOST, sizeof(cm->host));

	cm->client_id = conf_sys_dev_id;

	pub->qos = MQTT_QOS_1;
	pub->pub_completed = mqttv1_default_pub_completed;

	sub->qos = MQTT_QOS_2;
	sub->sub_handle = mqttv1_default_sub_handle;
	sub->suback_cb = mqttv1_default_suback_cb;

	timer_handler_init(&cm->reconn_timer, mqttv1_reconn_timer_handler);
	callback_init(&cm->continue_callback, mqttv1_client_continue_cb, cm);
}

#ifdef AYLA_FINAL_SUPPORT
void mqttv1_client_final(void)
{
	int ret;
	struct mqttv1_client_manager *cm = &gv_client_manager;

	client_lock();
	mqttv1_msg_reset(&cm->msg);
	mqttv1_disconnect_reset(cm);
	mqtt_client_abort(cm->mc);
	client_unlock();
	mqtt_client_free(cm->mc);
	memset(cm, 0, sizeof(*cm));
}
#endif

static void mqttv1_reconn_disconnected_cb(struct mqtt_client *mc, void *arg)
{
	enum al_err ret;
	struct mqttv1_client_manager *cm = (struct mqttv1_client_manager *)arg;

	mqttv1_default_disconnected_cb(mc, arg);
	cm->disconnected_cb = mqttv1_default_disconnected_cb;

	mqtt_log(LOG_DEBUG2, "client is reconnecting...");
	/* TBD: Check the return value? */
	ret = mqttv1_connect(cm);
	switch (ret) {
	case AL_ERR_IN_PROGRESS:
		break;
	case AL_ERR_OK:
		/* Not reach */
		ASSERT_NOTREACHED();
		break;
	default:
		break;
	}
}

static int mqttv1_reconnect(void)
{
	int ret;
	struct mqttv1_client_manager *cm = &gv_client_manager;

	cm->disconnected_cb = mqttv1_reconn_disconnected_cb;
	ret = mqtt_client_disconnect(cm->mc, cm->disconnected_cb);
	if (!ret) {
		MQTTV1_CH_STATE(cm, conn_state, MC_CS_DISCONNECTING);
	}
	return ret;
}

void mqttv1_client_set_arg(void *arg)
{
	struct mqttv1_client_manager *cm = &gv_client_manager;

	cm->arg = arg;
}

void mqttv1_client_set_keepalive(u16 keepalive)
{
	struct mqttv1_client_manager *cm = &gv_client_manager;

	if (cm->keepalive == keepalive) {
		return;
	}
	cm->keepalive = keepalive;
	mqtt_client_set_conn_keepalive(cm->mc, keepalive);

	/* If client is connected, reconnect */
	switch (cm->conn_state) {
	case MC_CS_CONNECTED:
	case MC_CS_CONNECTING:
		/* TBD: Check the return value? */
		mqttv1_reconnect();
		break;
	default:
		break;
	}
}

void mqttv1_client_set_notify_cb(void (*notify_cb)(void *arg))
{
	struct mqttv1_client_manager *cm = &gv_client_manager;

	cm->notify_cb = notify_cb;
}

void mqttv1_client_set_conn_state_cb(
		void (*conn_state_cb)(enum mqttv1_conn_state st, void *arg))
{
	struct mqttv1_client_manager *cm = &gv_client_manager;

	cm->conn_state_cb = conn_state_cb;
}

void mqttv1_client_disconnect(int clear_conn_flag)
{
	int ret;
	struct mqttv1_client_manager *cm = &gv_client_manager;

	if (clear_conn_flag) {
		cm->stay_connected = 0;
	}
	switch (cm->conn_state) {
	case MC_CS_CONNECTED:
	case MC_CS_CONNECTING:
		MQTTV1_CH_STATE(cm, conn_state, MC_CS_DISCONNECTING);
		ret = mqtt_client_disconnect(cm->mc,
		    mqttv1_default_disconnected_cb);
		if (ret) {
			MQTTV1_CH_STATE(cm, conn_state, MC_CS_DISCONNECTED);
			break;
		}
		break;
	case MC_CS_DISCONNECTED:
		mqtt_log(LOG_DEBUG, "client is already disconnected.");
		break;
	case MC_CS_DISCONNECTING:
		mqtt_log(LOG_DEBUG, "client is disconnecting.");
		break;
	default:
		break;
	}
}

/* Get request content type. */
static enum mc_ctype mqttv1_get_req_ctype(const char *type)
{
	if (!strcmp(type, "application/json")) {
		return MC_CTYPE_APP_JSON;
	} else if (!strcmp(type, "application/octet-stream")) {
		return MC_CTYPE_APP_STREAM;
	} else if (!strcmp(type,  "application/xml")) {
		return MC_CTYPE_APP_XML;
	}
	return MC_CTYPE_NONE;
}

/* Generate a request ID. */
static u32 mqttv1_gen_req_id(void)
{
	static u16 count;

	return ((clock_utc() & 0x0000ffff) << 16) + count++;
}

/* Get the type of the request URL. */
static enum mc_url_type mqttv1_get_url_type(const char *url)
{
	if (!strncmp(url, "/dsns/", sizeof("/dsns/") - 1)) {
		return MC_URL_GET_DSNS;
	}
	return MC_URL_NONE;
}

/* Generate a proof username. */
static char *mqttv1_gen_proof_username(const char *url, char *buf, u32 buf_len)
{
	char *p;
	int len;
	int reset = 0;

	len = snprintf(buf, buf_len, MQTTV1_PROOF_USERNAME_PREFIX);

	p = strrchr(url, '?');
	if (!p) {
		/* return "V1/PROOF" */
		return buf;
	}
	p++;

	if (strstr(p, "reset=1")) {
		p += (sizeof("reset=1") - 1);
		reset = 1;
		len += snprintf(buf + len, buf_len - len, "?r=1");
	}

	if (strstr(p, "test=1")) {
		len += snprintf(buf + len, buf_len - len, "%ct=1",
		    reset ? '&' : '?');
	}

	if (len > buf_len - 1) {
		return NULL;
	}
	return buf;
}

void mqttv1_client_set_broker(const char *hostname, u16 port)
{
	struct mqttv1_client_manager *cm = &gv_client_manager;

	strncpy(cm->host, hostname, sizeof(cm->host));
	cm->port = port;
}

enum al_err mqttv1_client_req_start(enum http_client_method method,
	const char *url,
	int hcnt, const struct http_hdr *hdrs, u32 payload_len,
	void (*resp_start_cb)(u32 http_status, int hcnt,
	    const struct http_hdr *hdrs, void *arg),
	size_t (*resp_payload_cb)(const void *part, size_t len, void *arg),
	void (*err_cb)(enum al_err err, void *arg))
{
	struct mqttv1_client_manager *cm = &gv_client_manager;
	struct mqttv1_pub *pub = &cm->pub;
	const char *method_str = NULL;
	char username[MQTTV1_USERNAME_MAX_LEN];
	const char *password;
	enum al_err err = AL_ERR_OK;
	size_t len;
	int ret;
	int i;

	ASSERT(client_locked);
	if (cm->req_pending) {
		return AL_ERR_IN_PROGRESS;
	}

	mqtt_client_log_set(cm->mc, mqttv1_client_log_mod_nr);

	cm->req_ctype = MC_CTYPE_NONE;
	cm->req_url_type = mqttv1_get_url_type(url);
	cm->ac_type = MC_AC_NONE;
	cm->paused = 0;

	if (pub->state != MC_PS_IDLE) {
		mqtt_log(LOG_ERR, "%s: pub not idle off %lu totlen %lu",
		    __func__, pub->offset, pub->total_len);
		return AL_ERR_INVAL_STATE;
	}
	pub->total_len = 0;
	pub->suffix = NULL;
	pub->suffix_len = 0;

	for (i = 0; i < hcnt; i++) {
		if (!strcasecmp(hdrs[i].name, "Content-Type")) {
			cm->req_ctype = mqttv1_get_req_ctype(hdrs[i].val);
		} else if (!strcasecmp(hdrs[i].name, HTTP_CLIENT_KEY_FIELD)) {
			mqtt_log(LOG_DEBUG, "%s auth:token", __func__);
			password = hdrs[i].val;
			if (!strncmp(hdrs[i].val, HTTP_CLIENT_AUTH_VER,
			    sizeof(HTTP_CLIENT_AUTH_VER) - 1)) {
				password += sizeof(HTTP_CLIENT_AUTH_VER) - 1;
				while (*password == ' ') {
					password++;
				}
			}
			cm->ac_type = MC_AC_AUTH_TOKEN;
			cm->req_url_type = MC_URL_NONE;
			err = mqtt_client_set_conn_account(cm->mc,
			    MQTTV1_AUTH_TOKEN_USERNAME, password);
			if (err) {
				return err;
			}
		} else if (!strcasecmp(hdrs[i].name,
		    HTTP_CLIENT_INIT_AUTH_HDR)) {
			mqtt_log(LOG_DEBUG, "%s auth:rsa", __func__);
			ASSERT(mqttv1_gen_proof_username(url, username,
			    sizeof(username)));
			cm->ac_type = MC_AC_PROOF;
			err = mqtt_client_set_conn_account(cm->mc, username,
			    hdrs[i].val);
			if (err) {
				return err;
			}
		}
	}

	if (cm->req_url_type == MC_URL_GET_DSNS &&
	    cm->conn_state != MC_CS_CONNECTED) {
		goto connect;
	}

	switch (method) {
	case HTTP_REQ_GET:
		method_str = "GET";
		break;
	case HTTP_REQ_PUT:
		method_str = "PUT";
		break;
	case HTTP_REQ_POST:
		method_str = "POST";
		break;
	default:
		ASSERT_NOTREACHED();
	}

	cm->req_id = mqttv1_gen_req_id();
	len = snprintf(cm->req_buf, sizeof(cm->req_buf),
	    "{\"req_id\":%lu,\"method\":\"%s\",\"url\":\"%s\",\"header\":{",
	    cm->req_id, method_str, url);

	for (i = 0; i < hcnt; i++) {
		len += snprintf(cm->req_buf + len, sizeof(cm->req_buf) - len,
		    "\"%s\":\"%s\",",
		    hdrs[i].name, hdrs[i].val);
	}

	*(cm->req_buf + len - 1) = '}';

	switch (method) {
	case HTTP_REQ_GET:
		len += snprintf(cm->req_buf + len,
		    sizeof(cm->req_buf) - len, "}");
		break;
	case HTTP_REQ_POST:
	case HTTP_REQ_PUT:
		len += snprintf(cm->req_buf + len, sizeof(cm->req_buf) - len,
		    ",\"payload\":");
		switch (cm->req_ctype) {
		case MC_CTYPE_APP_JSON:
			if (payload_len == 0) {
				len += snprintf(cm->req_buf + len,
				    sizeof(cm->req_buf) - len, "{}}");
			} else {
				pub->suffix_len = sizeof("}") - 1;
				pub->suffix = "}";
			}
			break;
		case MC_CTYPE_APP_STREAM:
			len += snprintf(cm->req_buf + len,
			    sizeof(cm->req_buf) - len, "{\"value\":\"");
			if (payload_len == 0) {
				snprintf(cm->req_buf + len,
				    sizeof(cm->req_buf) - len, "\"}}");
			} else {
				pub->suffix_len = sizeof("\"}}") - 1;
				pub->suffix = "\"}}";
			}
			break;
		case MC_CTYPE_NONE:
			ASSERT(!payload_len);
			len += snprintf(cm->req_buf + len,
			    sizeof(cm->req_buf) - len, "null}");
			break;
		default:
			ASSERT_NOTREACHED();
			break;
		}
		break;
	default:
		ASSERT_NOTREACHED();
		break;
	}

	if (len > sizeof(cm->req_buf) - 1) {
		mqtt_log(LOG_ERR, "req too long");
		return AL_ERR_BUF;
	}
	pub->offset = len;
	pub->total_len = pub->offset + payload_len + pub->suffix_len;

connect:
	err = mqttv1_connect(cm);
	switch (err) {
	case AL_ERR_OK:
		/* Publish topic */
		ret = mqttv1_publish_req_start(cm);
		if (ret) {
			return AL_ERR_ERR;
		}
		cm->req_pending = 1;
		cm->stay_connected = 1;
		break;
	case AL_ERR_IN_PROGRESS:
		cm->req_pending = 1;
		cm->stay_connected = 1;
		mqttv1_reconn_reset(cm);
		err = AL_ERR_OK;
		break;
	default:
		err = AL_ERR_ERR;
		goto end;
	}
	cm->req_err_cb = err_cb;
	cm->resp_start_cb = resp_start_cb;
	cm->resp_payload_cb = resp_payload_cb;
end:
	return err;
}

enum al_err mqttv1_client_send_payload(const void *part, u32 len)
{
	enum al_err ret;
	struct mqttv1_client_manager *cm = &gv_client_manager;
	struct mqttv1_pub *pub = &cm->pub;

	if (!cm->req_pending) {
		return AL_ERR_ERR;
	}

	if (pub->state != MC_PS_PUBING) {
		return AL_ERR_ERR;
	}

	log_bytes(mqttv1_client_log_mod_nr, LOG_SEV_DEBUG2, part, len,
	    "pub msg");
	ret = mqtt_client_publish_payload(cm->mc, part, len);
	if (ret) {
		if (ret == AL_ERR_BUF) {
			return ret;
		}
		mqtt_log(LOG_ERR, "publish rc %d", ret);
		return ret;
	}
	pub->offset += len;
	mqttv1_client_send_suffix();
	return AL_ERR_OK;
}

void mqttv1_client_abort(void)
{
	struct mqttv1_client_manager *cm = &gv_client_manager;

	cm->req_pending = 0;
	cm->stay_connected = 0;
}

void mqttv1_client_timeout_abort(void)
{
	struct mqttv1_client_manager *cm = &gv_client_manager;

	cm->req_pending = 0;
}

void mqttv1_client_set_sent_cb(void (*sent_cb)(void *arg))
{
	struct mqttv1_client_manager *cm = &gv_client_manager;

	cm->sent_cb = sent_cb;
}
