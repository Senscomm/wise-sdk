/*
 * Copyright 2023 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifdef AYLA_SERVER_ALT_SUPPORT
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <ayla/utypes.h>
#include <ayla/assert.h>
#include <ada/err.h>
#include <ayla/endian.h>
#include <ayla/log.h>
#include <ayla/mod_log.h>
#include <ayla/ayla_proto_mcu.h>
#include <ayla/callback.h>
#include <ayla/clock.h>
#include <ayla/uri_code.h>
#include <ayla/http.h>
#include <ayla/ipaddr_fmt.h>
#include <ayla/timer.h>
#include <al/al_net_addr.h>
#include <al/al_net_stream.h>
#include <al/al_os_mem.h>
#include <ada/server_req.h>
#include <ada/ada_wifi.h>
#include <ada/client.h>
#include "ada_mbuf.h"
#include "client_timer.h"
#include "client_lock.h"

#define SERVER_VERSION	"HTTP/1.1"

#define SERVER_NUM_TCP_CON	8	/* maximum number of open connections */
#define SERVER_REQ_MAX_AGE	50000	/* max age (ms) when req list full */
#define SERVER_RX_LEN_MAX	1024	/* max request body */

#define SERVER_DEFAULT_RESPONSE_HEADER \
	"Transfer-Encoding: chunked\r\n" \
	"\r\n"

