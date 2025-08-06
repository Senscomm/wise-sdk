/*
 * Copyright 2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <sys/queue.h>
#include <ayla/log.h>
#include <ayla/mod_log.h>
#include <ayla/uri_code.h>
#include <ayla/http.h>
#include <ayla/nameval.h>
#include <al/al_httpd.h>
#include <al/al_os_mem.h>
#include <al/al_err.h>
#include <scm_http_server.h>
#include <cmsis_os.h>

/*
 * HTTPD - server APIs.
 */
#define PFM_HTTPD_STACK_SIZE	4700	/* stack size for daemon */
#define PFM_HTTPD_URIS		40	/* max unique URI / method sets */
#define PFM_HTTPD_HEADERS	8	/* max headers on requests */
#define PFM_HTTPD_SOCKETS	10	/* max open sockets */

#if CONFIG_VFS_MAX_FDS < PFM_HTTPD_SOCKETS + 3
#error wise httpd requires CONFIG_VFS_MAX_FDS >= PFM_HTTPD_SOCKETS + 3
#error "Increase CONFIG_VFS_MAX_FDS by menuconfig."
#endif

#define PFM_HTTPD_ARGS	5	/* max arg strings in query */

#define PFM_HTTPD_LOG(sev, fmt, ...) \
	log_put_mod(MOD_LOG_SERVER, sev "%s: " fmt, __func__, ##__VA_ARGS__)

static STAILQ_HEAD(pfm_httpd_uri_head, pfm_httpd_uri) pfm_httpd_uri_head =
	STAILQ_HEAD_INITIALIZER(pfm_httpd_uri_head);

struct al_httpd {
	httpd_handle_t server;
	struct pfm_httpd_uri *unknown_uri;	/* handler for unknown URI */
};

static wise_err_t pfm_httpd_handler(httpd_req_t *req);

/*
 * HTTP header from request.
 * These are allocated as requested, and freed with the sess.
 * Requesting the same header repeatedly is not advised.
 */
struct pfm_httpd_header {
	STAILQ_ENTRY(pfm_httpd_header) list;
	/* the header value follows struct */
};

/*
 * Context for a session (connection).
 * The resource and query argument strings and headers for the current
 * request are dynamically allocated under this struct.
 */
struct pfm_httpd_sess {
	char *resource;
	char *argv[PFM_HTTPD_ARGS];
	u8 argc;
	u8 close_requested;		/* upper layer requests close */
	size_t offset;
	void *resume_arg;		/* arg for async resume */
	osThreadId_t task_handle;	/* server task */
	osMessageQueueId_t queue;		/* queue of buffers */
	STAILQ_HEAD(, pfm_httpd_header) headers;
};

struct pfm_httpd_uri {
	enum al_http_method method;
	const char *uri;
	STAILQ_ENTRY(pfm_httpd_uri) list;
	u8 registered;

	/*
	 * this handler returns NULL on success, or another handler to be
	 * used on failure.
	 */
	int (*(*handler)(struct al_httpd_conn *))(struct al_httpd_conn *,
	     void *);
};

static struct al_httpd al_httpd;

/*
 * Free a session.
 * Called from httpd_server in esp-idf.
 */
static void pfm_httpd_sess_free(void *ctx)
{
	struct pfm_httpd_sess *sess = ctx;
	struct pfm_httpd_header *hdr;

	PFM_HTTPD_LOG(LOG_DEBUG2, "free sess %p", sess);
	if (!sess) {
		return;
	}
	while (1) {
		hdr = STAILQ_FIRST(&sess->headers);
		if (!hdr) {
			break;
		}
		STAILQ_REMOVE_HEAD(&sess->headers, list);
		al_os_mem_free(hdr);
	}
    osMessageQueueDelete(sess->queue);
	al_os_mem_free(sess);
}

static wise_err_t pfm_httpd_reg_uri(struct pfm_httpd_uri *urip)
{
	struct al_httpd *state = &al_httpd;
	wise_err_t err;
	httpd_uri_t handler;
	httpd_method_t method;

	switch (urip->method) {
	case AL_HTTPD_METHOD_GET:
		method = HTTP_GET;
		break;
	case AL_HTTPD_METHOD_HEAD:
		method = HTTP_HEAD;
		break;
	case AL_HTTPD_METHOD_POST:
		method = HTTP_POST;
		break;
	case AL_HTTPD_METHOD_PUT:
		method = HTTP_PUT;
		break;
	case AL_HTTPD_METHOD_DELETE:
		method = HTTP_DELETE;
		break;
	default:
		return AL_ERR_INVAL_VAL;
	}

	memset(&handler, 0, sizeof(handler));
	handler.uri = urip->uri;
	handler.method = method;
	handler.handler = pfm_httpd_handler;
	handler.user_ctx = urip;

	err = httpd_register_uri_handler(state->server, &handler);
	if (err) {
		PFM_HTTPD_LOG(LOG_ERR, "reg failed err %#x uri %s",
		    err, urip->uri);
		return err;
	}
	urip->registered = 1;
	return WISE_OK;
}

static void pfm_httpd_reg_uris(void)
{
	struct pfm_httpd_uri *urip;

	STAILQ_FOREACH(urip, &pfm_httpd_uri_head, list) {
		if (urip->registered) {
			continue;
		}
		(void)pfm_httpd_reg_uri(urip);
	}
}

/*
 * Build session context for request.
 * Split URI into strings of resource and args.
 * Unexpand the URL escapes in the args.
 * Returns 0 on success, -1 on error, but ignores excessive query args.
 */
static struct pfm_httpd_sess *pfm_httpd_sess_alloc(httpd_req_t *req)
{
	struct pfm_httpd_sess *sess;
	size_t len;
	size_t rlen;
	ssize_t rc;
	const char *sp;
	const char *qp;
	char *cp;
	char sep = '?';

	len = strlen(req->uri) + 1;
	sess = al_os_mem_alloc(sizeof(*sess) + len);
	if (!sess) {
		PFM_HTTPD_LOG(LOG_WARN, "alloc failed");
		return NULL;
	}
	memset(sess, 0, sizeof(*sess));
	STAILQ_INIT(&sess->headers);

	cp = (char *)(sess + 1);
	sess->resource = cp;
	rlen = len;
	sp = req->uri;
	sess->argc = 0;
	sess->task_handle = osThreadGetId();
	sess->queue = osMessageQueueNew(4, sizeof(u8), NULL); /* only one wakeup needed */
	if (!sess->queue) {
		al_os_mem_free(sess);
		PFM_HTTPD_LOG(LOG_WARN, "queue alloc failed");
		return NULL;
	}

	while (1) {
		qp = strchr(sp, sep);
		if (qp) {
			len = qp - sp;		/* length to separator */
		} else {
			len = strlen(sp);
		}
		rc = uri_decode_n(cp, rlen, sp, len);
		if (rc < 0) {
			PFM_HTTPD_LOG(LOG_WARN, "URI decode error: \"%s\"",
			    req->uri);
			break;
		}

		if (sep == '?') {
			sess->resource = cp;
		} else if (sess->argc >= ARRAY_LEN(sess->argv)) {
			PFM_HTTPD_LOG(LOG_WARN, "args limit reached: \"%s\"",
			    req->uri);
			break;
		} else {
			sess->argv[sess->argc] = cp;
			sess->argc++;
		}
		cp += rc + 1;
		rlen -= rc + 1;
		if (!qp) {
			break;
		}
		sp = qp + 1;
		sep = '&';
	}
	return sess;
}

/*
 * URL handler.
 * Entry for handlers from esp-idf HTTPD URL callback.
 *
 * Note: these incoming requests occur in the esp-idf httpd thread.
 * Reads and writes must also be done from that thread, so it may block in
 * the handler to wait for calls from other threads that may
 * be responding to the request.
 */
static wise_err_t pfm_httpd_handler(httpd_req_t *req)
{
	struct pfm_httpd_uri *urip = req->user_ctx;
	struct pfm_httpd_sess *sess;
	struct al_httpd_conn *conn = (struct al_httpd_conn *)req;
	int (*resume)(struct al_httpd_conn *, void *arg);
	osStatus_t rc;
	wise_err_t err;
	u8 evt;

	log_thread_id_set("s");

	ASSERT(urip);			/* guaranteed by httpd */
	ASSERT(urip->handler);		/* guaranteed by us */

	sess = req->sess_ctx;
	if (sess) {			/* this is not expected */
		PFM_HTTPD_LOG(LOG_ERR, "freeing sess context %p", sess);
		pfm_httpd_sess_free(sess);
		req->sess_ctx = NULL;
	}

	/*
	 * Allocate the session context.
	 * Also parses URI and query string.
	 */
	sess = pfm_httpd_sess_alloc(req);
	if (!sess) {
		httpd_resp_send_500(req);
		return -1;
	}

	/*
	 * Set context here.
	 * Set "ignore" flag so that httpd will not free it except on close.
	 */
	req->ignore_sess_ctx_changes = true;
	req->sess_ctx = sess;
	req->free_ctx = pfm_httpd_sess_free;

	/*
	 * Call upper-layer handler.
	 */
	resume = urip->handler(conn);

	/*
	 * If there is a resume function returned, wait for al_httpd_complete()
	 * to be called, then call resume().  Repeat until it returns 0.
	 */
	while (resume) {
		PFM_HTTPD_LOG(LOG_DEBUG2, "paused");
		rc = osMessageQueueGet(sess->queue, &evt, NULL, osWaitForever);
		if (rc != osOK) {
			PFM_HTTPD_LOG(LOG_ERR, "queue recv rc %d", rc);
			err = WISE_FAIL;
			goto free;
		}
		if (sess->close_requested) {
			break;
		}
		PFM_HTTPD_LOG(LOG_DEBUG2, "resuming arg %p", sess->resume_arg);
		if (!resume(conn, sess->resume_arg)) {
			break;
		}
	}
	if (sess->close_requested) {
		PFM_HTTPD_LOG(LOG_DEBUG2, "close requested");
		err = WISE_FAIL;
		goto free;
	}
	err = WISE_OK;
free:
	pfm_httpd_sess_free(sess);
	req->sess_ctx = NULL;
	return err;
}

static wise_err_t pfm_httpd_error_handle(httpd_req_t *req,
		httpd_err_code_t http_err)
{
	struct pfm_httpd_uri *urip = req->user_ctx;
	struct al_httpd *state = &al_httpd;

	switch (http_err) {
	case HTTPD_404_NOT_FOUND:
		if (state->unknown_uri) {
			urip = state->unknown_uri;
			req->user_ctx = urip;
			return pfm_httpd_handler(req);
		}
		break;
	default:
		break;
	}
	return -1;
}

int al_httpd_reg_url_cb(const char *url, enum al_http_method method,
	int (*(*cb)(struct al_httpd_conn *))(struct al_httpd_conn *, void *))
{
	struct al_httpd *state = &al_httpd;
	struct pfm_httpd_uri *urip;

	if (!cb) {
		return -1;
	}

	/*
	 * Look for existing URI.
	 * The upper layer should not repeat registration but does.
	 * There are three lists of URIs, one in ADA, AL, and ESP-IDF.
	 */
	STAILQ_FOREACH(urip, &pfm_httpd_uri_head, list) {
		if (urip->method == method && !strcmp(urip->uri, url)) {
			urip->handler = cb;
			return 0;
		}
	}
	urip = state->unknown_uri;
	if (urip && !*url) {
		urip->handler = cb;
		return 0;
	}

	urip = al_os_mem_calloc(sizeof(*urip));
	if (!urip) {
		return -1;
	}
	urip->handler = cb;
	urip->method = method;
	urip->uri = url;

	/*
	 * Special case: empty URL on GET means to handle unknown URLs.
	 */
	if (!*url) {
		state->unknown_uri = urip;
		return 0;
	}

	/*
	 * Register now if server is started.  Register later otherwise.
	 */
	if (state->server) {
		if (pfm_httpd_reg_uri(urip)) {
			al_os_mem_free(urip);
			return -1;
		}
	}
	STAILQ_INSERT_TAIL(&pfm_httpd_uri_head, urip, list);
	return 0;
}

const char *al_httpd_get_method(struct al_httpd_conn *conn)
{
	httpd_req_t *req = (httpd_req_t *)conn;

	switch (req->method) {
	case HTTP_GET:
		return "GET";
	case HTTP_HEAD:
		return "HEAD";
	case HTTP_POST:
		return "POST";
	case HTTP_PUT:
		return "PUT";
	case HTTP_DELETE:
		return "DELETE";
	default:
		break;
	}
	return NULL;
}

const char *al_httpd_get_resource(struct al_httpd_conn *conn)
{
	httpd_req_t *req = (httpd_req_t *)conn;
	struct pfm_httpd_sess *sess = req->sess_ctx;

	return sess ? sess->resource : NULL;
}

const char *al_httpd_get_url_arg(struct al_httpd_conn *conn, const char *name)
{
	httpd_req_t *req = (httpd_req_t *)conn;
	struct pfm_httpd_sess *sess = req->sess_ctx;
	char *cp;
	unsigned int i;
	size_t len = strlen(name);

	if (!sess) {
		return NULL;
	}
	for (i = 0; i < sess->argc; i++) {
		cp = strchr(sess->argv[i], '=');
		if (!cp) {
			continue;
		}
		if (len == cp - sess->argv[i] &&
		    !memcmp(sess->argv[i], name, len)) {
			return cp + 1;
		}
	}
	return NULL;
}

/*
 * Get HTTP version.
 * This is not exposed by esp-idf and not needed by ADA.
 */
const char *al_httpd_get_version(struct al_httpd_conn *conn)
{
	return NULL;
}

/*
 * Get request header.
 * Headers are allocated as needed and freed at the end of the session.
 */
const char *al_httpd_get_req_header(struct al_httpd_conn *conn,
	const char *name)
{
	httpd_req_t *req = (httpd_req_t *)conn;
	struct pfm_httpd_sess *sess = req->sess_ctx;
	struct pfm_httpd_header *hdr;
	size_t len;
	char *val;
	wise_err_t err;

	if (!req || !sess) {
		return NULL;
	}
	len = httpd_req_get_hdr_value_len(req, name);
	if (!len) {
		return NULL;
	}
	hdr = al_os_mem_alloc(sizeof(*hdr) + len + 1);
	if (!hdr) {
		return NULL;
	}
	memset(hdr, 0, sizeof(*hdr));
	val = (char *)(hdr + 1);

	err = httpd_req_get_hdr_value_str(req, name, val, len + 1);
	if (err) {
		PFM_HTTPD_LOG(LOG_ERR, "header %s err %#x", name, err);
		al_os_mem_free(hdr);
		return NULL;
	}
	val[len] = '\0';
	STAILQ_INSERT_HEAD(&sess->headers, hdr, list);
	return val;
}

const struct al_net_addr *al_httpd_get_local_ip(struct al_httpd_conn *conn)
{
	return NULL; /* note: no longer used */
}

const struct al_net_addr *al_httpd_get_remote_ip(struct al_httpd_conn *conn)
{
	return NULL; /* note: no longer used */
}

int al_httpd_complete(struct al_httpd_conn *conn, void *arg)
{
	httpd_req_t *req = (httpd_req_t *)conn;
	struct pfm_httpd_sess *sess;
	u8 evt = 0;

	if (!req) {
		PFM_HTTPD_LOG(LOG_ERR, "no req");
		return -1;
	}
	sess = req->sess_ctx;
	if (!sess) {
		PFM_HTTPD_LOG(LOG_ERR, "no session");
		return -1;
	}
	sess->resume_arg = arg;
	if (sess->task_handle == osThreadGetId()) {
		PFM_HTTPD_LOG(LOG_ERR, "called from httpd task");
		return -1;
	}
	/* wakeup handler task */
	osMessageQueuePut(sess->queue, &evt, 0, 0);
	return 0;
}

int al_httpd_read(struct al_httpd_conn *conn, char *buf, size_t buf_size)
{
	httpd_req_t *req = (httpd_req_t *)conn;
	struct pfm_httpd_sess *sess = req->sess_ctx;
	size_t len;
	int rc;

	if (!sess) {
		PFM_HTTPD_LOG(LOG_ERR, "no session");
		return -1;
	}
	if (sess->task_handle != osThreadGetId()) {
		PFM_HTTPD_LOG(LOG_ERR, "not called from httpd task");
		return -1;
	}
	if (sess->offset >= req->content_len) {
		return 0;
	}
	len = req->content_len - sess->offset;
	if (buf_size < len) {
		len = buf_size;
	}
	rc = httpd_req_recv(req, buf, len);
	if (rc < 0) {
		PFM_HTTPD_LOG(LOG_ERR, "recv err %#x", rc);
		return -1;
	}
	sess->offset += len;
	return rc;
}
static int pfm_httpd_write(struct al_httpd_conn *conn,
		const char *buf, size_t len)
{
	httpd_req_t *req = (httpd_req_t *)conn;
	struct pfm_httpd_sess *sess = req->sess_ctx;
	int rc;

	if (!conn || !sess) {
		PFM_HTTPD_LOG(LOG_ERR, "connection closed");
		return -1;
	}
	if (sess->task_handle != osThreadGetId()) {
		PFM_HTTPD_LOG(LOG_ERR, "not called from httpd task");
		return -1;
	}
	rc = httpd_send(req, buf, len);
	if (rc < 0) {
		PFM_HTTPD_LOG(LOG_ERR, "httpd_sock_err %d", rc);
		return -1;
	}
	if (rc < len) {
		PFM_HTTPD_LOG(LOG_ERR, "short send %d of %zu", rc, len);
		return -1;
	}
	return 0;
}

static const struct name_val pfm_httpd_status_msgs[] = {
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

const char *pfm_httpd_status_msg(unsigned int status)
{
	const char *msg;

	msg = lookup_by_val(pfm_httpd_status_msgs, status);
	return msg ? msg : "";
}

/*
 * Start response.
 */
int al_httpd_response(struct al_httpd_conn *conn, int status,
	const char *headers)
{
	char buf[80];		/* buffer for status message */
	int rc;

	rc = snprintf(buf, sizeof(buf), "HTTP/1.1 %d %s\r\n",
	    status, pfm_httpd_status_msg(status));
	if (rc >= sizeof(buf)) {
		rc = sizeof(buf) - 1;	/* truncation is not harmful here */
	}
	if (pfm_httpd_write(conn, buf, rc)) {
		return -1;
	}
	if (headers && pfm_httpd_write(conn, headers, strlen(headers))) {
		return -1;
	}
	return 0;
}

/*
 * Write response.
 * Note, this currently can contain headers until the first double-CRLF is sent.
 */
int al_httpd_write(struct al_httpd_conn *conn, const char *data, size_t size)
{
	return pfm_httpd_write(conn, data, size);
}

void al_httpd_close_conn(struct al_httpd_conn *conn)
{
	httpd_req_t *req = (httpd_req_t *)conn;
	struct pfm_httpd_sess *sess = req->sess_ctx;
	u8 evt = 0;

	if (!sess) {
		return;
	}
	PFM_HTTPD_LOG(LOG_DEBUG2, "close");
	sess->close_requested = 1;
	if (sess->task_handle != osThreadGetId()) {
		osMessageQueuePut(sess->queue, &evt, 0, 0);
	}
}

int al_httpd_start(u16 port)
{
	struct al_httpd *state = &al_httpd;
	httpd_config_t conf = HTTPD_DEFAULT_CONFIG();
	wise_err_t err;

	if (state->server) {
		return 0;
	}
	conf.server_port = port;
	conf.stack_size = PFM_HTTPD_STACK_SIZE;
	conf.max_open_sockets = PFM_HTTPD_SOCKETS;
	conf.max_uri_handlers = PFM_HTTPD_URIS;
	conf.max_resp_headers = PFM_HTTPD_HEADERS;

	err = httpd_start(&state->server, &conf);
	if (err) {
		PFM_HTTPD_LOG(LOG_ERR, "httpd_start failed failed err %#x",
		    err);
		return -1;
	}
	ASSERT(state->server);
	pfm_httpd_reg_uris();
	err = httpd_register_err_handler(state->server, HTTPD_404_NOT_FOUND,
	    pfm_httpd_error_handle);
	if (err) {
		PFM_HTTPD_LOG(LOG_WARN, "reg_err_handler failed err %#x", err);
	}
	return 0;
}

void al_httpd_stop(void)
{
	struct al_httpd *state = &al_httpd;
	struct pfm_httpd_uri *urip;
	wise_err_t err;

	if (!state->server) {
		return;
	}
	err = httpd_stop(state->server);
	if (err) {
		PFM_HTTPD_LOG(LOG_ERR, "stop failed err %#x", err);
		return;
	}
	state->server = NULL;

	/*
	 * httpd_stop() frees sessions and unregisters all URIs.
	 */
	STAILQ_FOREACH(urip, &pfm_httpd_uri_head, list) {
		urip->registered = 0;
	}
}
