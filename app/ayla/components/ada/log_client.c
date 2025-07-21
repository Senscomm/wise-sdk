/*
 * Copyright 2011 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <ayla/assert.h>
#include <ayla/utypes.h>
#include <ada/err.h>
#include <ayla/log.h>
#include <ayla/mod_log.h>
#include <ayla/http.h>
#include <ayla/conf.h>
#include <ayla/clock.h>
#include <ayla/json.h>
#include <ayla/endian.h>
#include <ayla/json.h>
#include <ayla/wifi_status.h>
#include <ayla/timer.h>
#include <ayla/callback.h>
#include <jsmn.h>
#include <ayla/jsmn_get.h>
#include <ada/client.h>
#include <ada/prop.h>
#include <ada/server_req.h>
#include <ada/ada_wifi.h>
#include <ada/client_ota.h>
#include <ada/ada_conf.h>
#include "client_int.h"
#include "client_timer.h"
#include "client_lock.h"
#include "client_req.h"

#define LOG_CLIENT_START_DELAY		10000	/* 10 seconds */
#define LOG_CLIENT_LOOP_CNT_MAX		4
#define LOG_STATE_BUF_SZ		(LOG_LINE*2)
#define LOG_TRAILER_LEN			2	/* closing JSON chars ]} */

/*
 * Log post request parts
 */
enum log_client_req_part {
	LC_HDR = 0,
	LC_FIRST_MSG,
	LC_MSG,
	LC_TRAILER,
	LC_PADDING
};

static void log_client_wakeup(void);

static void log_client_log(const char *fmt, ...)
{
	ADA_VA_LIST args;

	ADA_VA_START(args, fmt);
	log_put_va(MOD_LOG_LOGGER | LOG_MOD_NOSEND, fmt, args);
	ADA_VA_END(args);
}

/*
 * Done reading all available logs from the buffer.
 */
static void log_client_idle(void)
{
	struct client_state *state = &client_state;

	state->logging.notified = 0;
	state->logging.push = 0;
	state->logging.lc_state = LCS_IDLE;
}

/*
 * Timer timeout
 */
static void log_client_timeout(struct timer *timer)
{
	struct client_state *state = &client_state;

	if (state->logging.lc_state == LCS_WAIT_DELAY) {
		state->logging.lc_state = LCS_SEND_START;
		log_client_wakeup();
	}
}

/*
 * Encode log message header
 */
static size_t log_client_enc_msg_hdr(char *buf, size_t len, const char *id)
{
	return snprintf(buf, len, "{\"dsn\":\"%s\", \"logs\":[", id);
}

/*
 * Encode one log message.
 */
static size_t log_client_enc_msg(struct log_msg_head *head, char *msg,
    char *out, size_t out_limit, u8 comma_needed)
{
	char line[LOG_LINE*2];
	char *line_ptr;
	int len;

	line_ptr = json_format_string(line, sizeof(line), msg, head->len, 1);
	if (!line_ptr) {
		snprintf(line, sizeof(line), "--- line encoding too long ---");
	}
	len = snprintf(out, out_limit,
	    "%s{\"time\":\"%lu.%3.3u\",\"mod\":\"%s\","
	    "\"level\":\"%s\", \"text\":\"%s\"}",
	    (comma_needed) ? "," : "", head->time, head->msec,
	    log_mod_get_name(head->mod_nr),
	    log_sev_get((enum log_sev)head->sev), line_ptr);
	if (len >= out_limit) {
		/* this shouldn't happen */
		log_client_log(LOG_WARN "log line too long: \"%s\"", out);
		len = 0;
	}
	return len;
}

/*
 * Encode log message trailer
 */
static size_t log_client_enc_msg_trailer(char *buf, size_t len)
{
	return snprintf(buf, len, "]}");
}

/*
 * Post logs to server.
 */
