/*
 * Copyright 2020 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_ADA_MQTT_CLIENT_H__
#define __AYLA_ADA_MQTT_CLIENT_H__

#include <al/al_utypes.h>
#include <al/al_net_addr.h>
#include <al/al_err.h>

/**
 * Maximum amount of data to be delivered in a sub_handle() callback.
 */
#define MQTT_CLIENT_SUB_BUF_LEN	600	/* sets buffer sizes */

/**
 * Opaque structure representing a MQTT client.
 */
struct mqtt_client;

/**
 * Initialize mqtt client.
 */
void mqtt_client_init(void);

/**
 * Finalize mqtt client.
 */
void mqtt_client_final(void);

/**
 * Set the host addr to the mqtt client. It is used to connect the local
 * broker.
 * \param addr is a pointer of the host addr of the local mqtt broker
 */
void mqtt_set_host_addr(struct mqtt_client *mc,
	struct al_net_addr *addr);

/** Default Keep Alive, which is a time interval measured in seconds. */
#ifdef AYLA_MQTT_KEEPALIVE_INTVL
#define MQTT_CLIENT_DEFAULT_KEEPALIVE AYLA_MQTT_KEEPALIVE_INTVL
#else
#define MQTT_CLIENT_DEFAULT_KEEPALIVE 40
#endif
#if MQTT_CLIENT_DEFAULT_KEEPALIVE > 40
/*
 * A heartbeat cycle that is too long may cause connection issues when the
 * timeout of the TCP proxy server is less than this value.
 */
#warning The keeping alive period is too long.
#endif

/**
 * MQTT connection error code.
 */
enum mqtt_client_conn_err {
	MQTT_CONN_ERR_DNS_REQ = -3,	/**< DNS request error */
	MQTT_CONN_ERR_STREAM = -2,	/**< TCP or TLS stream error */
	MQTT_CONN_ERR_TIMEOUT = -1,	/**< Time out */
	MQTT_CONN_ERR_OK,		/**< Connection Accepted */
	MQTT_CONN_ERR_VERSION,		/**< Connection Refused:
					    unacceptable protocol version */
	MQTT_CONN_ERR_ID,		/**< Connection Refused:
					    identifier rejected */
	MQTT_CONN_ERR_BROKER,		/**< Connection Refused:
					    broker unavailable */
	MQTT_CONN_ERR_ACOUNT,		/**< Connection Refused:
					    bad user name or password */
	MQTT_CONN_ERR_NOT_AUTH,		/**< Connection Refused:
					    not authorised */
};

/**
 * MQTT Quality of Service levels.
 */
enum mqtt_client_qos {
	MQTT_QOS_0,	/**< QoS 0: At most once delivery */
	MQTT_QOS_1,	/**< QoS 1: At least once delivery */
	MQTT_QOS_2,	/**< QoS 2: Exactly once delivery */
};

/**
 * Allocate a MQTT client.
 *
 * \returns a pointer to opaque structure representing a MQTT client.
 */
struct mqtt_client *mqtt_client_new(void);

/**
 * Free a MQTT client.
 *
 * \param mc a pointer to opaque structure representing a MQTT client.
 * \returns zero if successful, -1 means this mqtt client is connected,
 *	need to call mqtt_client_disconnect and wait for disconnect_cb;
 */
int mqtt_client_free(struct mqtt_client *mc);

/**
 * Connect to MQTT broker with TLS asynchronously.
 *
 * By default, The keepalive of the connection is set to 60.
 *
 * \note The callback functions, connack_cb(), err_cb(), should be called in
 * the ADA thread, so use pfm_callback_pend() to send it to ADA thread.
 *
 * \param mc a pointer to opaque structure representing a MQTT client.
 * \param host is the broker host name or addr.
 * \param port is the mqtt broker port.
 * \param ssl_enable is the mqtt connect with tls or not.
 * \param client_id is a pointer to client id.
 * \param connack_cb is a function about the MQTT connack callback.
 *	It's parameters are the mc, which is a pointer to opaque structure
 *	representing a MQTT client, and the err, is the enumeration of return
 *	error, and the arg, is the value set by mqtt_client_set_arg().
 * \param err_cb is a function about the network connect error callback.
 *	It's parameters are the mc, which is a pointer to opaque structure
 *	representing a MQTT client, and the err, is the network connect error
 *	code, and the arg, is the value set by mqtt_client_set_arg().
 * \returns zero if successful, otherwise, non-zero.
 */
