/*
 * Copyright 2020 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __HTTP2MQTT_CLIENT_H__
#define __HTTP2MQTT_CLIENT_H__

#include "http_client.h"

#define MQTT_CLIENT_SERVER_PORT_SSL	8883

/**
 * Initialize http2mqtt client.
 */
void http2mqtt_client_init(void);

#ifdef AYLA_FINAL_SUPPORT
/**
 * Finalize http2mqtt client.
 */
void http2mqtt_client_final(void);
#endif

/**
 * Set MQTT keepalive.
 *
 * \param keepalive is the MQTT client keepalive interval.
 */
void http2mqtt_client_set_keepalive(u16 keepalive);

/**
 * Set a notify callback.
 *
 * When the device data changed, the DNC will send a notify message to device.
 *
 * \param hc is a pointer to opaque structure representing a http client.
 * \param notify_cb is a callback called when the device data changed.
 */
void http2mqtt_client_set_notify_cb(struct http_client *hc,
	void (*notify_cb)(void *arg));

/**
 * Set a mqtt connection state callback.
 *
 * \param hc is a pointer to opaque structure representing a http client.
 * \param conn_state_cb is a callback called when the mqtt connection
 *	state changes.
 */
void http2mqtt_client_set_conn_state_cb(struct http_client *hc,
	void (*conn_state_cb)(enum http2mqtt_conn_state st));

/**
 * Disconnect the MQTT client.
 *
 * \param hc is a pointer to opaque structure representing a http client.
 */
void http2mqtt_client_disconnect(struct http_client *hc);

/**
 * Abort the last request.
 *
 * \param hc is a pointer to opaque structure representing a http client.
 */
void http2mqtt_client_abort(struct http_client *hc);

/**
 * Start a http request.
 *
 * \param hc is a pointer to opaque structure representing a http client.
 * \param method is the request method.
 * \param resource is the request url.
 * \param hcnt is the count of the HTTP header tags.
 * \param hdrs is the header tags.
 */
void http2mqtt_client_req(struct http_client *hc,
    enum http_client_method method,
    const char *resource, int hcnt, const struct http_hdr *hdrs);

/**
 * Send a part of the http request payload.
 *
 * \note http2mqtt_client_send() can be used only to send HTTP payload data
 *	(chunked or non-chunked). If hc->http_tx_chunked is set,
 *	http2mqtt_client_send() will convert data to http chunked data.
 *	Do not use this function to send HTTP request.
 *
 * \param hc is a pointer to opaque structure representing a http client.
 * \param part is the part of payload.
 * \param len is the length of part.
 * \returns AE_OK on success, error code on failure.
 */
enum ada_err http2mqtt_client_send(struct http_client *hc,
    const void *part, u16 len);

/**
 * Send blank padding if needed for PUT or POST.
 *
 * \note Indicates body is complete.
 *	The buffer may be used for padding.
 *	This implementation can send the shorter body without padding.
 *	This can be set as a send_data_cb function.
 *
 * \param hc is a pointer to opaque structure representing a http client.
 */
void http2mqtt_client_send_pad(struct http_client *hc);

/**
 * Interface for the client to complete the send request.
 *
 * \note This should only be called from the send callback when
 *	it has completed the final send.
 *
 * \param hc is a pointer to opaque structure representing a http client.
 */
void http2mqtt_client_send_complete(struct http_client *hc);

/**
 * Reset the http client.
 *
 * \param hc is a pointer to opaque structure representing a http client.
 * \param mod_log_id is the module log numbers
 */
void http2mqtt_client_reset(struct http_client *hc, enum mod_log_id);

/**
 * Interface for the client to ask http2mqtt client to start receiving
 * server data again.
 *
 * \param hc is a pointer to opaque structure representing a http client.
 */
void http2mqtt_client_continue_recv(struct http_client *hc);

#endif /* __HTTP2MQTT_CLIENT_H__ */
