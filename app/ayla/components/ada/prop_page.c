/*
 * Copyright 2011-2012 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <ayla/utypes.h>
#include <ayla/assert.h>
#include <ada/err.h>
#include <ayla/base64.h>
#include <ayla/endian.h>
#include <ayla/http.h>
#include <ayla/log.h>
#include <ayla/nameval.h>
#include <ayla/conf.h>
#include <ayla/json.h>
#include <ayla/timer.h>
#include <jsmn.h>
#include <ayla/jsmn_get.h>
#include <al/al_os_mem.h>
#include <ada/prop.h>
#include <ada/prop_mgr.h>
#include <ada/server_req.h>

#include <ayla/clock.h>
#include <ayla/mod_log.h>
#include <ada/ada_conf.h>
#include <ada/client.h>
#include <ada/client_ota.h>
#include "client_int.h"
#include "client_lock.h"
#include "client_timer.h"


static void prop_page_close_cb(struct server_req *req)
{
	struct prop *prop = req->user_priv;

	if (prop && prop->prop_mgr_done) {
		prop->prop_mgr_done(prop);
		req->user_priv = NULL;
	}
}

/* The caller should free the return string */
static char *prop_get_escaped_str(const char *in)
{
	int quote = 0;
	char *pstr;
	size_t len = strlen(in);
	char *out;

	if (in == NULL) {
		return NULL;
	}

	server_log(LOG_DEBUG2 "%s: in %s", __func__, in);

	len = strlen(in);
	pstr = (char *)in;

	while (*pstr != '\0') {
		if (*pstr++ == '"') {
			quote++;
		}
	}

	server_log(LOG_DEBUG2 "%s: quote %d", __func__, quote);

	out = malloc(len + quote + 1);
	if (!out) {
		server_log(LOG_ERR "%s: cannot malloc %d buf",
		    __func__, (len + quote + 1));
		return NULL;
	}

	pstr = out;
	while (*in != '\0') {
		if (*in == '"') {
			*pstr++ = '\\';
			*pstr++ = *in;
		} else {
			*pstr++ = *in;
		}
		in++;
	}

	*pstr = '\0';
	server_log(LOG_DEBUG2 "%s: out %s", __func__, out);

	return out;
}


/*
 * Send body of response for GET from property.json.
 */
static void prop_page_output(struct server_req *req)
{
	struct prop *prop;
	char fmt_val[BASE64_LEN_EXPAND(PROP_VAL_LEN) + 1];
	char *outval;
	const char *quote;
	char *escaped_str = NULL;

	prop = req->user_priv;
	ASSERT(prop);

	if (prop->len > PROP_VAL_LEN) {
		server_log(LOG_WARN "%s: long prop \"%s\" truncated "
		    "support TBD", __func__, prop->name);
		prop->len = PROP_VAL_LEN;
	}

	prop_fmt(fmt_val, sizeof(fmt_val), prop->type,
	    prop->val, prop->len, &outval);

	quote = prop_type_is_str(prop->type) ? "\"" : "";

	if (prop->type == ATLV_UTF8) {
		escaped_str = prop_get_escaped_str(outval);
	} else {
		escaped_str = outval;
	}

	server_put(req,
	    "{\"name\":\"%s\","
	    "\"base_type\":\"%s\","
	    "\"value\":%s%s%s}",
	    prop->name, lookup_by_val(prop_types, prop->type),
	    quote, escaped_str, quote);

	if (prop->type == ATLV_UTF8) {
		if (escaped_str) {
			free(escaped_str);
		}
	}

	req->finish_write(req);
}

/*
 * This is the callback with the requested property value.
 */
static enum ada_err prop_page_get_cb(struct prop *prop, void *arg,
					enum ada_err error)
{
	struct server_req *req = arg;
	unsigned int status;
	char buf[SERVER_BUFLEN];	/* TBD temporary buffer for response */

	if (req->prop_abort) {
		server_free_aborted_req(req);
		return AE_OK;
	}

	status = HTTP_STATUS_OK;
	if (error || !prop) {
		status = HTTP_STATUS_NOT_FOUND;
	}

	req->buf = buf;
	req->len = 0;
	req->put_head(req, status, server_content_json);
	if (prop) {
		req->user_priv = prop;
		server_continue(req, prop_page_output);
	} else {
		req->finish_write(req);
	}
	req->buf = NULL;
	if (req->err == AE_BUF) {
		req->close_cb = prop_page_close_cb;
		return AE_IN_PROGRESS;
	}
	return AE_OK;
}

/*
 * Get Property JSON for a single property.
 */
void prop_page_json_get_one(struct server_req *req)
{
	char name_buf[PROP_NAME_LEN];
	char *name;
	enum ada_err error;

	name = server_get_arg_by_name(req, "name", name_buf, sizeof(name_buf));
	if (!name) {
		server_put_status(req, HTTP_STATUS_BAD_REQ);
		return;
	}

	error = ada_prop_mgr_get(name, prop_page_get_cb, req);

	switch (error) {
	case AE_OK:
		break;
	case AE_IN_PROGRESS:
		req->user_in_prog = 1;
		break;
	case AE_NOT_FOUND:
		server_put_status(req, HTTP_STATUS_NOT_FOUND);
		break;
	default:
		server_put_status(req, HTTP_STATUS_INTERNAL_ERR);
		break;
	}
}

#define PROP_TEST_MAXDUR  600 /* Maximum property test duration */
#define PROP_PAGE_JSON    30 /* Set properties state */

static char prop_test_mode;
static char prop_post_buf[SERVER_BUFLEN];
struct timer prop_test_timer;
const char ada_prop_test_cli_help[] = "<seconds>";

