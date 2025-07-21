/*
 * Copyright 2011-2015 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_CLIENT_H__
#define __AYLA_CLIENT_H__

#ifdef __cplusplus
extern "C" {
#endif

#define CLIENT_SETUP_TOK_LEN	10	/* max setup token length incl NUL */

/*
 * Total wait time Wi-Fi should wait for device service to report connection.
 */
#define CLIENT_PROP_WAIT	20000	/* property wait, milliseconds */
#define CLIENT_WAIT	(CLIENT_CONN_WAIT + CLIENT_PROP_WAIT)	/* ms */

#define CLIENT_CONN_WAIT	15000	/* TCP connect wait, matches hc */
#define CLIENT_LOCAL_WAIT	3000	/* lan response wait, milliseconds */
#define CLIENT_TRY_RECONN	2	/* tries until disconnecting */
#define CLIENT_TRY_THRESH	5	/* threshold till wait time increase */
#define CLIENT_RETRY_WAIT1	10000	/* init retry wait, milliseconds */
#define CLIENT_RETRY_WAIT2	300000	/* retry wait till threshold */
#define CLIENT_RETRY_WAIT3	600000	/* retry wait after threshold */
#define CLIENT_RETRY_WAIT_MAX	3600000 /* one hour */

enum client_conn_state {
	CS_DOWN,
	CS_DISABLED,	/* network up but client not enabled */
	CS_WAIT_CONN,	/* wait for connection to service */
	CS_WAIT_DSN,	/* waiting to get client DSN */
	CS_WAIT_ID,	/* waiting to get client ID */
	CS_WAIT_INFO_PUT, /* waiting to put client info */
	CS_WAIT_OEM_INFO_PUT, /* waiting to put client OEM info */
	CS_WAIT_CMD_PUT,  /* waiting to resp to cmd */
	CS_WAIT_GET,	/* waiting for GET commands response */
	CS_WAIT_PROP_GET, /* waiting for response to requested property GET */
	CS_WAIT_LANIP_GET, /* waiting for GET response to lanip key */
	CS_WAIT_POST,	/* waiting for POST response */
	CS_WAIT_LOGS_POST, /* waiting for logs POST response */
	CS_WAIT_METRICS_POST, /* waiting for metrics POST response */
	CS_WAIT_OTA_GET,  /* waiting for GET response of OTA patch fetch */
	CS_WAIT_OTA_PUT, /* waiting for send OTA status */
	CS_WAIT_CMD_STS_PUT, /* waiting to send belated cmd status */
	CS_WAIT_ECHO,	/* waiting for response for an echo post */
	CS_WAIT_EVENT,	/* waiting for event or polling interval */
	CS_WAIT_RETRY,	/* waiting to retry after connection or I/O error */
	CS_WAIT_PING,	/* waiting for ping reply from service */
	CS_WAIT_REG_WINDOW, /* waiting for POST response to start_reg_window */
	CS_WAIT_PROP_RESP, /* waiting to POST a property response */
	CS_WAIT_PROP,	/* waiting for request from property subsystem */
	CS_WAIT_FILE_GET, /* wait for file prop get */
};