static void log_client_send_data_cb(struct http_client *hc)
{
	struct client_state *state = &client_state;
	struct log_buf_ctxt *ctxt = state->logging.ctxt;
	char log_info[LOG_ENTRY_MAX_SZ];
	struct log_msg_head *head = (struct log_msg_head *)log_info;
	char *msg = (char *)(head + 1);
	ssize_t len;
	enum ada_err err;
	u32 padding_needed;
	u32 prev_seq;

	ASSERT(ctxt);
	log_buf_send_event(LOG_EVENT_NOSEND);

#ifdef AYLA_WIFI_SUPPORT
	/*
	 * Add wifi connection history to logs if not already added.
	 */
	adap_wifi_show_hist(1);
#endif

next_part:
	switch (hc->req_part) {
	case LC_HDR:
		len = log_client_enc_msg_hdr(state->buf, sizeof(state->buf),
		    conf_sys_dev_id);
		err = client_req_send(hc, state->buf, len);
		if (!err) {
			hc->req_part = LC_FIRST_MSG;
			goto next_part;
		}
		break;
	case LC_FIRST_MSG:
	case LC_MSG:
		prev_seq = log_buf_seq_get(ctxt);
		len = log_buf_get_next(ctxt, log_info, sizeof(log_info));
		if (len <= 0) {
			log_client_idle();
			hc->req_part = LC_TRAILER;
			goto next_part;
		}
		len = log_client_enc_msg(head, msg, state->buf,
		    sizeof(state->buf), hc->req_part == LC_MSG);
		if (hc->sent_len + len > LOG_SIZE - LOG_TRAILER_LEN) {
			log_buf_seq_set(ctxt, prev_seq);
			if (state->logging.lc_state == LCS_SENDING_HIGH) {
				client_step_enable(ADCP_POST_LOGS_HIGH);
			} else {
				client_step_enable(ADCP_POST_LOGS_LOW);
			}
			hc->req_part = LC_TRAILER;
			goto next_part;
		}
		err = client_req_send(hc, state->buf, len);
		if (!err) {
			hc->req_part = LC_MSG;
			goto next_part;
		}
		log_buf_seq_set(ctxt, prev_seq);
		break;
	case LC_TRAILER:
		len = log_client_enc_msg_trailer(state->buf,
		    sizeof(state->buf));
		err = client_req_send(hc, state->buf, len);
		if (err != AE_BUF) {
			state->logging.loop_cnt++;
		}
		if (!err) {
			hc->req_part = LC_PADDING;
			goto next_part;
		}
		break;
	case LC_PADDING:
		if (hc->sent_len >= LOG_SIZE) {
			client_req_send_complete(hc);
			break;
		}
		padding_needed = LOG_SIZE - hc->sent_len;
		if (padding_needed > sizeof(state->buf)) {
			padding_needed = sizeof(state->buf);
		}
		memset(state->buf, ' ', padding_needed);
		err = client_req_send(hc, state->buf, padding_needed);
		if (!err) {
			goto next_part;
		}
		break;
	}
}

enum ada_err log_client_recv_cb(struct http_client *hc, void *buf, size_t len)
{
	struct client_state *state = &client_state;

	log_client_log(LOG_DEBUG "%s buf %p notified %d loop %u", __func__,
	    buf, state->logging.notified, state->logging.loop_cnt);
	if (buf) {
		log_buf_send_event(LOG_EVENT_NOSEND);
	} else {
		log_buf_new_event();
		if (!state->logging.notified) {
			/* no new log messages have occurred while posting */
			state->logging.loop_cnt = 0;
		}
	}
	return client_recv_drop(hc, buf, len);
}

static void log_client_callback(void *arg)
{
	struct client_state *state = &client_state;
	char *prio;

	log_client_log(LOG_DEBUG "%s state %d notified %d push %d",
	    __func__, state->logging.lc_state, state->logging.notified,
	    state->logging.push);

	switch (state->logging.lc_state) {
	case LCS_IDLE:
		if (state->logging.push) {
			break;
		} else if (state->logging.notified) {
			log_client_log(LOG_DEBUG "delaying start");
			state->logging.lc_state = LCS_WAIT_DELAY;
			client_timer_set(&state->logging.timer,
			    LOG_CLIENT_START_DELAY);
		}
		return;
	case LCS_WAIT_DELAY:
	case LCS_SENDING_LOW:
		if (!state->logging.push) {
			return;
		}
		break;
	case LCS_SEND_START:
		break;
	case LCS_SENDING_HIGH:
	default:
		return;
	}
	if (state->logging.push) {
		prio = "high";
		state->logging.lc_state = LCS_SENDING_HIGH;
		client_step_enable(ADCP_POST_LOGS_HIGH);
	} else {
		prio = "low";
		state->logging.lc_state = LCS_SENDING_LOW;
		client_step_enable(ADCP_POST_LOGS_LOW);
	}
	log_client_log(LOG_DEBUG "sending logs %s priority", prio);
	client_wakeup();
}

/*
 * Execute next step for state machine.
 */
static void log_client_wakeup(void)
{
	struct client_state *state = &client_state;

	client_callback_pend(&state->logging.callback);
}

/*
 * Notify the log client of pending log messages. The push flag indicates
 * log messages should be pushed to the cloud immediately.
 *
 * This can be called on any thread. Signal to the client thread using flags
 * and wake up the log client to operate the state machine.
 *
 * This function is called for every log message and structured to do as
 * little computation as possible on each call.
 */
