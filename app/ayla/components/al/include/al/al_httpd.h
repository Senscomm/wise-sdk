/*
 * Copyright 2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_AL_COMMON_HTTPD_H__
#define __AYLA_AL_COMMON_HTTPD_H__

/**
 * @file
 * User interface of HTTPD (HTTP server).
 *
 * Server app calls al_httpd_reg_urls_handler() to register handler. When a
 * remote client connects to the HTTPD server, the connection is accepted and
 * an al_httpd_conn structure is created. The request header is parsed by
 * HTTPD, and the registered handler is called on the ADA thread. The request
 * payload is handled by the handler. The handler processes the request and
 * responds to the client when the response data is ready.
 *
 * If a handler cannot complete the request immediately, it returns a
 * function pointer to the function to be called to continue handling the
 * request.  When it is ready to be completed, al_httpd_complete() is called
 * to ask HTTPD to continue by calling that completion function on the ADA
 * client thread.
 *
 * The handler may or may not be called in the ADA thread, but is not allowed
 * to block in any case.
 *
 * In the implementation on Linux, httpd runs in ADA thread (single thread).
 * If registered callback is called right after the http request is parsed,
 * the payload data may not be fully obtained when the client sends the payload
 * data chunk by chunk. So in that single-thread model, the callback is
 * called only after the entire request is parsed.
 *
 * If httpd runs in another thread on the platform, the registered http callback
 * can be called in that other thread before the entire body of the request
 * has been received. TODO - not sure this is right.
 */
#include <stdlib.h>
#include <ayla/utypes.h>

/**
 * HTTP method or request type.
 */
enum al_http_method {
	AL_HTTPD_METHOD_BAD = 0,	/**< Invalid method */
	AL_HTTPD_METHOD_GET,		/**< GET method */
	AL_HTTPD_METHOD_HEAD,		/**< HEAD method */
	AL_HTTPD_METHOD_POST,		/**< POST method */
	AL_HTTPD_METHOD_PUT,		/**< PUT method */
	AL_HTTPD_METHOD_DELETE,		/**< DELETE method */
};

/**
 * The structure is the context used for client connection.
 */
struct al_httpd_conn;

/**
 * Start an HTTPD server.
 *
 * \param port is service port
 *
 * \return zero on success
 */
int al_httpd_start(u16 port);

/**
 * Stop httpd server
 */
void al_httpd_stop(void);

/**
 * Register a callback for a specified url and method.
 *
 * \param url is the resource portion of the HTTP URL.
 * If uri URL is an empty string, the callback (cb) is registered
 * as the handler to be used for URLs that not registered, and for any method.
 * \param method is the HTTP method.
 *
 * \param cb is a handler to process the request. The return value of cb is a
 * completion handler to continue processing the request, or NULL when the
 * handler has finished the request.
 *
 * \return zero on success.
 */
int al_httpd_reg_url_cb(const char *url, enum al_http_method method,
	int (*(*cb)(struct al_httpd_conn *))(struct al_httpd_conn *, void *));

/**
 * Get method of the current request.
 *
 * \param conn is pointer to connection context.
 *
 * \return an pointer to the method string in the request header.
 */
const char *al_httpd_get_method(struct al_httpd_conn *conn);

/**
 * Get resource in the current request.
 *
 * \param conn is pointer to connection context.
 *
 * \return an pointer to the resource string in the request header. NULL is
 * not found.
 *
 * Note: For an url like "http://host/my-query?a=123&b=hello", the
 * returned resource is "/my-query". To get argument a and b, please call:
 *	val1 = al_httpd_get_arg(struct al_httpd_conn *conn, "a");
 *	val2 = al_httpd_get_arg(struct al_httpd_conn *conn, "b");
 */
const char *al_httpd_get_resource(struct al_httpd_conn *conn);

/**
 * Get argument value in the current request's url.
 *
 * \param conn: pointer to connection context.
 * \param name: argument name.
 *
 * \return an pointer to the value string of the argument. NULL is not found.
 */
const char *al_httpd_get_url_arg(struct al_httpd_conn *conn, const char *name);

/**
 * Get HTTP protocol version in the current request.
 *
 * \param conn is a pointer to connection context.
 *
 * \return an pointer to the http version string in request header.
 */
const char *al_httpd_get_version(struct al_httpd_conn *conn);

/**
 * Get an header in the request.
 *
 * \param conn is a pointer to connection context.
 * \param name is the name of the item. It is case insensitive.
 *
 * \return a pointer to the header value.
 *
 * Note: if the name is "Date", and the received header line is
 * "Date:xxxxxxxx\r\n", then it returns a pointer to "xxxxxxxx".
 */
const char *al_httpd_get_req_header(struct al_httpd_conn *conn,
	const char *name);

/**
 * Get the local ip of the connection.
 *
 * \param conn is a pointer to connection context.
 *
 * \return ip address if success, or return NULL.
 */
const struct al_net_addr *al_httpd_get_local_ip(struct al_httpd_conn *conn);

/**
 * Get the remote ip of the connection.
 *
 * \param conn is a pointer to connection context.
 *
 * \return ip address if success, or return NULL.
 */
const struct al_net_addr *al_httpd_get_remote_ip(struct al_httpd_conn *conn);

/**
 * Continue handling a request that was paused.
 * The completion handler returned by the URL request handler should be called.
 *
 * \param conn is a pointer to connection context
 * \param arg is the caller's context.
 * \return zero on success.
 */
int al_httpd_complete(struct al_httpd_conn *conn, void *arg);

/**
 * Read the payload in the current request.
 *
 * \param conn is a pointer to connection context
 * \param buf is a buffer for data read
 * \param buf_size is the size of the buffer
 *
 * \return the number of bytes read. 0 if reached the end of the data.
 * Negative is error.
 */
int al_httpd_read(struct al_httpd_conn *conn, char *buf, size_t buf_size);

/**
 * Send HTTP response header to the client.
 *
 * \param conn is a pointer to connection context
 * \param status is HTTP status code.
 * \param headers is a string of response headers. The headers are separated
 *  by '\\r\\n'. The last header is ended in '\\r\\n' or '\\0'. It can be
 * NULL if no additional headers need to be specified.
 *
 * \return zero on success.
 */
int al_httpd_response(struct al_httpd_conn *conn, int status,
	const char *headers);

/**
 * Write HTTP response payload.
 *
 * \param conn is a pointer to connection context
 * \param data is the data to be written
 * \param size is size of the data
 *
 * \return zero on success.
 */
int al_httpd_write(struct al_httpd_conn *conn, const char *data, size_t size);

/**
 * Close httpd client connection.
 *
 * \param conn: pointer to connection context
 */
void al_httpd_close_conn(struct al_httpd_conn *conn);

/*\@}*/

#endif /* __AYLA_AL_COMMON_HTTPD_H__ */