#define CLIENT_CONN_STATES { \
	[CS_DOWN] =		"CS_DOWN",		\
	[CS_DISABLED] =		"CS_DISABLED",		\
	[CS_WAIT_CONN] =	"CS_WAIT_CONN",		\
	[CS_WAIT_DSN] =		"CS_WAIT_DSN",		\
	[CS_WAIT_ID] =		"CS_WAIT_ID",		\
	[CS_WAIT_INFO_PUT] =	"CS_WAIT_INFO_PUT",	\
	[CS_WAIT_OEM_INFO_PUT] = "CS_WAIT_OEM_INFO_PUT", \
	[CS_WAIT_CMD_PUT] =	"CS_WAIT_CMD_PUT",	\
	[CS_WAIT_GET] =		"CS_WAIT_GET",		\
	[CS_WAIT_PROP_GET] =	"CS_WAIT_PROP_GET",	\
	[CS_WAIT_LANIP_GET] =	"CS_WAIT_LANIP_GET",	\
	[CS_WAIT_POST] =	"CS_WAIT_POST",		\
	[CS_WAIT_LOGS_POST] =	"CS_WAIT_LOGS_POST",	\
	[CS_WAIT_METRICS_POST] = "CS_WAIT_METRICS_POST", \
	[CS_WAIT_OTA_GET] =	"CS_WAIT_OTA_GET",	\
	[CS_WAIT_OTA_PUT] =	"CS_WAIT_OTA_PUT",	\
	[CS_WAIT_CMD_STS_PUT] =	"CS_WAIT_CMD_STS_PUT",	\
	[CS_WAIT_ECHO] =	"CS_WAIT_ECHO",		\
	[CS_WAIT_EVENT] =	"CS_WAIT_EVENT",	\
	[CS_WAIT_RETRY] =	"CS_WAIT_RETRY",	\
	[CS_WAIT_PING] =	"CS_WAIT_PING",		\
	[CS_WAIT_REG_WINDOW] =	"CS_WAIT_REG_WINDOW",	\
	[CS_WAIT_PROP_RESP] =	"CS_WAIT_PROP_RESP",	\
	[CS_WAIT_PROP] =	"CS_WAIT_PROP",		\
	[CS_WAIT_FILE_GET] =	"CS_WAIT_FILE_GET",	\
}

enum client_http_req {
	CS_GET_INDEX,	/* GET dsns req */
	CS_GET_CMDS,	/* GET cmds req */
	CS_GET_COMMANDS, /* GET commands (and properties and schedules) req */
	CS_GET_LANIP,	/* GET lanip key */
	CS_POST_DATA,	/* POST data/prop req */
	CS_POST_DP_LOC,	/* POST DP_LOC req */
	CS_POST_IN_FIELD, /* POST In-Field provision req */
	CS_POST_LOGS,	/* POST logs req */
#ifdef AYLA_METRICS_SUPPORT
	CS_POST_METRICS, /* POST metrics req */
#endif
	CS_PUT_DP,	/* PUT Long DP req */
	CS_PUT_DP_CLOSE, /* PUT DP to mark it closed */
	CS_PUT_DP_FETCH, /* PUT Long DP fetch req */
	CS_GET_DP,	/* GET Long DP req */
	CS_GET_DP_LOC,	/* GET Location of Long DP */
	CS_PUT_INFO,	/* PUT Client Info req */
	CS_PUT_OEM_INFO,/* PUT OEM info req */
	CS_GET_OTA,	/* GET OTA patch */
	CS_PUT_OTA,	/* Put OTA failure */
	CS_GET_VAL,	/* Get Prop Val Req */
	CS_GET_ALL_VALS,/* Get All To-Dev Req */
	CS_PUT_PROP_ACK, /* PUT property ack */
	CS_PING,	/* PING req */
	CS_IDLE,	/* No on-going req */
};

struct client_lan_reg;
struct prop;
struct prop_dp_meta;

void client_init(void);
enum wifi_error client_status(void);
const char *client_host(void);
void client_set_setup_token(const char *);
int client_get_setup_token(char *token, size_t len);
void client_set_setup_location(char *);
void client_cli(int argc, char **argv);		/* deprecated API */
void client_log(const char *fmt, ...)
	ADA_ATTRIB_FORMAT(1, 2);
void client_server_reset(void);
void client_dest_up(u8 mask);
void client_dest_down(u8 mask);
enum ada_err client_prop_send_done_ext(u8 success, void *arg, u8 dest);
struct prop *client_prop_get_to_send(u8 dest_mask);
void client_prop_finish_send(u8 success, u8 dest_mask);

extern u8 mcu_feature_mask;

/*
 * Tcp Recv Payload
 */
struct recv_payload {
	void *data;		/* pointer to payload data */
	size_t len;		/* length of payload */
	size_t consumed;	/* bytes consumed so far from request */
	size_t tot_len;		/* total size of object from content-range */
};

enum prop_cb_status;

/*
 * Send the current valid destination mask.
 */
u8 client_valid_dest_mask(void);

/*
 * Mon_Spi is available for more pbufs. Ask http_client to resend.
 */
void client_tcp_recv_resend(void);

/*
 * Set callback to be used when ready to send a property to the service.
 * The client calls the callback when connected and ready to send the property.
 * The argument will be non-zero if the send is done and acked by the server.
 */