/*
 * property test mode timeout
 */
static void prop_test_timeout(struct timer *timer)
{
	prop_test_mode = 0;
}

/*
 * Check if property test is enabled
 */
int prop_test_enabled(void)
{
	return (prop_test_mode != 0);
}

/*
 * Iterator to handle the properties sub-object.
 */
static int prop_page_parse_prop(jsmn_parser *parser, jsmntok_t *obj,
				    void *client_lan)
{
	struct prop_recvd *prop = &prop_recvd;
	char type[20];
	jsmntok_t *prop_t;
	enum ada_err rc;

	prop_t = jsmn_get_val(parser, obj, "property");
	if (!prop_t) {
		return AE_INVAL_VAL;
	}

	if (jsmn_get_string(parser, prop_t, "name",
	    prop->name, sizeof(prop->name)) < 0) {
		return AE_INVAL_VAL;
	}

	if (jsmn_get_string(parser, prop_t, "value",
	   prop->val, sizeof(prop->val)) < 0) {
		return AE_INVAL_VAL;
	}

	if (jsmn_get_string(parser, prop_t, "base_type",
	   type, sizeof(type)) < 0) {
		return AE_INVAL_VAL;
	}

	prop->type = (enum ayla_tlv_type)lookup_by_name(prop_types, type);
	prop->offset = 0;

	server_log(LOG_DEBUG "%s: setting %s = %s",
	    __func__, prop->name, prop->val);

	rc = client_prop_set(prop);

	return rc;
}

/*
 * Parse POST property request
 */
static enum ada_err prop_page_parse_props(struct server_req *req)
{
	jsmn_parser parser;
	jsmntok_t tokens[PROP_PAGE_JSON];
	jsmntok_t *props;
	jsmnerr_t err;
	enum ada_err parse_err = AE_OK;

	/* Abort connection if requested */
	if (req->prop_abort) {
		server_free_aborted_req(req);
		return AE_ABRT;
	}

	jsmn_init_parser(&parser, req->buf, tokens, PROP_PAGE_JSON);
	err = jsmn_parse(&parser);
	if (err != JSMN_SUCCESS) {

		goto return_error;
	}

	props = jsmn_get_val(&parser, NULL, "properties");
	if (props) {
		if (props->type != JSMN_ARRAY) {

			goto return_error;
		}
		parse_err = (enum ada_err)jsmn_array_iterate(&parser, props,
		    prop_page_parse_prop, (void *)req);
		if (parse_err != AE_OK && parse_err != AE_IN_PROGRESS) {
			goto iterate_failed;
		}
	}

	/*
	 * Either there was no parse error, or we need to retry.
	 */
	if (parse_err == AE_IN_PROGRESS) {
		/* Mark a continuation to avoid the request being freed */
		req->user_in_prog = 1;
		req->err = AE_IN_PROGRESS;
		/* save the contents of the buffer, if necessary */
		if (req->buf != prop_post_buf) {
			strncpy(prop_post_buf, req->buf, SERVER_BUFLEN - 1);
		} else {
			req->buf = NULL;
		}
	} else {
		server_json_header(req);
		if (req->finish_write) {
			req->finish_write(req);
		}
	}
	return AE_OK;

iterate_failed:

return_error:
	req->http_status = HTTP_STATUS_BAD_REQ;
	server_json_header(req);
	if (req->finish_write) {
		req->finish_write(req);
	}
	return parse_err;
}

static void prop_page_json_post(struct server_req *req)
{
	if (prop_test_mode) {
		(void) prop_page_parse_props(req);
		/*
		 * Parser returns status (possibly asynchronously)
		 */
	} else {
		server_put_status(req, HTTP_STATUS_FORBID);
		if (req->finish_write) {
			req->finish_write(req);
		}
	}
}

static const struct url_list prop_page_url_list[] = {
	URL_POST("/properties.json", prop_page_json_post, REQ_SOFT_AP),
	URL_END
};

/*
 * Enable property test mode for x seconds; disable if seconds are 0
 */
static int ada_prop_test_enable(u16 seconds)
{
	if (seconds == 0) {
		client_lock();
		client_timer_cancel(&prop_test_timer);
		client_unlock();
		prop_test_mode = 0;
	} else if (seconds > 0 && seconds <= PROP_TEST_MAXDUR) {
		if (prop_test_mode) {
			client_lock();
			client_timer_cancel(&prop_test_timer);
			client_unlock();
		}
		prop_test_mode = 1;
		timer_handler_init(&prop_test_timer, prop_test_timeout);
		server_add_urls(prop_page_url_list);
		client_lock();
		client_timer_set(&prop_test_timer, seconds * 1000);
		client_unlock();
	} else if (seconds > PROP_TEST_MAXDUR) {
		return AE_INVAL_VAL;
	}
	return 0;
}

void ada_prop_test_cli(int argc, char **argv)
{
	char *end;
	int seconds;

	if (argc != 2) {
		printcli("prop-test: invalid usage");
		goto usage;
	}

	end = NULL;
	seconds = (int)strtol(argv[1], &end, 10);

	if ((end && strlen(end)) ||
	    ada_prop_test_enable((u16)seconds) == AE_INVAL_VAL) {
		printcli("prop-test: invalid range");
		return;
	}

	if (seconds > 0) {
		printcli("prop-test: enabled for %d seconds", seconds);
	} else if (seconds == 0) {
		printcli("prop-test: disabled");
	} else {
		printcli("prop-test: invalid value");
	}
	return;

usage:
	printcli("usage: prop-test [<seconds>]");
}