static void log_client_notify(int push)
{
	struct client_state *state = &client_state;

	if (state->logging.lc_state == LCS_DISABLED) {
		return;
	}
	if (state->logging.push) {
		/* nothing more to signal */
		return;
	} else if (state->logging.notified) {
		if (!push) {
			/* not pushing, already notified */
			return;
		}
		/* transition from not pushing to pushing */
		state->logging.push = push;
	}
	state->logging.notified = 1;
	log_client_wakeup();
}

int log_client_enable(int enable)
{
	struct client_state *state = &client_state;
	int is_enabled = (state->logging.lc_state != LCS_DISABLED);

	log_client_log(LOG_DEBUG "%s %d", __func__, enable);

	if (!enable == !is_enabled) {
		return -1;
	}

	if (enable && log_client_conf_enabled) {
		state->logging.lc_state = LCS_IDLE;
	} else if (!enable) {
		state->logging.lc_state = LCS_DISABLED;
	} else {
		return -1;
	}
	return 0;
}

/*
 * Clear out any state left over from an earlier connection.
 */
void log_client_reset(void)
{
	struct client_state *state = &client_state;

	log_client_log(LOG_DEBUG "%s", __func__);

	client_timer_cancel(&state->logging.timer);
	log_client_enable(0);
}

/*
 * log client command.
 */
void ada_log_client_cli(int argc, char **argv)
{
	struct client_state *state = &client_state;

	if (argc == 1) {
		printcli("log-client current state: %s",
		    (state->logging.lc_state != LCS_DISABLED) ?
		    "enabled" : "disabled");
		printcli("log-client config: %s",
		    log_client_conf_enabled ? "enabled" : "disabled");
		goto usage;
	}
	if (!mfg_or_setup_mode_ok()) {
		return;
	}
	if (argc < 2) {
usage:
		printcli("usage: log-client <enable|disable>");
		return;
	}
	if (!strcmp(argv[1], "enable")) {
		log_client_conf_enabled = 1;
		log_client_enable(1);
	} else if (!strcmp(argv[1], "disable")) {
		log_client_conf_enabled = 0;
		log_client_enable(0);
	} else {
		goto usage;
	}
}

void log_client_init(void)
{
	struct client_state *state = &client_state;

	callback_init(&state->logging.callback, log_client_callback, NULL);
	timer_handler_init(&state->logging.timer, log_client_timeout);
	log_buf_notify_set(log_client_notify);
}

/*
 * Post logs to server.
 */
int log_client_post_logs(struct client_state *state)
{
	struct http_client *hc;
	struct log_buf_ctxt *ctxt = state->logging.ctxt;
	static int failed;

	log_client_log(LOG_DEBUG "%s state %d loop %d",
	    __func__, state->logging.lc_state, state->logging.loop_cnt);

	client_step_disable(ADCP_POST_LOGS_HIGH);
	client_step_disable(ADCP_POST_LOGS_LOW);

	if (state->logging.lc_state == LCS_DISABLED) {
		return -1;
	}

	if (!ctxt) {
		ctxt = log_buf_open(0);
		if (!ctxt) {
			if (!failed) {
				failed = 1;
				log_client_log(LOG_ERR "log_buf_open failed");
			}
			return -1;
		}
		log_buf_ctxt_will_send(ctxt);
		failed = 0;
		state->logging.ctxt = ctxt;
	}

	if (state->logging.loop_cnt >= LOG_CLIENT_LOOP_CNT_MAX) {
		/*
		 * Break infinite logging loops that can occur if the
		 * logging process causes more log messages. This shouldn't
		 * normally happen but protect against it to mitigate rare
		 * cases when it does happen.
		 *
		 * Skip over remaining messages in buffer.
		 */
		log_client_log(LOG_WARN "excessive logging detected");
		log_buf_seq_set_end(ctxt);
	}
	if (!log_buf_has_more(ctxt)) {
		/* no logs to report */
		log_client_idle();
		state->logging.loop_cnt = 0;
		return -1;
	}
	log_buf_new_event();
	log_buf_send_event(LOG_EVENT_NOSEND);
	hc = client_req_ads_new();
	ASSERT(hc);
	hc->client_tcp_recv_cb = log_client_recv_cb;
	hc->client_send_data_cb = log_client_send_data_cb;
	state->conn_state = CS_WAIT_LOGS_POST;
	state->request = CS_POST_LOGS;
	hc->body_buf = NULL;
	hc->body_buf_len = 0;
	hc->body_len = LOG_SIZE;
	hc->mod_log_id |= LOG_MOD_NOSEND; /* avoid endless self-logging */
	hc->req_part = LC_HDR;
	snprintf(state->buf, sizeof(state->buf), "/api/v1/device/logs?dsn=%s",
	    conf_sys_dev_id);
	client_req_start(hc, HTTP_REQ_POST, state->buf, &http_hdr_content_json);
	return 0;
}

