/*
 * Copyright 2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_AL_COMMON_NET_STREAM_H__
#define __AYLA_AL_COMMON_NET_STREAM_H__

#include <ayla/utypes.h>
#include <al/al_err.h>
#include <platform/pfm_net_addr.h>

/**
 * @file
 * @brief Network stream interfaces
 *
 * The network stream interfaces adapt to the platform's network layer.
 * A stream represents one connection or network session between the
 * local system and a remote system (e.g., an internet server or a mobile app).
 *
 * Streams may be type TCP, TLS, DTLS, or UDP.
 * The platform may support only a subset of these types.
 * For a system using HTTP or MQTT, TLS and TCP are required.
 * For a system using COAP, DTLS is required.
 * UDP streams are no longer required at all.
 *
 * A stream is created using `al_net_stream_new()` and deleted with
 * `al_net_stream_close()`.
 *
 * The stream APIs are non-blocking and will not wait for a response from
 * the remote system.
 *
 * A callback function is set for receiving using `al_net_stream_set_recv_cb()`.
 * This function is called in the context of the ADA thread when data arrives.
 *
 * An argument passed the callback functions is set with
 * `al_net_stream_set_arg()`.
 *
 * An error callback function is registered via `al_net_stream_set_err_cb()`.
 * The error callback function is called remote side closes the connection
 * or when the network connection is lost, or when any unhandled error occurs.
 *
 * An active connection or session is made using `al_net_stream_connect()`
 * specifying the remote hostname, IP address, and port, as well as the
 * function to call once the connection is complete.
 *
 * A passive TCP conection may be created using `al_net_stream_listen()`.
 * This is currently only used for the alternative HTTP server on some systems.
 *
 * To send on a stream, call `al_net_stream_write()` to add data to the
 * output buffer.   To send all buffered data, call `al_net_stream_output()`.
 *
 * Notification of bytes successfully sent is given by a "sent" callback
 * function which is registered using `al_net_stream_set_sent_cb()`.
 */

/*
 * Doxygen / markdown table format ... not working TODO
 * ADA layer | HTTP    | HTTPS | MQTTS | COAP
 * :-------: | :-----: | :---: | :---: | :---:
 *  AL_NET_STREAM	                  |||
 *            |    NET_TLS             || DTLS
 *              NET_TCP               ||| UDP
 *
 */

/**
 * Maximum write length for al_net_stream_write().
 */
#define AL_NET_STREAM_WRITE_BUF_LEN	1400

/*
 * HTTP	    |   HTTPS	    |	COAP
 *------------------------------------
 *		STREAM
 *------------------------------------
 *	    |   NET_TLS	    |	DTLS
 * NET_TCP  |   NET_TCP	    |	COAP
 *	    |		    |	UDP
 *------------------------------------
 */

/**
 * Structure of stream control block.
 */
struct al_net_stream;

/**
 * Type of stream connection.
 *
 * Note that the platform may implement a subset of these, depending on
 * the supported network and client types.
 *
 * client-v1 will require TCP and TLS.
 * client-v2 will require UDP and DTLS, (but all types initially).
 */
enum al_net_stream_type {
	AL_NET_STREAM_TCP,  /*!< Use TCP */
	AL_NET_STREAM_TLS,  /*!< Use TLS over TCP */
	AL_NET_STREAM_UDP,  /*!< Use UDP */
	AL_NET_STREAM_DTLS, /*!< Use DTLS over UDP */
	AL_NET_STREAM_MAX,  /*!< al_net_stream_type limit */
};

/**
 * Open a new stream.
 *
 * \param type is the type of http client. It is also the stream type.
 *
 * \returns a pointer to struct al_net_stream, or NULL on failure.
 */
struct al_net_stream *al_net_stream_new(enum al_net_stream_type type);

/**
 * Close the specified stream.
 *
 * \param stream is the stream to be closed.
 */
enum al_err al_net_stream_close(struct al_net_stream *stream);

/**
 * Set the callback argument for the stream.
 *
 * \param stream is the stream.
 * \param arg is the opaque argument to be passed to receive, accept,
 * connected, and sent callbacks.
 */
void al_net_stream_set_arg(struct al_net_stream *stream, void *arg);

/**
 * Connect to remote server.
 *
 * \param stream is the stream.
 * \param hostname is the remote host name. It is used in certificates
 * checking.
 * \param host_addr is the remote host address.
 * \param port is the remote port.
 * \param connected is a callback which will be called when the
 * connection completes.
 * The first argument of connected() is the value set by
 * al_net_stream_set_arg().
 * The second argument is a pointer to al_net_stream structure.
 * The third argument is the error number.
 *
 * \returns zero on success, error code on failure.
 */
enum al_err al_net_stream_connect(struct al_net_stream *stream,
	const char *hostname, struct al_net_addr *host_addr, u16 port,
	enum al_err (*connected)(void *arg, struct al_net_stream *,
	enum al_err err));

