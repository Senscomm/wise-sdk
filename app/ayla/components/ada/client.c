/*
 * Copyright 2011-2022 Ayla Networks, Inc.  All rights reserved.
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
#include <ayla/base64.h>
#include <ayla/log.h>
#include <ayla/mod_log.h>
#include <ayla/http.h>
#include <ayla/tlv.h>
#include <ayla/ayla_proto_mcu.h>
#include <ayla/conf.h>
#include <ayla/clock.h>
#include <ayla/uri_code.h>
#include <ayla/nameval.h>
#include <ayla/parse.h>
#include <ayla/json.h>
#include <ayla/patch.h>
#include <ayla/timer.h>
#include <jsmn.h>

#include <al/al_net_if.h>
#include <ayla/wifi_error.h>
#include <ayla/wifi_status.h>
#include <ayla/jsmn_get.h>
#include <al/al_os_mem.h>
#include <al/al_net_dns.h>
#include <al/al_net_sntp.h>
#include <al/al_random.h>
#include <ada/prop.h>
#include <ada/server_req.h>
#include <ada/ada_conf.h>
#include <ada/client.h>
#include "client_req.h"
#include <ayla/ipaddr_fmt.h>
#include <ada/client_ota.h>
#include <ada/prop_mgr.h>
#include <ada/linker_text.h>
#include <ada/ada_wifi.h>
#include "client_common.h"
#include "client_int.h"
#include "client_lock.h"
#include "client_timer.h"
#include "test_svc_int.h"
#include "build.h"

#include "http2mqtt_client.h"
#ifdef AYLA_BATCH_PROP_SUPPORT
#include "ame.h"
#include "client_ame.h"
#include <ada/batch.h>
#include "batch_int.h"
#ifdef AYLA_BATCH_ADS_UPDATES
#include "client_batch.h"
#endif /* AYLA_BATCH_ADS_UPDATES */
#endif
#ifdef AYLA_LOCAL_CONTROL_SUPPORT
#include <ada/local_control.h>
#endif

/*
 * Flags for client info updates.
 */
#define CLIENT_UPDATE_MAJOR		(1 << 0)
#define CLIENT_UPDATE_MINOR		(1 << 1)
#define CLIENT_UPDATE_SWVER		(1 << 2)
#define CLIENT_UPDATE_LANIP		(1 << 3)
#define CLIENT_UPDATE_MODEL		(1 << 4)
#define CLIENT_UPDATE_SETUP		(1 << 5)
#define CLIENT_UPDATE_OEM		(1 << 6)
#define CLIENT_UPDATE_SSID		(1 << 7)
#define CLIENT_UPDATE_SETUP_LOCATION	(1 << 8)
#define CLIENT_UPDATE_PRODUCT_NAME	(1 << 9)
#define CLIENT_UPDATE_MAC		(1 << 10)
#define CLIENT_UPDATE_HW_ID		(1 << 11)
#define CLIENT_UPDATE_ALL	(CLIENT_UPDATE_MAJOR | CLIENT_UPDATE_MINOR | \
				CLIENT_UPDATE_SWVER | CLIENT_UPDATE_LANIP | \
				CLIENT_UPDATE_SSID | \
				CLIENT_UPDATE_MODEL | CLIENT_UPDATE_SETUP | \
				CLIENT_UPDATE_MAC | \
				CLIENT_UPDATE_HW_ID | \
				CLIENT_UPDATE_SETUP_LOCATION)

#ifndef BUILD_SDK
#define BUILD_SDK "bc"
#endif

#ifdef BUILD_VERSION
const char ada_version_build[] = "ADA " ADA_VERSION BUILD_NAME " " BUILD_SDK
#ifdef SDK_VERSION
			"-" SDK_VERSION
#endif /* SDK_VERION */

			" " BUILD_DATE " " BUILD_TIME " "
#ifdef BUILD_ENV
			BUILD_ENV "/"
#endif /* BUILD_ENV */
			BUILD_VERSION
#ifdef CHIP_HASH
			":" CHIP_HASH
#endif /* CHIP_HASH*/
			;
#else
const char ada_version_build[] = "ADA-" BUILD_SDK " " ADA_VERSION BUILD_NAME;
#endif /* BUILD_VERSION */
const char ada_version[] = "ADA-" BUILD_SDK " " ADA_VERSION BUILD_NAME;

struct client_state client_state;
u8 client_ame_parsed_buf[CLIENT_AME_PARSE_BUF_LEN];
struct ame_kvp client_ame_stack[50];
static const char *client_conn_states[] = CLIENT_CONN_STATES;

static void client_down_locked(u8 ip_down, u8 lc_down);
static void client_start(struct client_state *, struct http_client *);
static void client_commit_server(struct client_state *state);

static void client_send_next(struct http_client *, enum ada_err);
static void client_err_cb(struct http_client *);

static void client_get_cmds(struct client_state *);
static int client_put_info(struct client_state *);

#if defined(AYLA_LAN_SUPPORT) || defined(AYLA_LOCAL_CONTROL_SUPPORT)
static int client_get_lanip_key(struct client_state *);
#endif

static void client_mqtt_ssl_close(struct http_client *hc);

static int client_put_reg_window_start(struct client_state *);
static void client_cmd_put_rsp(struct client_state *, unsigned int status);
static void client_cmd_put(struct client_state *);
static void client_cmd_flush(struct server_req *, const char *);
static u8 tlv_get_bool(struct prop *prop);
static s32 tlv_get_int(struct prop *prop);
static u32 tlv_get_uint(struct prop *prop);
static char *tlv_get_utf8(struct prop *prop);

#ifdef AYLA_BATCH_PROP_SUPPORT
static enum ada_err client_send_batch_dp(struct prop *prop);
#endif

enum client_cb_use {
	CCB_CONN_UPDATE,
	CCB_LANIP,
	CCB_GET_DEV_ID,
	CCB_COUNT		/* count of callbacks.  must be last */
};

#ifdef AYLA_LOCAL_SERVER
/*
 * Handlers for local web pages.
 */
static void client_json_regtoken_get(struct server_req *);
static void client_json_status_get(struct server_req *);
static void client_json_time_get(struct server_req *);
static void client_json_time_put(struct server_req *);
#endif /* AYLA_LOCAL_SERVER */

/*
 * Handlers used in reverse-REST from ADS.
 */
static void client_cli_put(struct server_req *req);
static void client_json_getdsns_put(struct server_req *);
static void client_lanip_json_put(struct server_req *);

static struct callback client_cb[CCB_COUNT];

static void client_connectivity_update_cb(void *);
#if defined(AYLA_LAN_SUPPORT) || defined(AYLA_LOCAL_CONTROL_SUPPORT)
static void client_lanip_save(void *);
#endif
static void client_get_dev_id(void *);

struct client_cb_handler {
	void (*func)(void *);
};

/*
 * Initialization table for client callbacks in this file.
 */
static const struct client_cb_handler client_cb_handlers[] = {
	[CCB_CONN_UPDATE] = { .func = client_connectivity_update_cb },
#if defined(AYLA_LAN_SUPPORT) || defined(AYLA_LOCAL_CONTROL_SUPPORT)
	[CCB_LANIP] = { .func = client_lanip_save },
#endif
	[CCB_GET_DEV_ID] = { .func = client_get_dev_id },
};

/*
 * Client next step handlers.
 */
#ifdef AYLA_FILE_PROP_SUPPORT
static int client_file_step(struct client_state *);
#endif
static int client_cmd_put_step(struct client_state *);
static int client_cmd_step(struct client_state *);
static int client_put_cmd_sts(struct client_state *);
static int client_post_echo(struct client_state *);

u32 client_step_mask = ~(
    BIT(ADCP_REG_WINDOW_START)
#ifdef AYLA_METRICS_SUPPORT
    | BIT(ADCP_POST_METRICS)
#endif
    );

static const struct client_step client_steps[ADCP_REQ_COUNT] = {
#ifdef AYLA_FILE_PROP_SUPPORT
	[ADCP_PUT_FILE_PROP] = { .handler = client_file_step },
#endif
	[ADCP_PUT_INFO] = { .handler = client_put_info },
	[ADCP_PUT_OTA_STATUS] = { .handler = client_put_ota_status },
	[ADCP_CMD_PUT] = { .handler = client_cmd_put_step },
	[ADCP_CMD_GET] = { .handler = client_cmd_step },
	[ADCP_POST_RESP] = { .handler = client_put_cmd_sts },
#if defined(AYLA_LAN_SUPPORT) || defined(AYLA_LOCAL_CONTROL_SUPPORT)
	[ADCP_LAN_REQ] = { .handler = client_get_lanip_key },
#endif
	[ADCP_REG_WINDOW_START] = { .handler = client_put_reg_window_start },
#ifdef AYLA_LAN_SUPPORT
	[ADCP_LAN_CYCLE] = { .handler = client_lan_cycle },
#endif
	[ADCP_POST_LOGS_HIGH] = { .handler = log_client_post_logs },
	[ADCP_OTA_FETCH] = { .handler = client_ota_fetch_image },
	[ADCP_POST_ECHO] = { .handler = client_post_echo },
#ifdef AYLA_METRICS_SUPPORT
	[ADCP_POST_METRICS]  = { .handler = client_metrics_post },
#endif
	[ADCP_POST_LOGS_LOW] = { .handler = log_client_post_logs },
};

void client_log(const char *fmt, ...)
{
	ADA_VA_LIST args;

	ADA_VA_START(args, fmt);
	log_put_va(MOD_LOG_CLIENT, fmt, args);
	ADA_VA_END(args);
}

/*
 * Append HTTP URI arg to URI string in buffer.
 */
static void client_arg_add(char *uri, size_t uri_len, const char *fmt, ...)
	ADA_ATTRIB_FORMAT(3, 4);

static void client_arg_add(char *uri, size_t uri_len, const char *fmt, ...)
{
	ADA_VA_LIST args;
	size_t len;

	len = strlen(uri);
	if (len >= uri_len - 5) {	/* need room for at least "?x=y\0" */
		client_log(LOG_ERR
		    "arg_append: uri '%s' too long to append '%s'", uri, fmt);
#ifdef DEBUG /* don't crash release builds for this */
		ASSERT_NOTREACHED();
#endif /* DEBUG */
		return;
	}
	uri[len++] = (strchr(uri, '?') != NULL ? '&' : '?');
	ADA_VA_START(args, fmt);
	vsnprintf(uri + len, uri_len - len, fmt, args);
	ADA_VA_END(args);
}

/*
 * Maps server hostnames to region for both dev and OEM
 */
struct hostname_info {
	const char *region;		/* unique numeric region code */
	const char *domain;		/* region-specific domain name */
	const char *ntp_server;		/* region-specific NTP server */
};

/*
 * Update this table to support new regions and server hostnames
 */
static const struct hostname_info server_region_table[] = {
	{ "US", "aylanetworks.com", "pool.ntp.aylanetworks.com" },
	{ "CN", "ayla.com.cn", "pool.ntp.ayla.com.cn" },
};
/* First hostname table entry is the default */
static const struct hostname_info *SERVER_HOSTINFO_DEFAULT =
	server_region_table;
static const char *client_ntp_server;

/*
 * Register for callback when ADS reachability changes, or a new
 * connection attempt fails.
 * This callback is made inside the client thread, and must not block.
 * Multiple callbacks may be registered, and they'll all be called.
 * Callbacks may not be unregistered for now.
 */
void ada_client_event_register(void (*fn)(void *arg, enum ada_err), void *arg)
{
	struct client_state *state = &client_state;
	struct client_event_handler *hp;

	hp = al_os_mem_calloc(sizeof(*hp));
	ASSERT(hp);
	client_lock();
	hp->arg = arg;
	hp->handler = fn;
	hp->next = state->event_head;
	state->event_head = hp;
	client_unlock();
}

#if defined(AYLA_LAN_SUPPORT) || defined(AYLA_LOCAL_CONTROL_SUPPORT)
void ada_client_lanip_cb_register(void (*fn)(void))
{
	struct client_state *state = &client_state;

	/* currently only supports one callback */
	ASSERT(!state->lanip_cb);
	state->lanip_cb = fn;
}
#endif

/*
 * Send event (change in ADA connectivity status) with error code.
 * Handlers are called with the client_lock held, and must not block.
 */
static void client_event_send(enum ada_err err)
{
	struct client_state *state = &client_state;
	struct client_event_handler *hp;

	ASSERT(client_locked);
	for (hp = state->event_head; hp; hp = hp->next) {
		hp->handler(hp->arg, err);
	}
}

/*
 * Register a function to execute remote CLI commands sent to the device.
 */
void ada_client_command_func_register(void (*func)(const char *command))
{
	struct client_state *state = &client_state;

	state->cmd_exec_func = func;
}

/*
 * Lookup an entry in the server_region_table by region code
 * Returns NULL if code is invalid.
 */
static const struct hostname_info *client_lookup_host(const char *region)
{
	int i;

	if (!region) {
		return NULL;
	}
	for (i = 0; i < ARRAY_LEN(server_region_table); ++i) {
		if (!strcasecmp(region, server_region_table[i].region)) {
			return server_region_table + i;
		}
	}
	return NULL;
}

/*
 * Update the connectivity status to the property managers.
 *
 * A callback is used to update the connectivity status so that the
 * lock can be safely dropped.  The prop_mgrs may call client functions
 * during the status update.
 */
static void client_connectivity_update_cb(void *arg)
{
	struct client_state *state = arg;

	state->current_request = NULL;
	if ((state->valid_dest_mask ^ state->old_dest_mask) & NODES_ADS) {
		/* ADS connectivity state changed */
		if (state->valid_dest_mask & NODES_ADS) {
			if (!state->client_info_flags) {
				client_event_send(AE_OK);
			}
		} else {
			client_event_send(AE_NOTCONN);
		}
	}
	state->old_dest_mask = state->valid_dest_mask;
	client_unlock();
	prop_mgr_connect_sts(state->valid_dest_mask);
	client_lock();
}

u8 client_valid_dest_mask(void)
{
	struct client_state *state = &client_state;

	return state->valid_dest_mask;
}

void client_connectivity_update(void)
{
	struct client_state *state = &client_state;

	callback_enqueue(&state->callback_queue[CQP_HIGH],
	    &client_cb[CCB_CONN_UPDATE]);
	client_wakeup();
}

static int client_cmd_id(const struct ame_decoder *dec,
		void *arg, struct ame_kvp *kvp)
{
	struct client_state *state = &client_state;
	u32 val;
	enum ada_err err;

	err = dec->get_u32(kvp, NULL, &val);

	if (state->cmd_pending || state->ota.in_prog != COS_NONE) {
		return 0;
	}

	if (!err) {
		state->cmd.id = val;
	}
	return 0;
}

/*
 * Set resource or data string for reverse-REST command, using res_data buffer.
 * Add the prefix string as well.  Check for overflow.
 */
static char *client_cmd_add_res_data(const char *prefix, const char *str,
    size_t off)
{
	struct client_state *state = &client_state;
	int len;

	len = snprintf(state->cmd.res_data + off,
	    sizeof(state->cmd.res_data) - off,  "%s%s", prefix, str);
	if (len + off >= sizeof(state->cmd.res_data)) {
		client_log(LOG_ERR "cmd data + resource too long");
		state->cmd.overflow = 1;
		return NULL;
	}
	return state->cmd.res_data + off;
}

static int client_cmd_data(const struct ame_decoder *dec,
		void *arg, struct ame_kvp *kvp)
{
	struct client_state *state = &client_state;
	int off = 0;
	char buf[512];
	enum ada_err err;
	size_t length = sizeof(buf);

	if (state->cmd_pending || state->ota.in_prog != COS_NONE) {
		return 0;
	}

	err = dec->get_utf8(kvp, NULL, buf, &length);
	if (!err) {
		if (state->cmd.resource) {
			off = strlen(state->cmd.resource) + 1;
		}
		state->cmd.data = client_cmd_add_res_data("", buf, off);
	}
	return 0;
}

static int client_cmd_method(const struct ame_decoder *dec,
		void *arg, struct ame_kvp *kvp)
{
	struct client_state *state = &client_state;
	enum ada_err err;
	size_t len;

	if (state->cmd_pending || state->ota.in_prog != COS_NONE) {
		return 0;
	}

	len = sizeof(state->cmd.method);
	err = dec->get_utf8(kvp, NULL, state->cmd.method, &len);
	if (err) {
		state->cmd.method[0] = '\0';
		return 0;
	}
	return 0;
}

static int client_cmd_resource(const struct ame_decoder *dec,
		void *arg, struct ame_kvp *kvp)
{
	struct client_state *state = &client_state;
	int off = 0;
	char buf[512];
	enum ada_err err;
	size_t length = sizeof(buf);

	if (state->cmd_pending || state->ota.in_prog != COS_NONE) {
		return 0;
	}

	err = dec->get_utf8(kvp, NULL, buf, &length);
	if (!err) {
		if (state->cmd.data) {
			off = strlen(state->cmd.data) + 1;
		}
		state->cmd.resource =
		    client_cmd_add_res_data("/", buf, off);
	}
	return 0;
}

static int client_cmd_uri(const struct ame_decoder *dec,
		void *arg, struct ame_kvp *kvp)
{
	struct client_state *state = &client_state;
	enum ada_err err;
	size_t len;

	if (state->cmd_pending || state->ota.in_prog != COS_NONE) {
		return 0;
	}

	len = sizeof(state->cmd.uri);
	err = dec->get_utf8(kvp, NULL, state->cmd.uri, &len);
	if (err) {
		state->cmd.uri[0] = '\0';
		return 0;
	}
	return 0;
}

/*
 * No-op finish_write() handler for a completion of OTA download.
 */
static enum ada_err client_cmd_finish_ota(struct server_req *req)
{
	return AE_OK;
}

static int client_accept_cmd(const struct ame_decoder *dec,
		void *arg, struct ame_kvp *kvp)
{
	struct client_state *state = &client_state;
	struct server_req *cmd_req = &state->cmd_req;
	char *resource = state->cmd.resource;
	const char *method = state->cmd.method;

	if (state->ota.in_prog != COS_NONE) {
		return 0; /* Later */
	}
	if (state->cmd_pending) {
		state->cmd_event = 1;
		return 0;	/* Only execute 1 cmd per get */
	}
	CLIENT_DEBUG(LOG_DEBUG,
	    "id=%lu method=%s resource=%s, uri=%s",
	    state->cmd.id, state->cmd.method, state->cmd.resource,
	    state->cmd.uri);
	CLIENT_DEBUG(LOG_DEBUG2, "data \"%s\"", state->cmd.data);
	/* XXX TBD handle OTA like other commands later */
	if (!strcmp(method, "PUT") && !strcmp(resource, "/ota.json") &&
	    !state->cmd.overflow) {
		server_req_init(cmd_req);
		cmd_req->suppress_out = 1;
		cmd_req->post_data = state->cmd.data;

		cmd_req->put_head = client_cmd_put_head;
		cmd_req->write_cmd = client_cmd_flush;
		cmd_req->finish_write = client_cmd_finish_ota;

		client_ota_json_put(cmd_req);
		return 0;
	}
	state->cmd_pending = 1;

	return 0;
}

static int client_accept_cmds(const struct ame_decoder *dec,
		void *arg, struct ame_kvp *kvp)
{
	struct client_state *state = &client_state;

	state->get_cmds_parsed = 1;
	return 0;
}

#ifdef AYLA_FILE_PROP_SUPPORT
static int client_accept_dp_url(const struct ame_decoder *dec,
		void *arg, struct ame_kvp *kvp)
{
	struct prop_recvd *prop = &prop_recvd;
	char buf[TLV_MAX_STR_LEN / 2];
	enum ada_err err;
	size_t len = sizeof(buf);

	err = dec->get_utf8(kvp, NULL, buf, &len);

	if (!err) {
		/*
		 * store the URL in prop->val
		 */
		strncpy(prop->file_info.file, buf,
		    sizeof(prop->file_info.file) - 1);
		prop->is_file = 1;
	}
	return 0;
}

static int client_accept_dp_loc(const struct ame_decoder *dec,
		void *arg, struct ame_kvp *kvp)
{
	struct prop_recvd *prop = &prop_recvd;
	char buf[TLV_MAX_STR_LEN / 2];
	enum ada_err err;
	size_t len = sizeof(buf);

	err = dec->get_utf8(kvp, NULL, buf, &len);

	if (!err) {
		strncpy(prop->file_info.location, buf,
		    sizeof(prop->file_info.location) - 1);
		prop->is_file = 1;
	}
	return 0;
}

static int client_accept_dp(const struct ame_decoder *dec,
		void *arg, struct ame_kvp *kvp)
{
	struct client_state *state = &client_state;
	struct prop_recvd *prop = &prop_recvd;

	if (state->request == CS_GET_DP_LOC) {
		if (!prop->file_info.file[0]) {
			goto missing_info;
		}
		state->wait_for_file_get = 1;
		return 0;
	}
	if (!prop->file_info.location[0] ||
	    (!prop->file_info.file[0] && prop->type == ATLV_LOC)) {
missing_info:
		CLIENT_LOGF(LOG_WARN, "missing loc or file info");
		return AE_INVAL_VAL;
	}
	/*
	 * host mcu must immediately
	 * do a PUT after. client will not do any other operations
	 * until the host mcu does a PUT for the file property.
	 * If the host MCU tries to do any other op, a NAK will be
	 * returned (UNEXPECTED_OP).
	 */
	state->prop_send_cb_arg = prop->file_info.location;
	state->wait_for_file_put = 1;

	return 0;
}
#endif

/*
 * Parse a string from JSON into a buffer, checking for length.
 */
static int client_parse_string(const char *field, char *buf, size_t buf_len,
		const struct ame_decoder *dec, struct ame_kvp *kvp)
{
	enum ada_err err;
	size_t len = buf_len;

	err = dec->get_utf8(kvp, NULL, buf, &len);
	if (err || len >= buf_len) {
		*buf = '\0';
		if (len >= buf_len) {
			client_log(LOG_WARN "%s: len %zd is too long",
			    field, len);
		} else {
			client_log(LOG_WARN "%s: parse error %d", field, err);
		}
		return -1;
	}
	return 0;
}

static int client_api_major(const struct ame_decoder *dec,
		void *arg, struct ame_kvp *kvp)
{
	struct client_state *state = &client_state;
	u32 val;
	enum ada_err err;

	err = dec->get_u32(kvp, NULL, &val);
	if (!err && val == CLIENT_API_MAJOR) {
		state->client_info_flags &= ~CLIENT_UPDATE_MAJOR;
	}
	return 0;
}

static int client_api_minor(const struct ame_decoder *dec,
		void *arg, struct ame_kvp *kvp)
{
	struct client_state *state = &client_state;
	u32 val;
	enum ada_err err;

	err = dec->get_u32(kvp, NULL, &val);
	if (!err && val == CLIENT_API_MINOR) {
		state->client_info_flags &= ~CLIENT_UPDATE_MINOR;
	}
	return 0;
}

