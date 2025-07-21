/*
 * Copyright 2012 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_HTTP_CLIENT_H__
#define __AYLA_HTTP_CLIENT_H__

#include <ayla/timer.h>
#include <al/al_net_dns.h>
#include <ada/metrics.h>
#include <ada/err.h>

#define HTTP_CLIENT_SERVER_PORT	80
#define HTTP_CLIENT_SERVER_PORT_SSL	443

#define HTTP_CLIENT_BUF_LEN	640	/* buffer size for initial request */

#define HTTP_CLIENT_TCP_WAIT	30000	/* TCP connect wait, millsecs */
#define HTTP_CLIENT_CONN_WAIT	50000	/* TCP+SSL connect wait, millsecs */
#define HTTP_CLIENT_RETRY_WAIT	10000	/* retry wait, milliseconds */
#define HTTP_CLIENT_TRANS_WAIT  10000	/* TCP send/recv wait, milliseconds */
#define HTTP_CLIENT_RECV_WAIT  20000	/* TCP Recv wait, milliseconds */
#define HTTP_CLIENT_OPEN_WAIT    3000	/* TCP Open Wait */
#define HTTP_CLIENT_MEM_WAIT    60000	/* TCP ERR_MEM Wait */
#define HTTP_CLIENT_KEEP_OPEN_WAIT    60000	/* TCP Keep Open Wait */
#define HTTP_CLIENT_MCU_WAIT    15000	/* TCP Wait for MCU to consume recv */
#define HTTP_CLIENT_TRY_COUNT	3	/* tries before giving up on server */
#define HTTP_LOC_CLIENT_TRY_COUNT 2	/* tries before giving up on loc cli */
#define HTTP_CLIENT_TX_HDR_LIMIT 5	/* max headers to send */

enum http_client_method {
	HTTP_REQ_NONE = 0,
	HTTP_REQ_GET,
	HTTP_REQ_PUT,
	HTTP_REQ_POST,
};

enum http_client_state {
	HCS_IDLE,
	HCS_DNS,	/* resolving host name */
	HCS_CONN,	/* wait for connection to service */
	HCS_CONN_TCP,	/* have tcp conn, waiting for ssl */
	HCS_SEND,	/* waiting for send to complete */
	HCS_HEAD,	/* receiving HTTP headers */
	HCS_CHUNKED_HEAD, /* receiving chunked size */
	HCS_CONTENT,	/* receiving HTTP content */
	HCS_OPEN,	/* ready to send data */
	HCS_KEEP_OPEN,	/* done with prev, ready for another request */
	HCS_WAIT_RETRY,	/* waiting to retry after connection or I/O error */
	HCS_WAIT_TCP_SENT, /* waiting for tcp sent callback */
	HCS_WAIT_CLI_RECV, /* waiting for client recv callback */
	HCS_COMPLETE,	/* waiting to do completion callback then idle */
	HCS_MAX
};

#define HTTP_CLIENT_STATE_NAMES {		\
	[HCS_IDLE]		= "idle",	\
	[HCS_DNS]		= "dns",	\
	[HCS_CONN]		= "conn",	\
	[HCS_CONN_TCP]		= "conn_tcp",	\
	[HCS_SEND]		= "send",	\
	[HCS_HEAD]		= "head",	\
	[HCS_CHUNKED_HEAD]	= "chunked_head",\
	[HCS_CONTENT]		= "content",	\
	[HCS_OPEN]		= "open",	\
	[HCS_KEEP_OPEN]		= "keep_open",	\
	[HCS_WAIT_RETRY]	= "wait_retry",	\
	[HCS_WAIT_TCP_SENT]	= "wait_tcp_sent",\
	[HCS_WAIT_CLI_RECV]	= "wait_cli_recv",\
	[HCS_COMPLETE]		= "complete",	\
}

enum http_client_chunk_state {
	HC_CHUNK_START,	/* sending chunk length */
	HC_CHUNK_BODY,	/* sending chunk body */
	HC_CHUNK_END	/* send chunk end delimiter */
};

enum http_client_error {
	HC_ERR_NONE = 0,
	HC_ERR_DNS,
	HC_ERR_MEM,
	HC_ERR_CONNECT,
	HC_ERR_SEND,
	HC_ERR_RECV,
	HC_ERR_HTTP_PARSE,
	HC_ERR_HTTP_REDIR,
	HC_ERR_HTTP_STATUS,
	HC_ERR_CLOSE,
	HC_ERR_CLIENT_AUTH,
	HC_ERR_CONN_CLSD,
	HC_ERR_TIMEOUT,
};

/**
 * mqtt connection state.
 */
enum http2mqtt_conn_state {
	H2M_DISCONNECTED,
	H2M_CONNECTED,
};

/*
 * Extra headers
 */
struct http_hdr {
	const char *name;
	const char *val;
};

