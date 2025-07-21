/*
 * Copyright 2011-2015 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <sys/queue.h>

#include <ayla/utypes.h>
#include <ayla/endian.h>
#include <ayla/assert.h>
#include <ayla/log.h>
#include <ayla/mod_log.h>
#include <ayla/ayla_proto_mcu.h>
#include <ayla/clock.h>
#include <ayla/conf.h>
#include <ayla/uri_code.h>
#include <ayla/http.h>
#include <ayla/cmd.h>
#include <ayla/parse.h>
#include <ayla/wifi_status.h>
#include <ayla/nameval.h>
#include <ayla/callback.h>
#include <ayla/ipaddr_fmt.h>

#include <ada/err.h>
#include <ada/ada_conf.h>
#include <ada/prop.h>
#include <ada/server_req.h>
#include <ada/ada_wifi.h>
#include <ada/client.h>
#include <al/al_os_mem.h>
#include <al/al_net_if.h>
#include "client_timer.h"

const char server_content_json[] = "Content-Type: text/json\r\n";
const char server_content_html[] = "Content-Type: text/html\r\n";

static const struct name_val server_status[] = {
	{ "OK",				HTTP_STATUS_OK },
	{ "Accepted",			HTTP_STATUS_ACCEPTED },
	{ "No Content",			HTTP_STATUS_NO_CONTENT },
	{ "Bad Request",		HTTP_STATUS_BAD_REQ },
	{ "Found",			HTTP_STATUS_FOUND },
	{ "Forbidden",			HTTP_STATUS_FORBID },
	{ "Not Found",			HTTP_STATUS_NOT_FOUND },
	{ "Not Acceptable",		HTTP_STATUS_NOT_ACCEPT },
	{ "Conflict",			HTTP_STATUS_CONFLICT },
	{ "Precondition failed",	HTTP_STATUS_PRECOND_FAIL },
	{ "Too Many Requests",		HTTP_STATUS_TOO_MANY },
	{ "Internal Server Error",	HTTP_STATUS_INTERNAL_ERR },
	{ "Service Unavailable",	HTTP_STATUS_SERV_UNAV },
	{ NULL, 0 }
};

const char *server_status_msg(unsigned int status)
{
	const char *msg;

	msg = lookup_by_val(server_status, status);
	return msg ? msg : "";
}

void server_log(const char *fmt, ...)
{
	ADA_VA_LIST args;

	ADA_VA_START(args, fmt);
	log_put_va(MOD_LOG_SERVER, fmt, args);
	ADA_VA_END(args);
}

static const char server_not_found_body[] =
	"<!doctype html>\n"
	"<html><head><title>404 - Page not found</title></head><body>\n"
	"<h1>Page not found.</h1>\n"
	"<p><a href=\"/\">Return to home page</a></p>\n"
	"</body>\n</html>\n";

int (*server_not_found_hook)(struct server_req *req);

void server_get_not_found(struct server_req *req)
{
	if (server_not_found_hook && !server_not_found_hook(req)) {
		return;
	}
	req->put_head(req, HTTP_STATUS_NOT_FOUND, server_content_html);
	if (req->admin) {
		return;		/* no body for reverse-REST */
	}
	server_put_pure_len(req,
	    server_not_found_body, sizeof(server_not_found_body) - 1);
}

/*
 * Put formatted text and send if it will not exceed the remaining
 * response length.
 *
 * Returns -1 if the response would be truncated.
 * Returns -1 on failure, otherwise returns the remaining length.
 */
int server_put(struct server_req *req, const char *fmt, ...)
{
	va_list args;
	int len;
	int rlen = MAX_S32;

	va_start(args, fmt);
	len = vsnprintf(req->buf, SERVER_BUFLEN, fmt, args);
	va_end(args);
	if (len >= SERVER_BUFLEN) {
		server_log(LOG_ERR "dropped part of response for %s: "
		    "len %d truncated in buffer", req->resource, len);
		return -1;
	}
	if (req->resp_len_limited) {
		if (len > req->resp_len_rem) {
			server_log(LOG_WARN "dropped part of response for %s: "
			    "response len exceeded.", req->resource);
			return -1;
		}
		req->resp_len_rem -= len;
		rlen = (int)req->resp_len_rem;
	}
	req->len = len;
	server_put_flush(req, NULL);
	return rlen;
}