void client_clock_set(u32 new_time, enum clock_src src)
{
	u32 now;
	enum clock_src prev_src;
	char buf[24];

	now = clock_utc();
	prev_src = clock_source();

	/*
	 * If the clock source is not better than the previous clock source,
	 * and the time is close enough to the previous time, skip setting it.
	 * It is important that the clock_source be set if it is better.
	 */
	if (src <= prev_src && ABS(new_time - now) <= CLIENT_TIME_FUDGE) {
		return;
	}
	if (clock_set(new_time, src)) {
		return;
	}
	if (new_time != now) {
		clock_fmt(buf, sizeof(buf), now);
		client_log(LOG_INFO "clock was %s UTC", buf);
		clock_fmt(buf, sizeof(buf), new_time);
		client_log(LOG_INFO "clock now %s UTC", buf);
	}
	if (src != prev_src) {
		client_log(LOG_DEBUG2 "clock source was %x now %x",
		    prev_src, src);
	}
	prop_mgr_event(PME_TIME, NULL);
}

static int client_sw_version(const struct ame_decoder *dec,
		void *arg, struct ame_kvp *kvp)
{
	struct client_state *state = &client_state;
	char buf[256];

	if (client_parse_string("version", buf, sizeof(buf), dec, kvp)) {
		return 0;
	}
	if (!strcmp(buf, adap_conf_sw_build())) {
		state->client_info_flags &= ~CLIENT_UPDATE_SWVER;
	}

	return 0;
}

#if defined(AYLA_LAN_SUPPORT) || defined(AYLA_LOCAL_CONTROL_SUPPORT)
static int client_local_access_enabled(const struct ame_decoder *dec,
		void *arg, struct ame_kvp *kvp)
{
	struct client_state *state = &client_state;
	enum ada_err err;
	s32 val;

	err = dec->get_boolean(kvp, NULL, &val);

	/*
	 * If set to "true" one or more of the local control interfaces
	 * is enabled. Set the lanip_fetch flag to indicate the client
	 * should check later if local control configuration (lanip) needs
	 * to be fetched from the service.
	 */
	if (!err && val) {
		state->lanip_fetch = 1;
	}
	return 0;
}
#endif

static int client_setup_location(const struct ame_decoder *dec,
		void *arg, struct ame_kvp *kvp)
{
	struct client_state *state = &client_state;
	char buf[256];

	client_parse_string("setup_location", buf, sizeof(buf), dec, kvp);
	if (state->setup_location == NULL ||
	    !strcmp(buf, state->setup_location)) {
		state->client_info_flags &= ~CLIENT_UPDATE_SETUP_LOCATION;
	}

	return 0;
}

static int client_reg_flag(const struct ame_decoder *dec,
		void *arg, struct ame_kvp *kvp)
{
	struct ada_conf *cf = &ada_conf;
	u32 flag = 0;
	u8 reg_user;
	enum ada_err err;

	err = dec->get_u32(kvp, NULL, &flag);
	if (err) {
		return 0;
	}
	reg_user = flag != 0;
	if (cf->reg_user ^ reg_user) {
		cf->reg_user = reg_user;
		client_conf_reg_persist();
		cf->event_mask &= ~(CLIENT_EVENT_UNREG | CLIENT_EVENT_REG);
		cf->event_mask |=
		    (reg_user ? CLIENT_EVENT_REG : CLIENT_EVENT_UNREG);
		adap_conf_reg_changed();
	}
	return 0;
}

static int client_reg_type(const struct ame_decoder *dec,
		void *arg, struct ame_kvp *kvp)
{
	struct client_state *state = &client_state;

	client_parse_string("reg_type",
	    state->reg_type, sizeof(state->reg_type), dec, kvp);
	return 0;
}

static int client_reg_token(const struct ame_decoder *dec,
		void *arg, struct ame_kvp *kvp)
{
	struct ada_conf *cf = &ada_conf;

	client_parse_string("reg_token",
	    cf->reg_token, sizeof(cf->reg_token), dec, kvp);
	return 0;
}

static int client_lan_ip(const struct ame_decoder *dec,
		void *arg, struct ame_kvp *kvp)
{
	struct client_state *state = &client_state;
	u32 lan_ip;
	u32 local_addr;
	struct al_net_if *nif;
	char buf[64];

	if (client_parse_string("lan_ip", buf, sizeof(buf), dec, kvp)) {
		return 0;
	}
	lan_ip = ipaddr_fmt_str_to_ipv4(buf);

	nif = al_net_if_get(AL_NET_IF_DEF);
	local_addr = (nif) ? al_net_if_get_ipv4(nif) : 0;

	if (lan_ip == local_addr) {
		state->client_info_flags &= ~CLIENT_UPDATE_LANIP;
	}
	return 0;
}

static int client_model(const struct ame_decoder *dec,
		void *arg, struct ame_kvp *kvp)
{
	struct client_state *state = &client_state;
	char buf[256];

	if (client_parse_string("model", buf, sizeof(buf), dec, kvp)) {
		return 0;
	}

	if (!strcmp(buf, conf_sys_model)) {
		state->client_info_flags &= ~CLIENT_UPDATE_MODEL;
	}
	return 0;
}

static int client_oem(const struct ame_decoder *dec,
		void *arg, struct ame_kvp *kvp)
{
	struct client_state *state = &client_state;
	char buf[256];

	if (client_parse_string("oem", buf, sizeof(buf), dec, kvp) ||
	    strcmp(buf, oem)) {
		state->client_info_flags |= CLIENT_UPDATE_OEM;
	}
	return 0;
}

static int client_oem_model(const struct ame_decoder *dec,
		void *arg, struct ame_kvp *kvp)
{
	struct client_state *state = &client_state;
	char buf[256];

	if (client_parse_string("oem_model", buf, sizeof(buf), dec, kvp) ||
	    strcmp(buf, oem_model)) {
		state->client_info_flags |= CLIENT_UPDATE_OEM;
	}
	return 0;
}

static int client_template_version(const struct ame_decoder *dec,
		void *arg, struct ame_kvp *kvp)
{
	struct client_state *state = &client_state;

	if (client_parse_string("template_version", state->template_version,
	    sizeof(state->template_version), dec, kvp)) {
		state->client_info_flags |= CLIENT_UPDATE_OEM;
		return 0;
	}
	if (!ada_host_template_version || !ada_host_template_version[0]) {
		if (!state->template_version_timeout) {
			state->client_info_flags |= CLIENT_UPDATE_OEM;
		}
	} else if (strcmp(state->template_version, ada_host_template_version)) {
		state->client_info_flags |= CLIENT_UPDATE_OEM;
	}
	return 0;
}

/*
 * Get URI-encoded SSID, or empty string if none, into buffer.
 */
static void client_get_ssid_uri(char *buf, size_t len)
{
	char ssid[32];
	int slen;

	slen = adap_wifi_get_ssid(ssid, sizeof(ssid));
	*buf = '\0';
	if (slen > 0) {
		uri_encode(buf, len, ssid, slen, uri_arg_allowed_map);
	}
}

static int client_ssid(const struct ame_decoder *dec,
		void *arg, struct ame_kvp *kvp)
{
	struct client_state *state = &client_state;
	char ssid_uri[SSID_URI_LEN];
	char buf[256];

	if (client_parse_string("ssid", buf, sizeof(buf), dec, kvp)) {
		return 0;
	}
	client_get_ssid_uri(ssid_uri, sizeof(ssid_uri));
	if (!strcmp(buf, ssid_uri)) {
		state->client_info_flags &= ~CLIENT_UPDATE_SSID;
	}

	return 0;
}

static int client_recv_mac_addr(const struct ame_decoder *dec,
		void *arg, struct ame_kvp *kvp)
{
	struct client_state *state = &client_state;
	struct ada_conf *cf = &ada_conf;
	u8 mac[6];
	char buf[256];

	if (client_parse_string("mac", buf, sizeof(buf), dec, kvp)) {
		return 0;
	}
	if (!parse_mac(mac, buf) &&
	    !memcmp(mac, cf->mac_addr, sizeof(mac))) {
		state->client_info_flags &= ~CLIENT_UPDATE_MAC;
	}
	return 0;
}

static int client_recv_hw_id(const struct ame_decoder *dec,
		void *arg, struct ame_kvp *kvp)
{
	struct client_state *state = &client_state;
	struct ada_conf *cf = &ada_conf;
	char buf[256];

	if (client_parse_string("hwid", buf, sizeof(buf), dec, kvp)) {
		return 0;
	}
	if (!strcmp(buf, cf->hw_id)) {
		state->client_info_flags &= ~CLIENT_UPDATE_HW_ID;
	}
	return 0;
}

static void client_listen_warn(struct timer *arg)
{
	struct client_state *state = &client_state;

	if ((state->valid_dest_mask & NODES_ADS) && !state->ads_listen) {
		client_log(LOG_WARN "listen not enabled");
		state->ads_cmds_en = 1;
		client_wakeup();
	}
}

static int client_accept_id(const struct ame_decoder *dec,
		void *arg, struct ame_kvp *kvp)
{
	struct client_state *state = &client_state;
	struct ada_conf *cf = &ada_conf;
	struct prop_recvd *prop = &prop_recvd;

	client_log(LOG_INFO "module name \"%s\" key %s.",
	    prop->name, prop->val);
	strncpy(state->client_key, prop->val, CLIENT_KEY_LEN - 1);
	if ((state->client_info_flags & CLIENT_UPDATE_PRODUCT_NAME) == 0) {
		strncpy(cf->host_symname, prop->name,
		    sizeof(cf->host_symname) - 1);
	}
	return 0;
}

static int client_log_host(const struct ame_decoder *dec,
		void *arg, struct ame_kvp *kvp)
{
	struct client_state *state = &client_state;
	char buf[256];

	if (client_parse_string("log_host", buf, sizeof(buf), dec, kvp)) {
		return 0;
	}
	if (!buf[0]) {
		return 0;
	}

	/*
	 * Note, the host received from the cloud is ignored but its presence
	 * or absence is used to indicate if logging to the server is enabled.
	 */
	state->logging.enabled = 1;
	return 0;
}

static int client_setup_logc(const struct ame_decoder *dec,
		void *arg, struct ame_kvp *kvp)
{
	struct client_state *state = &client_state;

	if (state->logging.enabled) {
		client_log(LOG_DEBUG "logging to service is enabled");
	}

	return 0;
}

/*
 * Allow log client enable/disable through reverse-rest
 */
void client_log_client_json_put(struct server_req *req)
{
	struct client_state *state = &client_state;
	jsmn_parser parser;
	jsmntok_t tokens[LOG_CLIENT_TOKEN_MAX];
	jsmnerr_t err;
	long enable;
	const char logc_str[] = "log-client";
	const char abled[] = "abled";

	client_lock();
	jsmn_init_parser(&parser, req->post_data, tokens, ARRAY_LEN(tokens));
	err = jsmn_parse(&parser);
	if (err != JSMN_SUCCESS) {
		server_log(LOG_WARN "%s jsmn err %d", __func__, err);
		goto inval;
	}
	if (jsmn_get_long(&parser, NULL, "enabled", &enable)) {
		goto inval;
	}
	if (!enable) {
		server_log(LOG_INFO "%s dis%s", logc_str, abled);
		state->logging.enabled = 0;
		log_client_enable(0);
		goto no_content;
	}
	state->logging.enabled = 1;
	log_client_enable(1);
	server_log(LOG_INFO "%s en%s", logc_str, abled);
no_content:
	server_put_status(req, HTTP_STATUS_NO_CONTENT);
	client_unlock();
	return;

inval:
	server_put_status(req, HTTP_STATUS_BAD_REQ);
	client_unlock();
}

static const struct ame_tag client_ame_cmd_tags[] = {
	AME_TAG("id", NULL, client_cmd_id),
	AME_TAG("data", NULL, client_cmd_data),
	AME_TAG("method", NULL, client_cmd_method),
	AME_TAG("resource", NULL, client_cmd_resource),
	AME_TAG("uri", NULL, client_cmd_uri),
	AME_TAG(NULL, NULL, NULL)
};

static const struct ame_tag client_ame_cmd_inner[] = {
	AME_TAG("cmd", client_ame_cmd_tags, NULL),
	AME_TAG(NULL, NULL, NULL)
};

static const struct ame_tag client_ame_cmd[] = { /* unnamed outer object */
	AME_TAG("", client_ame_cmd_inner, client_accept_cmd),
	AME_TAG(NULL, NULL, NULL)
};

#ifdef AYLA_FILE_PROP_SUPPORT
static const struct ame_tag client_ame_dp_inner[] = {
	AME_TAG("location", NULL, client_accept_dp_loc),
	AME_TAG("file", NULL, client_accept_dp_url),
	AME_TAG(NULL, NULL, NULL)
};

static const struct ame_tag client_ame_dp_outer[] = {
	AME_TAG("datapoint", client_ame_dp_inner, NULL),
	AME_TAG_END
};

static const struct ame_tag client_ame_dp[] = {
	AME_TAG("", client_ame_dp_outer, client_accept_dp),
	AME_TAG_END
};
#endif

static const struct ame_tag client_ame_props_cmds[] = {
	AME_TAG("properties", client_ame_prop, NULL),
	AME_TAG("cmds", client_ame_cmd, client_accept_cmds),
	AME_TAG("schedules", client_ame_prop, NULL),
	AME_TAG(NULL, NULL, NULL)
};

const struct ame_tag client_ame_cmds_inner[] = {
	AME_TAG("commands", client_ame_props_cmds, NULL),
	AME_TAG(NULL, NULL, NULL)
};

const struct ame_tag client_ame_cmds[] = {
	AME_TAG("", client_ame_cmds_inner, NULL), /* unnamed outer object */
	AME_TAG(NULL, NULL, NULL)
};

static const struct ame_tag client_ame_cmds_only_inner[] = {
	AME_TAG("cmds", client_ame_props_cmds, NULL),
	AME_TAG(NULL, NULL, NULL)
};

const struct ame_tag client_ame_cmds_only[] = {
	AME_TAG("", client_ame_cmds_only_inner, NULL),	/* unnamed outer obj */
	AME_TAG(NULL, NULL, NULL)
};

static const struct ame_tag client_ame_log_server_props[] = {
	AME_TAG("host", NULL, client_log_host),
	AME_TAG(NULL, NULL, NULL)
};

static const struct ame_tag client_ame_dev_props[] = {
	AME_TAG("product_name", NULL, client_prop_name),
	AME_TAG("api_major", NULL, client_api_major),
	AME_TAG("api_minor", NULL, client_api_minor),
	AME_TAG("hwsig", NULL, client_recv_hw_id),
	AME_TAG("sw_version", NULL, client_sw_version),
	AME_TAG("lan_ip", NULL, client_lan_ip),
	AME_TAG("mac", NULL, client_recv_mac_addr),
	AME_TAG("model", NULL, client_model),
	AME_TAG("oem", NULL, client_oem),
	AME_TAG("oem_model", NULL, client_oem_model),
	AME_TAG("template_version", NULL, client_template_version),
	AME_TAG("key", NULL, client_prop_val),
	AME_TAG("registered", NULL, client_reg_flag),
	AME_TAG("registration_type", NULL, client_reg_type),
	AME_TAG("regtoken", NULL, client_reg_token),
	AME_TAG("ssid", NULL, client_ssid),
	AME_TAG("log_server", client_ame_log_server_props,
	    client_setup_logc),
#if defined(AYLA_LAN_SUPPORT) || defined(AYLA_LOCAL_CONTROL_SUPPORT)
	/*
	 * lan-enabled is deprecated. It is here for backward compatibility
	 * to older cloud deployments
	 */
	AME_TAG("lan_enabled", NULL, client_local_access_enabled),
	AME_TAG("local_access_enabled", NULL, client_local_access_enabled),
#endif
	AME_TAG("setup_location", NULL, client_setup_location),
	AME_TAG(NULL, NULL, NULL)
};

static const struct ame_tag client_ame_device[] = {
	AME_TAG("device", client_ame_dev_props, client_accept_id),
	AME_TAG(NULL, NULL, NULL)
};

static const struct ame_tag client_ame_id[] = {
	AME_TAG("", client_ame_device, NULL),	/* unnamed outer object */
	AME_TAG(NULL, NULL, NULL)
};


/*
 * Returns a mask of the failed destinations
 */
u8 client_get_failed_dests(void)
{
	struct client_state *state = &client_state;

	return state->failed_dest_mask;
}

void client_req_abort(struct http_client *hc)
{
	struct client_state *state = &client_state;

	ASSERT(client_locked);
	ASSERT(hc == &state->http_client);

	client_timer_cancel(&state->req_timer);
	if (hc->is_mqtt) {
		http2mqtt_client_abort(hc);
		client_mqtt_ssl_close(hc);
	} else {
		http_client_abort(hc);
	}
	state->request = CS_IDLE;
}

/*
 * Close http_client request and make sure we're not called back.
 */
static void client_close(struct client_state *state)
{
	struct http_client *hc = &state->http_client;

	client_req_abort(hc);
}

static void client_retry_after(struct client_state *state, u32 delay_ms)
{
	if (state->retries < 255) {
		state->retries++;
	}
	client_wait(state, delay_ms);
}

static void client_retry(struct client_state *state)
{
	struct http_client *hc = &state->http_client;
	u32 delay_ms;

	client_close(state);
	state->conn_state = CS_WAIT_RETRY;

	if (state->retries < 1) {
		delay_ms = CLIENT_RETRY_WAIT1;
	} else if (state->retries < CLIENT_TRY_THRESH) {
		http_client_set_retry_limit(hc, 2);
		delay_ms = CLIENT_RETRY_WAIT2;
	} else {
		http_client_set_retry_limit(hc, 1);
		delay_ms = CLIENT_RETRY_WAIT3;
	}
	client_retry_after(state, delay_ms);
}

/*
 * Retry very slowly after serious, nearly-unrecoverable error.
 * Someone will need to do something manual on the cloud side to
 * re-enable this device, but we don't want it to give up completely.
 * There could be a DNS problem, or other network issue.
 *
 * The first retry is 5 minutes, then 10, 20, 40, 60.  60 is the limit.
 */
static void client_retry_err(struct client_state *state)
{
	u32 delay_ms = CLIENT_RETRY_WAIT3;
	u32 retries = state->retries;

	client_close(state);
	if (retries < CLIENT_TRY_THRESH) {
		retries = CLIENT_TRY_THRESH;
	}
	if (retries > 20) {
		retries = 20;	       /* factor of 1M */
	}
	delay_ms <<= retries - CLIENT_TRY_THRESH;
	if (delay_ms > CLIENT_RETRY_WAIT_MAX) {
		delay_ms = CLIENT_RETRY_WAIT_MAX;
	}
	state->conn_state = CS_WAIT_RETRY;
	client_retry_after(state, delay_ms);
}


static void client_retry_exp(struct client_state *state)
{
	s32 random;
	u32 delay; /* ms */
	u32 base;
	struct client_backoff {
		u32 base;	/* starting base wait time in milliseconds */
		u32 limit;	/* maximum base wait time */
		u32 rate;	/* increase in percentage each retry */
		u32 range;	/* amount of randomness to add in percent / 2 */
	};
	static const struct client_backoff conf_backoff = {
		.base = 1000,
		.limit = 120000,
		.rate = 200,
		.range = 100,
	};
	const struct client_backoff *p;

	p = &conf_backoff;	/* to come from client_conf.c eventually */
	if (state->retries == 0) {
		base = p->base;
	} else {
		base = state->backoff_prev_base * p->rate / 100;
	}

	if (base > p->limit) {
		base = p->limit;
	}
	state->backoff_prev_base = base;

	al_random_fill(&random, sizeof(random));
	random %= (s32)(base * p->range / 100);
	delay = base + random;

	client_close(state);
	state->conn_state = CS_WAIT_RETRY;
	client_retry_after(state, delay);
}

/*
 * Enable the log_client if the host has been given
 * and its not already enabled.
 */
static void client_logging(int enable)
{
	struct client_state *state = &client_state;

	if (!state->logging.enabled) {
		return;
	}

	if (log_client_enable(enable)) {
		return;
	}

	if (enable) {
		client_log(LOG_INFO "enabling log client");
	} else {
		client_log(LOG_INFO "disabling log client");
	}
}

/*
 * Return start of host portion of URL string.
 */
static const char *client_url_host_start(const char *url)
{
	const char *host_start;

	host_start = strstr(url, "://");
	if (!host_start) {
		return NULL;
	}
	host_start += 3;	/* add 3 to get past the :// */
	return host_start;
}

/*
 * Return resource portion of URL string.
 */
static const char *client_url_resource(const char *url)
{
	const char *res;

	res = client_url_host_start(url);
	if (res) {
		res = strchr(res, '/');
	}
	return res;
}

/*
 * Set the server to whats used for file properties (i.e. S3)
 */
static int client_set_file_prop_server(struct client_state *state)
{
	struct prop_recvd *prop = &prop_recvd;
	struct http_client *hc = &state->http_client;
	const char *host_start;
	const char *host_end;
	int len;

	if (!prop->is_file) {
		return -1;
	}
	/* extract the hostname from the FILE url */
	host_start = client_url_host_start(prop->file_info.file);
	host_end = client_url_resource(prop->file_info.file);

	if (!host_start || !host_end || host_end == host_start) {
		client_log(LOG_ERR "no host found in %s", prop->file_info.file);
		return -1;
	}
	len = host_end - host_start;
	if (len > sizeof(hc->host) - 1) {
		len = sizeof(hc->host) - 1;
	}
	strncpy(hc->host, host_start, len);
	hc->host[len] = '\0';

	if (!strncmp(prop->file_info.file, "https", 5)) {
		hc->ssl_enable = 1;
		hc->host_port = HTTP_CLIENT_SERVER_PORT_SSL;
	} else {
		hc->ssl_enable = 0;
		hc->host_port = HTTP_CLIENT_SERVER_PORT;
	}
	hc->accept_non_ayla = 1;	/* file properties can talk to S3 */
	hc->client_auth = 0;

	return 0;
}

/*
 * Determine if the ADS client is allow to do a GET operation
 */
static int client_can_get(struct client_state *state)
{
	struct prop_recvd *prop = &prop_recvd;

	return !state->lan_cmd_pending &&
	    !state->get_echo_inprog && !state->cmd_rsp_pending &&
/* XXX  get_echo_inprog gets stuck on somehow -- XXXX */
	    state->ads_cmds_en && !state->http_client.prop_callback &&
	    !prop->is_file;
}

/*
 * Check if we're done with echoes.
 * If we were doing a GET from ADS, continue.
 */
