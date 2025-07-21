/*
 * Copyright 2011-2015 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_SERVER_REQ_H__
#define __AYLA_SERVER_REQ_H__

#include <ayla/log.h>
#include <ada/err.h>
#include <sys/queue.h>

#define HTTPD_PORT	80

#define SERVER_BUFLEN 1024	/* size of onstack buffer */
#ifdef TCP_MSS
#define SERVER_SND_BUF	(4 * TCP_MSS)	/* extra space to fit large web page */
#else
#define SERVER_SND_BUF	(4 * 536)	/* extra space to fit large web page */
#endif

#define SERVER_URI_LEN	256	/* max URI length including query string */
#define SERVER_URL_GROUPS 16	/* max number of URL tables */

/*
 * method.
 */
enum server_method {
	REQ_BAD = 0,
	REQ_GET,
	REQ_GET_HEAD,
	REQ_POST,
	REQ_PUT,
	REQ_DELETE,
};

/*
 * Request flags.
 * In struct url_list, these flags indicate which types of access is allowed.
 * As an argument to server_req() only one flag should be set to indicate
 * the type of access being used.
 *
 * LOC_REQ is used for open HTTP requests (just setting up LAN mode).
 * APP_REQ is for LAN mode.
 * ADS_REQ are requests from the cloud, currently via reverse-ReST.
 * SEC_WIFI_REQ is for LAN mode from the AP interface (secure Wi-Fi setup).
 * REQ_SOFT_AP is for open HTTP requests (currently only for property tests),
 *
 * Note: REQ_SOFT_AP can be set in the call to server_req() when the request
 * comes from any interface if AP is also up.  So to handle LOC_REQ,
 * it is safest to also handle SOFT_REQ_AP.
 */
#define LOC_REQ		0x01	/* Allow local request (not encrypted) */
#define APP_REQ		0x02	/* Allow mobile app request (encrypted) */
#define ADS_REQ		0x04	/* Allow ADS request (encrypted) */
#define SEC_WIFI_REQ	0x08	/* Allow for secure Wi-Fi setup (encrypted) */
#define REQ_SOFT_AP	0x20	/* Allow unencrypted access in SoftAP mode */

#define APP_ADS_REQS		(APP_REQ | ADS_REQ)

struct prop;
struct server_req;

struct url_list {
	enum server_method method;
	u8 req_flags;
	const char *url;
	void (*url_op)(struct server_req *);
	const void *arg;
};

/*
 * Macros for initializing url_list tables.
 */
#define URL_GET(url, func, flags) { REQ_GET, flags, url, func }
#define URL_GET_ARG(url, func, flags, arg) { REQ_GET, flags, url, func, arg }
#define URL_PUT(url, func, flags) { REQ_PUT, flags, url, func }
#define URL_POST(url, func, flags) { REQ_POST, flags, url, func }
#define URL_DELETE(url, func, flags) { REQ_DELETE, flags, url, func }
#define URL_END	{ .method = REQ_BAD, .url = NULL }

/*
 * Per-request state.
 */
struct server_req {
	char resource[SERVER_URI_LEN];	/* file or page name including query */
	const struct url_list *url;
	enum {
		REQ_INIT,
		REQ_HEAD,
		REQ_DATA,
		REQ_READY,
		REQ_DONE,
		REQ_ERR,
		REQ_CLOSING,
	} state;
	enum server_method method;
	void *req_impl;		/* for private use by requestor */
	void *prov_impl;	/* for private use by content provider */
	char *post_data;	/* PUT or POST data, if any */
	u16 content_len;	/* for post, the content length value */
	char *buf;		/* stack scratch buffer, size SERVER_BUFLEN */
	size_t len;		/* used bytes in buffer */

	/* Function to put header of response */
	void (*put_head)(struct server_req *req, unsigned int status,
			const char *content_type);

	/* Function to flush data to tcp_write */
	void (*write_cmd)(struct server_req *req, const char *msg);

	/* Function to call after all writes for a req are complete */
	enum ada_err (*finish_write)(struct server_req *req);

	/* Function to call to resume request in server context */
	void (*continue_req)(struct server_req *req);

	u8 host_match:1;	/* hostname in request matches ours */
	u8 host_present:1;	/* hostname present in request */
	u8 suppress_out:1;	/* suppress output (for HEAD requests) */
	u8 static_alloc:1;	/* statically allocated request, not on list */
	u8 head_sent:1;		/* HTTP header completely sent */
	u8 prop_abort:1;        /* underlying TCP connection gone. Abort */
	u8 admin:1;		/* indicates this req has admin privs */
	u8 keep_open:1;		/* HTTP 1.1 connection */
	u8 user_in_prog:1;	/* user has dependent request in progress */
	u8 resp_len_limited:1;	/* response length is limited by resp_len_rem */
	u16 resp_len_rem;	/* remaining length allowed for response */
	u16 http_status;	/* status for reverse-REST, zero means 200 */

	enum ada_err err;	/* error, if any, that occurred on last write */

	/*
	 * Support for continuing a get request after a put fails due to
	 * lack of space in the TCP layer.
	 */
	void (*resume)(struct server_req *); /* continuation function for get */
	void *user_priv;	/* private state when get function resumed */
	struct callback *tcpip_cb; /* optional tcpip_cb after close */
	void (*close_cb)(struct server_req *); /* server close callback */
	char *(*get_arg_by_name)(struct server_req *req, const char *name,
		char *buf, size_t len);
};

