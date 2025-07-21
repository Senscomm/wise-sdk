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
#include <ctype.h>

#include <ayla/assert.h>
#include <ayla/utypes.h>
#include <ayla/log.h>
#include <ayla/mod_log.h>
#include <ayla/http.h>
#include <ayla/clock.h>
#include <ayla/parse.h>
#include <ayla/tlv.h>
#include <ayla/parse.h>
#include <ayla/timer.h>

#include <al/al_utypes.h>

#include <ada/client.h>
#include <al/al_net_dns.h>
#include "mqttv1_client.h"
#include "http2mqtt_client.h"

#include "client_timer.h"
#include "client_lock.h"

/*
 * Macro to make logging easier
 */
#define HTTP2MQTT_CLIENT_LOGF(_hc, _level, _format, ...) \
	http2mqtt_client_log(_hc, _level "%s: " _format, \
	__func__, ##__VA_ARGS__)

#define HTTP2MQTT_CLIENT_DEBUG(_hc, _level, _format, ...) \
	HTTP2MQTT_CLIENT_LOGF(_hc, _level, _format, ##__VA_ARGS__)

#define HTTP_CLIENT_CT_LEN "Content-Length"
#define HTTP_CLIENT_CT_TYPE "Content-Type"
#define HTTP_CLIENT_CT_RANGE "Content-Range"
#define HTTP_CLIENT_CHUNK_ENCODING "transfer-encoding"
#define HTTP_CLIENT_X_RUNTIME "X-Runtime"
#define HTTP_CLIENT_DATE "Date"
#define HTTP_CLIENT_DATE_ARG_CNT 6

static void http2mqtt_client_timeout(struct timer *arg);
static void http2mqtt_client_log(struct http_client *, const char *fmt, ...)
	ADA_ATTRIB_FORMAT(2, 3);

/*
 * Prints log messages for the mod_log_id of this http_client.
 */
static void http2mqtt_client_log(struct http_client *hc, const char *fmt, ...)
{
	ADA_VA_LIST args;
	ADA_VA_START(args, fmt);
	log_put_va(hc->mod_log_id, fmt, args);
	ADA_VA_END(args);
}

/*
 * Parses the content length of the http header. If found, sets content_given.
 */
static void http2mqtt_client_parse_len(struct http_client *hc, const char *val)
{
	unsigned long len;
	char *errptr;

	len = strtoul(val, &errptr, 10);
	if (*errptr != '\0' || len >= MAX_U32) {
		HTTP2MQTT_CLIENT_LOGF(hc, LOG_WARN, "bad len %s", val);
		return;
	}
	hc->content_len = len;
	hc->content_given = 1;
}

/*
 * Parses encoding type in the header if given.
 */
static void http2mqtt_client_parse_encoding(struct http_client *hc,
	const char *val)
{
	if (!strcasecmp(val, "chunked")) {
		hc->chunked_enc = 1;
	} else {
		HTTP2MQTT_CLIENT_LOGF(hc, LOG_WARN, "bad enc %s", val);
	}
}

static void http2mqtt_client_parse_key(struct http_client *hc, const char *val)
{
	size_t len;

	len = snprintf(hc->auth_hdr, hc->auth_hdr_size,
	    HTTP_CLIENT_AUTH_VER " %s", val);
	if (len >= hc->auth_hdr_size) {
		HTTP2MQTT_CLIENT_LOGF(hc, LOG_WARN, "auth hdr too long");
	}
}

/*
 * Parses the range header of the http response if given. If found,
 * sets range_given and range_bytes.
 */
static void http2mqtt_client_parse_range(struct http_client *hc,
	const char *val)
{
	unsigned long bytes;
	char *errptr;
	char range_argv[2][32];
	int len = strlen(val);
	int i, array, index;

	memset(range_argv, 0, sizeof(range_argv));
	for (i = 0, array = 0, index = 0; i < len; i++) {
		if (val[i] == ' ' || val[i] == '\t') {
			array++;
			index = 0;
			continue;
		}
		range_argv[array][index++] = val[i];
	}

	errptr = strchr(range_argv[1], '/');
	if (!errptr) {
		goto bad_range;
	}
	bytes = strtoul(errptr + 1, &errptr, 10);
	if (*errptr != '\0' || bytes >= MAX_U32) {
bad_range:
		HTTP2MQTT_CLIENT_LOGF(hc, LOG_WARN, "bad range %s",
		    range_argv[0]);
		return;
	}
	hc->range_bytes = bytes;
	hc->range_given = 1;
}

static void http2mqtt_client_parse_header(struct http_client *hc, int hcnt,
	const struct http_hdr *hdrs)
{
	int i;
	for (i = 0; i < hcnt; i++) {
		if (!strcasecmp(HTTP_CLIENT_KEY_FIELD, hdrs[i].name)) {
			http2mqtt_client_parse_key(hc, hdrs[i].val);
		} else if (!strcasecmp(HTTP_CLIENT_CT_LEN, hdrs[i].name)) {
			http2mqtt_client_parse_len(hc, hdrs[i].val);
		} else if (!strcasecmp(HTTP_CLIENT_CT_RANGE, hdrs[i].name)) {
			http2mqtt_client_parse_range(hc, hdrs[i].val);
		} else if (!strcasecmp(HTTP_CLIENT_CHUNK_ENCODING,
		    hdrs[i].name)) {
			http2mqtt_client_parse_encoding(hc, hdrs[i].val);
		}
	}
}

/*
 * Schedule reconnect/retry after wait.
 */
static void http2mqtt_client_wait(struct http_client *hc, u32 delay)
{
	client_timer_cancel(&hc->hmc_timer); /* in case of reset/commit */
	switch (hc->state) {
	case HCS_CONN:
	case HCS_CONN_TCP:
	case HCS_WAIT_RETRY:
	case HCS_OPEN:
	case HCS_KEEP_OPEN:
	case HCS_SEND:
	case HCS_HEAD:
	case HCS_CONTENT:
	case HCS_WAIT_TCP_SENT:
	case HCS_WAIT_CLI_RECV:
		break;
	default:
		HTTP2MQTT_CLIENT_LOGF(hc, LOG_ERR, "unexpected state %x",
		    hc->state);
		break;
	}
	if (delay) {
		client_timer_set(&hc->hmc_timer, delay);
	}
}

static void http2mqtt_client_abort_err(struct http_client *hc)
{
	ASSERT(client_locked);
	http2mqtt_client_abort(hc);
	if (hc->client_err_cb) {
		hc->client_err_cb(hc);
	}
}

/*
 * Abort req and go to idle
 */
static void http2mqtt_client_idle_close(struct http_client *hc)
{
	HTTP2MQTT_CLIENT_DEBUG(hc, LOG_DEBUG, "state %x, hc_error %d",
	    hc->state, hc->hc_error);
	client_timer_cancel(&hc->hmc_timer);
	if ((hc->hc_error == HC_ERR_TIMEOUT)
	    || (hc->hc_error == HC_ERR_SEND)) {
		mqttv1_client_timeout_abort();
	} else {
		mqttv1_client_abort();
	}

	if (hc->state != HCS_CONN && hc->state != HCS_WAIT_RETRY) {
		/* If we're retrying a connection, don't reset retries. */
		hc->retries = 0;
	}
	hc->state = HCS_IDLE;
	hc->req_outstanding = 0;
}

/*
 * Reset the retry and hand control back to the parent.
 */
static void http2mqtt_client_start_pending(struct http_client *hc)
{
	hc->hc_error = HC_ERR_NONE;

	ASSERT(client_locked);
	hc->retries = 0;
	hc->req_outstanding = 0;
	if (hc->client_next_step_cb) {
		hc->client_next_step_cb(hc);
	}
}

/*
 * Abort req, go to idle.
 */
void http2mqtt_client_abort(struct http_client *hc)
{
	http2mqtt_client_idle_close(hc);
	hc->retries = 0;
}

static void http2mqtt_client_timeout(struct timer *arg)
{
	struct http_client *hc =
	    CONTAINER_OF(struct http_client, hmc_timer, arg);

	HTTP2MQTT_CLIENT_LOGF(hc, LOG_WARN, "state %x", hc->state);
	hc->hc_error = HC_ERR_TIMEOUT;
	http2mqtt_client_abort_err(hc);
}

/*
 * Interface for the client to ask mqttv1_client to start receiving
 * server data again.
 */
void http2mqtt_client_continue_recv(struct http_client *hc)
{
	mqttv1_client_continue();
}

/*
 * Send callback for portion of buffer.
 * This part is never sent with chunked encoding.
 */
static int http2mqtt_client_req_send(struct http_client *hc,
				const void *buf, size_t *lenp)
{
	enum ada_err err;
	size_t len;

	len = *lenp;
	if (!len) {
		return 0;
	}
	err = http2mqtt_client_send(hc, buf, len);
	if (err == AE_BUF) {
		return -1;
	}
	*lenp = 0;
	if (err != AE_OK) {
		HTTP2MQTT_CLIENT_DEBUG(hc, LOG_ERR, "err = %s",
		    ada_err_string(err));
		hc->hc_error = HC_ERR_SEND;
		http2mqtt_client_abort_err(hc);
		return -1;
	}
	return 0;
}

/*
 * Call the client's send callback
 */
static void http2mqtt_client_issue_send_cb(struct http_client *hc)
{
	http2mqtt_client_wait(hc, HTTP_CLIENT_TRANS_WAIT);
	hc->hc_error = HC_ERR_SEND;

	if (http2mqtt_client_req_send(hc, hc->body_buf, &hc->body_buf_len)) {
		return;
	}

	if (hc->client_send_data_cb) {
		hc->client_send_data_cb(hc);
	} else {
		http2mqtt_client_send_complete(hc);
	}
}

enum ada_err http2mqtt_client_send(struct http_client *hc,
	const void *buf, u16 len)
{
	enum al_err err = AL_ERR_NOTCONN;

	if (hc->state == HCS_WAIT_TCP_SENT) {
		return AE_BUF;
	}
	ASSERT(hc->state == HCS_OPEN || hc->state == HCS_SEND);
	if (hc->state != HCS_OPEN && hc->state != HCS_SEND) {
		return AE_ERR;
	}
	client_timer_cancel(&hc->hmc_timer);
	hc->state = HCS_SEND;

	log_bytes(hc->mod_log_id, LOG_SEV_DEBUG2, buf, len, "http_tx");
	err = mqttv1_client_send_payload(buf, len);
	if (err != AL_ERR_OK) {
		if (err == AL_ERR_BUF) {
			hc->state = HCS_WAIT_TCP_SENT;
			hc->hc_error = HC_ERR_SEND;
			return AE_BUF;
		}
		HTTP2MQTT_CLIENT_DEBUG(hc, LOG_DEBUG2, "err = %s",
		    al_err_string(err));
		return ada_err_from_al_err(err);
	}
	hc->sent_len += len;
	return AE_OK;
}

/*
 * Send blank padding if needed for PUT or POST.
 * Indicates body is complete.
 * The buffer may be used for padding.
 * This implementation can send the shorter body without padding.
 * This can be set as a send_data_cb function.
 */
void http2mqtt_client_send_pad(struct http_client *hc)
{
	size_t padding_needed;
	enum ada_err err;
	char buf[512];
	size_t len = sizeof(buf);

	if (hc->sending_chunked || hc->sent_len < 0 ||
	    hc->http_tx_chunked) {
		return;
	}

	memset(buf, ' ', len);

	while (hc->sent_len < hc->body_len) {
		padding_needed = hc->body_len - hc->sent_len;
		if (padding_needed > len) {
			padding_needed = len;
		}
		err = http2mqtt_client_send(hc, buf, padding_needed);
		if (err != AE_OK) {
			return;
		}
	}
	http2mqtt_client_send_complete(hc);
}

/*
 * Interface for the client to complete the send request.
 * This should only be called from the send callback when it has completed the
 * final send.
 */
void http2mqtt_client_send_complete(struct http_client *hc)
{
	ASSERT(client_locked);
	ASSERT(hc->state == HCS_SEND);
	if (hc->state == HCS_SEND) {
		hc->state = HCS_HEAD;
		hc->hc_error = HC_ERR_RECV;
		http2mqtt_client_wait(hc, HTTP_CLIENT_RECV_WAIT);
	}
}

/*
 * Callback from mqttv1_client when connection fails or gets reset.
 */
static void http2mqtt_client_err(enum al_err err, void *arg)
{
	struct http_client *hc = arg;

	HTTP2MQTT_CLIENT_DEBUG(hc, LOG_DEBUG2, "err = %s",
	    al_err_string(err));

	switch (err) {
	case AL_ERR_TIMEOUT:
		hc->hc_error = HC_ERR_TIMEOUT;
		break;
	case AL_ERR_AUTH_FAIL:
		hc->hc_error = HC_ERR_HTTP_STATUS;
		hc->http_status = HTTP_STATUS_UNAUTH;
		break;
	case AL_ERR_CLSD:
	case AL_ERR_RST:
		if (hc->state == HCS_KEEP_OPEN ||
		    hc->state == HCS_IDLE) {
			http2mqtt_client_idle_close(hc);
		}
		hc->hc_error = HC_ERR_CONN_CLSD;
		break;
	default:
		hc->hc_error = HC_ERR_CONN_CLSD;
		break;
	}
	http2mqtt_client_abort_err(hc);
}

static void http2mqtt_client_conn_state_cb(
	enum mqttv1_conn_state st, void *arg)
{
	struct http_client *hc = arg;
	void (*close_cb)(struct http_client *);

	ASSERT(client_locked);
	hc->is_connected = (st == MCS_CONNECTED) ? 1 : 0;
	HTTP2MQTT_CLIENT_DEBUG(hc, LOG_DEBUG, "is_connected = %d",
	    hc->is_connected);

	if (!hc->is_connected && hc->wait_close) {
		hc->wait_close = 0;
		close_cb = hc->close_cb;
		if (close_cb) {
			hc->close_cb = NULL;
			close_cb(hc);
		}
	}

	if (hc->conn_state_cb) {
		hc->conn_state_cb((enum http2mqtt_conn_state)st);
	}

	if (hc->is_connected && hc->req_outstanding && hc->state == HCS_SEND) {
		http2mqtt_client_issue_send_cb(hc);
	}
}

static void http2mqtt_client_sent_cb(void *arg)
{
	struct http_client *hc = arg;

	if (hc->is_connected && hc->req_outstanding &&
	    (hc->state == HCS_SEND || hc->state == HCS_WAIT_TCP_SENT)) {
		hc->state = HCS_SEND;
		http2mqtt_client_issue_send_cb(hc);
	}
}

void http2mqtt_client_disconnect(struct http_client *hc)
{
	int clear_conn_flag = 1;

	http2mqtt_client_abort(hc);
	if (hc->is_connected) {
		hc->wait_close = 1;
	}
	if (hc->hc_error == HC_ERR_SEND) {
		clear_conn_flag = 0;
	}
	mqttv1_client_disconnect(clear_conn_flag);
}

static void http2mqtt_client_resp_start_cb(u32 http_status, int hcnt,
	const struct http_hdr *hdrs, void *arg)
{
	struct http_client *hc = arg;

	if (hc->state != HCS_HEAD) {
		return;
	}

	client_timer_cancel(&hc->hmc_timer);
	http2mqtt_client_parse_header(hc, hcnt, hdrs);

	/*
	 * HTTP parse is complete, check status and call client_recv_cb.
	 */
	hc->http_status = http_status;
	HTTP2MQTT_CLIENT_DEBUG(hc, LOG_DEBUG, "HTTP status = %lu",
	    http_status);

	switch (http_status) {
	case HTTP_STATUS_OK:
	case HTTP_STATUS_CREATED:
	case HTTP_STATUS_PAR_CONTENT:
	case HTTP_STATUS_NO_CONTENT:
		break;
	default:
		if (http_status >= HTTP_STATUS_REDIR_MIN &&
		    http_status <= HTTP_STATUS_REDIR_MAX) {
			hc->hc_error = HC_ERR_HTTP_REDIR;
		} else {
			hc->hc_error = HC_ERR_HTTP_STATUS;
		}
		goto error_close;
		break;
	}

	hc->state = HCS_CONTENT;
	http2mqtt_client_wait(hc, HTTP_CLIENT_TRANS_WAIT);
	return;

error_close:
	http2mqtt_client_abort_err(hc);
}

static size_t http2mqtt_client_resp_payload_cb(const void *part, size_t len,
	void *arg)
{
	struct http_client *hc = arg;
	enum ada_err err = AE_OK;

	if (hc->state != HCS_CONTENT) {
		return len;
	}
	client_timer_cancel(&hc->hmc_timer);

	if (part && len) {
		log_bytes(hc->mod_log_id, LOG_SEV_DEBUG2, part, len, "http_rx");
		err = hc->client_tcp_recv_cb(hc, (void *)part, len);
	}

	if (err == AE_BUF) {
		/*
		 * Client paused.
		 * Here hc->recv_consumed gives the amount of the buffer
		 * that was received. The rest should be repeated after
		 * the stream is continued.
		 */
		mqttv1_client_pause();
		return hc->recv_consumed;
	}

	if (err) {
		HTTP2MQTT_CLIENT_DEBUG(hc, LOG_DEBUG2, "err = %s",
		    ada_err_string(err));
	}

	if (err == AE_INVAL_VAL) {
		http2mqtt_client_abort_err(hc);
		return len;
	}

	if (part == NULL && len == 0) {
		hc->state = HCS_IDLE;
		hc->hc_error = HC_ERR_NONE;
		hc->client_tcp_recv_cb(hc, NULL, 0);
		http2mqtt_client_start_pending(hc);
	} else {
		http2mqtt_client_wait(hc, HTTP_CLIENT_TRANS_WAIT);
	}
	return len;
}

void http2mqtt_client_init(void)
{
	mqttv1_client_init();
}

#ifdef AYLA_FINAL_SUPPORT
void http2mqtt_client_final(void)
{
	mqttv1_client_final();
}
#endif

void http2mqtt_client_set_keepalive(u16 keepalive)
{
	mqttv1_client_set_keepalive(keepalive);
}

void http2mqtt_client_set_notify_cb(struct http_client *hc,
	void (*notify_cb)(void *arg))
{
	hc->notify_cb = notify_cb;
}

static void http2mqtt_client_notify_cb(void *arg)
{
	struct http_client *hc = arg;

	if (!hc->notify_cb) {
		return;
	}
	hc->notify_cb(hc);
}

void http2mqtt_client_set_conn_state_cb(struct http_client *hc,
	void (*conn_state_cb)(enum http2mqtt_conn_state st))
{
	hc->conn_state_cb = conn_state_cb;
}

static void http2mqtt_client_req_start(struct http_client *hc)
{
	enum al_err err;

	mqttv1_client_log_set((enum mod_log_id)((u8)MOD_LOG_MQTT |
	    ((u8)hc->mod_log_id & LOG_MOD_NOSEND)));
	mqttv1_client_set_broker((const char *)hc->host, hc->host_port);
	err = mqttv1_client_req_start(hc->method, hc->req_buf,
	    hc->hdr_cnt, hc->hdrs,
	    hc->body_len, http2mqtt_client_resp_start_cb,
	    http2mqtt_client_resp_payload_cb, http2mqtt_client_err);
	if (err) {
		HTTP2MQTT_CLIENT_DEBUG(hc, LOG_ERR, "err = %d (%s)",
		    err, al_err_string(err));
	}

	if (err != AL_ERR_OK) {
		hc->hc_error = HC_ERR_SEND;
		http2mqtt_client_abort_err(hc);
	} else if (!hc->is_connected) {
		http2mqtt_client_wait(hc, HTTP_CLIENT_CONN_WAIT);
	} else {
		http2mqtt_client_issue_send_cb(hc);
	}
}

void http2mqtt_client_req(struct http_client *hc,
    enum http_client_method method,
    const char *resource, int hcnt, const struct http_hdr *hdrs)
{
	ssize_t len;

	ASSERT(client_locked);
	if (!hc->hmc_initialized) {
		timer_handler_init(&hc->hmc_timer, http2mqtt_client_timeout);
		mqttv1_client_set_arg(hc);
		mqttv1_client_set_notify_cb(http2mqtt_client_notify_cb);
		mqttv1_client_set_conn_state_cb(
		    http2mqtt_client_conn_state_cb);
		mqttv1_client_set_sent_cb(http2mqtt_client_sent_cb);
		hc->hmc_initialized = 1;
	}

	if (hc->req_outstanding) {
		return;
	}

	hc->req_outstanding = 1;
	hc->content_given = 0;
	hc->content_len = 0;
	hc->recv_consumed = 0;
	hc->conn_close = 0;
	hc->chunked_enc = 0;
	hc->range_given = 0;
	hc->http_status = 0;
	hc->state = HCS_SEND;

	hc->method = method;
	len = snprintf(hc->req_buf, sizeof(hc->req_buf), "%s", resource);
	ASSERT(len < sizeof(hc->req_buf));
	hc->req_len = len;

	ASSERT(hcnt < ARRAY_LEN(hc->hdrs));
	memcpy(hc->hdrs, hdrs, hcnt * sizeof(*hdrs));
	hc->hdr_cnt = hcnt;

	if (hc->wait_close) {
		hc->close_cb = http2mqtt_client_req_start;
	} else {
		http2mqtt_client_req_start(hc);
	}
}

void http2mqtt_client_reset(struct http_client *hc, enum mod_log_id mod_id)
{
	ASSERT(client_locked);
	client_timer_cancel(&hc->hmc_timer);
	client_timer_cancel(&hc->hc_timer);
	al_net_addr_set_ipv4(&hc->host_addr, 0);
	hc->hc_error = HC_ERR_NONE;
	hc->req_outstanding = 0;
	hc->state = HCS_IDLE;
	hc->mod_log_id = mod_id;
	hc->sending_chunked = 0;
	hc->http_tx_chunked = 0;
	hc->chunked_eof = 0;
	hc->prop_callback = 0;
	hc->req_part = 0;
	hc->conn_wait = HTTP_CLIENT_CONN_WAIT;
	hc->retry_limit = HTTP_CLIENT_TRY_COUNT;
	hc->retry_wait = HTTP_CLIENT_RETRY_WAIT;
}
