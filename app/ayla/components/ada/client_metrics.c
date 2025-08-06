/*
 * Copyright 2019 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifdef AYLA_METRICS_SUPPORT

#include <ayla/assert.h>
#include <ayla/utypes.h>
#include <ada/err.h>
#include <ayla/log.h>
#include <ayla/mod_log.h>
#include <ayla/http.h>
#include <ayla/conf.h>
#include <ayla/clock.h>
#include <ayla/timer.h>

#include <ada/prop.h>
#include <ada/server_req.h>
#include <ada/ada_conf.h>
#include <ada/client.h>
#include "client_req.h"
#include <ada/client_ota.h>
#include "client_int.h"
#include "client_lock.h"
#include <ada/metrics.h>
#include "metrics_int.h"

#define CLIENT_METRICS_SIZE	1000	/* payload size for metrics */
#define CLIENT_METRICS_R_SIZE	64	/* min space rqmt for next record */

/*
 * Metrics post request parts
 */
enum client_send_metrics_req_part {
	CSM_REQ_HDR = 1,
	CSM_MSG_HDR,
	CSM_BUCKET_HDR,
	CSM_BUCKET_HDR_CONTINUE,
	CSM_METRIC,
	CSM_METRIC_CONTINUE,
	CSM_BUCKET_TRAILER,
	CSM_MSG_TRAILER,
	CSM_PADDING
};

/*
 * Encode metrics message header
 */
static size_t client_metrics_enc_msg_hdr(char *buf, size_t len, const char *id,
    const char *model)
{
	return snprintf(buf, len, "{\"s\":\"%s\",\"m\":\"%s\",\"h\":[", id,
	    model);
}

/*
 * Encode metrics bucket header
 */
static size_t client_metrics_enc_bucket_hdr(char *buf, size_t len,
    struct metrics_bucket *bucket, u8 cont)
{
	u32 utc_time = clock_ms_to_utc(bucket->hdr.start_time);
	u32 duration = bucket->hdr.duration;

	if (!duration) {
		/* Partially completed bucket */
		duration = (u32)(clock_total_ms() - bucket->hdr.start_time);
	}
	duration = (duration + 500) / 1000;

	return snprintf(buf, len, "%s{\"s\":%lu,\"d\":%lu,\"m\":{",
	    cont ? "," : "", utc_time, duration);
}

/*
 * Encode metric
 */
static size_t client_metrics_enc_metric(char *buf, size_t len,
    struct metric_entry *metric, u8 cont)
{
	struct metric_sampled *smetric;
	struct metric_duration *dmetric;
	u32 u32value;
	s32 s32value;
	size_t out_len = 0;
	const struct metric_info *minfo = metric->info;
	char suffix[16];
	size_t offset = 0;

	suffix[0] = '\0';
	if (metric->instance) {
		offset = snprintf(suffix, sizeof(suffix), "#%u",
		    metric->instance);
	}
	if (metric->code) {
		snprintf(&suffix[offset], sizeof(suffix) - offset, "!%d",
		    metric->code);
	}

	switch (minfo->type) {
	case METRIC_TYPE_COUNTER:
		u32value = metric->counter.count;
		if (u32value) {
			out_len = snprintf(buf, len, "%s\"%s%s\":%lu",
			    cont ? "," : "", minfo->tag, suffix, u32value);
		}
		break;
	case METRIC_TYPE_SAMPLED:
		smetric = &metric->sampled;
		if (smetric->count > 0) {
			s32value = (s32)(smetric->sum / smetric->count);
			out_len = snprintf(buf, len, "%s\"%s%s\":%ld",
			    cont ? "," : "", minfo->tag, suffix, s32value);
		}
		break;
	case METRIC_TYPE_DURATION:
		dmetric = &metric->duration;
		if (dmetric->count > 0) {
			s32value = (s32)(dmetric->sum / dmetric->count);
			out_len = snprintf(buf, len, "%s\"%s%s\":%ld",
			    cont ? "," : "", minfo->tag, suffix, s32value);
		}
		break;
	default:
		/* unknown type, skip it */
		break;
	}
	return out_len;
}

/*
 * Encode metrics bucket trailer
 */
static size_t client_metrics_enc_bucket_trlr(char *buf, size_t len)
{
	return snprintf(buf, len, "}}");
}

/*
 * Encode metrics message trailer
 */
static size_t client_metrics_enc_msg_trlr(char *buf, size_t len)
{
	return snprintf(buf, len, "]}");
}

/*
 * Callback to generate metrics message payload
 */