void client_finish_echo(struct client_state *state, u8 finished_id)
{
	state->echo_dest_mask &= ~(finished_id);
	if (!state->get_echo_inprog) {
		return;
	}
	if (!state->echo_dest_mask) {
		if (state->conn_state == CS_WAIT_GET) {
			memset(&prop_recvd, 0, sizeof(prop_recvd));
			client_continue_recv(state);
		} else {
			state->get_echo_inprog = 0;
			client_wakeup();
		}
	}
}

/*
 * Timer handler to GET commands for ADS polling and health check.
 */
static void client_poll(struct timer *timer)
{
	struct client_state *state = &client_state;

	state->cmd_event = 1;
	client_wakeup();
}

/*
 * Client health check.
 * For use by watchdog system.  Returns 0 if client is OK or check is disabled.
 */
int ada_client_health_check(void)
{
	struct client_state *state = &client_state;
	u32 limit;
	u32 now;
	static u8 warned;

	if (!state->health_check_en) {
		return 0;
	}
	now = clock_ms();
	limit = CLIENT_HEALTH_MAX_INTVL(state->get_cmds_limit);
	if (clock_gt(now, state->get_cmds_time + limit)) {
		if (!warned) {
			warned = 1;
			limit /= 60 * 1000;
			client_log(LOG_WARN
			    "Agent has been unable to GET cmds for "
			    "%luH:%2.2luM.",
			    limit / 60, limit % 60);
		}
		return -1;
	}
	warned = 0;
	return 0;
}

static void ada_client_health_intvl_set(u32 limit)
{
	struct client_state *state = &client_state;
	s32 rtime;

	al_random_fill(&rtime, sizeof(rtime));
	rtime = rtime % CLIENT_HEALTH_GET_DIST(limit);
	limit += rtime;
	state->get_cmds_limit = limit;
}

void ada_client_health_test_mode(u8 enable)
{
	client_lock();
	ada_client_health_intvl_set(enable ? CLIENT_HEALTH_GET_INTVL_TEST :
	    CLIENT_HEALTH_GET_INTVL);
	client_wakeup();
	client_unlock();
}

void ada_client_health_check_en(void)
{
	struct client_state *state = &client_state;

	state->health_check_en = 1;
}

void ada_client_health_check_dis(void)
{
	struct client_state *state = &client_state;

	state->health_check_en = 0;
}

/*
 * Schedule next client wakeup for polling or health check.
 */
static void client_get_cmds_timer_set(struct client_state *state)
{
	u32 delay;
	u32 now;

	now = clock_ms();
	delay = state->get_cmds_time + state->get_cmds_limit;
	if (clock_gt(delay, now)) {
		delay -= now;
	} else {
		delay = CLIENT_RETRY_WAIT3;
	}
	client_timer_set(&state->poll_timer, delay);
}

/*
 * Receive for PUT or POST that does not expect data.
 */
enum ada_err client_recv_drop(struct http_client *hc,
					void *buf, size_t len)
{
	struct client_state *state = &client_state;

	if (buf) {
		hc->recv_consumed += len;
	} else {
		client_tcp_recv_done(state);
	}
	return AE_OK;
}

/*
 * Start HTTP request.
 * This shares the same HTTP client between ADS and AIS.
 * This must be called before starting the http_client request.
 */
struct http_client *client_req_new(enum client_connect_target tgt)
{
	struct client_state *state = &client_state;
	struct http_client *hc = &state->http_client;

	ASSERT(client_locked);
	if ((hc->state != HCS_IDLE && hc->state != HCS_KEEP_OPEN) ||
	    state->new_conn ||
	    state->tgt != tgt) {
		state->new_conn = 0;
		client_close(state);
	}
	if (state->tgt != tgt) {
		hc->is_mqtt = 0;
		switch (tgt) {
		case CCT_IMAGE_SERVER:
		case CCT_LAN:
		case CCT_REMOTE:
			client_ota_server(state);
			break;
		case CCT_FILE_PROP_SERVER:
			client_set_file_prop_server(state);
			break;
		case CCT_ADS:
			hc->is_mqtt = 1;
			client_commit_server(state);
			if (!CLIENT_HAS_KEY(state)) {
				log_client_reset();
			}
			break;
#ifdef AYLA_IN_FIELD_PROVISION_SUPPORT
		case CCT_IN_FIELD_PROVISION:
			client_commit_server(state);
			hc->client_auth = 0;
			break;
#endif
		case CCT_NONE:
		default:
			ASSERT_NOTREACHED();
		}
		state->tgt = tgt;
	}

	state->conn_state = CS_WAIT_CONN;
	state->ame_init = 0;

	hc->mod_log_id = MOD_LOG_CLIENT;
	hc->sending_chunked = 0;
	hc->http_tx_chunked = 0;
	hc->prop_callback = 0;
	hc->user_in_prog = 0;
	hc->content_len = 0;
	hc->body_len = 0;
	hc->sent_len = 0;
	hc->body_buf_len = 0;
	hc->auth_hdr = state->auth_hdr;
	hc->auth_hdr_size = sizeof(state->auth_hdr);

	hc->client_tcp_recv_cb = client_recv_drop;
	hc->client_err_cb = client_err_cb;
	hc->client_send_data_cb = NULL;
	state->cont_recv_hc = hc;
	state->failed_dest_mask = 0;

	hc->req_len = 0;
	hc->req_part = 0;

	return hc;
}

struct http_client *client_req_ads_new(void)
{
	return client_req_new(CCT_ADS);
}

#ifdef AYLA_FILE_PROP_SUPPORT
static struct http_client *client_req_file_new(void)
{
	return client_req_new(CCT_FILE_PROP_SERVER);
}
#endif

/*
 * Start HTTP request.
 * Called with lock held.  Drops lock momentarily.
 */
void client_req_start_head(struct http_client *hc,
		enum http_client_method method,
		const char *resource, u8 hcntin, const struct http_hdr *header)
{
	struct client_state *state = &client_state;
	const char *method_str;
	char req[CLIENT_GET_REQ_LEN];	 /* request without args */
	char buf[HTTP_MAX_TEXT];
	size_t len = 0;
	char *cp;
	int auth_len;
	char pub_key[CLIENT_CONF_PUB_KEY_LEN];
	int pub_key_len;
	struct http_hdr hdrs[HTTP_CLIENT_TX_HDR_LIMIT];
	unsigned int hcnt = 0;
	unsigned int i;
	int rc;

	ASSERT(client_locked);
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

	client_log(LOG_DEBUG "request %s %s %s",
	    hc->is_mqtt ? "MQTTS" : hc->ssl_enable ? "HTTPS" : "HTTP",
	    method_str, resource);

	ASSERT(hcntin + 2 <= ARRAY_LEN(hdrs));
	for (i = 0; i < hcntin; i++) {
		hdrs[hcnt++] = *header;		/* struct copy */
		header++;
	}

	if (hc->client_auth) {
		/*
		 * If we have the auth key, use it.  Otherwise present the
		 * client auth field which should yield the auth key.
		 * If the key doesn't work, we'll clear it out and retry.
		 */
		if (hc->auth_hdr[0] != '\0') {
			hdrs[hcnt].name = HTTP_CLIENT_KEY_FIELD;
			hdrs[hcnt++].val = hc->auth_hdr;
		} else {
			/*
			 * The pseudo-header for authentication does not
			 * include the args, unfortunately.  Form psuedo header.
			 */
			rc = snprintf(req, sizeof(req), "%s %s?",
			    method_str, resource);
			ASSERT(rc < sizeof(req));
			cp = strchr(req, '?');
			ASSERT(cp);		/* should find final '?' */
			if (cp) {
				*cp = '\0';
			}
			if (!hc->is_mqtt) {
				len = snprintf(buf, sizeof(buf), "%s ",
				    HTTP_CLIENT_AUTH_VER);
				if (len >= sizeof(buf)) {
					len = sizeof(buf) - 1;
				}
			}

			pub_key_len = adap_conf_pub_key_get(pub_key,
			    sizeof(pub_key));
			if (pub_key_len <= 0) {
				goto no_auth;
			}
			auth_len = client_auth_gen(pub_key, pub_key_len,
			    buf + len, sizeof(buf) - len, req);
			if (auth_len <= 0) {
				goto no_auth;
			}
			hdrs[hcnt].name = HTTP_CLIENT_INIT_AUTH_HDR;
			hdrs[hcnt++].val = buf;
		}
	}
	if (hc->is_mqtt) {
		http2mqtt_client_req(hc, method, resource, hcnt, hdrs);
	} else {
		http_client_req(hc, method, resource, hcnt, hdrs);
	}
	return;

no_auth:
	client_log(LOG_ERR "pub_key failure");
	client_retry_err(state);
	ASSERT(client_locked);
}

void client_req_start(struct http_client *hc, enum http_client_method method,
		const char *resource, const struct http_hdr *header)
{
	u8 hcnt;

	if (header == NULL) {
		hcnt = 0;
	} else {
		hcnt = 1;
	}
	client_req_start_head(hc, method, resource, hcnt, header);
}

enum ada_err client_req_send(struct http_client *hc,
		const void *buf, size_t len)
{
	enum ada_err err;

	if (hc->is_mqtt) {
		err = http2mqtt_client_send(hc, buf, len);
	} else {
		err = http_client_send(hc, buf, len);
	}
	return err;
}

void client_req_send_complete(struct http_client *hc)
{
	if (hc->is_mqtt) {
		http2mqtt_client_send_complete(hc);
	} else {
		http_client_send_complete(hc);
	}
}

#ifdef AYLA_FILE_PROP_SUPPORT
/*
 * Handle response from a file datapoint request.
 */
static enum ada_err client_recv_dp(struct http_client *hc,
					void *buf, size_t len)
{
	struct client_state *state = &client_state;
	struct recv_payload recv_payload;
	enum ada_err err = AE_OK;

	if (buf) {
		state->auth_fails = 0;
		if (state->mcu_overflow) {
			CLIENT_LOGF(LOG_WARN, "GET_DP drop: mcu ovrflow");
			return err;
		}
		if (!len) {
			client_wait(state, CLIENT_PROP_WAIT);
			return err;
		}
		if (!state->prop_send_cb) {
			return AE_INVAL_STATE;
		}
		recv_payload.data = buf;
		recv_payload.len = len;
		recv_payload.consumed = 0;
		err = state->prop_send_cb(PROP_CB_CONTINUE, &recv_payload);
		client_wait(state, CLIENT_PROP_WAIT);
		hc->recv_consumed = recv_payload.consumed;
		return err;
	}
	return client_recv_prop_done(hc);
}
#endif

/*
 * Start a GET or POST command through a property-subsystem callback.
 */
static void client_prop_cmd_send(struct client_state *state)
{
	enum ada_err err;

#ifdef AYLA_LAN_SUPPORT
	state->http_lan = NULL;
#endif
	state->conn_state = CS_WAIT_PROP;
	err = state->prop_send_cb(PROP_CB_BEGIN, NULL);
	if (err && err != AE_BUF) {
		client_log(LOG_ERR "%s err %d", __func__, err);
		client_retry(state);
	}
}

#ifdef CLIENT_STEP_SET_EN	/* make array non-constant if enabling this */
/*
 * Set up for a "next_step" callback at a given priority.
 */
void client_step_set(enum client_req_pri pri, int (*fn)(struct client_state *))
{
	struct client_step *step;
	int (*pending)(struct client_state *);

	ASSERT(pri < ARRAY_LEN(client_steps));
	step = &client_steps[pri];
	pending = step->handler;
	ASSERT(!pending || pending == fn);
	step->handler = fn;
}
#endif /* CLIENT_STEP_SET_EN */

/*
 * Call the all handlers in priority order until one returns 0 (success).
 */
static int client_req_next(struct client_state *state)
{
	int (*handler)(struct client_state *);
	const struct client_step *tp;
	u32 mask;
	u32 enabled_steps;

	/*
	 * If the device ID is not set, or there's been an error,
	 * restrict to those steps not always needing ADS connectivity.
	 */
	enabled_steps = client_step_mask;
	if (!CLIENT_HAS_KEY(state) ||
	    (state->conn_state == CS_DOWN)) {
		enabled_steps &= ADCP_STEPS_WITH_NO_ADS;
	}

	for (tp = client_steps, mask = 1;
	     tp < &client_steps[ARRAY_LEN(client_steps)];
	     tp++, mask <<= 1) {
		if (enabled_steps & mask) {
			handler = tp->handler;
			if (!handler) {
				continue;
			}
			if (handler && !handler(state)) {
				return 0;
			}
		}
	}
	return -1;
}

#ifdef AYLA_FILE_PROP_SUPPORT
static int client_file_step(struct client_state *state)
{
	if (state->prop_send_cb == prop_mgr_dp_put ||
	    state->prop_send_cb == prop_mgr_dp_req) {
		client_prop_cmd_send(state);
		return 0;
	}
	return -1;
}
#endif

static int client_cmd_put_step(struct client_state *state)
{
	if (state->cmd_pending && !state->cmd_rsp_pending) {
		client_cmd_put(state);
		return 0;
	}
	return -1;
}

static int client_cmd_step(struct client_state *state)
{
	int can_get;

	can_get = client_can_get(state) && state->cmd_event;

	if (state->prefer_get && can_get) {
		client_get_cmds(state);
		return 0;
	}
	if (state->prop_send_cb && (state->dest_mask & NODES_ADS) &&
	    !state->get_echo_inprog) {
		client_prop_cmd_send(state);
		return 0;
	}
	if (can_get) {
		client_get_cmds(state);
		return 0;
	}
	return -1;
}

/*
 * TCP callback for next client step
 */
static void client_next_step(void *arg)
{
	struct client_state *state = &client_state;
	struct callback_queue *cbq;
	struct callback *cb;

repeat:
	ASSERT(client_locked);

	switch (state->conn_state) {
	/*
	 * The following states are handled as general ADS or LAN next steps.
	 */
	case CS_WAIT_EVENT:
	case CS_DOWN:
#ifdef AYLA_LOCAL_CONTROL_SUPPORT
		lctrl_wakeup();
#endif
		break;
	/*
	 * No new requests can be generated in the following states.
	 */
	case CS_DISABLED:
	case CS_WAIT_PROP:
	case CS_WAIT_PROP_RESP:
		return;

	/*
	 * All other states are LAN-only.
	 */
	default:
#ifdef AYLA_LOCAL_CONTROL_SUPPORT
		lctrl_wakeup();
#endif
#ifdef AYLA_LAN_SUPPORT
		client_lan_cycle(state);
#endif
		return;
	}

	if (!http_client_is_ready(&state->http_client)) {
#ifdef AYLA_LAN_SUPPORT
		client_lan_cycle(state);
#endif
		return;
	}

	/*
	 * If we already have a current callback that's incomplete, recall it.
	 * We don't clear the pending flag to avoid missing new events.
	 */
	cb = state->current_request;
	if (cb) {
		cb->func(cb->arg);
		return;
	}

	/*
	 * Call first callback in the queue.
	 * This will start a request, so return until next callback.
	 */
	for (cbq = state->callback_queue;
	    cbq < &state->callback_queue[CQP_COUNT]; cbq++) {
		cb = callback_dequeue(cbq);
		if (cb) {
			state->current_request = cb;
			ASSERT(cb->pending);
			cb->pending = 0;
			cb->func(cb->arg);
			goto repeat;	/* for synchronous client */
		}
	}

	if (!client_req_next(state)) {
		goto repeat;		/* for synchronous client */
	}

	/*
	 * Nothing to do but wait.
	 */
	client_get_cmds_timer_set(state);
}

/*
 * Wakeup client to evaluate state.
 * Called with lock held.
 */
void client_wakeup(void)
{
	struct client_state *state = &client_state;

	ASSERT(client_locked);
	client_callback_pend(&state->next_step_cb);
}

/*
 * Callback from when HTTP client is done with all requests.
 * Done with current receive.  Decide whether to reconnect or wait.
 * May be called from any thread.
 */
static void client_next_step_cb(struct http_client *hc)
{
	client_wakeup();
}

#if defined(AYLA_LAN_SUPPORT) || defined(AYLA_LOCAL_CONTROL_SUPPORT)
static void client_lanip_save(void *arg)
{
	struct client_state *state = &client_state;

	ada_conf_lanip_save();
	client_log(LOG_DEBUG "lanip saved");

#ifdef AYLA_LOCAL_CONTROL_SUPPORT
	if (!(ada_lan_conf.enable_mask & LAN_CONF_ENABLE_LAN_BIT)) {
		lctrl_session_close_all();
	}
#endif

	/* notify callback of lanip update */
	if (state->lanip_cb) {
		state->lanip_cb();
	}
}

/*
 * Parse the lanip information (key, key_id, keep_alive)
 */
static int client_lanip_json_parse(char *json_str)
{
	struct ada_lan_conf *lcf = &ada_lan_conf;
	jsmn_parser parser;
	jsmntok_t tokens[CLIENT_LANIP_JSON];
	jsmntok_t *lanip;
	jsmntok_t *connections;
	jsmnerr_t err;
	char status[10];
	unsigned long tmp_key_id;
	unsigned long tmp_keep_alive;
	unsigned long tmp_auto_sync;
	char tmp_key[CLIENT_LANIP_KEY_SIZE + 1];
	u8 enable_mask = 0;

	jsmn_init_parser(&parser, json_str, tokens, CLIENT_LANIP_JSON);
	err = jsmn_parse(&parser);
	if (err != JSMN_SUCCESS) {
		CLIENT_LOGF(LOG_WARN, "jsmn err %d", err);
		goto error;
	}
	lanip = jsmn_get_val(&parser, NULL, "lanip");
	if (!lanip) {
		CLIENT_LOGF(LOG_WARN, "no lanip");
		goto error;
	}
	if (jsmn_get_string(&parser, lanip, "status",
	    status, sizeof(status)) < 0) {
		CLIENT_LOGF(LOG_WARN, "bad status");
		goto error;
	}
	if (strcmp(status, "enable")) {
		client_log(LOG_DEBUG "lan mode disabled");
		memset(lcf, 0, sizeof(*lcf));
		goto save;
	}
	if (jsmn_get_ulong(&parser, lanip, "lanip_key_id",
	    &tmp_key_id) || tmp_key_id > MAX_U16) {
		CLIENT_LOGF(LOG_WARN, "bad lanip_key_id");
		goto error;
	}
	if (jsmn_get_string(&parser, lanip, "lanip_key",
	    tmp_key, sizeof(tmp_key)) < 0) {
		CLIENT_LOGF(LOG_WARN, "bad lanip_key");
		goto error;
	}
	if (jsmn_get_ulong(&parser, lanip, "keep_alive",
	    &tmp_keep_alive) || tmp_keep_alive > MAX_U16) {
		CLIENT_LOGF(LOG_WARN, "bad keep_alive");
		goto error;
	}
	if (jsmn_get_ulong(&parser, lanip, "auto_sync", &tmp_auto_sync) ||
	    !tmp_auto_sync) {
		lcf->auto_echo = 0;
	} else {
		lcf->auto_echo = 1;
	}
	connections = jsmn_get_val(&parser, lanip, "connections");
	if (connections) {
#ifdef AYLA_LAN_SUPPORT
		if (jsmn_array_contains_string(&parser, connections, "LAN")) {
			enable_mask |= LAN_CONF_ENABLE_LAN_BIT;
		}
#endif
#ifdef AYLA_LOCAL_CONTROL_SUPPORT
		if (jsmn_array_contains_string(&parser, connections, "BLE")) {
			enable_mask |= LAN_CONF_ENABLE_BLE_BIT;
		}
		if (jsmn_array_contains_string(&parser, connections, "MTR")) {
			enable_mask |= LAN_CONF_ENABLE_MATTER_BIT;
		}
#endif
	} else {
		/*
		 * Absence of the "connections" field means LAN-mode only
		 * for backward compatibility.
		 */
#ifdef AYLA_LAN_SUPPORT
		enable_mask |= LAN_CONF_ENABLE_LAN_BIT;
#endif
	}

	if (!enable_mask) {
		CLIENT_LOGF(LOG_DEBUG, "LAN and LC all disable");
		goto error;
	}
	lcf->enable_mask = enable_mask;
	lcf->lanip_key_id = (u16)tmp_key_id;
	lcf->keep_alive = (u16)tmp_keep_alive;
	memcpy(lcf->lanip_key, tmp_key, sizeof(tmp_key));

save:
	lcf->enable_mask |= LAN_CONF_ENABLE_RECVD_BIT; /* recv'd from service */
	client_callback_pend(&client_cb[CCB_LANIP]);
	return 0;

error:
	CLIENT_LOGF(LOG_WARN, "LAN mode disabled");
	memset(lcf, 0, sizeof(*lcf));
	client_callback_pend(&client_cb[CCB_LANIP]);
	return -1;
}
#endif

/*
 * Connectivity to service has just come up. Notify host mcu and setup client.
 */
static void client_server_conn_up(struct client_state *state)
{
	if (!(state->valid_dest_mask & NODES_ADS)) {
		client_dest_up(NODES_ADS);
		client_timer_set(&state->listen_timer, CLIENT_LISTEN_WARN_WAIT);
	}
	state->serv_conn = 1;
}

/*
 * Timeout for retrying the command request.
 */
static void client_cmd_timeout(struct timer *arg)
{
	struct client_state *state = &client_state;

	state->cmd_event = 1;
	client_wakeup();
}

void client_tcp_recv_done(struct client_state *state)
{
	struct http_client *hc = &state->http_client;

	ASSERT(client_locked);

	client_timer_cancel(&state->req_timer);

	if (!hc->hc_error && hc->client_auth) {
		state->auth_fails = 0;
	}
	state->current_request = NULL;
	state->retries = 0;
	state->connect_time = clock_ms();
	state->request = CS_IDLE;
	state->conn_state = CS_WAIT_EVENT;
	client_wakeup();
}

/*
 * receive of AME data.
 */
enum ada_err client_recv_ame(struct http_client *hc, void *buf, size_t len)
{
	struct client_state *state = &client_state;
	enum ada_err err = AE_OK;

	state->auth_fails = 0;
	client_wait(state, CLIENT_PROP_WAIT);

	ASSERT(state->ame_init);
	err = client_ame_parse(&state->ame, buf, len);

	if (err == AE_OK) {
		client_log(LOG_DEBUG "AME parse finish");
		hc->recv_consumed += len;
	} else if (err == AE_UNEXP_END) {
		client_log(LOG_DEBUG "AME parse continue");
		hc->recv_consumed += len;
		err = AE_OK;
	} else if (err == AE_BUF) {
		client_log(LOG_DEBUG "AME parse pause");
		hc->recv_consumed = state->ame.ds.offset;
	} else {
		client_log(LOG_WARN "AME parse failed err %d", err);
		client_retry_err(state);
		err = AE_INVAL_VAL;
	}
	return err;
}