/*
 * Output a pure message, one that can be zero-copied and won't change.
 */
void server_put_pure_len(struct server_req *req, const char *msg, size_t len)
{
	req->len = len;
	server_put_flush(req, msg);
}

/*
 * Output a pure message, one that can be zero-copied and won't change.
 */
void server_put_pure(struct server_req *req, const char *msg)
{
	server_put_pure_len(req, msg, strlen(msg));
}

/*
 * Send a NUL-terminated buffer as the next part of the response to the request.
 */
void server_put_flush(struct server_req *req, const char *msg)
{
	if (req->suppress_out) {
		return;
	}
	if (!msg) {
		msg = req->buf;
	}
	req->write_cmd(req, msg);
}

/*
 * Write JSON HTTP header.
 */
void server_json_header(struct server_req *req)
{
	if (req->http_status != HTTP_STATUS_BAD_REQ) {
		req->put_head(req, HTTP_STATUS_OK, server_content_json);
	} else {
		server_put_status(req, HTTP_STATUS_BAD_REQ);
	}
}

/*
 * Get an allocated copy of named arg from server request. Caller must free
 * returned buffer.
 */
char *server_get_dup_arg_by_name(struct server_req *req, const char *name)
{
	return server_get_arg_by_name(req, name, NULL, 0);
}

/*
 * Get value of named arg from server request to the supplied buffer.
 */
char *server_get_arg_by_name(struct server_req *req, const char *name,
				char *buf, size_t len)
{
	const char *arg;
	const char *val;
	const char *next;
	const char *endp;
	size_t name_len = strlen(name);
	size_t vlen;
	ssize_t rc;

	if (req->get_arg_by_name) {
		return req->get_arg_by_name(req, name, buf, len);
	}

	arg = strchr(req->resource, '?');
	if (!arg) {
		return NULL;
	}
	arg++;
	for (endp = arg + strlen(arg); arg < endp; arg = next) {
		val = strchr(arg, '=');
		if (!val) {
			break;
		}
		val++;
		next = strchr(arg, '&');
		if (next) {
			vlen = next - val;
			next++;
		} else {
			vlen = endp - val;
			next = endp;
		}
		if (val >= next || arg + name_len + 1 != val) {
			continue;
		}
		if (strncmp(arg, name, name_len)) {
			continue;
		}
		if (buf == NULL) {
			buf = (char *)al_os_mem_alloc(vlen + 1);
			if (buf == NULL) {
				break;
			}
			len = vlen + 1;
		}
		rc = uri_decode_n(buf, len, val, vlen);
		if (rc < 0 || rc >= len) {
			break;
		}
		return buf;
	}
	return NULL;
}

/*
 * Get long argument with default of 0.
 * Returns non-zero on error.  Fills in *valp with value, or 0 on error.
 */
int server_get_long_arg_by_name(struct server_req *req, const char *name,
		 long *valp)
{
	char buf[20];
	char *cp;
	char *errptr;
	long val;

	*valp = 0;
	cp = server_get_arg_by_name(req, name, buf, sizeof(buf));
	if (!cp) {
		return -1;
	}
	val = strtoul(cp, &errptr, 0);
	if (*errptr != '\0') {
		return -1;
	}
	*valp = val;
	return 0;
}

/*
 * Get boolean argument with default of 0.
 * Ignores errors.  Missing or improper URL query strings are ignored.
 */
u8 server_get_bool_arg_by_name(struct server_req *req, const char *name)
{
	long val;

	server_get_long_arg_by_name(req, name, &val);
	return val != 0;
}