static void client_metrics_send_cb(struct http_client *hc)
{
	struct client_state *state = &client_state;
	enum ada_err err = AE_OK;
	struct metrics_bucket *bucket;
	struct metric_entry *metric;
	const struct metric_info *minfo;
	struct metric_cat_info *cinfo;
	size_t len = 0;
	u32 padding_needed;

next_part:
	switch (hc->req_part) {
	case CSM_MSG_HDR:
		len = client_metrics_enc_msg_hdr(state->buf,
		    sizeof(state->buf), conf_sys_dev_id, oem_model);
		err = client_req_send(hc, state->buf, len);
		if (!err) {
			hc->req_part = CSM_BUCKET_HDR;
			goto next_part;
		}
		break;
	case CSM_BUCKET_HDR:
	case CSM_BUCKET_HDR_CONTINUE:
		if (hc->sent_len >
		    CLIENT_METRICS_SIZE - CLIENT_METRICS_R_SIZE) {
			hc->req_part = CSM_MSG_TRAILER;
			goto next_part;
		}
		bucket = metrics_get_read_ptr();
		/* Only completed buckets for now */
		if (!bucket || !bucket->hdr.duration) {
			hc->req_part = CSM_MSG_TRAILER;
			goto next_part;
		}
		len = client_metrics_enc_bucket_hdr(state->buf,
		    sizeof(state->buf), bucket,
		    hc->req_part == CSM_BUCKET_HDR_CONTINUE);
		err = client_req_send(hc, state->buf, len);
		if (!err) {
			hc->req_part = CSM_METRIC;
			goto next_part;
		}
		break;
	case CSM_METRIC:
	case CSM_METRIC_CONTINUE:
		if (hc->sent_len >
		    CLIENT_METRICS_SIZE - CLIENT_METRICS_R_SIZE) {
			hc->req_part = CSM_BUCKET_TRAILER;
			goto next_part;
		}
		bucket = metrics_get_read_ptr();
		metric = &bucket->metric[state->metric_idx];
		minfo = metric->info;
		if (minfo) {
			cinfo = metrics_get_category_info(minfo->category);
			ASSERT(cinfo);
			if (minfo->level <= cinfo->level) {
				len = client_metrics_enc_metric(state->buf,
				    sizeof(state->buf), metric,
				    hc->req_part == CSM_METRIC_CONTINUE);
				if (len) {
					err = client_req_send(hc, state->buf,
					    len);
					if (err) {
						break;
					}
					hc->req_part = CSM_METRIC_CONTINUE;
				}
			}
		}
		if (++state->metric_idx >= ARRAY_LEN(bucket->metric)) {
			state->metric_idx = 0;
			hc->req_part = CSM_BUCKET_TRAILER;
		}
		goto next_part;
	case CSM_BUCKET_TRAILER:
		len = client_metrics_enc_bucket_trlr(state->buf,
		    sizeof(state->buf));
		err = client_req_send(hc, state->buf, len);
		if (!err) {
			bucket = metrics_adv_read_ptr();
			state->metric_idx = 0;
			if (!bucket) {
				hc->req_part = CSM_MSG_TRAILER;
			} else {
				hc->req_part = CSM_BUCKET_HDR_CONTINUE;
			}
			goto next_part;
		}
		break;
	case CSM_MSG_TRAILER:
		len = client_metrics_enc_msg_trlr(state->buf,
		    sizeof(state->buf));
		err = client_req_send(hc, state->buf, len);
		if (!err) {
			hc->req_part = CSM_PADDING;
			goto next_part;
		}
		break;
	case CSM_PADDING:
		if (hc->sent_len >= CLIENT_METRICS_SIZE) {
			bucket = metrics_get_read_ptr();
			if (bucket && bucket->hdr.duration) {
				/* more complete buckets to send */
				client_step_enable(ADCP_POST_METRICS);
			}
			client_req_send_complete(hc);
			break;
		}
		padding_needed = CLIENT_METRICS_SIZE - hc->sent_len;
		if (padding_needed > sizeof(state->buf)) {
			padding_needed = sizeof(state->buf);
		}
		memset(state->buf, ' ', padding_needed);
		err = client_req_send(hc, state->buf, padding_needed);
		if (!err) {
			goto next_part;
		}
		break;
	}

	if (err && err != AE_BUF) {
		client_log(LOG_ERR "metrics send err %d", err);
		/* skip to next bucket in case data in bucket is problem */
		bucket = metrics_adv_read_ptr();
		state->metric_idx = 0;
		client_req_abort(hc);
	}
}

/*
 * Post metrics to server.
 */
int client_metrics_post(struct client_state *state)
{
	struct http_client *hc;
	struct metrics_bucket *bucket = metrics_get_read_ptr();

	client_step_disable(ADCP_POST_METRICS);
	if (!bucket || !bucket->hdr.start_time || !bucket->hdr.duration) {
		/* no data to report */
		return -1;
	}
	hc = client_req_ads_new();
	ASSERT(hc);
	hc->client_tcp_recv_cb = client_recv_drop;
	hc->client_send_data_cb = client_metrics_send_cb;
	state->conn_state = CS_WAIT_METRICS_POST;
	state->request = CS_POST_METRICS;
	hc->body_buf = NULL;
	hc->body_buf_len = 0;
	hc->body_len = CLIENT_METRICS_SIZE;

	snprintf(state->buf, sizeof(state->buf),
	    "/metricsservice/v1/devices/%s/metrics", state->client_key);

	hc->req_part = CSM_MSG_HDR;
	client_req_start(hc, HTTP_REQ_POST, state->buf, &http_hdr_content_json);
	return 0;
}

static void client_metrics_report(void)
{
	client_step_enable(ADCP_POST_METRICS);
	client_wakeup();
}

void client_metrics_init(void)
{
	struct http_client *hc = &client_state.http_client;

	hc->metrics_instance = 0;	/* service is instance 0 */
	metrics_pages_init();
	metrics_init(client_metrics_report);
}
#endif /* AYLA_METRICS_SUPPORT */