static enum ada_err client_recv_cmd_put(struct http_client *hc,
					void *buf, size_t len)
{
	struct client_state *state = &client_state;

	if (buf) {
		hc->recv_consumed += len;
		return AE_OK;
	}
	server_req_done_callback(&state->cmd_req);
	state->cmd_pending = 0;
	state->get_cmds_time = clock_ms();

	/*
	 * Free buffer which was allocated in client_rev_rest_put().
	 */
	al_os_mem_free((void *)hc->body_buf);
	hc->body_buf = NULL;

	client_tcp_recv_done(state);
	return AE_OK;
}

static enum ada_err client_recv_sts_put(struct http_client *hc,
					void *buf, size_t len)
{
	struct client_state *state = &client_state;

	if (buf) {
		return client_recv_drop(hc, buf, len);
	}

	/*
	 * Command status reported. We're ready to
	 * apply the patch now.
	 */
	client_ota_save_done(state);
	client_tcp_recv_done(state);
	if (state->ota_server.uri) {
		al_os_mem_free(state->ota_server.uri);
		state->ota_server.uri = NULL;
	}

	return AE_OK;
}

#if defined(AYLA_LAN_SUPPORT) || defined(AYLA_LOCAL_CONTROL_SUPPORT)
static enum ada_err client_recv_lanip(struct http_client *hc,
					void *buf, size_t len)
{
	struct client_state *state = &client_state;

	if (buf) {
		memcpy(state->buf + state->recved_len, buf, len);
		state->recved_len += len;
		return AE_OK;
	}
	state->lanip_fetch = 0;
	state->buf[state->recved_len] = '\0';
	client_lanip_json_parse(state->buf);
	client_tcp_recv_done(state);
	return AE_OK;
}
#endif

/*
 * Notify client that a 206 (partial content) was received
 * in the previous get. So re-mark the cmd_event flag.
 */
void client_notify_if_partial(void)
{
	struct client_state *state = &client_state;

	client_lock();
	if (state->partial_content) {
		state->partial_content = 0;
		state->cmd_event = 1;
	}
	client_unlock();
}

void client_get_dev_id_pend(struct client_state *state)
{
	client_log(LOG_DEBUG "%s: state=%s", __func__,
	    client_conn_states[state->conn_state]);
	callback_enqueue(&state->callback_queue[CQP_HIGH],
	    &client_cb[CCB_GET_DEV_ID]);
}

static void client_mqtt_notify_cb(void *arg)
{
	struct client_state *state = &client_state;

	ASSERT(client_locked);
	client_log(LOG_INFO "MQTT notification");
	if (state->wait_for_file_put || state->wait_for_file_get) {
		/*
		 * tell the host mcu that there's a pending update
		 * so he can abort the file operation if he wants
		 */
		prop_mgr_event(PME_NOTIFY, NULL);
	}
	state->cmd_event = 1;
	client_timer_cancel(&state->poll_timer);
	client_wakeup();
}

static void client_mqtt_conn_state_cb(enum http2mqtt_conn_state st)
{
	struct client_state *state = &client_state;

	if (state->tgt != CCT_ADS) {
		return;
	}
	if (st == H2M_DISCONNECTED) {
		client_log(LOG_WARN "MQTT down event: state=%s",
		    client_conn_states[state->conn_state]);
		state->ads_listen = 0;
		state->serv_conn = 0;
		client_dest_down(NODES_ADS);
	} else if (CLIENT_HAS_KEY(state)) {
		client_server_conn_up(state);
	}
}

static void client_mqtt_ssl_close(struct http_client *hc)
{
	if (hc->is_connected && hc->ssl_enable) {
		http2mqtt_client_disconnect(hc);
	}
}

/*
 * TCP timeout for handling re-connect or retry.
 */
static void client_timeout(struct timer *timer)
{
	struct client_state *state = &client_state;
	struct http_client *hc = &state->http_client;

	switch (state->conn_state) {
	case CS_DOWN:
	case CS_DISABLED:
		client_down_locked(1, 0);
		return;
	case CS_WAIT_EVENT:
	case CS_WAIT_RETRY:
		break;
	default:
		if (!http_client_is_ready(hc)) {
			if (hc->is_mqtt) {
				http2mqtt_client_abort(hc);
			} else {
				http_client_abort(hc);
			}
			hc->hc_error = HC_ERR_TIMEOUT;
			client_err_cb(hc);
			return;
		}
		client_close(state);
		break;
	}
	state->conn_state = CS_WAIT_EVENT;
	if (!hc->is_connected) {
		client_get_dev_id_pend(state);
	}
	client_wakeup();
}

/*
 * Schedule reconnect/retry after wait.
 */
void client_wait(struct client_state *state, u32 delay)
{
	client_timer_cancel(&state->req_timer);	/* in case of reset/commit */

	ASSERT(client_locked);
	switch (state->conn_state) {
	case CS_WAIT_RETRY:
		CLIENT_DEBUG(LOG_DEBUG2, "RETRY in %lu ms", delay);
		break;
	case CS_WAIT_EVENT:
	case CS_WAIT_FILE_GET:
	case CS_WAIT_DSN:
	case CS_WAIT_ID:
	case CS_WAIT_INFO_PUT:
	case CS_WAIT_OEM_INFO_PUT:
	case CS_WAIT_GET:
	case CS_WAIT_PROP_GET:
	case CS_WAIT_OTA_GET:
	case CS_WAIT_POST:
	case CS_WAIT_LOGS_POST:
	case CS_WAIT_METRICS_POST:
	case CS_WAIT_ECHO:
	case CS_WAIT_CMD_PUT:
	case CS_WAIT_OTA_PUT:
	case CS_WAIT_CMD_STS_PUT:
	case CS_WAIT_PING:
	case CS_WAIT_REG_WINDOW:
	case CS_WAIT_CONN:
		CLIENT_DEBUG(LOG_DEBUG2, "state %s set timeout %lu ms",
		    client_conn_state_string(state->conn_state), delay);
		break;
	default:
		CLIENT_DEBUG(LOG_DEBUG2, "bad state %x", state->conn_state);
		break;
	}
	if (delay) {
		client_timer_set(&state->req_timer, delay);
	}
}

/*
 * Receive for device ID request.
 */
static enum ada_err client_recv_index(struct http_client *hc,
					void *buf, size_t len)
{
	struct client_state *state = &client_state;

	ASSERT(client_locked);
	if (buf) {
		return client_recv_ame(hc, buf, len);
	}

	if (CLIENT_HAS_KEY(state)) {
		http_client_set_retry_limit(hc, -1);
		client_server_conn_up(state);
		if (conf_was_reset) {
			conf_was_reset = 0;
			ada_conf_persist_reset();
		}
		client_logging(1);
	} else {
		client_get_dev_id_pend(state);
	}
	client_tcp_recv_done(state);
	return AE_OK;
}

/*
 * GET device ID.
 */
static void client_get_dev_id(void *arg)
{
	struct client_state *state = arg;
	struct ada_conf *cf = &ada_conf;
	struct http_client *hc;
	char req[CLIENT_GET_REQ_LEN];

	if (clock_source() < CS_SNTP) {
		client_log(LOG_DEBUG "get DSN waiting for SNTP");
		state->current_request = NULL;
		return;
	}

#ifdef AYLA_IN_FIELD_PROVISION_SUPPORT
	if (!conf_sys_dev_id[0]) {
		client_ifp_get_dsn(state);
		return;
	}
#endif

	client_log(LOG_INFO "get DSN %s", conf_sys_dev_id);

	state->client_key[0] = '\0';
	state->client_info_flags |= CLIENT_UPDATE_ALL;

	if (state->setup_token[0] == '\0') {
		state->client_info_flags &= ~CLIENT_UPDATE_SETUP;
	}

	snprintf(req, sizeof(req), "/dsns/%s.json", conf_sys_dev_id);

	if (conf_was_reset) {
		client_arg_add(req, sizeof(req), "reset=1");
	}
	if (cf->test_connect) {
		client_arg_add(req, sizeof(req), "test=1");
	}
	if (log_snap_saved) {
		client_arg_add(req, sizeof(req), "snapshot=%u", log_snap_saved);
	}

	hc = client_req_ads_new();
	ASSERT(hc);
	hc->client_tcp_recv_cb = client_recv_index;
	state->conn_state = CS_WAIT_ID;
	state->request = CS_GET_INDEX;

	memset(&prop_recvd, 0, sizeof(prop_recvd));
	client_ame_init(&state->ame,
	    client_ame_stack, ARRAY_LEN(client_ame_stack),
	    client_ame_parsed_buf, sizeof(client_ame_parsed_buf),
	    client_ame_id, NULL);
	state->ame_init = 1;

	client_log(LOG_DEBUG "host %s, uri %s", hc->host, req);
	if (!hc->hmc_initialized) {
		http2mqtt_client_set_conn_state_cb(hc,
		    client_mqtt_conn_state_cb);
		http2mqtt_client_set_notify_cb(hc, client_mqtt_notify_cb);
	}
	client_req_start(hc, HTTP_REQ_GET, req, NULL);
}

#if defined(AYLA_LAN_SUPPORT) || defined(AYLA_LOCAL_CONTROL_SUPPORT)
/*
 * GET lanip_key from service
 */
static int client_get_lanip_key(struct client_state *state)
{
	struct ada_lan_conf *lcf = &ada_lan_conf;
	struct http_client *hc;
	char uri[CLIENT_GET_REQ_LEN];

	/*
	 * Check if local control configuration (lanip) info needs to be
	 * fetched. If the device already has a lanip_key (e.g. loaded from
	 * configuration), fetching isn't necessary. The fetch will normally
	 * only be needed after a factory reset, when the configuration has
	 * been wiped.
	 */
	if (!state->lanip_fetch || CLIENT_LANIP_HAS_KEY(lcf)) {
		state->lanip_fetch = 0;
		return -1;
	}
	hc = client_req_ads_new();
	hc->client_tcp_recv_cb = client_recv_lanip;
	state->conn_state = CS_WAIT_LANIP_GET;
	state->request = CS_GET_LANIP;
	state->recved_len = 0;

	snprintf(uri, sizeof(uri), "/devices/%s/lan.json", state->client_key);
	client_req_start(hc, HTTP_REQ_GET, uri, &http_hdr_content_json);
	return 0;
}
#endif

/*
 * Handle response from GET /commands.
 */
enum ada_err client_recv_cmds(struct http_client *hc, void *payload, size_t len)
{
	struct client_state *state = &client_state;

	if (payload) {
		return client_recv_ame(hc, payload, len);
	}

	client_log(LOG_DEBUG "client_recv_cmds GET done");

	client_timer_cancel(&state->cmd_timer);
	if (state->get_cmds_parsed && !state->cmd_pending) {
		state->get_cmds_time = clock_ms();
	}
	state->get_cmds_fail = 0;
	state->prefer_get = 0;
	if ((state->request == CS_GET_COMMANDS && state->get_all) ||
	    state->request == CS_GET_ALL_VALS) {
		state->get_all = 0;
		prop_mgr_event(PME_GET_INPUT_END, NULL);
	}
	if (hc->http_status == HTTP_STATUS_PAR_CONTENT) {
		state->partial_content = 1;
	}
	state->get_echo_inprog = 0;
	client_tcp_recv_done(state);
	return AE_OK;
}

/*
 * Handle response for GET of commands for property subsystem.
 */
enum ada_err client_recv_prop_cmds(struct http_client *hc,
				void *buf, size_t len)
{
	enum ada_err err;

	err = client_recv_cmds(hc, buf, len);
	if (buf || err != AE_OK) {
		return err;
	}
	return client_recv_prop_done(hc);
}

/*
 * GET /commands from service.
 */
static void client_get_cmds(struct client_state *state)
{
	struct http_client *hc;
	const struct ame_tag *ame_tag;
	int signal;
	const char *uri;

	/* XXX used to set these only after the send finished without errors */
	state->get_echo_inprog = 1;	/* inhibit GETs from LAN clients */
	state->cmd.data = NULL;
	state->cmd.resource = NULL;
	state->cmd.overflow = 0;
	state->cmd_event = 0;
	state->mcu_overflow = 0;
	state->get_cmds_parsed = 0;

	hc = client_req_ads_new();
	ASSERT(hc);
	hc->client_tcp_recv_cb = client_recv_cmds;
	state->conn_state = CS_WAIT_GET;

	if (state->ads_listen) {
		state->request = CS_GET_COMMANDS;
		uri = "commands";
		ame_tag = client_ame_cmds;
	} else {
		state->request = CS_GET_CMDS;
		uri = "cmds";
		ame_tag = client_ame_cmds_only;
	}

	snprintf(state->buf, sizeof(state->buf),
	    "/devices/%s/%s.json", state->client_key, uri);

	if (state->get_all && state->request == CS_GET_COMMANDS) {
		client_arg_add(state->buf, sizeof(state->buf), "input=true");
		prop_mgr_event(PME_GET_INPUT_START, NULL);
	}
	if (!adap_net_get_signal(&signal)) {
		client_arg_add(state->buf, sizeof(state->buf), "signal=%d",
		    signal);
	}

	memset(&prop_recvd, 0, sizeof(prop_recvd));
	client_ame_init(&state->ame,
	    client_ame_stack, ARRAY_LEN(client_ame_stack),
	    client_ame_parsed_buf, sizeof(client_ame_parsed_buf),
	    ame_tag, NULL);
	state->ame_init = 1;

	client_req_start(hc, HTTP_REQ_GET, state->buf, NULL);
}

#ifdef AYLA_FILE_PROP_SUPPORT
/*
 * Parse datapoint location data
 *
 * The expected format of the location data is: "/<prop_id>/<dp_id>"
 *
 * Inputs:
 * loc - pointer to start of location data
 * len - length if loc is not null terminated,
 *       -1 if loc data is null terminated
 *
 * Returns:
 *	NULL if location data is invalid,
 *	pointer to '/' before <dp_id> if valid
 *
 */
static const char *client_dp_loc_parse(const char *loc, int len)
{
	int i;

	if (len < 0) {
		len = PROP_LOC_LEN;
	}
	if (len < 4 || loc[0] != '/') {
		return NULL;
	}
	for (i = 1; i < len && loc[i]; ++i) {
		if (loc[i] == '/') {
			return &loc[i];
		}
	}

	return NULL;
}

/*
 * Given a location in format /<prop_id>/<dp_id>, returns a string in the
 * format: "/devices/<id>/properties/<prop_id>/<prefix>datapoints/<dp_id>"
 */
enum ada_err client_convert_loc_to_url_str_pref(const char *loc,
	char *dest, int dest_len, const char *dp_prefix)
{
	struct client_state *state = &client_state;
	char prop_id[PROP_LOC_LEN];
	const char *dp_id;
	size_t len;

	dp_id = client_dp_loc_parse(loc, -1);
	if (!dp_id) {
		return AE_INVAL_VAL;
	}
	len = dp_id - loc;
	if (len <= 0 || len >= PROP_LOC_LEN) {
		return AE_INVAL_VAL;
	}
	strncpy(prop_id, loc, len);
	prop_id[len] = '\0';
	snprintf(dest, dest_len, "/devices/%s/properties%s/%sdatapoints%s",
	    state->client_key, prop_id, dp_prefix, dp_id);

	return AE_OK;
}

static int client_convert_loc_to_url_str(const char *loc,
	char *dest, int dest_len)
{
	return client_convert_loc_to_url_str_pref(loc, dest, dest_len, "");
}

/*
 * Handle response for PUT of a file datapoint.
 */
static enum ada_err client_recv_dp_put(struct http_client *hc,
					void *buf, size_t len)
{
	struct client_state *state = &client_state;
	struct prop_recvd *prop = &prop_recvd;
	int success;

	if (buf) {
		hc->recv_consumed += len;
		return AE_OK;
	}
	if (state->request == CS_PUT_DP_CLOSE ||
	    (state->request == CS_PUT_DP &&
	    !prop->file_info.file[0])) {
		state->wait_for_file_put = 0;
		prop->is_file = 0;	/* prop_recvd can be overwritten */
	}
	success = hc->http_status == HTTP_STATUS_OK ||
	    hc->http_status == HTTP_STATUS_PAR_CONTENT;
	client_prop_send_done(state, success, state->prop_send_cb_arg,
	    NODES_ADS, hc);
	if (state->request == CS_PUT_DP_FETCH || hc->chunked_eof) {
		client_tcp_recv_done(state);
	}
	return AE_OK;
}

/*
 * Close FILE DP put. The location is in the format: <prop_id>/<dp_id>
 * We need to do a PUT on <prop_id>/datapoints/<dp_id>
 */
enum ada_err client_close_dp_put(const char *loc)
{
	struct client_state *state = &client_state;
	struct http_client *hc;

	if (client_convert_loc_to_url_str(loc, state->ame_buf,
	    sizeof(state->ame_buf))) {
		return AE_INVAL_VAL;
	}

	hc = client_req_ads_new();
	state->request = CS_PUT_DP_CLOSE;
	hc->client_tcp_recv_cb = client_recv_dp_put;

	client_req_start(hc, HTTP_REQ_PUT, state->ame_buf, NULL);

	/* erase prop_recvd since DP put is complete */
	memset(&prop_recvd, 0, sizeof(prop_recvd));
	return AE_OK;
}

/*
 * Send dp put to server.
 */
enum ada_err client_send_dp_put(const u8 *prop_val, size_t prop_val_len,
	const char *prop_loc, u32 offset, size_t tot_len, u8 eof)
{
	struct client_state *state = &client_state;
	struct http_client *hc = &state->http_client;
	struct prop_recvd *prop = &prop_recvd;
	enum ada_err err = AE_OK;
	const char *uri;
	const struct http_hdr headers[] = {
		{http_hdr_content_stream.name, http_hdr_content_stream.val},
		{http_hdr_accept_tag.name, http_hdr_accept_tag.val},
	};

	eof |= (tot_len == prop_val_len);
	if (hc->client_tcp_recv_cb != client_recv_dp_put) {
		if (prop->file_info.file[0]) {
			hc = client_req_file_new();
		} else {
			hc = client_req_ads_new();
		}
		state->request = CS_PUT_DP;
		hc->client_send_data_cb = client_send_post;
		hc->client_tcp_recv_cb = client_recv_dp_put;
		hc->sending_chunked = 1; /* app chunking (not HTTP chunking) */
		hc->chunked_eof = eof;
		hc->body_len = tot_len;

		if (prop->file_info.file[0]) {
			uri = client_url_resource(prop->file_info.file);
		} else {
			if (client_convert_loc_to_url_str_pref(prop_loc,
			    state->ame_buf, sizeof(state->ame_buf),
			    "message_")) {
				client_log(LOG_ERR "message loc invalid: %s",
				    prop_loc);
				return AE_NOT_FOUND;
			}
			uri = state->ame_buf;
		}
		client_req_start_head(hc, HTTP_REQ_PUT,
		    uri, ARRAY_LEN(headers), headers);
	} else {
		err = client_req_send(hc, prop_val, prop_val_len);
		if (err && err != AE_BUF) {
			CLIENT_DEBUG(LOG_DEBUG, "write err %d",
			    err);
			return err;
		}
		if (!err && eof) {
			hc->chunked_eof = 1;
			hc->client_send_data_cb = NULL;
		}
	}

	return err;
}

/*
 * Handle response for fetched datapoint PUT.
 */
static enum ada_err client_recv_dp_fetched(struct http_client *hc,
    void *buf, size_t len)
{
	struct client_state *state = &client_state;
	int success;

	if (buf) {
		return AE_OK;
	}

	success = hc->http_status == HTTP_STATUS_OK ||
	    hc->http_status == HTTP_STATUS_PAR_CONTENT;
	client_prop_send_done(state, success, state->prop_send_cb_arg,
	    NODES_ADS, hc);
	client_tcp_recv_done(state);
	return AE_OK;
}

/*
 * Indicate to the service that datapoint has been fetched.
 */
enum ada_err client_send_dp_fetched(const char *prop_loc)
{
	struct client_state *state = &client_state;
	struct http_client *hc;
	size_t ame_len;
	char url_str[PROP_LOC_LEN + 40];

	if (client_convert_loc_to_url_str(prop_loc, url_str, sizeof(url_str))) {
		return AE_INVAL_VAL;
	}

	ame_len = snprintf(state->ame_buf, sizeof(state->ame_buf),
	    "{\"datapoint\":{\"fetched\":true}}");
	if (ame_len >= sizeof(state->ame_buf)) {
		ame_len = sizeof(state->ame_buf) - 1;
	}

	hc = client_req_ads_new();
	hc->client_tcp_recv_cb = client_recv_dp_fetched;
	hc->body_buf = state->ame_buf;
	hc->body_buf_len = ame_len;
	hc->body_len = ame_len;
	state->request = CS_PUT_DP_FETCH;

	client_req_start(hc, HTTP_REQ_PUT, url_str, &http_hdr_content_json);
	return AE_OK;
}
#endif

/*
 * Handle response for fetched file datapoint post.
 */
static enum ada_err client_recv_ack_response(struct http_client *hc,
    void *buf, size_t len)
{
	u8 success = 1;
	struct client_state *state = &client_state;

	if (buf) {
		return AE_OK;
	}
	client_prop_send_done(state, success, NULL, NODES_ADS, hc);
	client_tcp_recv_done(state);
	return AE_OK;
}

/*
 * Send ack status to ADS.
 */
enum ada_err client_send_prop_ack(struct prop *prop)
{
	struct client_state *state = &client_state;
	struct http_client *hc = &state->http_client;
	enum ada_err err = AE_OK;
	size_t ame_len;
	u8 hcnt = 2;
	struct http_hdr msg_prop_http_header[2] = {
		{http_hdr_content_json.name, http_hdr_content_json.val},
		{http_hdr_accept_tag.name, http_hdr_accept_tag.val},
	};
	const struct http_hdr *header = msg_prop_http_header;
	struct prop_ack *ack;

	ack = prop->ack;
	ASSERT(ack);

	client_log(LOG_DEBUG2 "%s prop:%s ack id:%s status:%u msg:%ld dest:%x",
	    __func__, prop->name, ack->id, ack->status, ack->msg, ack->src);

#ifdef AYLA_LAN_SUPPORT
	if (ack->src & ~NODES_ADS) {
		return client_lan_ack_send(prop);
	}
#endif

	ame_len = snprintf(state->ame_buf, sizeof(state->ame_buf),
	    "{\"datapoint\":"
	    "{\"id\":\"%s\",\"ack_status\":%d,\"ack_message\":%ld}}",
	    ack->id,
	    ack->status ? HTTP_STATUS_BAD_REQ : HTTP_STATUS_OK,
	    ack->msg);

	snprintf(state->buf, sizeof(state->buf),
	    "/dev/v1/dsns/%s/properties/%s/datapoints/%s.json",
	    conf_sys_dev_id, prop->name, ack->id);