int mqtt_client_connect(struct mqtt_client *mc,
	const char *host, u16 port,
	int ssl_enable, const char *client_id, void (*connack_cb)
	(struct mqtt_client *mc, enum mqtt_client_conn_err err,
	void *arg),
	void (*err_cb)(struct mqtt_client *mc,
	enum al_err err, void *arg));

/**
 * Set the callback argument for a MQTT client.
 *
 * \param mc a pointer to opaque structure representing a MQTT client.
 * \param arg is the opaque argument to be passed to connack_cb, err_cb,
 *	sub_handle, suback_cb, unsuback_cb, and pub_completed callbacks.
 */
void mqtt_client_set_arg(struct mqtt_client *mc, void *arg);

/**
 * Set the mqtt client connect options username and password.
 *
 * \param mc a pointer to opaque structure representing a MQTT client.
 * \param username is a pointer to username.
 * \param password is a pointer to connect options about password.
 * \returns zero if successful, otherwise, non-zero.
 */
enum al_err mqtt_client_set_conn_account(struct mqtt_client *mc,
	const char *username, const char *password);

/**
 * Set the mqtt client connect options keepalive.
 *
 * \param mc a pointer to opaque structure representing a MQTT client.
 * \param keepalive is the mqtt client keepalive interval.
 * \returns zero if successful, otherwise, non-zero.
 */
int mqtt_client_set_conn_keepalive(struct mqtt_client *mc,
	u16 keepalive);

/**
 * Disconnect with MQTT broker.
 *
 * \note The callback function, disconnected_cb(), should be called in
 * the ADA thread, so use pfm_callback_pend() to send it to ADA thread.
 *
 * \param mc a pointer to opaque structure representing a MQTT client.
 * \param disconnected_cb is the function about disconnected callback.
 *	It's parameters are the mc, which is a pointer to opaque structure
 *	representing a MQTT client, and the arg, which is the value set by
 *	mqtt_client_set_arg().
 * \returns zero if successful, otherwise, non-zero.
 */
int mqtt_client_disconnect(struct mqtt_client *mc,
	void (*disconnected_cb)(struct mqtt_client *mc, void *arg));

/**
 * Subscribe a topic to MQTT broker.
 *
 * \note The callback functions, sub_handle(), suback_cb(), should be called in
 * the ADA thread, so use pfm_callback_pend() to send it to ADA thread.
 *
 * \param mc a pointer to opaque structure representing a MQTT client.
 * \param topic is a pointer to the subscription topic.
 * \param qos is the requested Quality of Service for this subscription.
 * \param sub_handle is the function about subscribe message handle.
 *	It's parameters are the payload, which is a pointer to payload,
 *	and the len, is the size of payload (bytes), and the total len,
 *	is the length of this subscribe message total payload, and the arg,
 *	is the value set by mqtt_client_set_arg(), and the the mc, which is a
 *	pointer to opaque structure representing a MQTT client.
 *	It returns the number of bytes used. If less than the len provided is
 *	returned, mqtt_client_continue() must be called when ready to receive
 *	more.
 * \param suback_cb is the function about suback callback.
 *	It's parameters are the arg, is the value set by mqtt_client_set_arg(),
 *	and the err, is the unsubscription return error, AL_ERR_OK on success,
 *	AL_ERR_TIMEOUT on timeout, others on failure, and the the mc, which is
 *	a pointer to opaque structure representing a MQTT client.
 * \returns zero if successful, otherwise, non-zero.
 */
int mqtt_client_subscribe_topic(struct mqtt_client *mc,
	const char *topic,
	enum mqtt_client_qos qos,
	size_t (*sub_handle)(struct mqtt_client *mc,
		void *arg, const void *payload, size_t len, size_t total_len),
	void (*suback_cb)(struct mqtt_client *mc,
		void *arg, enum al_err err));

/**
 * Pause the delivery of all topics and messages.
 *
 * This is used when a sub_handle() callback is unable to process all of
 * the data immediately.  No further calls to sub_handle() callbacks will
 * be made until mqtt_client_continue() is called.
 *
 * \param mc a pointer to the MQTT client.
 */