#define SERVER_PRIV_LOG(_priv, _level, _format, ...) \
	server_log(_level "id %u %s: " _format, \
	    (_priv)->id, __func__, ##__VA_ARGS__)
#define SERVER_PRIV_LOG_BYTES(_priv, _sev, _buf, _len, _format, ...) \
	log_bytes(MOD_LOG_SERVER, _sev, _buf, _len, "id %u " _format, \
	    (_priv)->id, ##__VA_ARGS__)

/*
 * Server states as they are used here:
 *
 * REQ_INIT: receiving the first line with method and URL.
 *	req->line_len is valid.
 * REQ_HEAD: parsing headers.
 * REQ_DATA: receiving the body of the request (for PUT or POST).
 * REQ_READY: handling the request, sending output, maybe paused.
 * REQ_DONE: response completely written and being sent.
 *	Wait for output to complete then transition back to REQ_INIT or close.
 * REQ_ERR: an error has occurred and the request may be aborted or closed,
 *	ignoring further output calls.
 * REQ_CLOSING - stream will be closed once all output is sent.
 *
 * Asynchronous completion:
 * If a URL handler cannot provide the data immediately, e.g., it needs to
 * get a value from an external MCU, it sets req->err to AE_IN_PROGRESS and
 * sets req->user_in_prog to 1 and returns.  Later, when the data is available,
 * it calls server_continue() with the function to be called to deliver the
 * data.  The handler is responsible for timing out the request.
 *
 * Receive flow-control: Data after the headers is kept in mbufs.
 * This is not a problem as the ADA thread stack is fairly large for TLS
 * and other uses.
 * If additional data is received past the end of the current request,
 * the stream is paused and resumed after the request is handled, so the
 * receive handler will be re-called with the data for the next request.
 *
 * Send flow-control: if req->write_cmd (server_write()) cannot send the
 * data, it sets req->err to AE_BUF and will not write additional data.
 * When server_sent() is called indicating all pending data has been sent,
 * the resume call is made.
 * In the normal case, the handler is just called again when more data can
 * be sent, but the writes that have completed successfuly already are skipped.
 * The upper layer may set req->resume to a different function to call when send
 * can continue.
 *
 * Send data is always chunked.  The client is expected to be HTTP/1.1.
 * This is consistent with the al_httpd-based server.
 *
 * Resource limits: to deal with denial-of-service attacks, we limit how
 * many streams can be open at one time.  When a new stream is accepted,
 * an old idle connection may be closed or the new stream can be rejected.
 * We also limit how much data can be accepted, by acknowledging
 * receipt only for one request at a time, and limiting the content-length.
 *
 * If a request is open and waiting for data too long, it is closed.  This
 * time limit should be longer than the LAN mode timeout to allow the mobile app
 * to use a single connection to maintain a LAN mode session.
 */

/* Notes on request completion checks.
 *
 * The server_req is only freed once no more calls or references are possible.
 * The free always happens from a callback.
 *
 * The possible closes are:
 *
 * 1. request completes synchronously, with keep_open off.
 *	- server_req_handle calls server_complete and check_done
 * 2. request completes async.
 *	- handler resumes via server_continue or calls finish_write on error.
 *	- in resume callback, handler calls finish_write (server_complete) also.
 *	- after server_complete, if output pending, send callback handles it.
 * Either way the request completion should be after last sent callback.
 *	- in sent callback, if nothing more pending, issue callbacks including
 *	callback to free the request.
 * 3. the request times out or gets kicked off the list because other requests
 * arrive.
 *	- also call check_done
 * Keep-open may apply.
 * server_check_close() decides whether to close.
 * server_continue() cannot free.
 */

/*
 * Private per-stream data, handling a series of requests.
 */
struct server_req_priv {
	LIST_ENTRY(server_req_priv) list; /* active list of requests */
	struct server_req req;
	struct http_state http_state;
	struct al_net_stream *stream;
	size_t line_len;		/* valid bytes in req->resource[] */
	size_t content_len_in;
	struct ada_mbuf *rx_mbuf;	/* received post data payload */
	size_t sending_len;		/* tx bytes outstanding */
	u8 id;				/* index ID of request */
	u8 rx_paused;			/* stream receive is paused */
	u8 tx_unbuf;			/* send transmit data without mbuf */
	u8 status_logged:1;
	u16 tx_calls;			/* sends done since last resume */
	u16 tx_calls_ok;		/* sends done without error */
	u32 req_time;			/* time last request started */
	struct callback free_callback;	/* callback which free's request */
	struct callback resume_callback; /* callback to resume handling */
	struct timer idle_timer;	/* time limit on incomplete request */
};

#define SERVER_PRIV(reqp) CONTAINER_OF(struct server_req_priv, req, reqp)

static struct al_net_stream *server_listen_stream;
static LIST_HEAD(, server_req_priv) server_req_hd;
static u8 server_req_list_sz;

/*
 * Static functions.
 */
static void server_resume_cb(void *arg);
static void server_put_head(struct server_req *req, unsigned int status,
			const char *content_type);
static void server_write(struct server_req *req, const char *msg);
static enum ada_err server_complete(struct server_req *req);
static void server_continue_req(struct server_req *req);
static void server_idle_timeout(struct timer *);

static void server_parse_host(struct http_state *hs, int argc, char **argv);
static void server_parse_len(struct http_state *hs, int argc, char **argv);
static void server_parse_conn(struct http_state *hs, int argc, char **argv);

static const struct http_tag server_http_tags[] = {
	{"Host", server_parse_host},
	{"Connnection", server_parse_conn},
	{"Content-Length", server_parse_len},
	{NULL, NULL}
};

static const char *server_method_names[] = {
	[REQ_BAD] = "invalid",
	[REQ_GET] = "GET",
	[REQ_GET_HEAD] = "HEAD",
	[REQ_POST] = "POST",
	[REQ_PUT] = "PUT",
	[REQ_DELETE] = "DELETE",
};

static void server_mbufs_free(struct ada_mbuf **mbufp)
{
	ada_mbuf_free(*mbufp);
	*mbufp = NULL;
}

static void server_priv_free(struct server_req_priv *priv)
{
	ASSERT(client_locked);
	client_timer_cancel(&priv->idle_timer);
	SERVER_PRIV_LOG(priv, LOG_DEBUG2, "list %u", server_req_list_sz);
	ASSERT(server_req_list_sz);
	server_req_list_sz--;
	LIST_REMOVE(priv, list);
	server_mbufs_free(&priv->rx_mbuf);
	al_os_mem_free(priv);
}

static void server_priv_free_cb(void *arg)
{
	server_priv_free(arg);
}

/*
 * Reinitialize the request for an additional request.
 */
static void server_priv_init(struct server_req_priv *priv)
{
	struct server_req *req = &priv->req;

	server_req_init(req);
	req->state = REQ_INIT;
	req->put_head = server_put_head;
	req->write_cmd = server_write;
	req->finish_write = server_complete;
	req->continue_req = server_continue_req;

	priv->line_len = 0;
	priv->content_len_in = 0;
	priv->tx_unbuf = 0;
	priv->tx_calls = 0;
	priv->tx_calls_ok = 0;
	priv->status_logged = 0;
	ASSERT(!priv->sending_len);
	server_mbufs_free(&priv->rx_mbuf);

	if (priv->rx_paused) {
		priv->rx_paused = 0;
		if (priv->stream) {
			al_net_stream_continue_recv(priv->stream);
		}
	}
	client_lock();
	client_timer_set(&priv->idle_timer, SERVER_REQ_MAX_AGE);
	client_unlock();
}

static struct server_req_priv *server_priv_alloc(void)
{
	struct server_req_priv *priv;
	static u8 id;

	priv = al_os_mem_calloc(sizeof(*priv));
	if (!priv) {
		server_log(LOG_ERR "accept: failed to alloc req size %zd",
		    sizeof(*priv));
		return NULL;
	}
	id = (id + 1) % 100;	/* Limit to two digits.  Duplicates unlikely. */
	priv->id = id;
	LIST_INSERT_HEAD(&server_req_hd, priv, list);
	server_req_list_sz++;
	callback_init(&priv->free_callback, server_priv_free_cb, priv);
	callback_init(&priv->resume_callback, server_resume_cb, priv);
	timer_handler_init(&priv->idle_timer, server_idle_timeout);

	SERVER_PRIV_LOG(priv, LOG_DEBUG2, "size %u list %u",
	    sizeof(*priv), server_req_list_sz);

	server_priv_init(priv);
	return priv;
}

static void server_req_free(struct server_req *req)
{
	struct server_req_priv *priv = SERVER_PRIV(req);

	if (req->user_in_prog) {
		SERVER_PRIV_LOG(priv, LOG_DEBUG2, "in_prog wait");
		req->prop_abort = 1;
		return;
	}

	/*
	 * Use callback for free to occur after any other callbacks pended by
	 * server_req_done_callback().
	 */
	client_callback_pend(&priv->free_callback);
}

void server_free_aborted_req(struct server_req *req)
{
	req->user_in_prog = 0;
	server_req_free(req);
}

/*
 * Handle closing the request after all response data is sent.
 */
static void server_stream_close(struct server_req *req)
{
	struct server_req_priv *priv = SERVER_PRIV(req);
	struct al_net_stream *stream = priv->stream;
	enum al_err err;

	SERVER_PRIV_LOG(priv, LOG_DEBUG2, "close");
	if (stream) {
		err = al_net_stream_close(stream);
		priv->stream = NULL;
		if (err != AL_ERR_OK) {		/* close should never fail */
			SERVER_PRIV_LOG(priv, LOG_WARN, "err %u", err);
		}
	}
	server_req_free(req);
}

/*
 * Start closing the request and its stream.
 * If output is in progress, allow that to complete.
 * Handle callbacks for sent but cancel further receive callbacks.
 */
void server_close(struct server_req *req)
{
	struct server_req_priv *priv = SERVER_PRIV(req);
	struct al_net_stream *stream = priv->stream;

	SERVER_PRIV_LOG(priv, LOG_DEBUG2, "closing");
	req->state = REQ_CLOSING;
	req->keep_open = 0;
	if (stream) {
		al_net_stream_set_recv_cb(stream, NULL);
		al_net_stream_set_err_cb(stream, NULL);
	}
	server_mbufs_free(&priv->rx_mbuf);
	if (priv->sending_len) {
		return;
	}
	server_stream_close(req);
}

/*
 * Abort a request that is taking too long.
 */
static void server_abort(struct server_req *req)
{
	char stack_buf[SERVER_BUFLEN];

	if (req->prop_abort) {
		return;
	}
	switch (req->state) {
	case REQ_HEAD:
	case REQ_DATA:
		if (!req->head_sent) {
			req->buf = stack_buf;
			server_put_head(req, HTTP_STATUS_TOO_MANY, NULL);
			req->buf = NULL;
		}
		break;
	case REQ_INIT:
	case REQ_DONE:
	case REQ_ERR:
	case REQ_CLOSING:
	default:
		break;
	}
	req->prop_abort = 1;
	server_close(req);
}

/*
 * After running the URL handler, handle completion if appropriate.
 */
static void server_check_done(struct server_req *req)
{
	struct server_req_priv *priv = SERVER_PRIV(req);

	if (req->state != REQ_DONE || req->user_in_prog) {
		return;
	}
	if (priv->sending_len) {
		return;
	}
	server_req_done_callback(req);
	if (req->keep_open) {
		server_priv_init(priv);
	} else {
		server_close(req);
	}
}

static void server_resume(struct server_req *req)
{
	struct server_req_priv *priv = SERVER_PRIV(req);
	char stack_buf[SERVER_BUFLEN];

	priv->tx_calls = 0;
	req->err = AE_OK;
	req->user_in_prog = 0;
	req->buf = stack_buf;
	if (req->resume) {
		req->resume(req);
	}
	req->buf = NULL;
	req->len = 0;
	server_check_done(req);
}

static void server_resume_cb(void *arg)
{
	struct server_req_priv *priv = arg;
	struct server_req *req = &priv->req;

	server_resume(req);
}

static void server_continue_req(struct server_req *req)
{
	struct server_req_priv *priv = SERVER_PRIV(req);

	/* pend callback without using client_lock here or in the callback */
	ada_callback_pend(&priv->resume_callback);
}

/*
 * Callback from stream when data has been sent.
 * More data may be sent or the request may be complete.
 */
static void server_sent(void *arg, struct al_net_stream *stream, size_t len)
{
	struct server_req_priv *priv = arg;
	struct server_req *req = &priv->req;
	enum ada_err prev_err;

	if (len > priv->sending_len) {
		len = priv->sending_len;
	}
	priv->sending_len -= len;
	if (priv->sending_len) {
		return;
	}

	prev_err = req->err;
	req->err = AE_OK;
	req->post_data = NULL;

	if (req->state == REQ_CLOSING) {
		server_stream_close(req);
		return;
	}
	if (prev_err == AE_BUF) {
		server_resume(req);
	}
	server_check_done(req);
}

static void server_write_ll(struct server_req *req,
	const char *buf, size_t len)
{
	struct server_req_priv *priv = SERVER_PRIV(req);
	enum al_err err;

	if (req->err != AE_OK || !priv->stream) {
		return;
	}

	/*
	 * Skip write if previously done successfully.
	 * This is to handle send flow control where al_net_stream_write() has
	 * previously returned AL_ERR_BUF.  See notes at top.
	 */
	priv->tx_calls++;
	if (priv->tx_calls <= priv->tx_calls_ok) {
		return;
	}
	err = al_net_stream_write(priv->stream, buf, len);
	if (err != AL_ERR_OK) {
		req->err = ada_err_from_al_err(err);
		return;
	}
	priv->tx_calls_ok++;
	priv->sending_len += len;
}

static enum ada_err server_complete(struct server_req *req)
{
	struct server_req_priv *priv = SERVER_PRIV(req);
	const char end_chunk[] = "0\r\n\r\n";

	if (req->state == REQ_DONE) {
		return AE_OK;
	}
	if (req->head_sent) {
		server_write_ll(req, end_chunk, sizeof(end_chunk) - 1);
		if (req->err == AE_BUF) {
			return AE_OK;	/* will repeat via resume */
		}
	}
	req->state = REQ_DONE;
	req->post_data = NULL;
	if (req->err != AE_BUF && priv->stream) {
		al_net_stream_output(priv->stream);
	}
	return AE_OK;
}

/*
 * Write a portion of the response, using chunked encoding.
 * If the msg is NULL, use req->buf.
 * The length of the message is in req->len.
 */
static void server_write(struct server_req *req, const char *msg)
{
	struct server_req_priv *priv = SERVER_PRIV(req);
	char buf[25];
	size_t len;

	if (!req->len || req->err != AE_OK ||
	    req->suppress_out || req->prop_abort) {
		return;
	}
	if (!msg) {
		msg = req->buf;
	}
	SERVER_PRIV_LOG_BYTES(priv, LOG_SEV_DEBUG2, msg, req->len, "tx");

	len = snprintf(buf, sizeof(buf), "%x\r\n", req->len);
	ASSERT(len < sizeof(buf));
	server_write_ll(req, buf, len);
	server_write_ll(req, msg, req->len);
	server_write_ll(req, "\r\n", 2);
	req->len = 0;
}

/*
 * Call from server_add_urls() to register URLs, in case the server
 * filters requests.  This one doesn't.
 */
void server_reg_urls(const struct url_list *list)
{
}

/*
 * Parse request from full first line.
 */
static void server_parse_req(struct server_req_priv *priv)
{
	struct server_req *req = &priv->req;
	char *method = req->resource;
	char *res;
	char *cp;
	char *version;
	const char *msg = NULL;

	cp = strchr(method, ' ');
	if (!cp) {
		msg = "no resource";
		goto error;
	}
	*cp++ = '\0';
	res = cp;
	cp = strchr(res, ' ');
	if (!cp) {
		msg = "no version";
		goto error;
	}
	*cp++ = '\0';
	version = cp;
	if (!strcmp(version, "HTTP/1.1")) {
		req->keep_open = 1;
	} else if (!strcmp(version, "HTTP/1.0")) {
		req->keep_open = 0;
	} else {
		msg = "invalid version";
		goto error;
	}

	req->method = server_parse_method(method);
	memmove(req->resource, res, version - res);
	if (req->method == REQ_BAD) {
		msg = "invalid method";
		goto error;
	}

	req->state = REQ_HEAD;
	return;

error:
	SERVER_PRIV_LOG(priv, LOG_ERR, "%s", msg);
	req->method = REQ_BAD;	/* cause send of invalid req response */
	req->state = REQ_HEAD;
}

/*
 * Receive data for the request line, the first line of the request.
 * Will change state to REQ_HEAD if the header is completely parsed.
 * Will change state to REQ_ERR if the header is invalid.
 * Return the number of bytes used.
 */
static int server_recv_req_line(struct server_req_priv *priv,
		void *buf, size_t len)
{
	struct server_req *req = &priv->req;
	char *cp;
	u8 seen_cr;

	if (!priv->line_len) {
		priv->req_time = clock_ms();
		http_parse_req_init(&priv->http_state, server_http_tags);
	}

	seen_cr = priv->line_len && req->resource[priv->line_len - 1] == '\r';
	for (cp = buf; cp < (char *)buf + len; cp++) {
		if (seen_cr) {
			if (*cp == '\n') {
				priv->line_len--;	/* remove '\r' */
				req->resource[priv->line_len] = '\0';
				server_parse_req(priv);
				cp++;
				break;
			}
			return -1;			/* invalid input */
		}
		if (priv->line_len >= sizeof(req->resource)) {
			return -1;
		}
		req->resource[priv->line_len] = *cp;
		priv->line_len++;
		if (*cp == '\r') {
			seen_cr = 1;
		}
	}
	return cp - (char *)buf;
}

static void server_req_handle(struct server_req_priv *priv)
{
	char stack_buf[SERVER_BUFLEN];
	struct server_req *req = &priv->req;
	const struct url_list *url_entry;
	u8 privilege = LOC_REQ;

	server_log(LOG_DEBUG "id %u request: %s %s",
	    priv->id, server_method_names[req->method], req->resource);

#ifdef AYLA_WIFI_SUPPORT
	if (adap_wifi_in_ap_mode()) {
		/* Note: priv should depend on network interface, not AP mode */
		privilege |= REQ_SOFT_AP;
	}
#endif
	url_entry = server_find_handler(req, req->resource,
	    req->method, privilege);

	if (priv->rx_mbuf) {
		/* Add NUL-termination, length already reserved */
		ada_mbuf_chain_append_data(priv->rx_mbuf, "", 1);
		req->post_data = ada_mbuf_payload(priv->rx_mbuf);
	}

	req->buf = stack_buf;
	req->len = 0;
	req->err = AE_OK;
	req->url = url_entry;
	if (req->method == REQ_BAD) {
		req->put_head(req, HTTP_STATUS_BAD_REQ, NULL);
	} else if (!url_entry || (req->host_present && !req->host_match)) {
		server_get_not_found(req);
	} else {
		req->resume = url_entry->url_op;
		url_entry->url_op(req);
	}
	req->buf = NULL;
	if (!req->user_in_prog) {
		server_complete(req);
		return;
	}
	server_check_done(req);
}

/*
 * server HTTP receive.
 */
static enum al_err server_recv(void *arg, struct al_net_stream *stream,
		void *buf, size_t len_in)
{
	struct server_req_priv *priv = arg;
	struct server_req *req = &priv->req;
	struct ada_mbuf *mbuf;
	size_t len = len_in;
	size_t rlen;
	int rc;

	SERVER_PRIV_LOG_BYTES(priv, LOG_SEV_DEBUG2, buf, len, "rx");
#ifdef AYLA_WIFI_SUPPORT
	adap_wifi_stayup();
#endif

	switch (req->state) {
	case REQ_INIT:
		rc = server_recv_req_line(priv, buf, len);
		if (rc < 0) {
			goto close;
		}
		ASSERT(rc <= len);
		al_net_stream_recved(stream, rc);
		len -= rc;
		buf += rc;
		if (req->state == REQ_ERR) {
			server_put_head(req, HTTP_STATUS_BAD_REQ, NULL);
			goto close;
		}
		if (req->state != REQ_HEAD) {
			break;
		}
		FALL_THROUGH;
		/* fall-through */
	case REQ_HEAD:
		rc = http_parse(&priv->http_state, buf, len);
		if (rc < 0) {
			server_put_head(req, HTTP_STATUS_BAD_REQ, NULL);
			goto close;
		}
		ASSERT(rc <= len);
		al_net_stream_recved(stream, rc);
		buf += rc;
		len -= rc;
		if (priv->http_state.state != HS_DONE) {
			break;
		}
		req->state = REQ_DATA;
		if (!priv->content_len_in) {
			goto ready;
		}
		if (!len) {
			break;
		}
		FALL_THROUGH;
		/* fall-through */
	case REQ_DATA:
		if (priv->content_len_in > SERVER_RX_LEN_MAX) {
			server_put_head(req, HTTP_STATUS_REQ_LARGE, NULL);
			goto close;
		}
		rlen = 0;
		if (priv->rx_mbuf) {
			rlen = ada_mbuf_tot_len(priv->rx_mbuf);
		}
		if (len > priv->content_len_in - rlen) {
			len = priv->content_len_in - rlen;
			priv->rx_paused = 1;
		}
		rlen += len;
		al_net_stream_recved(stream, len);

		/* keep room for NUL termination */
		mbuf = ada_mbuf_alloc(len + 1);
		if (!mbuf) {
			goto error;
		}
		ada_mbuf_trim(mbuf, len);
		memcpy(ada_mbuf_payload(mbuf), buf, len);

		/* coalesce the mbuf with the previous mbuf, if any */
		if (priv->rx_mbuf) {
			ada_mbuf_cat(priv->rx_mbuf, mbuf);
			mbuf = ada_mbuf_coalesce(priv->rx_mbuf);
			if (!mbuf) {
				SERVER_PRIV_LOG(priv, LOG_WARN,
				    "coalesce failed");
				ada_mbuf_free(priv->rx_mbuf);
				priv->rx_mbuf = NULL;
				goto error;
			}
		}
		priv->rx_mbuf = mbuf;
		if (rlen >= priv->content_len_in) {
			goto ready;
		}
		break;
	case REQ_READY:
ready:
		client_lock();
		client_timer_cancel(&priv->idle_timer);
		client_unlock();
		priv->req_time = clock_ms();
		req->state = REQ_READY;
		server_req_handle(priv);	/* may free the request */
		break;
	case REQ_DONE:
	case REQ_CLOSING:
	default:
		break;
	case REQ_ERR:
		goto close;
	}
	return AL_ERR_OK;

error:
	server_put_head(req, HTTP_STATUS_INTERNAL_ERR, NULL);
close:
	req->keep_open = 0;
	server_close(req);
	return AL_ERR_OK;
}

/*
 * server HTTP err.
 */
static void server_err(void *arg, enum al_err err)
{
	struct server_req_priv *priv = arg;
	struct server_req *req = &priv->req;

	ASSERT(priv);
	SERVER_PRIV_LOG(priv, LOG_DEBUG, "err %u: %s", err, al_err_string(err));

	if (!priv->stream) {
		return;
	}
	priv->sending_len = 0;		/* force immediate close */
	server_close(req);
}

static void server_put_head(struct server_req *req, unsigned int status,
			const char *headers)
{
	struct server_req_priv *priv = SERVER_PRIV(req);
	size_t len;
	const char *status_msg = server_status_msg(status);

	len = snprintf(req->buf, SERVER_BUFLEN,
	    SERVER_VERSION " %u %s\r\n%s" SERVER_DEFAULT_RESPONSE_HEADER,
	    status, status_msg, headers ? headers : "");
	ASSERT(len < SERVER_BUFLEN);

	if (!priv->status_logged) {
		priv->status_logged = 1;
		server_log(LOG_DEBUG "id %u status: %u %s",
		    priv->id, status, status_msg);
		SERVER_PRIV_LOG_BYTES(priv, LOG_SEV_DEBUG2,
		    req->buf, len, "tx");
	}

	server_write_ll(req, req->buf, len);
	if (req->err == AE_OK) {
		req->head_sent = 1;
		if (req->method == REQ_GET_HEAD) {
			req->suppress_out = 1;
		}
	}
}

static void server_idle_timeout(struct timer *timer)
{
	struct server_req_priv *priv =
	    CONTAINER_OF(struct server_req_priv, idle_timer, timer);
	struct server_req *req = &priv->req;

	SERVER_PRIV_LOG(priv, LOG_DEBUG, "timeout");
	client_unlock();
	server_abort(req);
	client_lock();
}

/*
 * Defend against denial-of-service attacks by limiting the number of
 * requests outstanding.
 * If more than the limit, close the oldest one, preferably one that is idle.
 */
static void server_queue_check(void)
{
	struct server_req *req;
	struct server_req_priv *priv;
	struct server_req_priv *old_priv = NULL;
	struct server_req_priv *idle_priv = NULL;
	struct server_req_priv *tail_priv = NULL;

	if (LIST_EMPTY(&server_req_hd)) {
		return;
	}
	if (server_req_list_sz < SERVER_NUM_TCP_CON - 1) {
		return;
	}

	LIST_FOREACH(priv, &server_req_hd, list) {
		req = &priv->req;
		if (req->prop_abort) {
			continue;
		}
		if (!old_priv || clock_gt(old_priv->req_time, priv->req_time)) {
			old_priv = priv;
		}
		if (req->state == REQ_INIT && (!idle_priv ||
		     clock_gt(idle_priv->req_time, priv->req_time))) {
			idle_priv = priv;
		}
		tail_priv = priv;
	}
	priv = idle_priv;
	if (!priv) {
		priv = old_priv;
	}
	if (!priv) {
		priv = tail_priv;
	}
	if (!priv) {
		return;	/* can't happen */
	}
	req = &priv->req;
	SERVER_PRIV_LOG(priv, LOG_WARN, "aborting");
	server_abort(req);
}

static void server_accept(void *arg, struct al_net_stream *stream,
		const struct al_net_addr *peer_addr, u16 peer_port)
{
	struct server_req_priv *priv;
	char buf[20];

	server_queue_check();
	priv = server_priv_alloc();
	if (!priv) {
		al_net_stream_close(stream);
		return;
	}
	priv->stream = stream;

	al_net_stream_set_arg(stream, priv);
	al_net_stream_set_recv_cb(stream, server_recv);
	al_net_stream_set_err_cb(stream, server_err);
	al_net_stream_set_sent_cb(stream, server_sent);
	client_lock();
	client_timer_set(&priv->idle_timer, SERVER_REQ_MAX_AGE);
	client_unlock();

	SERVER_PRIV_LOG(priv, LOG_DEBUG, "from %s:%u",
	    ipaddr_fmt_ipv4_to_str(al_net_addr_get_ipv4(peer_addr),
	    buf, sizeof(buf)),
	    peer_port);
}

void ada_server_up(void)
{
	struct al_net_stream *stream;
	enum al_err err;

	if (server_listen_stream) {
		return;
	}
	stream = al_net_stream_new(AL_NET_STREAM_TCP);
	if (!stream) {
		server_log(LOG_ERR "%s: cannot allocate stream", __func__);
		return;
	}

	err = al_net_stream_listen(stream, NULL, HTTPD_PORT,
	    SERVER_NUM_TCP_CON, server_accept);
	if (err != AL_ERR_OK) {
		server_log(LOG_ERR "%s: listen err %d", __func__, err);
		al_net_stream_close(stream);
		return;
	}
	server_listen_stream = stream;
}

static void server_close_all(void)
{
	struct server_req *req;
	struct server_req_priv *priv;

	LIST_FOREACH(priv, &server_req_hd, list) {
		req = &priv->req;
		server_abort(req);
	}
}

void ada_server_down(void)
{
	struct al_net_stream *stream = server_listen_stream;

	if (!stream) {
		return;
	}
	server_listen_stream = NULL;
	al_net_stream_close(stream);
	server_close_all();
}

static void server_parse_host(struct http_state *hs, int argc, char **argv)
{
	struct server_req_priv *priv =
	    CONTAINER_OF(struct server_req_priv, http_state, hs);
	struct server_req *req = &priv->req;

	req->host_present = 1;
	if (argc != 1) {
		return;
	}
	req->host_match = server_host_match(argv[0]);
}

static void server_parse_len(struct http_state *hs, int argc, char **argv)
{
	struct server_req_priv *priv =
	    CONTAINER_OF(struct server_req_priv, http_state, hs);
	char *errptr;
	unsigned long len;

	if (argc != 1) {
		return;
	}
	len = strtoul(argv[0], &errptr, 10);
	if (errptr == argv[0] || *errptr || len > MAX_U32) {
		return;
	}
	priv->content_len_in = (size_t)len;
}

static void server_parse_conn(struct http_state *hs, int argc, char **argv)
{
	struct server_req_priv *priv =
	    CONTAINER_OF(struct server_req_priv, http_state, hs);
	struct server_req *req = &priv->req;

	if (argc != 1) {
		return;
	}
	if (!strcasecmp(argv[0], "close")) {
		req->keep_open = 0;
	} else if (!strcasecmp(argv[0], "keep-alive")) {
		req->keep_open = 1;
	}
}

/*
 * Obsolete redirection handler for Wi-Fi setup.
 * Keep this API for source-compatibility with older host apps.
 */
void server_enable_redir(void)
{
}
#endif /* SERVER_ALT_SUPPORT */
