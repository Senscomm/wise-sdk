/*
 * Copyright 2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef AYLA_SERVER_ALT_SUPPORT
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <al/al_httpd.h>

#include <ayla/assert.h>
#include <ayla/utypes.h>
#include <ada/err.h>
#include <ayla/endian.h>
#include <ayla/log.h>
#include <ayla/mod_log.h>
#include <ayla/uri_code.h>
#include <ayla/http.h>
#include <ayla/parse.h>
#include <al/al_os_mem.h>
#include <al/al_os_lock.h>
#include <al/al_clock.h>
#include <ada/server_req.h>
#include <ada/client.h>
#include <ada/ada_wifi.h>
#include "client_timer.h"

#define DEFAULT_RESPONSE_HEADER \
	"Transfer-Encoding: chunked\r\n" \
	"\r\n"

#define SERVER_ERR_HEADER \
	"Content-length: 0\r\n" \
	"\r\n"

static const char *server_method_names[] = {
	[REQ_BAD] = "BAD",
	[REQ_GET] = "GET",
	[REQ_GET_HEAD] = "HEAD",
	[REQ_POST] = "POST",
	[REQ_PUT] = "PUT",
	[REQ_DELETE] = "DELETE",
};

static void server_put_head(struct server_req *req, unsigned int status,
			const char *hdr_msg)
{
	struct al_httpd_conn *conn;

	server_log(LOG_DEBUG "response status %u", status);
	conn = (struct al_httpd_conn *)req->req_impl;
	AYLA_ASSERT(conn);
	if (al_httpd_response(conn, status, hdr_msg)) {
		goto out;
	}
	if (al_httpd_write(conn, DEFAULT_RESPONSE_HEADER,
	    sizeof(DEFAULT_RESPONSE_HEADER) - 1)) {
		goto out;
	}
	req->head_sent = 1;
	if (req->method == REQ_GET_HEAD) {
		req->suppress_out = 1;
	}
	return;
out:
	req->err = AE_CLSD;	/* should translate the error */
}

static int server_put_err(struct al_httpd_conn *conn, unsigned int status)
{
	server_log(LOG_DEBUG "response status %u", status);
	return al_httpd_response(conn, status, SERVER_ERR_HEADER);
}

static void server_req_free(struct server_req *req)
{
	if (req->static_alloc) {
		return;
	}
	al_os_mem_free(req);
}

/*
 * Write a portion of the response, not necessarily the final write.
 * If the msg is NULL, use req->buf.
 * The length of the message is in req->len.
 */
static void server_write(struct server_req *req, const char *msg)
{
	struct al_httpd_conn *conn;
	char buf[25];
	int len;

	AYLA_ASSERT(req->req_impl);
	conn = (struct al_httpd_conn *)req->req_impl;

	if (req->len == 0 || req->err != AE_OK || req->suppress_out) {
		return;
	}
	if (!msg) {
		msg = req->buf;
	}
	log_bytes(MOD_LOG_SERVER, LOG_SEV_DEBUG2,
	    msg, req->len, "tx");

	len = snprintf(buf, sizeof(buf), "%x\r\n", req->len);
	if (al_httpd_write(conn, buf, len)) {
		req->err = AE_ERR;
		goto out;
	}
	if (al_httpd_write(conn, msg, req->len)) {
		req->err = AE_ERR;
		goto out;
	}
	if (al_httpd_write(conn, "\r\n", 2)) {
		req->err = AE_ERR;
	}
out:
	req->len = 0;
}

/*
 * Indicate the request is complete.  Send the last chunk of the response.
 */
static enum ada_err server_complete(struct server_req *req)
{
	struct al_httpd_conn *conn = req->req_impl;
	const char end_chunk[] = "0\r\n\r\n";

	ASSERT(conn);
	if (req->head_sent &&
	    al_httpd_write(conn, end_chunk, sizeof(end_chunk) - 1)) {
		req->err = AE_ERR;
	}
	if (req->user_in_prog && req->tcpip_cb) {
		/*
		 * TODO: callback should be after response is completely sent
		 * and TCP ACKs received. This is not thought possible with
		 * the AL layer as it is or the ESP-IDF server.
		 */
		ada_callback_pend(req->tcpip_cb);
	}
	return AE_OK;
}

