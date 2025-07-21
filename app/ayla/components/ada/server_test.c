/*
 * Copyright 2023 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifdef AYLA_SERVER_TEST_SUPPORT

#include <string.h>
#include <ayla/utypes.h>
#include <ayla/assert.h>
#include <ayla/log.h>
#include <ayla/mod_log.h>
#include <ayla/http.h>
#include <ayla/timer.h>
#include <ada/server_req.h>
#include <al/al_os_mem.h>
#include "client_lock.h"
#include "client_timer.h"

#define SERVER_TEST_PATT_LEN	10
#define SERVER_TEST_PATT_LEN_MAX 20000
#define SERVER_TEST_PATT_DEFAULT 100
#define SERVER_TEST_BUF_LEN	256
#define SERVER_TEST_BUF_LEN_MAX	SERVER_BUFLEN

struct server_test {
	struct server_req *req;
	struct timer timer;
};

/*
 * Fill buffer with a test pattern.
 * The pattern has the offset every 10 bytes padded by dashes.
 */
static void server_test_patt_fill(char *out, size_t len, size_t offset)
{
	char buf[SERVER_TEST_PATT_LEN + 1];
	char *bp;
	size_t tlen;
	size_t patt_offset;

	patt_offset = (offset / SERVER_TEST_PATT_LEN) * SERVER_TEST_PATT_LEN;
	while (len) {
		tlen = snprintf(buf, sizeof(buf), "%4.4u------", patt_offset);
		if (tlen >= sizeof(buf)) {
			tlen = sizeof(buf) - 1;
			/* truncation is OK */
		}
		bp = buf + (offset - patt_offset);
		tlen = tlen - (offset - patt_offset);
		if (tlen > len) {
			tlen = len;
		}
		memcpy(out, bp, tlen);
		patt_offset += SERVER_TEST_PATT_LEN;
		offset = patt_offset;
		len -= tlen;
		out += tlen;
	}
}

static void server_test_send(struct server_req *req)
{
	char *buf;
	long len;
	long buf_len;
	size_t tlen;
	size_t offset;
	int rlen;

	if (server_get_long_arg_by_name(req, "len", &len)) {
		len = SERVER_TEST_PATT_DEFAULT;
	}
	if (len < 0 || len > SERVER_TEST_PATT_LEN_MAX) {
		server_log(LOG_ERR "invalid len value %ld", len);
		server_put_status(req, HTTP_STATUS_BAD_REQ);
		return;
	}

	if (server_get_long_arg_by_name(req, "buf", &buf_len)) {
		buf_len = SERVER_TEST_BUF_LEN;
	}
	if (buf_len < 0 || buf_len > SERVER_TEST_BUF_LEN_MAX) {
		server_log(LOG_ERR "invalid buf_len value %ld", buf_len);
		server_put_status(req, HTTP_STATUS_BAD_REQ);
		return;
	}
	buf = al_os_mem_alloc(buf_len);
	if (!buf) {
		server_log(LOG_ERR "invalid buf alloc of %ld failed", buf_len);
		server_put_status(req, HTTP_STATUS_INTERNAL_ERR);
		return;
	}

	server_put_status(req, HTTP_STATUS_OK);
	for (offset = 0; offset < len; offset += tlen) {
		tlen = len - offset;
		if (tlen >= buf_len) {
			tlen = buf_len - 1;
		}
		server_test_patt_fill(buf, tlen, offset);
		buf[tlen] = '\0';

		rlen = server_put(req, buf);
		if (rlen < 0) {
			server_log(LOG_ERR "%s: rlen %d", __func__, rlen);
			break;
		}
		if (req->err != AE_OK && req->err != AE_BUF) {
			server_log(LOG_ERR "%s: err %d %s",
			    __func__, req->err, ada_err_string(req->err));
			break;
		}
	}
	al_os_mem_free(buf);
	if (req->err == AE_OK) {
		req->finish_write(req);
	}
}

static void server_test_timeout(struct timer *timer)
{
	struct server_test *test =
	    CONTAINER_OF(struct server_test, timer, timer);
	struct server_req *req = test->req;

	client_unlock();
	al_os_mem_free(test);
	server_continue(req, server_test_send);
	client_lock();
}

/*
 * Test the following features and cases:
 *	argument handling.
 *	in-progress / deferred response.
 *	flow-control on response.
 */
static void server_test_get(struct server_req *req)
{
	struct server_test *test;
	long delay = 0;

	server_get_long_arg_by_name(req, "delay", &delay);
	if (delay) {
		test = al_os_mem_calloc(sizeof(*test));
		if (!test) {
			server_put_status(req, HTTP_STATUS_INTERNAL_ERR);
			return;
		}
		test->req = req;
		timer_handler_init(&test->timer, server_test_timeout);
		client_lock();
		client_timer_set(&test->timer, delay);
		client_unlock();
		req->user_in_prog = 1;
		return;
	}
	server_test_send(req);
}

static void server_test_put(struct server_req *req)
{
	const char *buf = req->post_data;

	if (!buf) {
		server_log(LOG_ERR "no PUT/POST data");
	} else {
		log_bytes(MOD_LOG_SERVER, LOG_SEV_DEBUG, buf, strlen(buf),
		     "test post data");
	}
	server_test_get(req);
}

static const struct url_list server_test_urls[] = {
	URL_GET("/test", server_test_get, LOC_REQ),
	URL_PUT("/test", server_test_put, LOC_REQ),
	URL_POST("/test", server_test_put, LOC_REQ),
	URL_END
};

void server_test_init(void)
{
	server_add_urls(server_test_urls);
}

#endif /* AYLA_SERVER_TEST_SUPPORT */