#ifdef STATIC_WEB_CONTENT_IN_MEMORY
/*
 * Static web pages are inside addressable memory.
 */

#ifdef XXD_BIN_TO_C
struct server_buf {
	const char *content_type;
	const void *buf;
	unsigned int *len;
};

#define SERVER_BUF_INIT(text, resource, type) { \
	.buf = LINKER_TEXT_START(text),		\
	.len = &LINKER_TEXT_SIZE(text),		\
	.content_type = type,			\
		}

#define SERVER_BUF_LEN(bufp)	((size_t)*((bufp)->len))

#elif defined(CONFIG_IDF_CMAKE)

struct server_buf {
	const char *content_type;
	const void *buf;
	const void *end;
};

#define SERVER_BUF_INIT(text, resource, type) { \
	.buf = LINKER_TEXT_START(text),		\
	.end = LINKER_TEXT_END(text),		\
	.content_type = type,			\
		}

#define SERVER_BUF_LEN(bufp)	((char *)(bufp)->end - (char *)(bufp)->buf)

#else
struct server_buf {
	const char *content_type;
	const void *buf;
	size_t len;
};

#define SERVER_BUF_INIT(text, resource, type) { \
	.buf = LINKER_TEXT_START(text),		\
	.len = (size_t)LINKER_TEXT_SIZE(text),	\
	.content_type = type,			\
}

#define SERVER_BUF_LEN(bufp)	((bufp)->len)
#endif

#else
/*
 * Static web pages are stored as 'resource' files inside flash.
 */
struct server_buf {
	const char *content_type;
	const char *file;
};

#define SERVER_BUF_INIT(text, resource, type) { \
	.file = resource,			\
	.content_type = type,			\
}
#endif /* STATIC_WEB_CONTENT_IN_MEMORY */


/*
 * Start server.
 * Call when network interface comes up.
 */
void ada_server_up(void);

/*
 * Deprecated alternative to ada_server_up().
 */
static inline void server_up(void)
{
	ada_server_up();
}

/*
 * Stop server.
 */
void ada_server_down(void);

/*
 * Enable server redirection
 */
void server_enable_redir(void);

/*
 * Hook for URLs which are not otherwise found.
 * The function pointed to here is called if no handler is registered
 * for the URL.  If this hook handles the request successfully, it returns 0.
 */
extern int (*server_not_found_hook)(struct server_req *);

/*
 * Macro to make logging easier
 */
#define SERVER_LOGF(_level, _format, ...) \
	server_log(_level "%s: " _format, __func__, ##__VA_ARGS__)

void server_log(const char *fmt, ...) ADA_ATTRIB_FORMAT(1, 2);

/*
 * Add URLs to be handled.
 */
void server_add_urls(const struct url_list *);

/*
 * Register URLs with lower-level
 */
void server_reg_urls(const struct url_list *);

/*
 * Put HTTP response with no body.
 */
void server_put_status(struct server_req *req, unsigned int status);

/*
 * Get status message from status code.
 */
const char *server_status_msg(unsigned int status);

void server_put_flush(struct server_req *req, const char *msg);
void server_put_pure(struct server_req *, const char *);
void server_put_pure_len(struct server_req *req, const char *msg, size_t len);
int server_put(struct server_req *, const char *fmt, ...)
	ADA_ATTRIB_FORMAT(2, 3);
char *server_get_arg_by_name(struct server_req *, const char *name,
				char *buf, size_t len);
int server_get_long_arg_by_name(struct server_req *, const char *name,
		 long *valp);
u8 server_get_bool_arg_by_name(struct server_req *, const char *name);
char *server_get_dup_arg_by_name(struct server_req *, const char *name);
char *server_get_arg_len(struct server_req *, char **valp, size_t *);
void server_continue(struct server_req *, void (*resume)(struct server_req *));
const struct url_list *server_find_handler(struct server_req *req,
			const char *url, enum server_method, u8 priv);
void server_free_aborted_req(struct server_req *req);
void server_get_not_found(struct server_req *req);
enum server_method server_parse_method(const char *method);

void metrics_config_json_put(struct server_req *);

void client_ota_json_put(struct server_req *);
#if defined(AYLA_LAN_SUPPORT) && !defined(DISABLE_LAN_OTA)
void client_lanota_json_put(struct server_req *);
#endif

/*
 * JSON interfaces.
 */
void server_json_header(struct server_req *);

/*
 * Allow log client enable/disable through reverse-rest
 */
void client_log_client_json_put(struct server_req *req);

/*
 * Initialize a staticly-allocated server request.
 */
void server_req_init(struct server_req *);

/*
 * Perform close callbacks after server response is delivered.
 */
void server_req_done_callback(struct server_req *);

extern const char server_content_json[];
extern const char server_content_html[];
extern const char server_html_head[];
extern const char server_json_head[];

/*
 * Determine whether host from incoming request matches our IP address.
 * Returns 1 on a match, 0 otherwise.
 */
int server_host_match(const char *host);

void server_test_init(void);		/* optional test pages */

#endif /* __AYLA_SERVER_REQ_H__ */