void client_send_callback_set(enum ada_err (*callback)(enum prop_cb_status stat,
				void *arg), u8 dest_mask);

/*
 * Returns a mask of the failed destinations
 */
u8 client_get_failed_dests(void);

#ifdef AYLA_FILE_PROP_SUPPORT
/*
 * Aborts any ongoing file operations
 */
void client_abort_file_operation(void);
#endif

/*
 * Send data.  May be called only from the send_callback set above.
 */
enum ada_err client_send_data(struct prop *);

/*
 * Send property ack.
 */
enum ada_err client_send_prop_ack(struct prop *prop);

#ifdef AYLA_FILE_PROP_SUPPORT
/*
 * Send dp loc request to server.
 */
enum ada_err client_send_dp_loc_req(const char *name,
			const struct prop_dp_meta *);

/*
 * Send dp put to server.
 */
enum ada_err client_send_dp_put(const u8 *prop_val, size_t prop_val_len,
		const char *prop_loc, u32 offset, size_t tot_len, u8 eof);

/*
 * Close FILE DP put
 */
enum ada_err client_close_dp_put(const char *loc);

/*
 * Fetch the s3 location of the file datapoint
 */
enum ada_err client_get_dp_loc_req(const char *prop_loc);

/*
 * Fetch the datapoint at the location and offset.
 */
enum ada_err client_get_dp_req(const char *prop_loc,
				u32 data_off, u32 data_end);

/*
 * Indicate to the service that MCU has fetched the dp.
 */
enum ada_err client_send_dp_fetched(const char *prop_loc);
#endif

/*
 * Notify client that a 206 (partial content) was received
 * in the previous get. So re-mark the cmd_event flag.
 */
void client_notify_if_partial(void);

/*
 * Send changed data to LAN App
 */
enum ada_err client_send_lan_data(struct client_lan_reg *, struct prop *, int);

struct mem_file {
	void *buf;
	size_t len;
	size_t max_len;
};

extern u8 ssl_enable;
enum conf_token;

int client_auth_encrypt(void *key, size_t key_len,
			void *buf, size_t, const char *req);
int client_auth_gen(void *key, size_t key_len,
			void *buf, size_t len, const char *req);

/*
 * Get vaue of "name" from ADS. If name isn't given, get all props.
 */
enum ada_err client_get_prop_val(const char *name);

/*
 * Convert string to "cents" value.
 */
long client_prop_strtocents(const char *val, char **errptr);

/*
 * Allow client to fetch prop and cmd updates from ADS
 */
void client_enable_ads_listen(void);

/*
 * Return current connectivity information
 */
u8 client_get_connectivity_mask(void);

/*
 * Return current events to send
 */
u8 client_get_event_mask(void);

#ifdef AYLA_FILE_PROP_SUPPORT
/*
 * Indicate if a file property transfer is ongoing.
 */
u8 client_ongoing_file(void);
#endif

/*
 * Update pending event mask
 */
void client_set_event_mask(u8 mask);

/*
 * Return 1 if the LAN mode is enabled in configuration
 */
int client_lanmode_is_enabled(void);

/*
 * Return 1 if the local control mode is enabled in configuration
 */
int client_lctrl_is_enabled(void);

/*
 * Return 1 if the lan_conf data was received from the service. Return 0
 * if the data didn't originate from the service. For example, the
 * device has been factory reset and not yet contacted the service.
 */
int client_lanconf_recvd(void);

/*
 * Return 1 if a user is registered to this device
 */
int client_is_reg_user(void);

/*
 * Start registration window.
 */
void ada_client_reg_window_start(void);

#define client_reg_window_start	ada_client_reg_window_start	/* deprecated */

#ifdef SERVER_DEV_PAGES
/*
 * Sets the callback so that client sends the sched debug info
 */
void client_set_sched_debug_cb(int value);
#endif

/*
 * Set the clock
 */
enum clock_src;
void client_clock_set(u32 new_time, enum clock_src src);

/*
 * Reset the mcu overflow flag (in case its set)
 */
void client_reset_mcu_overflow(void);

/*
 * Set MCU's feature mask. Called from data_tlv.
 */