	hc = client_req_ads_new();
	hc->client_tcp_recv_cb = client_recv_ack_response;
	hc->body_buf = state->ame_buf;
	hc->body_buf_len = ame_len;
	hc->body_len = ame_len;
	state->request = CS_PUT_PROP_ACK;
	state->conn_state = CS_WAIT_PROP_RESP;

	client_req_start_head(hc, HTTP_REQ_PUT, state->buf, hcnt, header);
	return err;

}

static enum ada_err client_recv_post_done(struct http_client *hc)
{
	struct client_state *state = &client_state;
	enum ada_err err;

	err = client_prop_send_done(state, 1, state->prop_send_cb_arg,
	    NODES_ADS, hc);
	if (err == AE_BUF) {
		return err;
	}
	state->prefer_get = 1;
	client_tcp_recv_done(state);

	/*
	 * If not enabled to get commands, defer the enable-listen warning.
	 * This is in case there are a lot of properties to send first.
	 */
	if (!state->ads_cmds_en) {
		client_timer_set(&state->listen_timer, CLIENT_LISTEN_WARN_WAIT);
	}
	return err;
}

static enum ada_err client_recv_echo(struct http_client *hc,
					void *buf, size_t len)
{
	struct client_state *state = &client_state;

	if (buf) {
		return client_recv_drop(hc, buf, len);
	}
	client_finish_echo(state, NODES_ADS);
	state->prefer_get = 1;
	client_tcp_recv_done(state);
	return AE_OK;
}

static enum ada_err client_recv_post(struct http_client *hc,
					void *buf, size_t len)
{
	if (buf) {
		return client_recv_drop(hc, buf, len);
	}
	return client_recv_post_done(hc);
}

/*
 * Encode datapoint metadata.
 * JSON: "metadata":{"key1":"val1","key2":"val2"}}
 */
static void client_ame_format_dp_meta(const struct ame_encoder *enc,
	struct ame_encoder_state *es,
	const struct prop_dp_meta *dp_meta,
	u32 prefix)
{
	struct ame_key key;
	const struct prop_dp_meta *dp_meta_ptr = dp_meta;
	const char meta_tag[] = "metadata";

	key.tag = meta_tag;
	key.display_name = meta_tag;
	enc->enc_key(es, prefix, &key);
	prefix = EF_PREFIX_O;

	while (dp_meta_ptr < dp_meta + PROP_MAX_DPMETA) {
		if (!dp_meta_ptr->key[0] || !dp_meta_ptr->value[0]) {
			break;
		}
		key.tag = dp_meta_ptr->key;
		enc->enc_utf8(es, prefix, &key, dp_meta_ptr->value);
		prefix = EF_PREFIX_C;
		dp_meta_ptr++;
	}
	enc->enc_suffix(es, EF_SUFFIX_E);	/* end of object */
}

static int client_ame_format_dp_data(struct prop *prop,
	char *buf, size_t size)
{
	const struct ame_encoder *enc = client_state.ame.enc;
	struct ame_encoder_state *es = &client_state.ame.es;
	struct ame_key key;
	char *vp;
	u32 vu;
	s32 vs;
	char c_save;

	key.display_name = NULL;
	ame_encoder_buffer_set(es, (u8 *)buf, size);
	key.tag = "datapoint";
	enc->enc_key(es, EF_PREFIX_O, &key);
	key.tag = "name";
	enc->enc_utf8(es, EF_PREFIX_O, &key, prop->name);
	key.tag = "base_type";
	enc->enc_utf8(es, EF_PREFIX_C, &key,
	    lookup_by_val(prop_types, prop->type));

	key.tag = "value";
	switch (prop->type) {
	case ATLV_BOOL:
		vu = tlv_get_bool(prop);
		enc->enc_u32(es, EF_PREFIX_C, &key, vu);
		break;
	case ATLV_INT:
		vs = tlv_get_int(prop);
		enc->enc_s32(es, EF_PREFIX_C, &key, vs);
		break;
	case ATLV_UINT:
		vu = tlv_get_uint(prop);
		enc->enc_u32(es, EF_PREFIX_C, &key, vu);
		break;
	case ATLV_CENTS:
		vs = tlv_get_int(prop);
		enc->enc_d32(es, EF_PREFIX_C, &key, vs, 2);
		break;
	case ATLV_UTF8:
		vp = tlv_get_utf8(prop);
		c_save = vp[prop->len];
		vp[prop->len] = '\0';
		enc->enc_utf8(es, EF_PREFIX_C, &key, vp);
		vp[prop->len] = c_save;
		break;
	default:
		CLIENT_LOGF(LOG_WARN, "prop type err");
		return -1;
	}

	if (prop->dp_meta) {
		client_ame_format_dp_meta(enc, es, prop->dp_meta, EF_PREFIX_C);
	}
	enc->enc_suffix(es, EF_SUFFIX_E);
	enc->enc_suffix(es, EF_SUFFIX_E);

	return es->offset;
}

/*
 * Send changed data to server.
 *
 * The agent_echo flag indicates this is an echo by the agent, as opposed to by
 * the property manager or host app.
 */
static enum ada_err client_send_data_int(struct prop *prop, int agent_echo)
{
	struct client_state *state = &client_state;
#ifdef AYLA_LAN_SUPPORT
	struct client_lan_reg *lan = state->http_lan;
#endif
	struct http_client *hc = NULL;
	enum ada_err err = AE_OK;
	char uri[CLIENT_GET_REQ_LEN];
	char *buf = state->buf;
	char *value;
	int data_len;
	size_t cnt;

	ASSERT(prop);
	ASSERT(prop->name);

#ifdef AYLA_LAN_SUPPORT
	if (lan) {
		return client_send_lan_data(lan, prop, agent_echo);
	}
#endif
	/*
	 * Format into short buffer, to determine the size of request.
	 */
	data_len = client_ame_format_dp_data(prop,
	    state->buf, sizeof(state->buf));

	if (data_len < 0) {
		prop_fmt(state->buf, sizeof(state->buf), prop->type,
		    prop->val, prop->len, &value);
		CLIENT_LOGF(LOG_WARN, "ame_format err");
		CLIENT_LOGF(LOG_DEBUG,
		    "name=\"%s\" val=\"%s\" type=%s",
		    prop->name, value, lookup_by_val(prop_types, prop->type));
		return AE_INVAL_VAL;
	}
	buf = state->buf;

	/*
	 * If buffer was too short, alloc a replacement and repeat format.
	 */
	if (data_len >= sizeof(state->buf)) {
		buf = al_os_mem_alloc(data_len + 1);
		if (!buf) {
			err = AE_ALLOC;
			goto on_error;
		}
		cnt = client_ame_format_dp_data(prop, buf, data_len);
		ASSERT(cnt > 0);
		ASSERT(cnt == data_len);
	}

	hc = client_req_ads_new();
	state->request = CS_POST_DATA;
	hc->body_buf = buf;
	hc->body_buf_len = data_len;
	hc->body_len = data_len;
	if (agent_echo) {
		state->conn_state = CS_WAIT_ECHO;
		hc->client_tcp_recv_cb = client_recv_echo;
	} else {
		hc->client_tcp_recv_cb = client_recv_post;
	}
	snprintf(uri, sizeof(uri),
	    "/devices/%s/properties/%s/datapoints.json%s",
	    state->client_key, prop->name,
	    prop->echo ? "?echo=true" : "");
	client_req_start(hc, HTTP_REQ_POST, uri, &http_hdr_content_json);
	if (buf != state->buf) {
		al_os_mem_free(buf);
	}
	return AE_OK;

on_error:
	if (err != AE_OK) {
		CLIENT_LOGF(LOG_ERR, "send err %d", err);
		return err;
	}
	if (hc) {
		hc->client_send_data_cb = NULL;
		hc->prop_callback = 0;
	}
	return err;
}


enum ada_err client_send_data(struct prop *prop)
{
#ifdef AYLA_BATCH_PROP_SUPPORT
	if (prop->type == ATLV_BATCH) {
		return client_send_batch_dp(prop);
	}
#endif
	return client_send_data_int(prop, 0);
}

#ifdef AYLA_FILE_PROP_SUPPORT
/*
 * Format body with metadata for client_send_dp_loc_req.
 * JSON: {"datapoint":{"metadata":{"key1":"val1","key2":"val2"}}}
 */
static size_t client_ame_format_dp_loc_req(const struct prop_dp_meta *meta,
		    void *buf, size_t size)
{
	const struct ame_encoder *enc = client_state.ame.enc;
	struct ame_encoder_state *es = &client_state.ame.es;
	const AME_KEY1(dp_key, "datapoint");

	ame_encoder_buffer_set(es, (u8 *)buf, size);
	enc->enc_key(es, EF_PREFIX_O, &dp_key);	/* datapoint object */
	client_ame_format_dp_meta(enc, es, meta, EF_PREFIX_O);
	enc->enc_suffix(es, EF_SUFFIX_E);
	enc->enc_suffix(es, EF_SUFFIX_E);
	if (es->offset > size) {
		CLIENT_LOGF(LOG_ERR, "buffer overflow");
		return 0;		/* send without metadata */
	}
	return es->offset;
}

/*
 * Send dp loc request to server.
 */
enum ada_err client_send_dp_loc_req(const char *name,
				const struct prop_dp_meta *meta)
{
	struct client_state *state = &client_state;
	struct http_client *hc;
	const struct http_hdr header[] = {
		{"Range", "bytes=0-99999"},	/* workaround for cloud issue */
		{http_hdr_content_json.name, http_hdr_content_json.val},
		{http_hdr_accept_tag.name, http_hdr_accept_tag.val},
	};

	if (state->get_echo_inprog) {
		/*
		 * we're in the middle of doing ECHOs to LAN clients
		 * so the prop_recvd structure is being used. we need that
		 * structure to store FILE url information. So abort
		 * this operation for now and wait until we get called again.
		 */
		 return AE_BUSY;
	}
	state->prop_send_cb_arg = NULL;

	hc = client_req_ads_new();
	hc->client_tcp_recv_cb = client_recv_prop_val;
	client_ame_init(&state->ame,
	    client_ame_stack, ARRAY_LEN(client_ame_stack),
	    client_ame_parsed_buf, sizeof(client_ame_parsed_buf),
	    client_ame_dp, NULL);
	state->ame_init = 1;
	state->conn_state = CS_WAIT_POST;
	state->request = CS_POST_DP_LOC;

	state->ame_buf[0] = '\0';
	if (meta) {
		hc->body_len = client_ame_format_dp_loc_req(meta,
		    state->ame_buf, sizeof(state->ame_buf));
		hc->body_buf_len = hc->body_len;
	}

	snprintf(state->buf, sizeof(state->buf),
	    "/devices/%s/properties/%s/datapoints.json",
	    state->client_key, name);

	hc->body_buf = state->ame_buf;
	client_req_start_head(hc, HTTP_REQ_POST, state->buf,
	    ARRAY_LEN(header), header);
	return AE_OK;
}

/*
 * Fetch the s3 location of the file datapoint
 */
enum ada_err client_get_dp_loc_req(const char *prop_loc)
{
	struct client_state *state = &client_state;
	struct http_client *hc;
	struct prop_recvd *prop = &prop_recvd;
	const struct http_hdr header[] = {
		{http_hdr_content_json.name, http_hdr_content_json.val},
		{http_hdr_accept_tag.name, http_hdr_accept_tag.val},
	};

	if (state->get_echo_inprog) {
		/*
		 * we're in the middle of doing ECHOs to LAN clients
		 * so the prop_recvd structure is being used. we need that
		 * structure to store FILE url information. So abort
		 * this operation for now and wait until we get called again.
		 */
		 return AE_BUSY;
	}
	if (client_convert_loc_to_url_str(prop_loc,
	    state->buf, sizeof(state->buf))) {
		return AE_INVAL_VAL;
	}

	hc = client_req_ads_new();
	hc->client_tcp_recv_cb = client_recv_prop_cmds;

	state->request = CS_GET_DP_LOC;
	state->get_echo_inprog = 1;

	memset(&prop_recvd, 0, sizeof(prop_recvd));
	client_ame_init(&state->ame,
	    client_ame_stack, ARRAY_LEN(client_ame_stack),
	    client_ame_parsed_buf, sizeof(client_ame_parsed_buf),
	    client_ame_dp, NULL);
	state->ame_init = 1;

	if (prop->file_info.file[0]) {
		client_req_start(hc, HTTP_REQ_GET, state->buf,
		    &http_hdr_content_json);
	} else {
		client_req_start_head(hc, HTTP_REQ_GET, state->buf,
		    ARRAY_LEN(header), header);
	}
	return AE_OK;
}

/*
 * Fetch the datapoint at the location and offset.
 */
enum ada_err client_get_dp_req(const char *prop_loc, u32 data_off, u32 data_end)
{
	struct client_state *state = &client_state;
	struct http_client *hc;
	struct prop_recvd *prop = &prop_recvd;
	const char *url;
	struct http_hdr headers[] = {
		{http_hdr_accept_tag.name, http_hdr_accept_tag.val},
		{"Range", state->buf},
	};
	u8 hcnt = ARRAY_LEN(headers);

	if (prop->file_info.file[0]) {
		url = client_url_resource(prop->file_info.file);
		hc = client_req_file_new();
	} else {
		if (client_convert_loc_to_url_str_pref(prop_loc,
		    state->ame_buf, sizeof(state->ame_buf), "")) {
			client_log(LOG_ERR "message loc invalid: %s", prop_loc);
			return AE_NOT_FOUND;
		}
		url = state->ame_buf;
		hc = client_req_ads_new();
	}

	state->conn_state = CS_WAIT_FILE_GET;
	state->request = CS_GET_DP;
	hc->client_tcp_recv_cb = client_recv_dp;
	state->prop_send_cb_arg = hc;

	snprintf(state->buf, sizeof(state->buf), "bytes=%lu-%lu",
	    data_off, data_end);

	client_log(LOG_DEBUG "GET dp %s", state->buf);
	client_req_start_head(hc, HTTP_REQ_GET, url, hcnt, headers);
	state->get_echo_inprog = 1;
	return AE_OK;
}
#endif

/*
 * Handle completion of PUT oem_info
 */
static void client_info_put_done(struct client_state *state,
				struct http_client *hc)
{
	const char *msg;

	/*
	 * Warning: do not change the format of these messages for OEM info
	 * without considering and testing the ayla_verify script.
	 * The expected format is "client: OEM info put <status>" where
	 * status is either "OK", "not needed", or "put failed".
	 * There can be more info on the line after "failed".
	 */
	msg = "info put";
	if (state->request == CS_PUT_OEM_INFO) {
		msg = "OEM info put";		/* expected - see note above */
	} else if (state->info_flags_sending & CLIENT_UPDATE_OEM) {
		msg = "info put OEM auth";
	}
	if (hc->hc_error != HC_ERR_NONE) {
		if (hc->hc_error == HC_ERR_HTTP_STATUS) {
			client_log(LOG_WARN LOG_EXPECTED
			    "%s failed status %lu",
			     msg, hc->http_status);
		} else {
			client_log(LOG_WARN LOG_EXPECTED
			    "%s failed err %u",
			     msg, hc->hc_error);
		}
		if (state->request == CS_PUT_INFO) {
			state->client_info_flags |= state->info_flags_sending;
		}
	} else {
		client_log(LOG_INFO LOG_EXPECTED "%s OK", msg);
	}
	state->client_info_flags &= ~state->info_flags_sending;
	state->info_flags_sending = 0;
	if (!state->serv_conn) {
		http_client_set_retry_limit(hc, -1);
		client_server_conn_up(state);
	} else {
		client_event_send(AE_OK);
	}
	client_tcp_recv_done(state);
}

/*
 * Receive response for PUT oem_info.json
 */
static enum ada_err client_recv_oem_info(struct http_client *hc,
					void *payload, size_t len)
{
	struct client_state *state = &client_state;
	struct ada_conf *cf = &ada_conf;
	jsmn_parser parser;
	jsmntok_t tokens[40];	/* plenty of tokens for expansion */
	jsmnerr_t jerr;
	char prodname[CLIENT_CONF_SYMNAME_LEN + 1];

	if (payload) {
		if (len + state->recved_len > sizeof(state->buf)) {
			CLIENT_LOGF(LOG_ERR, "recv buf len exceeded");
			len = sizeof(state->buf) - state->recved_len;
		}
		memcpy(state->buf + state->recved_len, payload, len);
		state->recved_len += len;
		return AE_OK;
	}

	/*
	 * Receive complete.  Check length and NUL-terminate buffer.
	 */
	len = state->recved_len;
	if (len >= sizeof(state->buf)) {
		client_log(LOG_WARN "%s response too long %zu", __func__, len);
		return AE_ERR;
	}
	state->buf[len] = '\0';

	/*
	 * Parse response.
	 */
	jsmn_init_parser(&parser, state->buf, tokens, ARRAY_LEN(tokens));
	jerr = jsmn_parse(&parser);
	if (jerr != JSMN_SUCCESS) {
		client_log(LOG_WARN LOG_EXPECTED
		    "OEM info put failed json parse err %d", jerr);
		return AE_ERR;
	}
	if (jsmn_get_string(&parser, NULL, "registration_type",
	    state->reg_type, sizeof(state->reg_type)) <= 0) {
		client_log(LOG_WARN "%s no reg_type", __func__);
	}
	if (jsmn_get_string(&parser, NULL, "product_name",
	    prodname, sizeof(prodname)) <= 0) {
		client_log(LOG_WARN "%s no product_name", __func__);
	}
	if ((state->client_info_flags & CLIENT_UPDATE_PRODUCT_NAME) == 0) {
		strncpy(cf->host_symname, prodname,
		    sizeof(cf->host_symname) - 1);
	}
	client_info_put_done(state, hc);
	return AE_OK;
}

static void client_template_timeout(struct timer *tm)
{
	struct client_state *state = &client_state;

	if (!ada_host_template_version || !ada_host_template_version[0]) {
		client_log(LOG_WARN "template_version not set by host app");
		state->template_version_timeout = 1;
		client_wakeup();
	}
}

/*
 * Check OEM info.
 * The OEM and OEM model should be in the configuration or built into
 * the host app. The OEM key must be set in manufacturing.
 * Check them here.
 * The template version may be set later, so is checked in put_oem_info().
 */
static int client_check_oem_info(void)
{
	int conf_error = 0;

	if (!oem[0]) {
		client_log(LOG_ERR "OEM ID not set");
		conf_error = 1;
	}
	if (!oem_model[0]) {
		client_log(LOG_ERR "OEM model not set");
		conf_error = 1;
	}
	if (!oem_key_len) {
		client_log(LOG_ERR "OEM key not set");
		conf_error = 1;
	}
	return conf_error;
}

static int client_put_oem_info(struct client_state *state)
{
	struct http_client *hc;
	char uri[CLIENT_GET_REQ_LEN];
	size_t outlen;
	int rc;
	char buf[BASE64_LEN_EXPAND(CONF_OEM_KEY_MAX) + 1];
	int conf_error;

	conf_error = client_check_oem_info();

	/*
	 * The template version may be set by an external MCU but should come
	 * through the host app.
	 */
	if (!ada_host_template_version || !ada_host_template_version[0]) {
		client_log(LOG_WARN "template_version is not set");
		if (!timer_active(&state->template_timer)) {
			client_timer_set(&state->template_timer,
			    CLIENT_TEMPLATE_TIMEOUT);
		}
		return -1;
	}
	if (conf_error) {
		return -1;
	}

	/*
	 * Form payload.
	 */
	outlen = sizeof(buf);
	rc = ayla_base64_encode((u8 *)oem_key, oem_key_len, (u8 *)buf, &outlen);
	if (rc < 0) {
		client_log(LOG_ERR "oem_key: base64 fail %d", rc);
		return -1;
	}
	rc = snprintf(state->ame_buf, sizeof(state->ame_buf),
	    "{\"oem\":\"%s\", \"oem_model\":\"%s\","
	    "\"oem_key\":\"%s\",\"template_version\":\"%s\"}",
	    oem, oem_model, buf,
	    ada_host_template_version ? ada_host_template_version : "");
	if (rc < 0 || rc >= sizeof(state->ame_buf)) {
		client_log(LOG_ERR "oem_info too long (%d)", rc);
		return -1;
	}

	/*
	 * Send request.
	 */
	hc = client_req_ads_new();
	ASSERT(hc);
	hc->client_tcp_recv_cb = client_recv_oem_info;
	state->conn_state = CS_WAIT_INFO_PUT;
	state->request = CS_PUT_OEM_INFO;

	hc->body_buf = state->ame_buf;
	hc->body_buf_len = rc;
	hc->body_len = rc;

	state->info_flags_sending = CLIENT_UPDATE_OEM;
	state->client_info_flags &= ~CLIENT_UPDATE_OEM;

	snprintf(uri, sizeof(uri), "/devices/%s/oem_info.json",
	    state->client_key);
	client_req_start(hc, HTTP_REQ_PUT, uri, &http_hdr_content_json);
	return 0;
}

/*
 * Generate body of PUT of client info.
 * Fill provided buffer and return the length used.
 * This may be done once to determine the content-length and a second time
 * to generate the data.
 * Len must be long enough for oem_data + all flags, around 600 bytes.
 */