void mqtt_client_pause(struct mqtt_client *mc);

/**
 * Resume the delivery of all topics and messages.
 *
 * This is be called after mqtt_client_pause().
 *
 * \param mc a pointer to the MQTT client.
 */
void mqtt_client_continue(struct mqtt_client *mc);

/**
 * Unsubscribe a topic to MQTT broker.
 *
 * \note The callback function, unsuback_cb(), should be called in
 * the ADA thread, so use pfm_callback_pend() to send it to ADA thread.
 *
 * \param mc a pointer to opaque structure representing a MQTT client.
 * \param topic is a pointer to the unsubscription topic.
 * \param unsuback_cb is the function about unsuback callback.
 *	It's parameters are the arg which is the value set by
 *	mqtt_client_set_arg(),
 *	and the err, is the unsubscription return error, AL_ERR_OK on success,
 *	AL_ERR_TIMEOUT on timeout, others on failure, and the the mc, which is
 *	a pointer to opaque structure representing a MQTT client.
 * \returns zero if successful, otherwise, non-zero.
 */
int mqtt_client_unsubscribe_topic(struct mqtt_client *mc,
	const char *topic,
	void (*unsuback_cb)(struct mqtt_client *mc, void *arg,
	enum al_err err));

/**
 * Publish a topic header to MQTT broker.
 *
 * This function sends the mqtt publish header to MQTT broker.
 * The publish payload will be sent by mqtt_client_publish_payload().
 * If this function returns AL_ERR_IN_PROGRESS, it means that the last
 * publish has not been completed.
 *
 * \note The callback function, pub_completed(), should be called in
 * the ADA thread, so use pfm_callback_pend() to send it to ADA thread.
 *
 * \param mc a pointer to opaque structure representing a MQTT client.
 * \param topic is a pointer to the publish topic
 * \param qos is the requested Quality of Service for this publish message.
 * \param total_len is the size of the whole payload (bytes).
 * \param pub_completed is the function about publish completed callback.
 *	It's parameters are the arg which is the value set by
 *	mqtt_client_set_arg(),
 *	and the err, is the unsubscription return error, AL_ERR_OK on success,
 *	AL_ERR_TIMEOUT on timeout, others on failure, and the the mc, which is
 *	a pointer to opaque structure representing a MQTT client.
 * \returns zero on success, error code on failure.
 */
enum al_err mqtt_client_publish_topic_header(struct mqtt_client *mc,
	const char *topic, enum mqtt_client_qos qos, u32 total_len,
	void (*pub_completed)(struct mqtt_client *mc,
	void *arg, enum al_err err));

/**
 * Send publish payload data (chunked or non-chunked) to MQTT broker.
 *
 * This API must be used after calling mqtt_client_publish_topic()
 * when the publish payload total len is larger than 0.
 *
 * \param mc a pointer to opaque structure representing a MQTT client.
 * \param payload is a pointer to the publish content.
 * \param len is the size of the payload (bytes).
 * \returns zero if successful, error code on failure.  Returns AL_ERR_BUF
 * if the send should be repeated later.
 */
enum al_err mqtt_client_publish_payload(struct mqtt_client *mc,
	const void *payload, u32 len);

/**
 * Set callback for when mqtt_client_publish_payload() should be retried.
 *
 * \param mc a pointer to opaque structure representing a MQTT client.
 * \param arg is the opaque argument set by mqtt_client_set_arg().
 */
void mqtt_client_set_sent_cb(struct mqtt_client *mc,
	void (*sent_cb)(void *arg));

/**
 * Abort MQTT client.
 *
 * \param mc a pointer to opaque structure representing a MQTT client.
 */
void mqtt_client_abort(struct mqtt_client *mc);

/**
 * Set log module (subsystem) number and flags for MQTT.
 *
 * \param mc a pointer to opaque structure representing a MQTT client.
 * \param mod_nr is the logging module (subsystem) number for log_put_mod().
 */
void mqtt_client_log_set(struct mqtt_client *mc, enum mod_log_id mod_nr);

#endif /* __AYLA_ADA_MQTT_CLIENT_H__ */