static int server_read(struct server_req *req)
{
	struct al_httpd_conn *conn;
	size_t len;
	int rc;

	conn = (struct al_httpd_conn *)req->req_impl;
	ASSERT(conn);
	len = req->content_len;
	if (len == 0) {
		return 0;
	}
	if (len > SERVER_BUFLEN - 1) {
		server_log(LOG_WARN "read: content len over %u", SERVER_BUFLEN);
		req->state = REQ_ERR;
		return -1;
	}
	if (len <= req->len) {
		server_log(LOG_WARN "read: short buf len %u min %u",
		    (unsigned int)len, (unsigned int)req->len);
		return 0;
	}
	len -= req->len;

	while (req->len < req->content_len) {
		rc = al_httpd_read(conn, req->buf + req->len, len);
		if (rc < 0) {
			server_log(LOG_WARN "read: rc %d", rc);
			req->state = REQ_ERR;
			return -1;
		}
		if (rc) {
			log_bytes(MOD_LOG_SERVER, LOG_SEV_DEBUG2,
			    req->buf + req->len, rc, "rx");
			req->len += rc;
			req->buf[req->len] = '\0';
		}
	}
	return 0;
}

static char *server_get_arg(struct server_req *req, const char *name,
    char *buf, size_t len)
{
	const char *result;
	struct al_httpd_conn *conn;

	conn = (struct al_httpd_conn *)req->req_impl;
	ASSERT(conn);
	result = al_httpd_get_url_arg(conn, name);
	if (result && (strlen(result) < len)) {
		strcpy(buf, result);
		return buf;
	}
	return NULL;
}

static int server_user_in_prog_end_cb(struct al_httpd_conn *conn, void *arg)
{
	struct server_req *req = arg;
	char buf[SERVER_BUFLEN];

	ASSERT(req);
	ASSERT(conn);
	ASSERT(conn == (struct al_httpd_conn *)req->req_impl);
	if (req->user_in_prog && req->resume) {
		req->buf = buf;
		req->user_in_prog = 0;
		req->resume(req);
		if (req->finish_write) {
			req->finish_write(req);
		}
		req->buf = NULL;
	}
	server_req_free(req);
	SERVER_LOGF(LOG_DEBUG2, "Request finished asynchronously.");
	return 0;
}

/*
 * Handle a received packet for a request.
 * This is called by the server after parsing the headers
 * and finding the handler.
 */
static void server_req(struct server_req *req)
{
	char buf[SERVER_BUFLEN];
	char *msg;
	int rc;

	if (req->state == REQ_DATA) {
		req->buf = buf;
		req->post_data = buf;
		ASSERT(req->url);
		rc = server_read(req);
		if (rc) {
			if (rc < 0) {
				msg = "short body";
				goto error;
			}
			goto out;
		}
		req->state = REQ_READY;
	}

	if (req->state == REQ_READY) {
		req->buf = buf;
		req->len = 0;
		req->err = AE_OK;
		req->finish_write = server_complete;
		if (req->host_present == 1 && req->host_match == 0) {
			server_get_not_found(req);
		} else {
			req->url->url_op(req);
		}
		if (!req->user_in_prog) {
			server_complete(req);
		}
	}
out:
	req->buf = NULL;	/* on-stack buffer must no longer be used */
	return;

error:
	server_log(LOG_WARN "malformed req: %s", msg);
	req->state = REQ_ERR;
}

enum server_method server_req_method(struct server_req *req)
{
	enum server_method m;
	const char *method;
	struct al_httpd_conn *conn;

	conn = (struct al_httpd_conn *)req->req_impl;
	ASSERT(conn);
	method = al_httpd_get_method(conn);
	if (!method) {
		return REQ_BAD;
	}
	for (m = (enum server_method)(REQ_BAD + 1);
	    m < ARRAY_LEN(server_method_names); m++) {
		if (!strcmp(method, server_method_names[m])) {
			return m;
		}
	}
	return REQ_BAD;
}

static int server_read_headers(struct server_req *req)
{
	struct al_httpd_conn *conn = req->req_impl;
	unsigned long len;
	char *endptr;
	const char *hdr;

	hdr = al_httpd_get_req_header(conn, "Host");
	if (hdr) {
		req->host_present = 1;
		req->host_match = server_host_match(hdr);
		if (!req->host_match) {
			server_log(LOG_WARN "host %s no match", hdr);
		}
	}

	req->content_len = 0;
	hdr = al_httpd_get_req_header(conn, "Content-Length");
	if (hdr) {
		len = strtoul(hdr, &endptr, 10);
		if (*endptr != '\0') {
			server_log(LOG_WARN "invalid content len %s", hdr);
			server_put_err(conn, HTTP_STATUS_BAD_REQ);
			return -1;
		}
		req->content_len = len;
	}
	return 0;
}

/*
 * Continuation of server_req.  The resume handler is to be called to serve
 * the rest of the request.
 * This may operate in the server thread.
 */
static void server_continue_req(struct server_req *req)
{
	struct al_httpd_conn *conn = req->req_impl;

	al_httpd_complete(conn, req);
}

/*
 * Handle incoming request.
 * Note: this could be in a server thread.
 */