void client_set_mcu_features(u8 features);

/*
 * Enable client automatic sync time.zero -- disabled, non-zero -- enabled
 */
void client_sync_time_enable(int enabled);

/*
 * Continue receiving, e.g., after flow control by host MCU stopped receive.
 */
enum ada_err client_continue_recv(void *);

/*
 * Set region for ADS.
 */
int client_set_region(const char *region);

/*
 * Set configured hostname for ADS.
 */
int client_set_server(const char *server);

/*
 * Indicate that the client_conf may have been changed by the platform.
 */
void client_commit(void);

/*
 * Hold client for future requests.
 * This may be used for operations that require multiple HTTP requests.
 *
 * Returns non-zero on failure, if client is already held.
 */
int client_hold(void);

/*
 * Release hold, prior to issuing new HTTP request.
 * Returns non-zero on failure, if hold was cleared, e.g., by link loss.
 */
int client_release(void);

/**
 * Initialize the ADA client environment.
 *
 * Uses the global struct ada_conf for configuration.
 *
 * \return zero on success, -1 on error.
 */
int ada_init(void);

/*
 * Bring up the client, the cloud connection and all LAN mode and local control
 * interfaces.
 *
 * Returns non-zero if disabled or not configured.
 */
int ada_client_up(void);


/*
 * Shut down the client, the cloud connection and all LAN mode and local
 * control access.
 */
void ada_client_down(void);

/*
 * Bring up the client, the cloud connection and LAN mode access. That
 * is bring up everything that depends on IP connectivity.
 *
 * Returns non-zero if disabled or not configured.
 */
int ada_client_ip_up(void);

/*
 * Shut down the client, the cloud connection and all LAN mode access.
 */
void ada_client_ip_down(void);

/*
 * Bring up local control access.
 *
 * Returns non-zero on error.
 */
int ada_client_lc_up(void);

/*
 * Shut down local control access.
 */
void ada_client_lc_down(void);

/*
 * Register for callback when ADS reachability changes, or a new
 * connection attempt fails.
 * This callback is made inside the client thread, and must not block.
 * Multiple callbacks may be registered, and they'll all be called.
 * Callbacks may not be unregistered for now.
 */
void ada_client_event_register(void (*fn)(void *arg, enum ada_err), void *arg);

/*
 * Register for a callback when the lanip key changes.
 */
void ada_client_lanip_cb_register(void (*fn)(void));

/*
 * Get signal strength from network layer, usually Wi-Fi.
 * Returns 0 on success, -1 if not supported.
 */
int adap_net_get_signal(int *signal);

/*
 * Get health of client.
 * Returns non-zero if client has been unable to contact service recently,
 * or if it has detected other problems that might warrant a reset.
 */
int ada_client_health_check(void);

/*
 * Set client health check test mode.
 * This uses shorter time spans to verify the client health check feature.
 */
void ada_client_health_test_mode(u8 enable);

/*
 * Enable client health check.
 * The system comes up with it disabled by default.
 */
void ada_client_health_check_en(void);

/*
 * Disable client health check.
 * The system comes up with it disabled by default.
 */
void ada_client_health_check_dis(void);

/*
 * Register a function to execute remote CLI commands sent to the device.
 */
void ada_client_command_func_register(void (*func)(const char *command));

/*
 * Handle "client" CLI command for debugging and testing.
 */
void ada_client_cli(int argc, char **argv);

/*
 * Clear Client DNS cache and re-lookup DNS names needed to connect to the
 * service, both ADS and ANS.  This is for security monitoring software.
 */
void ada_client_dns_clear(void);

#ifdef AYLA_TEST_SERVICE_SUPPORT
/*
 * Enable use of the specified test server. This enables the device to
 * be configured via a mobile application to use test service instead of the
 * of the production service the device would normally use. The service to
 * enable is specified by its nickname, e.g. "us-test".
 */
int ada_client_test_svc_enable(const char *nickname);
#endif

/*
 * Pend a callback for the client thread.
 * For internal use only.
 * The client lock will *not* be held during the handler.
 */
struct callback;
void ada_callback_pend(struct callback *);

#ifdef __cplusplus
}
#endif

#endif /* __AYLA_CLIENT_H__ */
