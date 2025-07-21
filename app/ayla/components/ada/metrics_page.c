/*
 * Copyright 2019 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifdef AYLA_METRICS_SUPPORT

#include <string.h>

#include <ayla/assert.h>
#include <ayla/utypes.h>
#include <ayla/conf.h>
#include <ayla/uri_code.h>
#include <jsmn.h>
#include <ayla/jsmn_get.h>
#include <ayla/http.h>
#include <ada/server_req.h>
#include <ada/metrics.h>
#include "metrics_int.h"
#include "client_lock.h"

#define METRICS_JSON_TOKEN_CNT	100	/* number of tokens to alloc */

static const struct url_list metrics_urls[] = {
	URL_PUT("/metrics_cfg.json", metrics_config_json_put, ADS_REQ),
	URL_END
};

/*
 * Init server pages for metrics settings.
 */
void metrics_pages_init(void)
{
	server_add_urls(metrics_urls);
}

/*
 * Iterator to handle metrics_cfg overrides.
 */
static int metrics_config_json_overrides(jsmn_parser *parser,
	jsmntok_t *obj, void *dryrun)
{
	jsmntok_t *tok;
	ssize_t len;
	char name[METRICS_CAT_LEN_MAX];
	long level;

	/*
	 * Get name token.
	 */
	tok = jsmn_get_val(parser, obj, "c");
	if (!tok) {
		SERVER_LOGF(LOG_WARN, "no category");
		return -1;
	}
	len = uri_decode_n(name, sizeof(name) - 1,
	    parser->js + tok->start, tok->end - tok->start);
	if (len < 0) {
		SERVER_LOGF(LOG_WARN, "uri_dec category fail");
		return -1;
	}
	name[len] = '\0';

	if (jsmn_get_long(parser, obj, "l", &level)) {
		SERVER_LOGF(LOG_WARN, "no level");
		return -1;
	}
	if (level < METRIC_LEVEL_DISABLED ||
	   level > METRIC_LEVEL_DEBUG) {
		server_log(LOG_WARN "invalid metrics level %ld", level);
		return -1;
	}

	if (!dryrun) {
		SERVER_LOGF(LOG_INFO, "category %s level %d", name,
		    (int)level);
		metrics_set_category_level(name, (enum metric_level)level);
	}

	return 0;
}

/*
 * Put metrics_config.json
 * Sets metrics configuration.
 */
void metrics_config_json_put(struct server_req *req)
{
	jsmn_parser parser;
	jsmntok_t tokens[METRICS_JSON_TOKEN_CNT];
	jsmntok_t *metrics_cfg;
	jsmntok_t *overrides;
	jsmnerr_t err;
	long sampling_period;
	long reporting_interval;
	long default_level;

	client_lock();
	jsmn_init_parser(&parser, req->post_data, tokens, ARRAY_LEN(tokens));
	err = jsmn_parse(&parser);
	if (err != JSMN_SUCCESS) {
		SERVER_LOGF(LOG_WARN, "jsmn err %d", err);
		goto inval;
	}
	metrics_cfg = jsmn_get_val(&parser, NULL, "metrics_cfg");
	if (!metrics_cfg) {
		SERVER_LOGF(LOG_WARN, "no metrics_cfg");
		goto inval;
	}

	if (jsmn_get_long(&parser, metrics_cfg, "s", &sampling_period)) {
		goto inval;
	}
	server_log(LOG_INFO "sampling period %ld", sampling_period);
	if (sampling_period < METRICS_SAMPLE_MIN ||
	    sampling_period > METRICS_SAMPLE_MAX) {
		server_log(LOG_WARN "invalid metrics sampling period %ld",
		    sampling_period);
		goto inval;
	}

	if (jsmn_get_long(&parser, metrics_cfg, "r", &reporting_interval)) {
		goto inval;
	}
	server_log(LOG_INFO "reporting interval %ld", reporting_interval);
	if (reporting_interval < METRICS_REPORT_MIN ||
	    reporting_interval > METRICS_REPORT_MAX) {
		server_log(LOG_WARN "invalid metrics reporting interval %ld",
		    reporting_interval);
		goto inval;
	}

	if (jsmn_get_long(&parser, metrics_cfg, "l", &default_level)) {
		goto inval;
	}
	server_log(LOG_INFO "default level %ld", default_level);
	if (default_level < METRIC_LEVEL_DISABLED ||
	    default_level > METRIC_LEVEL_DEBUG) {
		server_log(LOG_WARN "invalid metrics level %ld", default_level);
		goto inval;
	}

	/* check for overrides */
	overrides = jsmn_get_val(&parser, metrics_cfg, "o");
	if (!overrides || overrides->type != JSMN_ARRAY) {
		SERVER_LOGF(LOG_DEBUG, "no overrides");
		goto overrides_done;
	}

	/* dryrun */
	if (jsmn_array_iterate(&parser, overrides,
	    metrics_config_json_overrides, (void *)1)) {
		SERVER_LOGF(LOG_WARN, "metrics config failed");
		goto inval;
	}

	/* initialize all categories to default level */
	metrics_set_category_levels((enum metric_level)default_level);

	/* set overrides */
	jsmn_array_iterate(&parser, overrides, metrics_config_json_overrides,
	    (void *)0);

overrides_done:
	metrics_set_controls(sampling_period, reporting_interval,
	    (enum metric_level)default_level);
	server_put_status(req, HTTP_STATUS_NO_CONTENT);
	client_unlock();
	return;
inval:
	server_put_status(req, HTTP_STATUS_BAD_REQ);
	client_unlock();
}
#endif
