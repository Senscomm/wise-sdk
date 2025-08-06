/*
 * Copyright 2011-2015 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_CLIENT_INT_H__
#define __AYLA_CLIENT_INT_H__

#include <ayla/callback.h>
#include "http_client.h"

#ifdef AYLA_LAN_SUPPORT
#include "lan_int.h"
#endif

#include "ame.h"
#include "client_ame.h"

/*
 * Internal client definitions.
 */

#define CLIENT_SERVER_DOMAIN_US    "aylanetworks.com"
#define CLIENT_SERVER_HOST_DEFAULT "ads-dev"

#define CLIENT_SERVER_HOST_DEF_FMT "mqtt-dev.%s"
#define CLIENT_SERVER_HOST_OEM_FMT "%s-%s-mqtt.%s"

#define CLIENT_SERVER_HOST_DEF_HTTP_FMT "ads-dev.%s"
#define CLIENT_SERVER_HOST_OEM_HTTP_FMT "%s-%s-device.%s"

#define CLIENT_SERVER_DEFAULT	CLIENT_SERVER_HOST_DEFAULT "." \
					CLIENT_SERVER_DOMAIN_US
#define CLIENT_SERVER_STAGE  "staging-ads.ayladev.com" /* staging server */

#define CLIENT_CMD_RETRY_WAIT	180000	/* retry wait for cmd retry */
#define CLIENT_LISTEN_WARN_WAIT 30000	/* ms before listen enable warning */
#define CLIENT_TEMPLATE_TIMEOUT	10000	/* ms to wait for host template ver */

#define CLIENT_KEY_LEN		16	/* maximum key length */

#define CLIENT_BUF_SIZE		560	/* buffer alloc for client */
#define CLIENT_API_MAJOR	1	/* API version we support */
#define CLIENT_API_MINOR	0	/* API minor version we support */

#define CLIENT_GET_REQ_LEN	128	/* size of buffer for ADS URL */
#define CLIENT_POST_REQ_LEN	500	/* size of buffer for POST request */
#define CLIENT_AUTH_FAILS	3	/* max authentication failures */

#define MAX_CMD_RESP		5000	/* max length of GET cmds in bytes */

/*
 * Size of buffer for ame parse
 */
#define CLIENT_AME_PARSE_BUF_LEN	1536

/*
 * Health check parameters.
 * The client (agent) must do a successful GET commands or cmds at least
 * every so often.  The GET is considered successful only if completely parsed.
 *
 * These times are in milliseconds.
 *
 * CLIENT_HEALTH_GET_INTVL is the maximum interval between GET commands.
 * If the client has not done one in that long, it will do one without any
 * notification.
 *
 * CLIENT_HEALTH_MAX_INTVL(x) is the maximum interval between GET commands for
 * the purposes of reporting good client health, computed from
 * x = CLIENT_HEALTH_GET_INTVL or x = CLIENT_HEALTH_GET_INTVL_TEST.
 */
#define CLIENT_HEALTH_GET_INTVL	(12 * 60 * 60 * 1000)
#define CLIENT_HEALTH_GET_INTVL_TEST	120000	/* test value for GET_INTVL */
#define CLIENT_HEALTH_GET_DIST(x) ((x) / 16)	/* random distribution range */
#define CLIENT_HEALTH_MAX_INTVL(x) ((x) * 2)	/* max in terms of GET_INTV */

/*
 * Macro to make logging easier
 */