struct http_client {
	enum http_client_state state;
	enum http_client_chunk_state chunk_state;
	struct al_net_stream *pcb;	/* connecting stream */
	char host[80];			/* server host name or IP address */
	struct al_net_addr host_addr;		/* server ip address */
	void *parent;			/* parent of the http_client struct */
	enum http_client_error hc_error; /* potential http client error */
	struct http_state http_state;	/* HTTP parser state */
	enum mod_log_id mod_log_id;
	enum http_client_method method;	/* method for current request */
	u8 retries;
	u8 ssl_enable;
	u8 client_auth;		/* use client authentication header */
	u8 req_pending;
	u8 hmc_initialized;	/* http2mqtt initialized */
	u8 req_outstanding;	/* http2mqtt req outstanding */
	u8 is_connected;	/* http2mqtt connection state */
	u8 is_mqtt;		/* http2mqtt req is send by mqtt */
	u8 content_given;	/* has the content-length been given? */
	u8 chunked_enc;		/* server resp contains chunked encoding */
	u8 conn_close;		/* server resp contains connection close */
	u8 range_given;		/* server resp contains the range */
	u8 sending_chunked;	/* app long data sent in many times */
	u8 http_tx_chunked;	/* http payload chunked encoding */
	u8 chunked_eof;		/* set to 1 if we sent eof of long */
	u8 prop_callback;	/* set to 1 if cli running prop_cb */
	u8 user_in_prog;	/* send is waiting for user progress */
	u8 accept_non_ayla;	/* accept cert from non_ayla (stream_mssl) */
	u8 in_send_cb;
	u8 wait_close;		/* wait for close to complete */
	u8 req_part;		/* part # of the current request */
	u16 host_port;          /* port # of host (for LAN support) */
	u32 range_bytes;	/* # of bytes given in the range header */
	u32 range_start;	/* starting offset from content-range header */
	u32 range_end;		/* ending offset from content-range header */
	u32 content_len;	/* expected content-length of incoming tcp */
	const void *body_buf;	/* put/post data, if any */
	size_t body_buf_len;	/* length of PUT/POST buf data not yet sent */
	size_t body_len;	/* content length for PUT/POST */
	ssize_t sent_len;	/* body length already sent */
	u32 http_status;	/* http status code from service response */
	u8 retry_limit;		/* # of retries limit */
	u16 retry_wait;		/* wait time between retries (in millisecs) */
	u16 conn_wait;		/* wait time for a connection (in millisecs) */
	size_t recv_consumed;	/* # of bytes consumed by client of tcp_recv */
	u32 retry_after;	/* time from HTTP retry-after header */
#ifdef AYLA_METRICS_SUPPORT
	u16 metrics_instance;	/* instance of metrics it use for this hc */
#endif
	char *auth_hdr;		/* auth header, NULL if not needed */
	size_t auth_hdr_size;	/* size of auth header buffer */

	/* Function Pointers For Callbacks to Client */
	void (*client_send_data_cb)(struct http_client *);
	void (*client_err_cb)(struct http_client *);
	enum ada_err (*client_tcp_recv_cb)(struct http_client *, void *,
	    size_t);
	void (*client_next_step_cb)(struct http_client *);
	void (*conn_state_cb)(enum http2mqtt_conn_state st);
	void (*notify_cb)(void *arg);
	void (*close_cb)(struct http_client *);
	struct timer hmc_timer;
	size_t req_len;		/* len of data in req_buf */
	char req_buf[HTTP_CLIENT_BUF_LEN];
	struct http_hdr hdrs[HTTP_CLIENT_TX_HDR_LIMIT];  /* MQTT only */
	u8 hdr_cnt;

	struct timer hc_timer;
	struct al_net_dns_req dns_req;
};

void http_client_start(struct http_client *);

extern const struct http_hdr http_hdr_content_json;
extern const struct http_hdr http_hdr_content_stream;
extern const struct http_hdr http_hdr_content_xml;
extern const struct http_hdr http_hdr_accept_tag;

int http_client_is_ready(struct http_client *);
int http_client_is_sending(struct http_client *);
void http_client_req(struct http_client *, enum http_client_method method,
    const char *resource, int hcnt, const struct http_hdr *hdrs);
void http_client_abort(struct http_client *);

/*
 * http_client_send() can be used only to send HTTP payload data (chunked
 * or non-chunked). If hc->http_tx_chunked is set, http_client_send()
 * will convert data to http chunked data. Do not use this function to send
 * HTTP request.
 */
enum ada_err http_client_send(struct http_client *, const void *, u16);
void http_client_send_pad(struct http_client *);
void http_client_send_complete(struct http_client *);
void http_client_reset(struct http_client *, enum mod_log_id);
void http_client_continue_recv(struct http_client *);
void http_client_set_conn_wait(struct http_client *hc, int wait);
void http_client_set_retry_wait(struct http_client *hc, int wait);
void http_client_set_retry_limit(struct http_client *hc, int limit);

#endif /* __AYLA_HTTP_CLIENT_H__ */