/**
 * Listen for connections.
 *
 * This is supported only for AL_NET_STREAM_TCP.
 * It creates a new stream for the incoming connection.
 *
 * \param stream is the listening stream.
 * \param local_addr is the local host address which can be NULL.
 * \param local_port is the local port number.
 * \param backlog is the number of pending connections to allow to be queued.
 * \param accept is a callback which will be called when a connection arrives.
 * The first argument of accept() is the value set by
 * al_net_stream_set_arg().
 * The second argument is a pointer to a new al_net_stream structure.
 * The third argument of accept is a pointer to the remote IP address.
 * The fourth argument of accept is the remote port number.
 * \returns zero on success, error code on error.
 */
enum al_err al_net_stream_listen(struct al_net_stream *stream,
	const struct al_net_addr *local_addr, u16 local_port, int backlog,
	void (*accept)(void *arg, struct al_net_stream *,
	    const struct al_net_addr *peer_addr, u16 peer_port));

/**
 * Get the connection status of net stream.
 *
 * \param stream is the stream.
 * \returns non-zero if the connection is established (connected).
 */
int al_net_stream_is_established(struct al_net_stream *stream);

/**
 * continue to receive.
 *
 * If the recv_cb does not handle all of the input, the receive is paused,
 * and recv_cb will not be called again until this function is called to
 * continue receiving.  After the continue, recv_cb will be called with the
 * unprocessed data.
 *
 * \param stream is the stream.
 */
void al_net_stream_continue_recv(struct al_net_stream *stream);

/**
 * Set the receive-callback for receiving data.
 *
 * \param stream is the stream.
 * \param recv_cb is the callback for received data.  This may be NULL.
 *
 * About receive flow control:
 * For TCP and TLS, The receive callback will call al_net_stream_recved()
 * to indicate how much of the packet has been processed.  If less than the
 * full amount is processed, the stream will be considered paused, and
 * further receive callbacks will not occur until al_net_stream_continue_recv()
 * is called for the stream.
 *
 * For UDP and DLTS, the receive callback will be called with entire packets,
 * and flow control (paused stream) is not supported.
 *
 * The first argument of recv_cb() is the value set by
 * al_net_stream_set_arg().
 * The second argument is a pointer to the al_net_stream structure.
 * The third argument is the data received,
 * The fourth argument is the data size.
 */
void al_net_stream_set_recv_cb(struct al_net_stream *stream,
	enum al_err (*recv_cb)(void *arg, struct al_net_stream *stream,
	void *data, size_t size));

/**
 * Indicate that bytes have been received.
 *
 * This call indicates that len bytes have been received and can be
 * acknowledged by the stream.
 * If less than the full amount delievered to the receive callback is
 * is to be acknowledged, the stream becomes paused.
 *
 * \param stream is the stream.
 * \param len is the number of bytes to be acknowledged.
 */
void al_net_stream_recved(struct al_net_stream *stream, size_t len);

/**
 * Put data to stream buffer. Actually transmission may or may not be delayed.
 *
 * About send flow control: The lower layers may not be able to send the data
 * that passed to `al_net_stream_write()`.
 * In this case, nothing is written and the error AL_ERR_BUF is returned.
 * The condition will persist until enough data has been sent to allow
 * more to be buffered.
 * When some of the buffered data has been sent, the sent callback is called,
 * and the upper layer may try another call to `al_net_stream_write()`.
 * That may also return AL_ERR_BUF, but eventually enough of the buffered data
 * will have been sent and a write should succeed.
 *
 * \param stream is the stream.
 * \param data is the data to be sent.
 * \param len is data size.
 * \returns zero on success.
 * \returns AL_ERR_BUF if the data cannot be sent temporarily until
 * previously buffered data is sent.
 */
enum al_err al_net_stream_write(struct al_net_stream *stream,
	const void *data, size_t len);

/**
 * Send any queued data buffered in the stream.
 *
 * \param stream is the stream.
 * \returns zero on success, error code on error.
 *
 * This sends any data that has been buffered on the stream. This should be
 * called after all the pfm_net_tcp_write() calls are complete for a request,
 * for example, or when a response is complete.
 * This call should not block waiting for output to be sent.
 */
enum al_err al_net_stream_output(struct al_net_stream *stream);

/**
 * Set a callback to be called when data is sent to remote.
 *
 * \param stream is the stream.
 * \param sent_cb is a callback function to be called when data is sent
 * and acknowledged. This may be NULL.
 *
 * The first argument of sent_cb() is the value set by al_net_stream_set_arg().
 * The second argument is stream handle.
 * The third argument is the number of bytes sent.
 */
void al_net_stream_set_sent_cb(struct al_net_stream *stream,
	void(*sent_cb)(void *arg, struct al_net_stream *stream,
	size_t len_sent));

/**
 * Set the error callback.
 *
 * \param stream is the stream.
 * \param err_cb is the callback for reporting error.  This may be NULL.
 * The first argument of err_cb() is the value set by al_net_stream_set_arg().
 * The second argument is the error number.
 */
void al_net_stream_set_err_cb(struct al_net_stream *stream,
	void (*err_cb)(void *arg, enum al_err err));

#endif /* __AYLA_AL_COMMON_NET_STREAM_H__ */