static int (*server_handler(struct al_httpd_conn *conn))
	(struct al_httpd_conn *, void *)
{
	enum server_method method;
	struct server_req *req;
	const struct url_list *tt;
	const char *url;
	size_t len;
	u8 priv;
	int rc;

	ASSERT(conn);
	req = al_os_mem_calloc(sizeof(*req));
	if (!req) {
		SERVER_LOGF(LOG_WARN, "req alloc failed");
		server_put_err(conn, HTTP_STATUS_TOO_MANY);
		goto req_free;
	}
	req->req_impl = conn;
	req->put_head = server_put_head;
	req->write_cmd = server_write;
	req->continue_req = server_continue_req;
	req->get_arg_by_name = server_get_arg;
	method = server_req_method(req);
	if (method == REQ_BAD) {
		server_put_err(conn, HTTP_STATUS_BAD_REQ);
		goto req_free;
	}
#ifdef AYLA_WIFI_SUPPORT
	adap_wifi_stayup();
#endif
	url = al_httpd_get_resource(conn);
	len = url ? strlen(url) : 0;
	if (!url || len >= sizeof(req->resource)) {
		server_put_err(conn, HTTP_STATUS_BAD_REQ);
		goto req_free;
	}
	memcpy(req->resource, url, len + 1);

	server_log(LOG_DEBUG "request: %s %s",
	    server_method_names[method], req->resource);

	priv = LOC_REQ;
#ifdef AYLA_WIFI_SUPPORT
	if (adap_wifi_in_ap_mode()) {
		priv |= REQ_SOFT_AP;
	}
#endif
	tt = server_find_handler(req, req->resource, method, priv);
	if (!tt) {
		SERVER_LOGF(LOG_DEBUG, "no handler");
		goto req_free;
	}
	req->url = tt;
	req->state = REQ_DATA;
	req->head_sent = 0;
	if (server_read_headers(req)) {
		goto req_free;
	}
	server_req(req);

	/*
	 * If handler is not complete, it will set user_in_prog, and should
	 * be called back at the resume handler that it has declared.
	 */
	if (req->user_in_prog) {
		SERVER_LOGF(LOG_DEBUG, "User in progress");
		return server_user_in_prog_end_cb;
	}
	rc = req->err;
	if (rc) {
		if (!req->head_sent) {
			server_put_err(conn, HTTP_STATUS_INTERNAL_ERR);
		}
	}

req_free:
	server_req_free(req);
	SERVER_LOGF(LOG_DEBUG2, "Request finished synchronously.");
	return NULL;
}

void server_free_aborted_req(struct server_req *req)
{
	struct al_httpd_conn *conn;

	conn = (struct al_httpd_conn *)req->req_impl;
	ASSERT(conn);
	if (req->user_in_prog) {
		al_httpd_complete(conn, req);
	}
}

#ifdef AYLA_WIFI_SUPPORT
static int (*server_url_not_found_handler(struct al_httpd_conn *conn))
	(struct al_httpd_conn *, void *)
{
	SERVER_LOGF(LOG_DEBUG, "resource \"%s\"", al_httpd_get_resource(conn));
	if (adap_wifi_in_ap_mode()) {
		return server_handler(conn);
	}
	server_put_err(conn, HTTP_STATUS_NOT_FOUND);
	return NULL;
}
#endif

void server_reg_urls(const struct url_list *list)
{
	const struct url_list *item;
	enum al_http_method method;
	int rc;
#ifdef AYLA_WIFI_SUPPORT
	u8 req_flags = LOC_REQ | REQ_SOFT_AP;
#else
	u8 req_flags = LOC_REQ;
#endif

	for (item = list; item->url; item++) {
		if (!(item->req_flags & req_flags)) {
			continue;	/* not applicable to local server */
		}
		switch (item->method) {
		case REQ_GET:
			method = AL_HTTPD_METHOD_GET;
			break;
		case REQ_POST:
			method = AL_HTTPD_METHOD_POST;
			break;
		case REQ_PUT:
			method = AL_HTTPD_METHOD_PUT;
			break;
		case REQ_DELETE:
			method = AL_HTTPD_METHOD_DELETE;
			break;
		default:
			ASSERT_NOTREACHED();
		}
		rc = al_httpd_reg_url_cb(item->url, method, server_handler);
		if (rc) {
			server_log(LOG_ERR "reg_url %s method %u failed",
			    item->url, method);
		}
	}
#ifdef AYLA_WIFI_SUPPORT
	/* Register url not found handler */
	al_httpd_reg_url_cb("", AL_HTTPD_METHOD_GET,
	    server_url_not_found_handler);
#endif
}

void ada_server_up(void)
{
	if (al_httpd_start(HTTPD_PORT)) {
		server_log(LOG_ERR "http server start failed");
		return;
	}
}

void ada_server_down(void)
{
	al_httpd_stop();
}

/*
 * Obsolete redirection handler for Wi-Fi setup.
 * Keep this API for source-compatibility with older host apps.
 */
void server_enable_redir(void)
{
}
#endif /* AYLA_SERVER_ALT_SUPPORT */