static size_t client_gen_info_data(struct client_state *state,
					char *buf, size_t buf_len, u16 flags)
{
	struct ada_conf *cf = &ada_conf;
	const struct ame_encoder *enc = client_state.ame.enc;
	struct ame_encoder_state *es = &client_state.ame.es;
	struct ame_key key;
	char ip[30];
	u32 ip4addr;
	char ssid_uri[SSID_URI_LEN];
#if defined(AYLA_SCM_SUPPORT)
    /* XXX: to avoid shadowing warning */
	char _oem_key[CONF_OEM_KEY_MAX];
#else
	char oem_key[CONF_OEM_KEY_MAX];
#endif
	const u8 *mac;
#if defined(AYLA_SCM_SUPPORT)
    /* XXX: to avoid shadowing warning */
	int _oem_key_len;
#else
	int oem_key_len;
#endif
	char mac_tmp[20];
	struct al_net_if *netif_default;
	char is_filled = 0;
	u32 prefix_flag = 0;

	key.display_name = NULL;
	ame_encoder_buffer_set(es, (u8 *)buf, buf_len);

	key.tag = "device";
	enc->enc_key(es, EF_PREFIX_O, &key);
	enc->enc_prefix(es, EF_PREFIX_O);

	if (flags & (CLIENT_UPDATE_MAJOR | CLIENT_UPDATE_MINOR)) {
		prefix_flag = is_filled ? EF_PREFIX_C : 0;
		key.tag = "api_major";
		enc->enc_u32(es, prefix_flag, &key, CLIENT_API_MAJOR);
		key.tag = "api_minor";
		enc->enc_u32(es, EF_PREFIX_C, &key, CLIENT_API_MINOR);
		is_filled = 1;
	}
	if (flags & CLIENT_UPDATE_SWVER) {
		prefix_flag = is_filled ? EF_PREFIX_C : 0;
		key.tag = "sw_version";
		enc->enc_utf8(es, prefix_flag, &key, adap_conf_sw_build());
		is_filled = 1;
	}
	netif_default = al_net_if_get(AL_NET_IF_DEF);
	if ((flags & CLIENT_UPDATE_LANIP) && netif_default) {
		ip4addr = al_net_if_get_ipv4(netif_default);
		prefix_flag = is_filled ? EF_PREFIX_C : 0;
		key.tag = "lan_ip";
		enc->enc_utf8(es, prefix_flag, &key,
		    ipaddr_fmt_ipv4_to_str(ip4addr, ip, sizeof(ip)));
		is_filled = 1;
	}
	if (flags & CLIENT_UPDATE_MODEL) {
		prefix_flag = is_filled ? EF_PREFIX_C : 0;
		key.tag = "model";
		enc->enc_utf8(es, prefix_flag, &key, conf_sys_model);
		is_filled = 1;
	}
	if (flags & CLIENT_UPDATE_SETUP) {
		prefix_flag = is_filled ? EF_PREFIX_C : 0;
		key.tag = "setup_token";
		enc->enc_utf8(es, prefix_flag, &key, state->setup_token);
		is_filled = 1;
	}
	if (flags & CLIENT_UPDATE_SETUP_LOCATION && state->setup_location) {
		prefix_flag = is_filled ? EF_PREFIX_C : 0;
		key.tag = "setup_location";
		enc->enc_utf8(es, prefix_flag, &key, state->setup_location);
		is_filled = 1;
	}
	if (flags & CLIENT_UPDATE_SSID) {
		client_get_ssid_uri(ssid_uri, sizeof(ssid_uri));
		prefix_flag = is_filled ? EF_PREFIX_C : 0;
		key.tag = "ssid";
		enc->enc_utf8(es, prefix_flag, &key, ssid_uri);
		is_filled = 1;
	}
	if (flags & CLIENT_UPDATE_PRODUCT_NAME) {
		prefix_flag = is_filled ? EF_PREFIX_C : 0;
		key.tag = "product_name";
		enc->enc_utf8(es, prefix_flag, &key, cf->host_symname);
		is_filled = 1;
	}
	if (flags & CLIENT_UPDATE_OEM) {
		prefix_flag = is_filled ? EF_PREFIX_C : 0;
		key.tag = "oem";
		enc->enc_utf8(es, prefix_flag, &key, oem);
		key.tag = "oem_model";
		enc->enc_utf8(es, EF_PREFIX_C, &key, oem_model);

		_oem_key_len = adap_conf_oem_key_get(_oem_key, sizeof(_oem_key));
		if (_oem_key_len > 0) {
			key.tag = "oem_key";
			enc->enc_opaque(es, EF_PREFIX_C, &key, _oem_key,
			    _oem_key_len);
		}
		is_filled = 1;
	}
	if (flags & CLIENT_UPDATE_MAC) {
		mac = cf->mac_addr;
		memset(mac_tmp, 0, sizeof(mac_tmp));
		snprintf(mac_tmp, sizeof(mac_tmp),
		    "%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x",
		    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		prefix_flag = is_filled ? EF_PREFIX_C : 0;
		key.tag = "mac";
		enc->enc_utf8(es, prefix_flag, &key, mac_tmp);
		is_filled = 1;
	}
	if (flags & CLIENT_UPDATE_HW_ID) {
		prefix_flag = is_filled ? EF_PREFIX_C : 0;
		key.tag = "hwsig";
		enc->enc_utf8(es, prefix_flag, &key, cf->hw_id);
		is_filled = 1;
	}
	if (!is_filled) {
		return 0;
	}
	enc->enc_suffix(es, EF_SUFFIX_E);
	enc->enc_suffix(es, EF_SUFFIX_E);

	return es->offset;
}

/*
 * Handle response from PUT info
 */
static enum ada_err client_recv_info(struct http_client *hc,
					void *payload, size_t len)
{
	struct client_state *state = &client_state;

	if (!payload) {
		client_info_put_done(state, hc);
	}
	return AE_OK;
}

/*
 * See if any client info received is incorrect and
 * should be updated to the server.
 */
static int client_put_info(struct client_state *state)
{
	struct http_client *hc;
	char uri[CLIENT_GET_REQ_LEN];
	u16 flags = state->client_info_flags;

	if (!flags) {
		return -1;
	}

	/*
	 * Normally OEM info is sent using PUT oem_info, but we use PUT info
	 * for OEM as well if the template version has timed out.
	 */
	if ((flags & CLIENT_UPDATE_OEM) && !state->template_version_timeout) {
		flags &= ~CLIENT_UPDATE_OEM;
		if (!flags) {
			return client_put_oem_info(state);
		}
	}

	hc = client_req_ads_new();
	ASSERT(hc);
	hc->client_tcp_recv_cb = client_recv_info;
	state->conn_state = CS_WAIT_INFO_PUT;
	state->request = CS_PUT_INFO;

	/*
	 * Generate data to determine content-len.
	 */
	hc->body_len = client_gen_info_data(state,
	    state->ame_buf, sizeof(state->ame_buf), flags);
	if (!hc->body_len) {
		state->client_info_flags = 0;
		state->conn_state = CS_WAIT_EVENT;
		return -1;
	}
	hc->body_buf = state->ame_buf;
	hc->body_buf_len = hc->body_len;

	/*
	 * Clear client_info_flags so we can record new changes
	 */
	state->info_flags_sending = flags;
	state->client_info_flags &= ~flags;

	snprintf(uri, sizeof(uri), "/devices/%s.json", state->client_key);
	client_req_start(hc, HTTP_REQ_PUT, uri, &http_hdr_content_json);
	return 0;
}

/*
 * Put reset.json
 * Parses a reset command. Schedules a reset for the module.
 */
static void client_reset_json_put(struct server_req *req)
{
	client_lock();
	req->tcpip_cb = &ada_conf_reset_cb;
	if (server_get_bool_arg_by_name(req, "factory")) {
		ada_conf_reset_factory = 1;
	}
	server_put_status(req, HTTP_STATUS_NO_CONTENT);
	client_unlock();
}

/*
 * Put lanip.json
 * Sets lanip key according to the tokens given.
 */
static void client_lanip_json_put(struct server_req *req)
{
#if defined(AYLA_LAN_SUPPORT) || defined(AYLA_LOCAL_CONTROL_SUPPORT)
	struct client_state *state = &client_state;

	client_lock();
	state->lanip_fetch = 0;
	if (client_lanip_json_parse(req->post_data)) {
		server_put_status(req, HTTP_STATUS_BAD_REQ);
	} else {
		server_put_status(req, HTTP_STATUS_NO_CONTENT);
	}
	client_unlock();
#else
	/* no LAN support: ignore request */
	server_put_status(req, HTTP_STATUS_NO_CONTENT);
#endif
}

static void client_cmd_put_rsp(struct client_state *state, unsigned int status)
{
	struct http_client *hc = &state->http_client;
	char uri[CLIENT_GET_REQ_LEN];

	if (state->tgt == CCT_LAN) {
		snprintf(uri, sizeof(uri), "/ota_status.json?status=%u",
		    status);
	} else {
		snprintf(uri, sizeof(uri),
		    "/devices/%s/%s?cmd_id=%lu&status=%u",
		    state->client_key, state->cmd.uri, state->cmd.id, status);
	}

	client_req_start(hc, HTTP_REQ_PUT, uri, &http_hdr_content_json);
}

/*
 * Send the PUT header for an ADS command response
 * Arg body_len is the anticipated length of the entire response.  It is used
 * only if the http_client request has not already started.
 */
static void client_rev_rest_put(struct client_state *state,
				const void *buf, size_t len, size_t body_len)
{
	struct server_req *req = &state->cmd_req;
	struct http_client *hc = &state->http_client;
	unsigned int status;
	void *data;

	if (req->req_impl) {
		if (len) {
			client_lock();
			req->err = client_req_send(hc, buf, len);
			if (req->err == AE_OK) {
				state->cmd.output_len += req->len;
			}
			client_unlock();
		}
		return;
	}

	status = req->http_status;
	if (!status || status == HTTP_STATUS_OK) {
		data = al_os_mem_alloc(len);
		if (!data) {
			req->err = AE_BUF;
			return;
		}
		memcpy(data, buf, len);
	} else {
		data = NULL;
		len = 0;
		body_len = 0;
	}

	client_lock();
	req->req_impl = hc;
	hc->body_buf = data;
	hc->body_buf_len = len;
	hc->body_len = body_len;

	hc->client_send_data_cb = http2mqtt_client_send_pad;
	hc->client_tcp_recv_cb = client_recv_cmd_put;
	state->conn_state = CS_WAIT_CMD_PUT;

	client_cmd_put_rsp(state, status);
	client_unlock();
}

/*
 * Command to flush cmd data from server to ADS
 * The message or req->buf may be on the stack, so they must
 * be copied to something more persistent for the send callback.
 */
static void client_cmd_flush(struct server_req *req, const char *msg)
{
	struct client_state *state = &client_state;
	const char *data;
	size_t len;

	if (req->suppress_out || req->len == 0 || req->err != AE_OK) {
		return;
	}

	if ((req->len + state->cmd.output_len) > (MAX_CMD_RESP - 1)) {
		return;
	}

	data = msg ? msg : req->buf;
	len = req->len;
	client_rev_rest_put(state, data, len, MAX_CMD_RESP);
	req->len = 0;
}

/*
 * Drop the header for a reverse-REST request.
 * Remember the status for later.
 */
void client_cmd_put_head(struct server_req *req, unsigned int status,
				const char *content_type)
{
	req->http_status = (u16)status;
}

/*
 * Start a PUT for a command status report - delayed response for rev-REST
 */
static int client_put_cmd_sts(struct client_state *state)
{
	struct http_client *hc;

	if (state->ota.in_prog != COS_CMD_STATUS || !state->ota.http_status) {
		return -1;
	}
#ifdef AYLA_LAN_SUPPORT
	if (state->ota_server.lan) {
		hc = client_req_new(CCT_LAN);
	} else {
		hc = client_req_ads_new();
	}
#else
	hc = client_req_ads_new();
#endif
	state->conn_state = CS_WAIT_CMD_STS_PUT;
	hc->client_tcp_recv_cb = client_recv_sts_put;
	client_cmd_put_rsp(state, state->ota.http_status);
	return 0;
}

/*
 * Handle response from PUT info
 */
static enum ada_err client_reg_window_recv(struct http_client *hc,
				void *payload, size_t len)
{
	struct client_state *state = &client_state;

	if (!payload) {
		client_step_disable(ADCP_REG_WINDOW_START);
		client_tcp_recv_done(state);
	}
	return AE_OK;
}

/*
 * Send PUT for register window start.
 */
static int client_put_reg_window_start(struct client_state *state)
{
	struct http_client *hc;
	char uri[CLIENT_GET_REQ_LEN];

	hc = client_req_ads_new();
	ASSERT(hc);
	hc->client_tcp_recv_cb = client_reg_window_recv;
	state->conn_state = CS_WAIT_REG_WINDOW;
	state->request = CS_PUT_INFO;

	/*
	 * Generate data to determine content-len.
	 */
	hc->body_buf = "{}";
	hc->body_buf_len = 2;
	hc->body_len = 2;

	snprintf(uri, sizeof(uri), "/devices/%s/start_reg_window.json",
	    state->client_key);
	client_req_start(hc, HTTP_REQ_PUT, uri, &http_hdr_content_json);
	return 0;
}

/*
 * Put registration.json
 * Notification of user registration change.
 */
static void client_registration_json_put(struct server_req *req)
{
	struct ada_conf *cf = &ada_conf;
	jsmn_parser parser;
	jsmntok_t tokens[CLIENT_REG_JSON_TOKENS];
	jsmntok_t *reginfo;
	jsmnerr_t err;
	char status[4];
	int changed = 0;

	client_lock();

	/* Ignore empty PUTs */
	if (req->post_data == NULL) {
		goto err;
	}

	/* Extract the status from the JSON registration record */
	jsmn_init_parser(&parser, req->post_data, tokens, ARRAY_LEN(tokens));
	err = jsmn_parse(&parser);
	if (err != JSMN_SUCCESS) {
		CLIENT_LOGF(LOG_WARN, "jsmn err %d", err);
		goto err;
	}
	reginfo = jsmn_get_val(&parser, NULL, "registration");
	if (!reginfo) {
		CLIENT_LOGF(LOG_WARN, "no registration");
		goto err;
	}
	if (jsmn_get_string(&parser, reginfo, "status",
	    status, sizeof(status)) < 0) {
		CLIENT_LOGF(LOG_WARN, "bad reg status");
		goto err;
	}

	/* Update ther user registration status and send event */
	cf->event_mask &= ~(CLIENT_EVENT_UNREG | CLIENT_EVENT_REG);
	if (!strcmp(status, "0")) {
		/* Deregistration */
		changed = cf->reg_user;
		cf->reg_user = 0;
		cf->event_mask |= CLIENT_EVENT_UNREG;
	} else if (!strcmp(status, "1")) {
		/* First registration */
		changed = !cf->reg_user;
		cf->reg_user = 1;
		cf->event_mask |= CLIENT_EVENT_REG;
	} else if (!strcmp(status, "2")) {
		/* Re-registration */
		if (cf->reg_user) {
			cf->reg_user = 0;
			adap_conf_reg_changed();
		}
		cf->reg_user = 1;
		cf->event_mask |= (CLIENT_EVENT_REG | CLIENT_EVENT_UNREG);
		changed = 1;
	}

	/* If config changed, save state and notify MCU */
	if (changed) {
		client_conf_reg_persist();
		adap_conf_reg_changed();
	}
	server_put_status(req, HTTP_STATUS_NO_CONTENT);
	client_unlock();
	return;

err:
	server_put_status(req, HTTP_STATUS_BAD_REQ);
	client_unlock();
}

/*
 * Finish the command response for ADS
 */
static enum ada_err client_cmd_finish_put(struct server_req *req)
{
	struct client_state *state = &client_state;
	struct http_client *hc = &state->http_client;

	client_rev_rest_put(state, req->buf, req->len, hc->body_buf_len);
	return AE_OK;
}

/*
 * Start a reverse-REST command to the internal server.
 * state->cmd and state->cmd_req have been partly set up with the request.
 * This is called for LAN mode as well as for ADS commands.
 */
void client_rev_rest_cmd(struct http_client *hc, u8 cmd_priv)
{
	struct client_state *state = &client_state;
	struct server_req *cmd_req = &state->cmd_req;
	const char *method = state->cmd.method;
	char *resource = state->cmd.resource;
	const struct url_list *tt;
	char buf[SERVER_BUFLEN];

	memset(&prop_recvd, 0, sizeof(prop_recvd));
	cmd_req->buf = buf;
	cmd_req->post_data = state->cmd.data;
	state->cmd.output_len = 0;

	CLIENT_LOGF(LOG_DEBUG2, "resource %s", resource);

	tt = server_find_handler(cmd_req, resource, server_parse_method(method),
	    cmd_priv);
	ASSERT(tt);
	cmd_req->url = tt;
	cmd_req->resume = tt->url_op;

	/*
	 * Start reverse-REST request.
	 * Drop client lock during the reverse-REST.
	 * The client won't re-use cmd_req because either cmd_pending or
	 * lan_cmd_pending are set.
	 */
#ifdef AYLA_LAN_SUPPORT
	ASSERT(state->cmd_pending || state->lan_cmd_pending);
#else
	ASSERT(state->cmd_pending);
#endif
	client_unlock();
	if (state->cmd.overflow) {
		server_put_status(cmd_req, HTTP_STATUS_REQ_LARGE);
	} else {
		tt->url_op(cmd_req);
	}
	if (!cmd_req->user_in_prog && cmd_req->err == AE_OK) {
		CLIENT_LOGF(LOG_DEBUG2, "finish write");
		cmd_req->finish_write(cmd_req);
	}
	client_lock();
}

/*
 * Execute the cmd request from ADS or LAN.  The cmd will PUT back the result
 */
static void client_cmd_put(struct client_state *state)
{
	struct server_req *cmd_req = &state->cmd_req;
	struct http_client *hc;

	server_req_init(cmd_req);
	ASSERT(client_locked);
	cmd_req->put_head = client_cmd_put_head;
	cmd_req->write_cmd = client_cmd_flush;
	cmd_req->finish_write = client_cmd_finish_put;
	cmd_req->admin = 1;
	hc = client_req_ads_new();

	client_rev_rest_cmd(hc, ADS_REQ);
	if (cmd_req->user_in_prog) {
		state->cmd_rsp_pending = 1;
	}
}

/*
 * Common error handling for client send callbacks.
 */
static void client_send_next(struct http_client *hc, enum ada_err err)
{
	struct client_state *state = &client_state;

	if (hc->user_in_prog) {
		return;
	}
	if (err == AE_OK) {
		if (!hc->chunked_eof) {
			if (hc->sending_chunked) {
				client_prop_send_done(state, 1,
				    NULL, NODES_ADS, hc);
				return;
			}
#ifdef AYLA_BATCH_PROP_SUPPORT
			if (hc->http_tx_chunked) {
				return;
			}
#endif
		}
		hc->sending_chunked = 0;
		client_req_send_complete(hc);
	} else if (err != AE_BUF) {
		if (state->conn_state == CS_WAIT_POST &&
		    err != AE_BUSY) {
			/* non-recoverable error */
			client_prop_send_done(state, 0, NULL, NODES_ADS, hc);
#ifdef AYLA_FILE_PROP_SUPPORT
			if (state->wait_for_file_get ||
			    state->wait_for_file_put) {
				client_unlock();
				client_abort_file_operation();
				client_lock();
			}
#endif
		}
		hc->prop_callback = 0;
		client_retry(state);
	}
}

/*
 * Start a POST for sending echo of a property.
 */
static int client_post_echo(struct client_state *state)
{
	struct prop_recvd *echo_prop;
	struct prop *prop;

	if (!(state->echo_dest_mask & NODES_ADS)) {
		return -1;
	}
	echo_prop = state->echo_prop;
	ASSERT(echo_prop);
#ifdef AYLA_LAN_SUPPORT
	state->http_lan = NULL;
#endif

	prop = &echo_prop->prop;
	prop->name = echo_prop->name;
	prop->val = echo_prop->val;
	prop->len = echo_prop->prop.len;
	prop->type = echo_prop->type;
	prop->echo = 1;

	client_send_data_int(prop, 1);
	return 0;
}

/*
 * Send callback for additional property POST data.
 */
void client_send_post(struct http_client *hc)
{
	struct client_state *state = &client_state;
	enum ada_err err;

	state->conn_state = CS_WAIT_POST;
	hc->prop_callback = 1;
#ifdef AYLA_LAN_SUPPORT
	state->http_lan = NULL;
#endif
	/* note: this callback might actually do a GET */
	err = state->prop_send_cb(PROP_CB_BEGIN, NULL);
	client_send_next(hc, err);
}

/*
 * Reset the mcu overflow flag (in case its set)
 */
void client_reset_mcu_overflow(void)
{
	client_state.mcu_overflow = 0;
}

#ifdef AYLA_FILE_PROP_SUPPORT
/*
 * Indicate if a file property transfer is ongoing.
 */
u8 client_ongoing_file(void)
{
	struct client_state *state = &client_state;

	return state->wait_for_file_get || state->wait_for_file_put;
}
#endif

/*
 * There are properties to send.
 * This is called by the prop module when a property changes value.
 */
void client_send_callback_set(enum ada_err (*callback)(enum prop_cb_status stat,
				void *arg), u8 dest_mask)
{
	struct client_state *state = &client_state;
#ifdef AYLA_LAN_SUPPORT
	struct ada_lan_conf *lcf = &ada_lan_conf;
	u8 lan_enable = lcf->enable_mask & LAN_CONF_ENABLE_LAN_BIT;
#else
	u8 lan_enable = 0;
#endif
	struct http_client *hc = &state->http_client;
#ifdef AYLA_FILE_PROP_SUPPORT
	struct prop_recvd *prop = &prop_recvd;
#endif

	if (!callback) {
		return;
	}
	client_lock();
	state->prop_send_cb = callback;
	state->mcu_overflow = 0;

	if (state->conn_state == CS_WAIT_POST &&
	    (hc->sending_chunked || hc->http_tx_chunked)) {
		/* in the middle of a FILE property upload or
		 * http payload data chunk.
		 */
		state->dest_mask = NODES_ADS;
		client_send_post(hc);
		goto unlock;
	}
	METRIC_INCR(M_PROP_SEND_ATTEMPTS, 0, 0);
	METRIC_OP_BEGIN(M_PROP_SEND_LATENCY, 0);
	state->dest_mask = state->valid_dest_mask;
	if (!dest_mask && !lan_enable) {
		/* Try sending to ADS even if we're not connected */
		state->dest_mask |= NODES_ADS;
	} else if (dest_mask) {
		state->dest_mask &= dest_mask;
		state->failed_dest_mask = dest_mask &
		    ~state->valid_dest_mask;
	}
#ifdef AYLA_FILE_PROP_SUPPORT
	if (callback == prop_mgr_dp_put && (!state->wait_for_file_put ||
	    !prop->is_file ||
	    strcmp(prop_dp_put_get_loc(), prop->file_info.location))) {
		/* if prop_mgr does an unexpected PUT_DP */
		client_log(LOG_DEBUG "file location name check failed");
		state->wait_for_file_put = 0;
		state->unexp_op = 1;
		prop->is_file = 0;
		client_prop_send_done(state, 0, NULL, state->dest_mask, hc);
		goto unlock;
	}
#endif
	if (!state->dest_mask) {
		client_connectivity_update();
		client_prop_send_done(state, 0, NULL, 0, hc);
		goto unlock;
	}
	if (state->dest_mask & NODES_ADS) {
		http_client_set_retry_limit(hc, -1);
		if (state->conn_state == CS_WAIT_RETRY ||
		    state->conn_state == CS_WAIT_CONN) {
			client_close(state);
			state->conn_state = CS_WAIT_EVENT;
		}
	}
	client_wakeup();
unlock:
	client_unlock();
}

#ifdef AYLA_FILE_PROP_SUPPORT
/*
 * Aborts any ongoing file operations
 */
void client_abort_file_operation(void)
{
	struct client_state *state = &client_state;

	client_lock();
	if (state->tgt == CCT_FILE_PROP_SERVER && state->wait_for_file_get) {
		state->get_echo_inprog = 0;
	}
	client_close(state);
	state->conn_state = CS_WAIT_EVENT;
	state->wait_for_file_put = 0;
	state->wait_for_file_get = 0;
	memset(&prop_recvd, 0, sizeof(prop_recvd));
	state->prop_send_cb = NULL;
	state->dest_mask = 0;
	client_wakeup();
	client_unlock();
}
#endif

/*
 * Callback from MQTT or HTTP client on error.
 */
static void client_err_cb(struct http_client *hc)
{
	struct client_state *state = &client_state;
	int slow_retry = 0;

	ASSERT(client_locked);
	client_timer_cancel(&state->req_timer);

	if (state->conn_state == CS_WAIT_LOGS_POST) {
		log_buf_send_event(LOG_EVENT_NOSEND);
	}
	client_log(LOG_DEBUG "http err %d", hc->hc_error);

	if (state->conn_state == CS_WAIT_GET) {
		state->get_echo_inprog = 0;
		state->cmd_event = 1;
		if (hc->hc_error != HC_ERR_HTTP_STATUS) {
			state->get_all = 1; /* props might've been dropped */
		}
		if (state->get_cmds_fail) {
			/*
			 * protection against the case where GET cmds
			 * keeps failing. reset the cmd_event flag
			 * after some time and try again.
			 */
			client_timer_set(&state->cmd_timer,
			    CLIENT_CMD_RETRY_WAIT);
			state->cmd_event = 0;
		}
		state->get_cmds_fail = 1;
	} else if (state->conn_state == CS_WAIT_PROP_GET) {
		state->get_echo_inprog = 0;
	}

	/*
	 * if an error occurs while doing a DP PUT, clear out all state from
	 * module. the host mcu must restart the dp
	 */
	if (state->wait_for_file_put) {
		state->wait_for_file_put = 0;
		state->get_echo_inprog = 0;
		memset(&prop_recvd, 0, sizeof(prop_recvd));
	}

	switch (hc->hc_error) {
	case HC_ERR_TIMEOUT:
		/*
		 * DEV-1911: Timeout may have happened due trying to echo
		 * prop to LAN in this case don't set mcu_overflow
		 */
		if (!state->get_echo_inprog) {
			state->mcu_overflow = 1;
		}
		if (state->request == CS_GET_COMMANDS) {
			prop_mgr_event(PME_TIMEOUT, NULL);
		} else {
			client_prop_send_done(state, 0, NULL, NODES_ADS, hc);
		}
		goto next_step;
		break;
	case HC_ERR_HTTP_STATUS:
		/*
		 * If authentication failed, clear out the auth header
		 * to force re-authentication.
		 * Also, fix the time if it's provided.
		 */
		if (hc->http_status == HTTP_STATUS_UNAUTH &&
		    hc->client_auth) {
			if (state->tgt == CCT_IMAGE_SERVER) {
				state->ota.auth_fail = 1;
			}

			/*
			 * If possibly due to OEM authentication or template
			 * association, handle the OEM put completion first,
			 * for the benefit of the ayla_verify script.
			 */
			if (state->conn_state == CS_WAIT_INFO_PUT ||
			    state->conn_state == CS_WAIT_OEM_INFO_PUT) {
				client_info_put_done(state, hc);
				state->conn_state = CS_WAIT_EVENT;
			}
			client_log(LOG_WARN "auth expiration or failure"
			    " state=%s", client_conn_states[state->conn_state]);
			hc->auth_hdr[0] = '\0';
			goto auth_error;
		}
		if (hc->http_status == HTTP_STATUS_TOO_MANY ||
		    hc->http_status == HTTP_STATUS_SERV_UNAV) {
			server_req_done_callback(&state->cmd_req);
			state->request = CS_IDLE;
			state->conn_state = CS_WAIT_EVENT;
			CLIENT_LOGF(LOG_DEBUG, "calling retry_exp"); /* XXX */
			client_retry_exp(state);
			return;
		}
		/* fall-through */
	case HC_ERR_HTTP_PARSE:
	case HC_ERR_HTTP_REDIR:
		switch (state->conn_state) {
		case CS_WAIT_OTA_GET:
			ada_ota_report_int(PB_ERR_GET);
			break;
		case CS_WAIT_OTA_PUT:
			ada_ota_report_int(PB_DONE);
			break;
		case CS_WAIT_CMD_STS_PUT:
			client_ota_set_sts_rpt(state, HTTP_STATUS_OK);
			break;
		case CS_WAIT_LANIP_GET:
			state->lanip_fetch = 0;
			break;
		case CS_WAIT_ID:
			client_get_dev_id_pend(state);
			goto retry;
		case CS_WAIT_CMD_PUT:
		case CS_WAIT_ECHO:
		case CS_WAIT_GET:
		case CS_WAIT_PING:
		case CS_WAIT_REG_WINDOW:
			goto retry;
		case CS_WAIT_INFO_PUT:
		case CS_WAIT_OEM_INFO_PUT:
			client_info_put_done(state, hc);
			break;
		case CS_WAIT_POST:
		case CS_WAIT_PROP_GET:
		case CS_WAIT_FILE_GET:
		default:
			client_prop_send_done(state, 0, hc, NODES_ADS, hc);
			break;
		}
next_step:
		state->request = CS_IDLE;
		state->conn_state = CS_WAIT_EVENT;
		client_wakeup();
		break;
	case HC_ERR_CONN_CLSD:
	default:
		if (state->tgt == CCT_IMAGE_SERVER || state->tgt == CCT_LAN ||
		    state->tgt == CCT_REMOTE) {
			if (state->retries > 5) {
				if (state->tgt == CCT_LAN) {
					/*
					 * For LAN OTA we send status to LAN
					 * server. So drop status if connection
					 * fails.
					 */
					state->ota.http_status = 0;
					state->ota_server.lan = 0;
					client_ota_cleanup(state);
				} else {
					ada_ota_report_int(PB_ERR_CONNECT);
				}
			}
			client_retry(state);
			break;
		}
retry:
		if (state->conn_state == CS_WAIT_CONN) {
			al_net_dns_cache_clear();
		}
		state->ads_listen = 0;
		state->ads_cmds_en = 0;
		if (state->valid_dest_mask & NODES_ADS) {
			state->valid_dest_mask &= ~NODES_ADS;
			prop_mgr_event(PME_DISCONNECT, (void *)NODES_ADS);
			client_connectivity_update();
			state->serv_conn = 0;
		}
		client_prop_send_done(state, 0, NULL, NODES_ADS, hc);
		if (state->echo_dest_mask & NODES_ADS) {
			prop_mgr_event(PME_ECHO_FAIL, state->echo_prop->name);
		}
		client_finish_echo(state, NODES_ADS);
		state->serv_conn = 0;

		if (state->ota.in_prog == COS_IN_PROG &&
		    state->ota_server.lan) {
			client_wakeup();	/* allow LAN activity */
			break;
		}
		if (slow_retry) {
			client_retry_err(state);
			break;
		}
		client_retry(state);
		break;
	}
	return;

	/*
	 * On authentication error, retry with RSA authentication without
	 * dropping the connection the first time.  After that, drop the
	 * connection and retry slowly.
	 */
auth_error:
	/*
	 * An authentication error while in WAIT_EVENT state
	 * indicates that MQTT used expired credentials while
	 * trying to recover from a network/cloud outage. Get Device ID to
	 * update the credentials and re-establish the MQTT connection.
	 */
	if (state->conn_state == CS_WAIT_ID ||
	    state->conn_state == CS_WAIT_EVENT) {
		client_get_dev_id_pend(state);
	}
	hc->hc_error = HC_ERR_CLIENT_AUTH;
	state->request = CS_IDLE;
	state->conn_state = CS_WAIT_EVENT;
	state->auth_fails++;
	/*
	 * Restart SNTP in case auth failed due to bad time.
	 * Auth retry will be delayed until next SNTP update
	 */
	if (al_net_sntp_start()) {
		client_log(LOG_ERR "%s: sntp_start failed", __func__);
	}
	if (state->auth_fails < CLIENT_TRY_RECONN) {
		client_wakeup();	/* retry while connected */
		return;
	}
	if (state->auth_fails >= CLIENT_AUTH_FAILS) {
		slow_retry = 1;	/* too many auth fails */
	}
	goto retry;
}

/*
 * Returns non-zero if the client is disabled or not configured.
 */
static int client_disabled(struct client_state *state)
{
	struct http_client *hc = &state->http_client;
	struct ada_conf *cf = &ada_conf;

	if (!cf->enable || hc->host[0] == '\0') {
		client_close(state);
		state->conn_state = CS_DISABLED;
		state->valid_dest_mask &= ~NODES_ADS;
		return -1;
	}
	return 0;
}

/*
 * Start client state machine by getting ADS host DNS address.
 */
static void client_start(struct client_state *state, struct http_client *hc)
{
	if (client_disabled(state)) {
		return;
	}

	state->conn_state = CS_WAIT_CONN;
	hc->sending_chunked = 0;
	hc->http_tx_chunked = 0;
	hc->prop_callback = 0;
	hc->user_in_prog = 0;
	http_client_start(hc);
}

/*
 * Set the server name.
 * This determines whether to use the configured server
 * or the OEM-specific server name in release builds.
 */
static void client_commit_server(struct client_state *state)
{
	struct http_client *hc = &state->http_client;
	struct ada_conf *cf = &ada_conf;
	const struct hostname_info *host_entry;

	host_entry = client_lookup_host(cf->region);
	if (!host_entry) {
		host_entry = SERVER_HOSTINFO_DEFAULT;
	}

	hc->host_port = MQTT_CLIENT_SERVER_PORT_SSL;
	if ((client_conf_server_change_en() || conf_sys_test_mode_get())
	    && cf->conf_server[0] != '\0') {
		snprintf(hc->host, sizeof(hc->host), "%s", cf->conf_server);
		if (cf->conf_port) {
			hc->host_port = cf->conf_port;
		}
		if (!client_ntp_server) {
			client_ntp_server = host_entry->ntp_server;
		}
	} else {
		client_check_oem_info();

		if (oem[0] && oem_model[0] && !cf->conf_serv_override) {
			if (hc->is_mqtt) {
				snprintf(hc->host, sizeof(hc->host),
				    CLIENT_SERVER_HOST_OEM_FMT,
				    oem_model, oem, host_entry->domain);
			} else {
				snprintf(hc->host, sizeof(hc->host),
				    CLIENT_SERVER_HOST_OEM_HTTP_FMT,
				    oem_model, oem, host_entry->domain);
				hc->host_port = HTTP_CLIENT_SERVER_PORT_SSL;
			}
		} else if (hc->is_mqtt) {
			snprintf(hc->host, sizeof(hc->host),
			    CLIENT_SERVER_HOST_DEF_FMT, host_entry->domain);
		} else {
			snprintf(hc->host, sizeof(hc->host),
			    CLIENT_SERVER_HOST_DEF_HTTP_FMT,
			    host_entry->domain);
			hc->host_port = HTTP_CLIENT_SERVER_PORT_SSL;
		}
		client_ntp_server = host_entry->ntp_server;
	}
	al_net_sntp_server_set(0, client_ntp_server);
	hc->ssl_enable = 1;
	hc->accept_non_ayla = 0;
	hc->client_auth = 1;
}

int client_set_server(const char *host)
{
#ifdef AYLA_TEST_SERVICE_SUPPORT
	struct client_state *state = &client_state;
	struct ada_conf *cf = &ada_conf;
	size_t len;
	const struct test_service *ts;

	ts = test_svc_lookup(host);
	if (!ts) {
		client_log(LOG_WARN "test service %s not found", host);
		return -1;
	}
	if (!ts->enable) {
		client_log(LOG_WARN "test service %s disabled", ts->nickname);
		return -1;
	}
	len = strlen(ts->hostname);
	if (len > sizeof(cf->conf_server) - 1) {
		return -1;
	}
	client_lock();
	memcpy(cf->conf_server, ts->hostname, len);
	cf->conf_server[len] = '\0';
	client_ntp_server = ts->ntp_server;
	client_commit_server(state);
	client_unlock();
	return 0;
#else
	return -1;
#endif
}

/*
 * Set region for server.
 * Returns non-zero on error.
 */
int client_set_region(const char *region)
{
	struct ada_conf *cf = &ada_conf;
	const struct hostname_info *new_host;

	if (region[0] == '\0') {
		cf->region = NULL;
		return 0;
	}
	new_host = client_lookup_host(region);
	if (!new_host) {
		return -1;
	}
	cf->region = new_host->region;
	return 0;
}

/*
 * Allow client to fetch prop and cmd updates from ADS
 */
void client_enable_ads_listen(void)
{
	struct client_state *state = &client_state;

	client_lock();
	if ((state->valid_dest_mask & NODES_ADS) && !state->ads_listen) {
		client_log(LOG_INFO "listen enabled");
		client_timer_cancel(&state->cmd_timer);
	}
	state->ads_listen = 1;
	state->ads_cmds_en = 1;
	state->cmd_event = 1;
	client_wakeup();
	client_unlock();
}

/*
 * Clear out any state left over from an earlier connection.
 */
static void client_reset(struct client_state *state)
{
	struct http_client *hc = &state->http_client;
	struct ada_conf *cf = &ada_conf;

	client_timer_cancel(&state->req_timer);
	client_timer_cancel(&state->cmd_timer);
	client_timer_cancel(&state->poll_timer);
	client_timer_cancel(&state->listen_timer);

	http2mqtt_client_reset(hc, MOD_LOG_CLIENT);
	hc->client_send_data_cb = NULL;
	hc->client_err_cb = client_err_cb;
	hc->client_tcp_recv_cb = NULL;
	hc->client_next_step_cb = client_next_step_cb;
	state->client_key[0] = '\0';
	cf->reg_token[0] = '\0';
	state->get_all = cf->get_all;
	state->cmd_event = 1;
	state->ads_listen = 0;
	state->ads_cmds_en = 0;
	state->auth_fails = 0;
	state->retries = 0;
	state->buf_len = 0;
	state->wait_for_file_put = 0;
	state->wait_for_file_get = 0;
#ifdef AYLA_BATCH_ADS_UPDATES
	client_batch_reset();
#endif
	client_close(state);
	client_commit_server(state);
}

/*
 * Reset the connection to the server, if up
 * Called when the OEM ID, OEM model, or region change.
 */
void client_server_reset(void)
{
	struct client_state *state = &client_state;

	client_lock();
	if (state->conn_state != CS_DOWN) {
		client_reset(state);
		client_connectivity_update_cb(state);	/* unlocks / relocks */
		client_start(state, &state->http_client);
	}
	client_unlock();
}

/*
 * Indicate that the configuration may have been changed by the platform.
 */
void client_commit(void)
{
	struct client_state *state = &client_state;

	client_lock();
	client_reset(state);

	/* Clear dest valid and clear prop send pending list */
	client_dest_down(NODES_ADS);

	if (state->conn_state != CS_DOWN) {
		client_get_dev_id_pend(state);
	}
	client_unlock();
}

/*
 * Set the hardware ID if the platform didn't set it.
 */
static void client_init_hw_id_default(struct ada_conf *cf)
{
	int hw_id_len = 17;
	char *hw_id;
	const u8 *mac;
	static u8 mac_buf[6];
	struct al_net_if *nif;

	mac = cf->mac_addr;
	if (!mac) {
		nif = al_net_if_get(AL_NET_IF_DEF);
		if (!nif) {
			client_log(LOG_ERR "%s: net_if not set", __func__);
			return;
		}
		if (al_net_if_get_mac_addr(nif, mac_buf)) {
			client_log(LOG_ERR "%s: get_mac_addr failed", __func__);
			return;
		}
		cf->mac_addr = mac_buf;
		mac = mac_buf;
	}
	if (!cf->hw_id || !cf->hw_id[0]) {
		hw_id = al_os_mem_alloc(hw_id_len);
		if (!hw_id) {
			client_log(LOG_ERR "ada_conf.hw_id not set");
			return;
		}
		snprintf(hw_id, hw_id_len,
		    "mac-%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x",
		    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		cf->hw_id = hw_id;
	}
}

static void client_sntp_callback(void)
{
	struct client_state *state = &client_state;

	client_lock();
	client_log(LOG_INFO "time set by SNTP. up time %llu ms",
	    clock_total_ms());
	if (!CLIENT_HAS_KEY(state) || state->auth_hdr[0] == '\0') {
		if (state->conn_state != CS_WAIT_ID) {
			client_get_dev_id_pend(state);
		}
		client_wakeup();
	}
	client_unlock();
}

int ada_client_ip_up(void)
{
	struct client_state *state = &client_state;
	struct ada_conf *cf = &ada_conf;
	const u8 *mac;
	struct al_net_if *netif_default;
	u32 ip4addr;
	char ip[30];

#ifndef AYLA_IN_FIELD_PROVISION_SUPPORT
	if (!conf_sys_dev_id[0]) {
		client_log(LOG_ERR "conf_sys_dev_id not set");
		return -1;
	}
#endif

	netif_default = al_net_if_get(AL_NET_IF_DEF);
	if (!netif_default) {
		client_log(LOG_ERR "net_if_get failed");
		return -1;
	}
	if (al_net_if_get_technology(netif_default) !=
	    AL_NET_IF_TECH_CELLULAR) {
		mac = cf->mac_addr;
		if (!(mac[0] | mac[1] | mac[2] | mac[3] | mac[4] | mac[5])) {
			client_log(LOG_ERR "ada_conf.mac_addr not set");
				return -1;
		}
	}
	if (mfg_or_setup_mode_active()) {
		client_log(LOG_WARN
		    "cloud use not allowed with setup_mode enabled");
		return -1;
	}
	client_lock();
	if (state->conn_state != CS_DOWN && state->conn_state != CS_DISABLED) {
		client_unlock();
		return 0;
	}
	client_reset(state);
	if (client_disabled(state)) {
		client_unlock();
		return -1;
	}
	client_event_send(AE_IN_PROGRESS);

	ip4addr = al_net_if_get_ipv4(netif_default);
	CLIENT_LOGF(LOG_INFO, "IP %s",
	    ipaddr_fmt_ipv4_to_str(ip4addr, ip, sizeof(ip)));
	client_get_dev_id_pend(state);
	state->conn_state = CS_WAIT_EVENT;
	client_wakeup();
	client_unlock();

	al_net_sntp_set_callback(client_sntp_callback);
	client_log(LOG_DEBUG "SNTP start.  up time %llu ms",
	    clock_total_ms());
	if (al_net_sntp_start()) {
		client_log(LOG_ERR "%s: sntp_start failed", __func__);
	}
	return 0;
}

#ifdef AYLA_LOCAL_CONTROL_SUPPORT
int ada_client_lc_up(void)
{
	return lctrl_up();
}
#endif

int ada_client_up(void)
{
#ifdef AYLA_LOCAL_CONTROL_SUPPORT
	int rc;

	rc = lctrl_up();
	if (rc) {
		return rc;
	}
#endif
	return ada_client_ip_up();
}

/*
 * Shut down some or all client access.
 *
 * ip_down: 1 - shut down IP access, 0 - leave IP access as is
 * lc_down: 1 - shut down local control, 0 - leave local control as is
 */
static void client_down_locked(u8 ip_down, u8 lc_down)
{
	struct client_state *state = &client_state;
	struct http_client *hc = &state->http_client;
	u8 dests = 0;

	if (ip_down) {
		dests |= (NODES_HOMEKIT | NODES_LAN | NODES_ADS);
		state->serv_conn = 0;
		state->conn_state = CS_DOWN;
		state->current_request = NULL;
		hc->client_err_cb = NULL;
	}
#ifdef AYLA_LOCAL_CONTROL_SUPPORT
	if (lc_down) {
		dests |= NODES_LC;
		lctrl_down();
	}
#endif
	if (dests) {
		client_prop_send_done(state, 0, NULL, dests, hc);
	}
	if (ip_down) {
		client_close(state);
		client_logging(0);
		state->get_echo_inprog = 0;
		state->lan_cmd_pending = 0;
		http2mqtt_client_disconnect(hc);
#ifdef AYLA_LAN_SUPPORT
		client_lan_reset(state);
#endif
		al_net_sntp_stop();
	}
	client_dest_down(dests);
}

void ada_client_ip_down(void)
{
	client_lock();
	client_down_locked(1, 0);
	client_unlock();
}

void ada_client_lc_down(void)
{
	client_lock();
	client_down_locked(0, 1);
	client_unlock();
}

void ada_client_down(void)
{
	client_lock();
	client_down_locked(1, 1);
	client_unlock();
}

static const struct url_list client_urls[] = {
	URL_PUT("/cli.text", client_cli_put, ADS_REQ),
	URL_GET("/config.json", conf_json_get, ADS_REQ),
	URL_PUT("/config.json", conf_json_put, ADS_REQ),
	URL_PUT("/logclient.json", client_log_client_json_put, ADS_REQ),
#ifdef AYLA_LOCAL_SERVER
	URL_GET("/regtoken.json", client_json_regtoken_get, LOC_REQ | APP_REQ),
	URL_GET("/status.json", client_json_status_get,
	    SEC_WIFI_REQ | REQ_SOFT_AP | APP_ADS_REQS),
	URL_GET("/time.json", client_json_time_get, APP_ADS_REQS),
#endif
	URL_PUT("/getdsns.json", client_json_getdsns_put, ADS_REQ),
#ifdef AYLA_LOCAL_SERVER
	URL_PUT("/time.json", client_json_time_put,
#ifdef AYLA_WIFI_SETUP_CLEARTEXT_SUPPORT
	    REQ_SOFT_AP |
#endif
	    SEC_WIFI_REQ | APP_REQ),
	URL_GET("/property.json", prop_page_json_get_one,
	    APP_REQ),
#endif
	URL_PUT("/lanip.json", client_lanip_json_put, ADS_REQ),
#if defined(AYLA_LAN_SUPPORT) && !defined(DISABLE_LAN_OTA)
	URL_PUT("/lanota.json", client_lanota_json_put, LOC_REQ | REQ_SOFT_AP),
#endif
	URL_PUT("/registration.json", client_registration_json_put, ADS_REQ),
	URL_PUT("/reset.json", client_reset_json_put, ADS_REQ),
	URL_END
};

void client_init(void)
{
	struct client_state *state = &client_state;
	struct ada_conf *cf = &ada_conf;
#ifdef AYLA_LAN_SUPPORT
	struct ada_lan_conf *lcf = &ada_lan_conf;
#endif
	const struct client_cb_handler *cbh;
	unsigned int i;

	client_log(LOG_INFO LOG_EXPECTED "%s", ada_version_build);

	client_init_hw_id_default(cf);

#ifdef AYLA_LAN_SUPPORT
	lcf->keep_alive = CLIENT_LAN_KEEPALIVE;
#endif

	http2mqtt_client_init();

	/*
	 * Init timers.
	 */
	timer_handler_init(&state->cmd_timer, client_cmd_timeout);
	timer_handler_init(&state->listen_timer, client_listen_warn);
	timer_handler_init(&state->poll_timer, client_poll);
	timer_handler_init(&state->req_timer, client_timeout);
	timer_handler_init(&state->template_timer, client_template_timeout);
	state->sync_time = 1;

#ifdef AYLA_BATCH_ADS_UPDATES
	client_batch_init();
#endif

#ifdef AYLA_LAN_SUPPORT
	timer_handler_init(&state->lan_reg_timer, client_lan_reg_timeout);
#endif
	ada_client_health_intvl_set(CLIENT_HEALTH_GET_INTVL);

	/*
	 * Initialize callbacks.
	 */
	for (cbh = client_cb_handlers, i = 0; i < ARRAY_LEN(client_cb_handlers);
	     cbh++, i++) {
		if (cbh->func) {
			callback_init(&client_cb[i], cbh->func, state);
		}
	}

	log_client_init();
	callback_init(&state->next_step_cb, client_next_step, NULL);
	client_prop_init(state);
	server_add_urls(client_urls);
#ifdef AYLA_SERVER_TEST_SUPPORT
	server_test_init();
#endif
#ifdef AYLA_LAN_SUPPORT
	client_lan_init();
#endif
#ifdef AYLA_LOCAL_CONTROL_SUPPORT
	lctrl_init();
#endif
#ifdef AYLA_TEST_SERVICE_SUPPORT
	om_init();
#endif
}

#ifdef AYLA_LOCAL_SERVER
static void client_json_status_get(struct server_req *req)
{
	struct client_state *state = &client_state;
	struct http_client *hc = &state->http_client;
	struct ada_conf *cf = &ada_conf;
	char sys_mac[20];
	enum ada_wifi_features features;

	features = adap_wifi_features_get();
	format_mac(cf->mac_addr, sys_mac, sizeof(sys_mac));
	server_json_header(req);
	server_put(req, "{\"DSN\":\"%s\","	/* "DSN" for compatibility */
	    "\"dsn\":\"%s\","			/* "dsn" is standard */
	    "\"model\":\"%s\","
	    "\"api_version\":\"1.0\","
	    "\"device_service\":\"%s\","
	    "\"mac\":\"%s\","
	    "\"last_connect_mtime\":%ld,"
	    "\"mtime\":%ld,"
	    "\"version\":\"%s\","
	    "\"build\":\"%s\","
	    "\"features\":["
	    "%s%s%s%s"
	    "\"" FEAT_STR_RSA_KE "\""
	    "]}",
	    conf_sys_dev_id, conf_sys_dev_id, conf_sys_model,
	    hc->host, sys_mac, state->connect_time,
	    clock_ms(), adap_conf_sw_version(), adap_conf_sw_build(),
	    (features & AWF_SIMUL_AP_STA) ? "\"" FEAT_STR_AP_STA "\"," : "",
	    (features & AWF_WPS) ? "\"" FEAT_STR_WPS "\"," : "",
	    (features & AWF_WPS_APREG) ? "\"" FEAT_STR_WPS_APREG "\"," : "",
	    conf_sys_test_mode_get() ? "\"" FEAT_STR_TEST_MODE "\"," : "");
}

static void client_json_regtoken_get(struct server_req *req)
{
	struct client_state *state = &client_state;
	struct ada_conf *cf = &ada_conf;
	char buf[CLIENT_CONF_REG_TOK_LEN + 4];
	const char *reg_token = "null";

	server_json_header(req);

	client_lock();
	if (cf->reg_token[0] != '\0' &&
	    strcasecmp(state->reg_type, "Display")) {
		snprintf(buf, sizeof(buf), "\"%s\"", cf->reg_token);
		reg_token = buf;
	}
	server_put(req,
	    "{\"regtoken\":%s,\"registered\":%u,\"registration_type\":\"%s\","
	    "\"host_symname\":\"%s\"}",
	    reg_token, cf->reg_user, state->reg_type,
	    cf->host_symname);
	client_unlock();
}

static void client_json_time_get(struct server_req *req)
{
	char buf[24];
	u32 utc_time = clock_utc();

	clock_fmt(buf, sizeof(buf), clock_local(&utc_time));
	server_json_header(req);
	server_put(req, "{\"time\":%lu,\"mtime\":%lu,\"set_at_mtime\":%lu,"
	    "\"clksrc\":%d,\"localtime\":\"%s\",\"timezone\":%d,"
	    "\"daylight_active\":%u,\"daylight_change\":%lu}",
	    utc_time, clock_ms(), clock_set_mtime, clock_source(), buf,
	    timezone_info.valid ? timezone_info.mins : 0,
	    daylight_info.valid ? daylight_info.active : 0,
	    daylight_info.valid ? daylight_info.change : 0);
}

static void client_json_time_put(struct server_req *req)
{
	jsmn_parser parser;
	unsigned long val;
	jsmntok_t tokens[4];
	jsmnerr_t err;

	jsmn_init_parser(&parser, req->post_data, tokens, 4);
	err = jsmn_parse(&parser);
	if (err != JSMN_SUCCESS) {
		CLIENT_LOGF(LOG_WARN, "jsmn err %d", err);
		goto inval;
	}
	if (jsmn_get_ulong(&parser, NULL, "time", &val)) {
		CLIENT_LOGF(LOG_WARN, "bad time");
		goto inval;
	}
	CLIENT_DEBUG(LOG_DEBUG2, "val %lu", val);
	client_lock();
	client_clock_set(val, CS_LOCAL);
	client_unlock();
	server_put_status(req, HTTP_STATUS_NO_CONTENT);
	return;
inval:
	server_put_status(req, HTTP_STATUS_BAD_REQ);
}
#endif /* AYLA_LOCAL_SERVER */

/*
 * Redirect print_cli output to client log info
 */
static int client_print_redirect(void *arg, const char *msg)
{
	while (*msg) {
		/* omit blank lines */
		if (*msg != '\r' && *msg != '\n') {
			break;
		}
		msg++;
	}
	if (*msg) {
		client_log(LOG_INFO "%s", msg);
	}

	return 0;
}

/*
 * Put a command string to the CLI and redirect the output to the log
 */
static void client_cli_put(struct server_req *req)
{
	struct client_state *state = &client_state;
	void (*cmd_exec_func)(const char *) = state->cmd_exec_func;

	server_put_status(req, HTTP_STATUS_NO_CONTENT);

	client_log(LOG_INFO "remote command \"%s\"", req->post_data);
	if (!cmd_exec_func) {
		client_log(LOG_WARN "no command handler registered");
		return;
	}

	print_remote_set(client_print_redirect, NULL);
	cmd_exec_func(req->post_data);
	print_remote_set(NULL, NULL);
}

/*
 * Refetch of key requested, after completing this command.
 */
static void client_json_getdsns_put(struct server_req *req)
{
	struct client_state *state = &client_state;

	client_lock();
	client_get_dev_id_pend(state);
	client_unlock();
	server_put_status(req, HTTP_STATUS_NO_CONTENT);
}

enum wifi_error client_status(void)
{
	struct client_state *state = &client_state;
	struct http_client *hc = &state->http_client;
	enum wifi_error error;

	switch (hc->hc_error) {
	case HC_ERR_NONE:
		error = WIFI_ERR_CLIENT_TIME;
		break;
	case HC_ERR_DNS:
		error = WIFI_ERR_CLIENT_DNS;
		break;
	case HC_ERR_MEM:
		error = WIFI_ERR_MEM;
		break;
	case HC_ERR_CLIENT_AUTH:
		error = WIFI_ERR_CLIENT_AUTH;
		break;
	default:
		error = WIFI_ERR_TIME;
		break;
	}
	return error;
}

/*
 * Return the server name being used.
 * This is not necessarily the one that's configured.
 */
const char *client_host(void)
{
	struct client_state *state = &client_state;
	struct http_client *hc = &state->http_client;
	struct ada_conf *cf = &ada_conf;

	if (!cf->enable) {
		return "";
	}
	return hc->host;
}

void client_set_setup_token(const char *token)
{
	struct client_state *state = &client_state;

	client_lock();
	snprintf(state->setup_token, sizeof(state->setup_token),
	    "%s", token);
	state->client_info_flags |= CLIENT_UPDATE_SETUP;
	client_wakeup();
	client_unlock();
}

int client_get_setup_token(char *token, size_t len)
{
	struct client_state *state = &client_state;
	if (len < CLIENT_SETUP_TOK_LEN) {
		client_log(LOG_ERR "%s: buffer not enough", __func__);
		return -1;
	}
	client_lock();
	memcpy(token, state->setup_token, CLIENT_SETUP_TOK_LEN);
	token[CLIENT_SETUP_TOK_LEN - 1] = '\0';
	client_unlock();
	return 0;
}
void client_set_setup_location(char *token)
{
	struct client_state *state = &client_state;

	/* free has no effect if setup_location is null */
	al_os_mem_free(state->setup_location);
	state->setup_location = token;
}

/*
 * Return current connectivity information
 */
u8 client_get_connectivity_mask(void)
{
	return client_state.valid_dest_mask;
}

void client_dest_up(u8 mask)
{
	struct client_state *state = &client_state;

	if (mask & state->valid_dest_mask) {
		/* already up */
		return;
	}
	state->valid_dest_mask |= mask;
	client_log(LOG_DEBUG "dest %02x up, dests %02x",
	    mask, state->valid_dest_mask);
	client_connectivity_update();
}

void client_dest_down(u8 mask)
{
	struct client_state *state = &client_state;

	if (!(mask & state->valid_dest_mask)) {
		/* already down */
		return;
	}
	state->valid_dest_mask &= ~mask;
	if (state->conn_state == CS_WAIT_ECHO) {
		state->failed_dest_mask |= (state->echo_dest_mask & mask);
	}
	state->echo_dest_mask &= ~mask;
	client_log(LOG_DEBUG "dest %02x down, dests %02x",
	    mask, state->valid_dest_mask);
	client_connectivity_update();
}

/*
 * Return 1 if the LAN mode is enabled in configuration
 */
int client_lanmode_is_enabled(void)
{
#ifdef AYLA_LAN_SUPPORT
	return (ada_lan_conf.enable_mask & LAN_CONF_ENABLE_LAN_BIT) != 0;
#else
	return 0;
#endif
}

/*
 * Return 1 if the local control mode is enabled in configuration
 */
int client_lctrl_is_enabled(void)
{
#ifdef AYLA_LOCAL_CONTROL_SUPPORT
	return (ada_lan_conf.enable_mask &
	    (LAN_CONF_ENABLE_BLE_BIT | LAN_CONF_ENABLE_MATTER_BIT)) != 0;
#else
	return 0;
#endif
}

/*
 * Return 1 if the lan_conf data was received from the service. Return 0
 * if the data didn't originate from the service. For example, the
 * device has been factory reset and not yet contacted the service.
 */
int client_lanconf_recvd(void)
{
#if defined(AYLA_LAN_SUPPORT) || defined(AYLA_LOCAL_CONTROL_SUPPORT)
	return (ada_lan_conf.enable_mask & LAN_CONF_ENABLE_RECVD_BIT) != 0;
#else
	return 0;
#endif
}

/*
 * Start registration window.
 * This may be called in another thread.
 */
void ada_client_reg_window_start(void)
{
	client_lock();
	client_log(LOG_DEBUG "reg start pending %u",
	   client_step_is_enabled(ADCP_REG_WINDOW_START));
	client_step_enable(ADCP_REG_WINDOW_START);
	client_wakeup();
	client_unlock();
}

/*
 * Clear DNS cache and make next ADS and ANS traffic re-resolve the DNS entry.
 * This is for security software that wants to monitor DNS responses.
 * It is unfortunate that this causes a new GET commands but should be only
 * once per device reboot.
 */
void ada_client_dns_clear(void)
{
	struct client_state *state = &client_state;

	client_lock();
	al_net_dns_cache_clear();
	state->new_conn = 1;
	state->cmd_event = 1;
	client_wakeup();
	client_unlock();
}

#ifdef AYLA_BATCH_PROP_SUPPORT

static void client_batch_save_dp_status(u32 batch_id, int status)
{
	struct client_state *state = &client_state;
	struct prop *prop;
	struct batch *b;
	struct batch_dp *dp;
	int i;

	prop = state->batch_prop;
	if (!prop || prop->type != ATLV_BATCH) {
		return;
	}
	b = prop->val;
	if (!b) {
		return;
	}
	for (i = 0; i < b->dps_cnt; i++) {
		dp = b->dps + i;
		if (dp->batch_id == batch_id) {
			dp->err = status;
			break;
		}
	}
}

static int client_batch_id_accept(const struct ame_decoder *dec,
		void *arg, struct ame_kvp *kvp)
{
	struct batch_dps_rsp *bdpr = (struct batch_dps_rsp *) arg;

	dec->get_u32(kvp, NULL, &bdpr->batch_id);
	if (bdpr->batch_id != 0 && bdpr->status > 0) {
		client_batch_save_dp_status(bdpr->batch_id, bdpr->status);
		bdpr->batch_id = 0;
		bdpr->status = -1;
	}
	return AE_OK;
}

static int client_batch_status_accept(const struct ame_decoder *dec,
		void *arg, struct ame_kvp *kvp)
{
	struct batch_dps_rsp *bdpr = (struct batch_dps_rsp *) arg;

	dec->get_s32(kvp, NULL, &bdpr->status);
	if (bdpr->batch_id != 0 && bdpr->status > 0) {
		client_batch_save_dp_status(bdpr->batch_id, bdpr->status);
		bdpr->batch_id = 0;
		bdpr->status = -1;
	}
	return AE_OK;
}

static const char batch_tag_name[] = "name";
static const char batch_tag_type[] = "base_type";
static const char batch_tag_value[] = "value";
static const char batch_tag_echo[] = "echo";
static const char batch_tag_time[] = "dev_time_ms";

static const char batch_tag_batch_id[] = "batch_id";
static const char batch_tag_status[] = "status";
static const char batch_tag_property[] = "property";
static const char batch_tag_datapoints[] = "batch_datapoints";

static const struct ame_tag client_batch_dps_inner_tag_list[] = {
	AME_TAG(batch_tag_batch_id, NULL, client_batch_id_accept),
	AME_TAG(batch_tag_status, NULL, client_batch_status_accept),
	AME_TAG(NULL, NULL, NULL)
};

static const struct ame_tag client_batch_dps_tag_list[] = {
	AME_TAG("", client_batch_dps_inner_tag_list, NULL),
	AME_TAG(NULL, NULL, NULL)
};

static const struct ame_tag client_batch_root_inner_tag_list[] = {
	AME_TAG(batch_tag_datapoints, client_batch_dps_tag_list, NULL),
	AME_TAG(NULL, NULL, NULL)
};

static const struct ame_tag client_batch_root_tag_list[] = {
	AME_TAG("", client_batch_root_inner_tag_list, NULL),
	AME_TAG(NULL, NULL, NULL)
};

static enum ada_err client_recv_batch_dps_rsp(struct http_client *hc,
	void *payload, size_t len)
{
	struct client_state *state = &client_state;
	if (payload) {
		client_wait(state, CLIENT_PROP_WAIT);
		client_ame_parse(&state->ame, payload, len);
	}
	return client_recv_post(hc, payload, len);
}
#endif /* AYLA_BATCH_PROP_SUPPORT */

static u8 tlv_get_bool(struct prop *prop)
{
	ASSERT(prop->type == ATLV_BOOL);
	return *(u8 *)prop->val;
}

static s32 tlv_get_int(struct prop *prop)
{
	s32 v;

	ASSERT(prop->type == ATLV_INT || prop->type == ATLV_CENTS);
	switch (prop->len) {
	case 1:
		v = *(s8 *)prop->val;
		break;
	case 2:
		v = *(s16 *)prop->val;
		break;
	case 4:
		v = *(s32 *)prop->val;
		break;
	default:
		ASSERT_NOTREACHED();
	}
	return v;
}

static u32 tlv_get_uint(struct prop *prop)
{
	u32 v;

	ASSERT(prop->type == ATLV_UINT);
	switch (prop->len) {
	case 1:
		v = *(u8 *)prop->val;
		break;
	case 2:
		v = *(u16 *)prop->val;
		break;
	case 4:
		v = *(u32 *)prop->val;
		break;
	default:
		ASSERT_NOTREACHED();
	}
	return v;
}

static char *tlv_get_utf8(struct prop *prop)
{
	ASSERT(prop->type == ATLV_UTF8);
	return (char *)prop->val;
}

#ifdef AYLA_BATCH_PROP_SUPPORT
static int client_format_dp_data(struct batch_dp *dp, u8 be_first,
		u8 be_last, char *buf, size_t size)
{
	const struct ame_encoder *enc = client_state.ame.enc;
	struct ame_encoder_state *es = &client_state.ame.es;
	struct prop *prop;
	struct ame_key key;
	char *vp;
	u32 vu;
	s32 vs;
	char c_save;

	key.display_name = NULL;
	if (!dp) {
		return 0;
	}
	prop = dp->prop;
	if (!prop) {
		return 0;
	}
	ame_encoder_buffer_set(es, (u8 *)buf, size);
	if (be_first) {
		key.tag = batch_tag_datapoints;
		enc->enc_key(es, EF_PREFIX_O, &key);
		enc->enc_prefix(es, EF_PREFIX_A);
	}
	key.tag = batch_tag_batch_id;
	enc->enc_u32(es, EF_PREFIX_O, &key, dp->batch_id);

	key.tag = batch_tag_property;
	enc->enc_key(es, EF_PREFIX_C, &key);

	key.tag = batch_tag_type;
	enc->enc_utf8(es, EF_PREFIX_O, &key,
	    lookup_by_val(prop_types, prop->type));

	key.tag = batch_tag_name;
	enc->enc_utf8(es, EF_PREFIX_C, &key, prop->name);

	key.tag = batch_tag_value;
	switch (prop->type) {
	case ATLV_BOOL:
		vu = tlv_get_bool(prop);
		enc->enc_u32(es, EF_PREFIX_C, &key, vu);
		break;
	case ATLV_INT:
		vs = tlv_get_int(prop);
		enc->enc_s32(es, EF_PREFIX_C, &key, vs);
		break;
	case ATLV_UINT:
		vu = tlv_get_uint(prop);
		enc->enc_u32(es, EF_PREFIX_C, &key, vu);
		break;
	case ATLV_CENTS:
		vs = tlv_get_int(prop);
		enc->enc_d32(es, EF_PREFIX_C, &key, vs, 2);
		break;
	case ATLV_UTF8:
		vp = tlv_get_utf8(prop);
		c_save = vp[prop->len];
		vp[prop->len] = '\0';
		enc->enc_utf8(es, EF_PREFIX_C, &key, vp);
		vp[prop->len] = c_save;
		break;
	default:
		return -1;
	}

	if (prop->echo) {
		key.tag = batch_tag_echo;
		enc->enc_boolean(es, EF_PREFIX_C, &key, 1);
	}

	key.tag = batch_tag_time;
	enc->enc_s64(es, EF_PREFIX_C, &key, dp->time_stamp);

	enc->enc_suffix(es, EF_SUFFIX_E);
	enc->enc_suffix(es, EF_SUFFIX_E);
	if (!be_last) {
		enc->enc_suffix(es, EF_SUFFIX_M);
	} else {
		enc->enc_suffix(es, EF_SUFFIX_Z);
		enc->enc_suffix(es, EF_SUFFIX_E);
	}
	return es->offset;
}

static size_t client_get_all_batch_dp_len(struct batch *b)
{
	int cnt = 0;
	struct batch_dp *dp;
	u8 first;
	u8 last;
	char buf[1024+256];
	int i;

	/* TODO - buffer not needed? */
	for (i = 0; i < b->dps_cnt; i++) {
		memset(buf, 0, sizeof(buf));
		dp = b->dps + i;
		first = (i == 0);
		last = (i >= (b->dps_cnt - 1));
		cnt += client_format_dp_data(dp, first, last, buf, sizeof(buf));
	}
	return cnt;
}

static enum ada_err client_send_batch_dp(struct prop *prop)
{
	int cnt;
	char uri[CLIENT_GET_REQ_LEN];
	struct client_state *state = &client_state;
	struct http_client *hc = &state->http_client;
	enum ada_err err = AE_OK;
	struct batch *b;
	struct batch_dp *dp;
	u8 first;
	u8 last;
	int size;
	char *buf;

	if (!prop) {
		return AE_INVAL_VAL;
	}
	if (prop->type != ATLV_BATCH) {
		return AE_INVAL_TYPE;
	}
	b = prop->val;
	if (!b) {
		return AE_INVAL_VAL;
	}
	if (!hc->prop_callback) {
		b->dps_tx = 0;	/* reset in case request is a retry */
		hc = client_req_ads_new();
		memset(&prop_recvd, 0, sizeof(prop_recvd));

		client_ame_init(&state->ame,
		    client_ame_stack, ARRAY_LEN(client_ame_stack),
		    client_ame_parsed_buf, sizeof(client_ame_parsed_buf),
		    client_batch_root_tag_list, &state->dps_rsp_ctx);

		state->batch_prop = prop;
		state->request = CS_POST_DATA;
		state->conn_state = CS_WAIT_POST;
		hc->client_send_data_cb = client_send_post;
		hc->client_tcp_recv_cb = client_recv_batch_dps_rsp;

		ASSERT(hc->client_next_step_cb == client_next_step_cb);
		hc->http_tx_chunked = 1;
		hc->chunked_eof = 0;

		hc->body_buf = state->buf;
		hc->body_len = client_get_all_batch_dp_len(b);
		hc->body_buf_len = 0;

		snprintf(uri, sizeof(uri),
		    "/dev/v1/dsns/%s/batch_datapoints.json",
		    conf_sys_dev_id);
		client_req_start(hc, HTTP_REQ_POST, uri,
		    &http_hdr_content_json);
		return AE_OK;
	}

	while (b->dps_tx >= 0 && b->dps_tx < b->dps_cnt) {
		first = (b->dps_tx == 0);
		last = (b->dps_tx >= (b->dps_cnt - 1));
		dp = b->dps + b->dps_tx;
		if (dp->prop->len >= sizeof(state->buf) - 256) {
			size = dp->prop->len + 256;
			buf = al_os_mem_alloc(size);
			if (!buf) {
				err = AE_ALLOC;
				goto on_error;
			}
		} else {
			size = sizeof(state->buf);
			buf = state->buf;
		}
		cnt = client_format_dp_data(dp, first, last, buf, size);
		ASSERT(cnt <= size);
		err = client_req_send(hc, buf, cnt);
		if (buf != state->buf) {
			al_os_mem_free(buf);
		}
		if (err) {
			goto on_error;
		}
		dp->tx_size = cnt;
		b->dps_tx += 1;
	}
	hc->http_tx_chunked = 0;
	hc->chunked_eof = 1;
	hc->prop_callback = 0;
	client_log(LOG_DEBUG2 "batch send complete");
	return AE_OK;

on_error:
	if (err != AE_BUF) {
		CLIENT_DEBUG(LOG_ERR, "send err %d", err);
	}
	return err;
}

#endif /* AYLA_BATCH_PROP_SUPPORT */