static const struct url_list server_url_not_found = {
	.url_op = server_get_not_found
};

static struct url_list const *server_url_groups[SERVER_URL_GROUPS];

static const struct name_val server_methods[] = {
	{ "GET", REQ_GET },
	{ "POST", REQ_POST },
	{ "PUT", REQ_PUT },
	{ "DELETE", REQ_DELETE },
	{ "HEAD", REQ_GET_HEAD },
	{ NULL, REQ_BAD }
};

void server_add_urls(const struct url_list *urls)
{
	int i;

	for (i = 0; i < SERVER_URL_GROUPS - 1; i++) {
		if (server_url_groups[i] == urls) {
			return;
		}
		if (!server_url_groups[i]) {
			server_url_groups[i] = urls;
#ifdef AYLA_LOCAL_SERVER
			server_reg_urls(urls);
#endif
			return;
		}
	}
	server_log(LOG_ERR "add_urls failed");
}

enum server_method server_parse_method(const char *method)
{
	return (enum server_method)lookup_by_name(server_methods, method);
}

/*
 * Find the URL list entry for the given URL based on the URL and method.
 * Also sets the method enum and resource string.
 * Returns the default not-found handler if the URL is not matched.
 */
const struct url_list *server_find_handler(struct server_req *req,
			const char *url, enum server_method method, u8 priv)
{
	const struct url_list *tt;
	const char *url_end;
	int i;
	size_t len;

	req->method = method;
	if (method == REQ_GET_HEAD) {
		method = REQ_GET;	/* find the GET handler */
	}
	if (url != req->resource) {
		snprintf(req->resource, sizeof(req->resource), "%s", url);
	}

	url_end = strchr(url, '?');
	if (url_end) {
		len = url_end - url;
	} else {
		len = strlen(url);
	}

	for (i = 0; i < SERVER_URL_GROUPS; i++) {
		tt = server_url_groups[i];
		if (!tt) {
			break;
		}
		for (; tt->url; tt++) {
			if (method == tt->method &&
			    tt->url &&
			    strlen(tt->url) == len &&
			    !strncmp(url, tt->url, len) &&
			    (tt->req_flags & priv)) {
				return tt;
			}
		}
	}
	return &server_url_not_found;
}

void server_put_status(struct server_req *req, unsigned int status)
{
	req->put_head(req, status, NULL);
}

void server_req_init(struct server_req *req)
{
	memset(req, 0, sizeof(*req));
	req->static_alloc = 1;
}

void server_req_done_callback(struct server_req *req)
{
	struct callback *tcpip_cb;
	void (*close_cb)(struct server_req *);

	tcpip_cb = req->tcpip_cb;
	req->tcpip_cb = NULL;
	close_cb = req->close_cb;
	req->close_cb = NULL;

	ada_callback_pend(tcpip_cb);
	if (close_cb) {
		close_cb(req);
	}
}

/*
 * Determine whether host from incoming request matches our IP address.
 * Returns 1 on a match, 0 otherwise.
 */
int server_host_match(const char *host)
{
	struct al_net_if *nif;
	enum al_net_if_type t;
	u32 addr;
	u32 addr1;

	addr1 = ipaddr_fmt_str_to_ipv4(host);
	if (!addr1) {
		return 0;
	}
	for (t = AL_NET_IF_DEF; t < AL_NET_IF_MAX; t++) {
		nif = al_net_if_get(t);
		if (!nif) {
			continue;
		}
		addr = al_net_if_get_ipv4(nif);
		if (!addr) {
			continue;
		}
		if (addr == addr1) {
			return 1;
		}
	}
	return 0;
}

/*
 * Continuation of server_req.
 * The resume handler is to be called to serve the rest of the request.
 * This may operate in the server thread.
 */
void server_continue(struct server_req *req,
	void (*resume)(struct server_req *))
{
	req->resume = resume;
	if (req->user_in_prog && req->continue_req) {
		req->continue_req(req);
		return;
	}
	resume(req);
}