#define CLIENT_LOGF(_level, _format, ...) \
	client_log(_level "%s: " _format, __func__, ##__VA_ARGS__)

#define CLIENT_DEBUG_EN		/* XXX temporary */

#ifdef CLIENT_DEBUG_EN
#define CLIENT_DEBUG(_level, _format, ...) \
	CLIENT_LOGF(_level, _format, ##__VA_ARGS__)
#else
#define CLIENT_DEBUG(_level, _format, ...)
#endif /* CLIENT_DEBUG_EN */

/*
 * OTA json request, parser expects this many tokens.
 *
 * 7 name-val pairs  + 1 extra, 2 start of objects + inner object name
 */
#define OTA_JSON_PUT_TOKENS (8 * 2 + 3)
#define OTA_LABEL_LEN		32	/* max size for OTA label string */

#define CLIENT_REG_JSON_TOKENS	8

PREPACKED_ENUM enum client_connect_target {
	CCT_NONE = 0,		/* not connected */
	CCT_ADS,		/* connect to ADS with TLS */
	CCT_IMAGE_SERVER,
	CCT_FILE_PROP_SERVER,
	CCT_LAN,
	CCT_REMOTE,
	CCT_IN_FIELD_PROVISION,
} PACKED_ENUM;

#if defined(AYLA_LAN_SUPPORT) || defined(AYLA_LOCAL_CONTROL_SUPPORT)
/*
 * LAN (local applications) definitions.
 */
#define CLIENT_LANIP_JSON	30	/* max # of tokens in lanip response */
#define CLIENT_LAN_KEEPALIVE	30	/* default LAN keepalive time, secs */
#define CLIENT_LAN_KEEPALIVE_GRACE 15	/* extra keepalive time allowed, secs */
#define CLIENT_LAN_OTA_HDR_SZ	256	/* size of encrypted header */
#endif

#define	CLIENT_TIME_FUDGE	1	/* # secs +/- that mod time can be */

#define LOG_CLIENT_TOKEN_MAX	40	/* max tokens in logclient.json body */

enum client_pri {
	CQP_HIGH,
	CQP_DEF,
	CQP_LOW,
	CQP_COUNT	/* must be last */
};

#define CLIENT_HIGH_QLEN	4
#define CLIENT_DEF_QLEN		8
#define CLIENT_LOW_QLEN		4

#ifdef AYLA_BATCH_PROP_SUPPORT
struct batch_dps_rsp {
	u32 batch_id;
	s32 status;
};
#endif

struct client_state {
	enum client_conn_state conn_state;
	char client_key[CLIENT_KEY_LEN]; /* client key on server */
	char reg_type[16];		/* registration type */
	char setup_token[CLIENT_SETUP_TOK_LEN]; /* connection setup token */
	char template_version[CONF_OEM_VER_MAX + 1]; /* OEM host version */
	char *setup_location;		/* location (if given) during setup */
	enum client_connect_target tgt; /* whom we're connected to */
	const char *region;		/* server region code from table */
	u16 client_info_flags;		/* flags for client info diffs */
	u16 info_flags_sending;		/* flags for client info being sent */
	u8 serv_conn:1;			/* internet connectivity to service */
	u8 get_all:1;			/* get all props/cmds */
	u8 ads_listen:1;		/* host mcu enabling GETs from ads */
	u8 prefer_get:1;		/* prioritize GETs over POSTs */
	u8 cmd_event:1;			/* client should GET commands */
	u8 ads_cmds_en:1;		/* enable GET cmds OR commands */
	u8 ame_init:1;			/* parse is ready for ame */
	u8 partial_content:1;		/* set to 1 if 206 status code rec */
	u8 cmd_pending:1;		/* set to 1 if ADS requested cmd */
	u8 cmd_delayed:1;		/* set to 1 if we need to delay rsp */
	u8 get_echo_inprog:1;		/* set to 1 if GET + Echo is in prog */
	u8 lan_cmd_pending:1;		/* set to 1 if a LAN requested cmd */
	u8 mcu_overflow:1;		/* 1 if mcu can't consume data */
	u8 get_cmds_fail:1;		/* 1 if get commands failed */
	u8 reset_at_commit:1;		/* 1 to reset when committing */
	u8 cmd_rsp_pending:1;		/* 1 if cmd_rsp is pending from mcu */
	u8 template_version_timeout:1;	/* template version not known yet */
	u8 wait_for_file_put:1;		/* 1 if waiting for a DP PUT */
	u8 wait_for_file_get:1;		/* 1 if waiting for a DP GET */
	u8 unexp_op:1;			/* 1 if host mcu tried unexp op */
	u8 get_cmds_parsed:1;		/* 1 if GET cmds subtag parsed */
	u8 lanip_fetch:1;		/* 1 if lanip config needs fetching */
	u8 new_conn:1;			/* 1 to use a new connection */
	u16 recved_len;			/* length of recved from ADS so far */
	/* prop update from MCU callback function */
	enum ada_err (*prop_send_cb)(enum prop_cb_status, void *);
	void *prop_send_cb_arg;		/* arg for the prop_send_cb */
	u32 connect_time;		/* time of last connection */
	u32 get_cmds_time;		/* time of last complete get cmds */
	u32 get_cmds_limit;		/* max interval between get cmds */
	u8 health_check_en;		/* 1 if health check is enabled */
	u8 retries;
	u8 auth_fails;			/* authorization failures */
	u8 dest_mask;			/* dest mask for update from host */
	u8 failed_dest_mask;		/* failed dests for prop update */
	u8 valid_dest_mask;		/* mask of all the valid dests */
	u8 old_dest_mask;		/* from last connectivity update */

	/*
	 * Queues for requests.
	 */
	struct callback *current_request; /* callback in progress */
	struct callback_queue callback_queue[CQP_COUNT];

	/*
	 * cmd being received from service.
	 */
	struct {
		u32 id;			/* id of the command */
		char method[16];	/* reverse-REST method req on device */
		u8 overflow;		/* command + data overflowed res_data */
		char res_data[512];	/* resource + data of the command */
		char *data;		/* where data starts in res_data */
		char *resource;		/* where resource starts in res_data */
		char uri[PROP_LOC_LEN];	/* uri to put the result */
		u32 output_len;		/* length of the output sent so far */
	} cmd;

	struct {
		enum {
			LCS_DISABLED = 0,	/* logging disabled */
			LCS_IDLE,		/* nothing to send */
			LCS_WAIT_DELAY,		/* waiting to start sending */
			LCS_SEND_START,		/* starting to send */
			LCS_SENDING_LOW,	/* sending at low priority */
			LCS_SENDING_HIGH,	/* sending at low priority */
		} lc_state;
		/* flags maintained separately for atomic thread signaling */
		u8 enabled;	/* enabled from cloud */
		u8 notified;	/* have been notified of pending logs */
		u8 push;	/* have been notified buffer near full */
		u8 loop_cnt;	/* # the times we've had more to send */
		struct log_buf_ctxt *ctxt; /* log buffer context */
		struct callback callback;
		struct timer timer;
	} logging;

	struct {
		char host[65];
		char *uri;
		u8 ssl;
		u8 lan;
		u8 remote;
		u16 port;
	} ota_server;

	struct {
		enum {
			COS_NONE = 0,	/* nothing going on */
			COS_NOTIFIED,	/* notified OTA driver */
			COS_STALL,	/* can't deliver more yet */
			COS_IN_PROG,	/* downloading patch */
			COS_CMD_STATUS,	/* send reverse-REST status */
		} in_prog;
		u8 data_recvd:1;	/* data received for current GET */
		u8 url_fetched:1;
		u8 auth_fail:1;		/* set to 1 if we had an OTA unauth */
		u8 retries:3;		/* # times MCU was notified of OTA */
		u8 chunk_retries:3;	/* times we tried to fetch a chunk */
		u8 pad;			/* padding length discarded at end */
		u16 http_status;	/* reverse-REST status */
		u32 prev_off;
		u32 off;
		struct ada_ota_info info; /* information about the OTA */
		u8 *img_sign;	/* SHA-256 signature of LAN OTA image */
		struct al_aes_ctxt *aes_ctx;
		struct al_hash_sha256_ctxt *sha_ctx;
		struct recv_payload recv_buf; /* Decrypted LAN OTA data */
	} ota;

	const struct ada_ota_ops *ota_ops[OTA_TYPE_CT];

	/*
	 * Status for OTA command.
	 * This can be given independently from an ongoing download.
	 * If it relates to the current download, it'll be given after the put.
	 */
	struct {
		u8 status;		/* OTA status code (may be zero) */
		enum ada_ota_type type;
	} ota_status;

	u16 conf_port;			/* dest port of server conn */
	size_t buf_len;			/* length used in buf */
	char buf[CLIENT_BUF_SIZE];
	char auth_hdr[HTTP_CLIENT_AUTH_LINE_LEN];   /* auth header value */
	struct http_client http_client;
	struct server_req cmd_req;

	enum client_http_req request;

#ifdef AYLA_METRICS_SUPPORT
	u16 metric_idx;			/* current bucket being uploaded */
#endif

#ifdef AYLA_LAN_SUPPORT
	struct client_lan_reg *http_lan; /* LAN for HTTP or NULL */
	struct client_lan_reg *lan_cmd_responder;

	struct {
		u8 lanip_random_key[CLIENT_LANIP_KEY_SIZE];
	} lanip;
#endif

	struct prop_recvd *echo_prop;	/* prop structure to echo */
	u8 echo_dest_mask;		/* dest mask of the echo */
	u8 sync_time;			/* enable sync time */

	struct http_client *cont_recv_hc;
	struct callback next_step_cb;	/* cb for next req */
	size_t long_val_index;
	u32 time_sync_time;		/* monotonic time of last time sync */

	struct timer cmd_timer;
	struct timer listen_timer;
	struct timer poll_timer;
	struct timer req_timer;
	struct timer sync_time_timer; /* timer for syncing time over HTTPS */
	struct timer template_timer;
#ifdef AYLA_LAN_SUPPORT
	struct timer lan_reg_timer;
#endif

	struct client_event_handler *event_head;
	u32 backoff_prev_base;	/* Backoff time on HTTP status 429&503 retry */
#if defined(AYLA_LAN_SUPPORT) || defined(AYLA_LOCAL_CONTROL_SUPPORT)
	void (*lanip_cb)(void);
#endif

#ifdef AYLA_BATCH_PROP_SUPPORT
	struct prop *batch_prop; /* currently sending batch-property */
	struct batch_dps_rsp dps_rsp_ctx;
#endif

	struct ame_state ame;
	char ame_buf[AME_MAX_TEXT];	/* for sending properties */

	void (*cmd_exec_func)(const char *); /* remote command handler */
};

struct client_event_handler {
	void (*handler)(void *arg, enum ada_err);
	void *arg;
	struct client_event_handler *next;
};

extern struct callback ada_conf_reset_cb;
extern u8 ada_conf_reset_factory;

extern struct client_state client_state;
extern struct prop_recvd prop_recvd;		/* incoming property */
extern u8 client_ame_parsed_buf[CLIENT_AME_PARSE_BUF_LEN];
extern struct ame_kvp client_ame_stack[50];

extern const struct ame_tag client_ame_cmds[];
extern const struct ame_tag client_ame_prop[];
extern const char *ada_host_template_version;	/* template version from host */

#define CLIENT_HAS_KEY(a) ((a)->client_key[0] != '\0')

void client_tcp_recv_done(struct client_state *);
enum ada_err client_recv_drop(struct http_client *hc, void *buf, size_t len);
void client_wait(struct client_state *, u32 delay);
struct http_client *client_req_new(enum client_connect_target tgt);
struct http_client *client_req_ads_new(void);
void client_req_start(struct http_client *hc, enum http_client_method method,
		const char *resource, const struct http_hdr *header);
enum ada_err client_req_send(struct http_client *, const void *buf, size_t len);
void client_req_send_complete(struct http_client *);
void client_req_abort(struct http_client *);

void client_wakeup(void);
void client_get_dev_id_pend(struct client_state *state);

void client_prop_init(struct client_state *);
enum ada_err client_prop_send_continue(struct client_state *state, void *arg);
enum ada_err client_prop_send_done(struct client_state *, u8 success,
				void *, u8 dest, struct http_client *);
enum ada_err client_recv_prop_done(struct http_client *);

enum ada_err client_prop_cmds_recv(struct http_client *, void *, size_t);
enum ada_err client_recv_prop_val(struct http_client *, void *, size_t);
enum ada_err client_recv_prop_cmds(struct http_client *, void *, size_t);
enum ada_err client_recv_cmds(struct http_client *, void *, size_t);
enum ada_err client_recv_ame(struct http_client *hc, void *buf, size_t len);
enum ada_err client_prop_set(struct prop_recvd *);

void prop_page_json_get_one(struct server_req *);
void conf_json_get(struct server_req *);
void conf_json_put(struct server_req *);

int client_prop_name(const struct ame_decoder *dec,
		void *arg, struct ame_kvp *kvp);
int client_prop_val(const struct ame_decoder *dec,
		void *arg, struct ame_kvp *kvp);

int client_put_ota_status(struct client_state *);
int client_ota_fetch_image(struct client_state *);
void client_ota_set_sts_rpt(struct client_state *, u16 sts);
void client_ota_save_done(struct client_state *);
void client_ota_server(struct client_state *);
void client_ota_cleanup(struct client_state *);
#ifndef ADA_BUILD_OTA_LEGACY
void ada_ota_report_int(enum patch_state status);
#else
void ada_ota_report_int(enum ada_ota_type, enum patch_state);
#endif

void client_conf_reg_persist(void);
int client_conf_pub_key_set_base64(const char *key_base64);

/*
 * Temporary functions planned to become static again.
 */
void client_finish_echo(struct client_state *, u8 finished_id);
void client_send_post(struct http_client *);
void client_connectivity_update(void);

/*
 * Put head of reverse-REST response.
 */
void client_cmd_put_head(struct server_req *, unsigned int status,
			const char *content_type);

/*
 * Start reverse-REST command.
 */
void client_rev_rest_cmd(struct http_client *, u8 priv);

#ifdef AYLA_METRICS_SUPPORT
void client_metrics_init(void);
int client_metrics_post(struct client_state *);
#endif

/*
 * In-field provisioning.
 */
void client_ifp_get_dsn(struct client_state *state);

int log_client_post_logs(struct client_state *state);

const char *client_conn_state_string(enum client_conn_state conn_state);

void om_init(void);

#endif /* __AYLA_CLIENT_INT_H__ */
