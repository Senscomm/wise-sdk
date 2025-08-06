/*
 * Copyright 2020 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __MQTTV1_CLIENT_H__
#define __MQTTV1_CLIENT_H__

#include "http_client.h"

/**
 * Connection state.
 */
enum mqttv1_conn_state {
	MCS_DISCONNECTED,
	MCS_CONNECTED,
};

/**
 * Initialize MQTT V1 client.
 */
void mqttv1_client_init(void);

#ifdef AYLA_FINAL_SUPPORT
/**
 * Finalize MQTT V1 client.
 */
void mqttv1_client_final(void);
#endif

/**
 * Set the callback argument.
 *
 * \param arg is the opaque argument to be passed to notify_cb,
 *	conn_state_cb, resp_start_cb, resp_payload_cb, and err_cb callbacks.
*/
void mqttv1_client_set_arg(void *arg);

/**
 * Set MQTT keepalive.
 *
 * \param keepalive is the MQTT client keepalive interval.
*/
void mqttv1_client_set_keepalive(u16 keepalive);

/**
 * Set a notify callback.
 *
 * When the device data changed, a notify message will be sent to device.
 *
 * \param notify_cb is a callback called when the device data changed.
 */
void mqttv1_client_set_notify_cb(void (*notify_cb)(void *arg));

/**
 * Set a connection state callback.
 *
 * \param conn_state_cb is a callback called when the connection state changes.
 */
void mqttv1_client_set_conn_state_cb(
	void (*conn_state_cb)(enum mqttv1_conn_state st, void *arg));

/**
 * Set a "sent data" callback.
 *
 * \param sent_cb is a callback called when some buffered data has been sent.
 * This indicates that the upper layer should try sending waiting data.
 */
void mqttv1_client_set_sent_cb(void (*sent_cb)(void *arg));

/**
 * Set broker.
 *
 * \param hostname is the broker host name or addr.
 * \param port is the mqtt broker port.
 */
void mqttv1_client_set_broker(const char *hostname, u16 port);

/**
 * Start a request.
 *
 * \note If payload_len > 0, \b mqttv1_client_send_payload() must be called,
 * which can be called multiple times to send all payloads.
 *
 * \param method is the request method.
 * \param url is the request url.
 * \param hcnt is the count of the HTTP header tags.
 * \param hdrs is the header tags.
 * \param payload_len is the length of the payload.
 * \param resp_start_cb is a callback called when the response is started.
 * \param resp_payload_cb is a callback called when the payload is received,
 *	if part and length is zero, it means the response has been completed.
 *	It returns the length processed.
 * \param err_cb is a callback called when an error occurs.
 *
 * \returns AL_ERR_OK on success, other error codes on failure.
 */
enum al_err mqttv1_client_req_start(enum http_client_method method,
	const char *url,
	int hcnt, const struct http_hdr *hdrs, u32 payload_len,
	void (*resp_start_cb)(u32 http_status, int hcnt,
	    const struct http_hdr *hdrs, void *arg),
	size_t (*resp_payload_cb)(const void *part, size_t len, void *arg),
	void (*err_cb)(enum al_err err, void *arg));

/**
 * Send a part of the request payload.
 *
 * \note This function must be called after \b mqttv1_client_req_start().
 *
 * \param part is the part of the payload.
 * \param len is the length of the part.
 *
 * \returns AL_ERR_OK on success, other error codes on failure.
 */
enum al_err mqttv1_client_send_payload(const void *part, u32 len);

/**
 * Abort the last request.
 */
void mqttv1_client_abort(void);

/**
 * Timeout abort the last request.
 */
void mqttv1_client_timeout_abort(void);

/**
 * Disconnect the MQTT client.
 */
void mqttv1_client_disconnect(int clear_conn_flag);

/**
 * Pause receive.
 *
 * This is used when resp_payload_cb() has handled some or all of the
 * buffer, but wants no more input until mqttv1_client_continue() is called.
 */
void mqttv1_client_pause(void);

/**
 * Continue receiving after paused by mqttv1_client_pause().
 */
void mqttv1_client_continue(void);

/**
 * Set log module (subsystem) number and flags for MQTTv1.
 */
void mqttv1_client_log_set(enum mod_log_id mod_nr);

#endif /* __MQTTV1_CLIENT_H__ */
