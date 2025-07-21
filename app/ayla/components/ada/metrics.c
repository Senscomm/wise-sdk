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
#include <stdlib.h>

#include <ayla/assert.h>
#include <ayla/utypes.h>
#include <al/al_os_mem.h>
#include <ada/err.h>
#include <ayla/log.h>
#include <ayla/clock.h>
#include <ayla/timer.h>
#include <ayla/conf.h>
#include <ada/client.h>
#include "client_timer.h"
#include <ada/metrics.h>
#include <ada/ada_conf.h>
#include "ada_lock.h"
#include "client_lock.h"
#include "metrics_int.h"

static struct metric_cat_info metric_categories[] = {
#ifdef METRICS_WIFI
	[METRICS_CAT_WIFI] = {
		.name = "wifi",
		.level = METRIC_LEVEL_DEFAULT
	},
#endif
#ifdef METRICS_HTTP
	[METRICS_CAT_HTTP] = {
		.name = "http",
		.level = METRIC_LEVEL_DEFAULT
	},
#endif
#ifdef METRICS_PROP
	[METRICS_CAT_PROP] = {
		.name = "prop",
		.level = METRIC_LEVEL_DEFAULT
	},
#endif
};

static const struct metric_info metrics_info[] = {
#ifdef METRICS_WIFI
	[M_WIFI_CONNECT_ATTEMPTS] = {
		.category = METRICS_CAT_WIFI,
		.tag = "wca",
		.type = METRIC_TYPE_COUNTER,
		.level = METRIC_LEVEL_SUMMARY,
	},
	[M_WIFI_CONNECT_SUCCESSES] = {
		.category = METRICS_CAT_WIFI,
		.tag = "wcs",
		.type = METRIC_TYPE_COUNTER,
		.level = METRIC_LEVEL_SUMMARY,
	},
	[M_WIFI_CONNECT_FAILURES] = {
		.category = METRICS_CAT_WIFI,
		.tag = "wcf",
		.type = METRIC_TYPE_COUNTER,
		.level = METRIC_LEVEL_SUMMARY,
	},
	[M_WIFI_CONNECT_LATENCY] = {
		.category = METRICS_CAT_WIFI,
		.tag = "wcl",
		.type = METRIC_TYPE_DURATION,
		.level = METRIC_LEVEL_SUMMARY,
	},
	[M_WIFI_CONNECT_ERRORS] = {
		.category = METRICS_CAT_WIFI,
		.tag = "wce",
		.type = METRIC_TYPE_COUNTER,
		.level = METRIC_LEVEL_DETAIL,
	},
	[M_WIFI_RSSI] = {
		.category = METRICS_CAT_WIFI,
		.tag = "wss",
		.type = METRIC_TYPE_SAMPLED,
		.level = METRIC_LEVEL_SUMMARY,
	},
#endif
#ifdef METRICS_HTTP
	[M_HTTP_CONNECT_ATTEMPTS] = {
		.category = METRICS_CAT_HTTP,
		.tag = "hca",
		.type = METRIC_TYPE_COUNTER,
		.level = METRIC_LEVEL_SUMMARY,
	},
	[M_HTTP_CONNECT_SUCCESSES] = {
		.category = METRICS_CAT_HTTP,
		.tag = "hcs",
		.type = METRIC_TYPE_COUNTER,
		.level = METRIC_LEVEL_SUMMARY,
	},
	[M_HTTP_CONNECT_FAILURES] = {
		.category = METRICS_CAT_HTTP,
		.tag = "hcf",
		.type = METRIC_TYPE_COUNTER,
		.level = METRIC_LEVEL_SUMMARY,
	},
	[M_HTTP_CONNECT_LATENCY] = {
		.category = METRICS_CAT_HTTP,
		.tag = "hcl",
		.type = METRIC_TYPE_DURATION,
		.level = METRIC_LEVEL_SUMMARY,
	},
	[M_HTTP_REQUEST_ATTEMPTS] = {
		.category = METRICS_CAT_HTTP,
		.tag = "hra",
		.type = METRIC_TYPE_COUNTER,
		.level = METRIC_LEVEL_SUMMARY,
	},
	[M_HTTP_REQUEST_SUCCESSES] = {
		.category = METRICS_CAT_HTTP,
		.tag = "hrs",
		.type = METRIC_TYPE_COUNTER,
		.level = METRIC_LEVEL_SUMMARY,
	},
	[M_HTTP_REQUEST_FAILURES] = {
		.category = METRICS_CAT_HTTP,
		.tag = "hrf",
		.type = METRIC_TYPE_COUNTER,
		.level = METRIC_LEVEL_SUMMARY,
	},
	[M_HTTP_REQUEST_LATENCY] = {
		.category = METRICS_CAT_HTTP,
		.tag = "hrl",
		.type = METRIC_TYPE_DURATION,
		.level = METRIC_LEVEL_SUMMARY,
	},
	[M_HTTP_STATUS] = {
		.category = METRICS_CAT_HTTP,
		.tag = "hsc",
		.type = METRIC_TYPE_COUNTER,
		.level = METRIC_LEVEL_DETAIL,
	},
#endif
#ifdef METRICS_PROP
	[M_PROP_SEND_ATTEMPTS] = {
		.category = METRICS_CAT_PROP,
		.tag = "psa",
		.type = METRIC_TYPE_COUNTER,
		.level = METRIC_LEVEL_SUMMARY,
	},
	[M_PROP_SEND_SUCCESSES] = {
		.category = METRICS_CAT_PROP,
		.tag = "pss",
		.type = METRIC_TYPE_COUNTER,
		.level = METRIC_LEVEL_SUMMARY,
	},
	[M_PROP_SEND_FAILURES] = {
		.category = METRICS_CAT_PROP,
		.tag = "psf",
		.type = METRIC_TYPE_COUNTER,
		.level = METRIC_LEVEL_SUMMARY,
	},
	[M_PROP_SEND_LATENCY] = {
		.category = METRICS_CAT_PROP,
		.tag = "psl",
		.type = METRIC_TYPE_DURATION,
		.level = METRIC_LEVEL_SUMMARY,
	},
	[M_PROP_SEND_STATUS] = {
		.category = METRICS_CAT_PROP,
		.tag = "psc",
		.type = METRIC_TYPE_COUNTER,
		.level = METRIC_LEVEL_DETAIL,
	},
#endif
};

struct metrics_conf {
	u32 sampling_period;	/* sampling period (sec) */
	u8 reporting_interval;	/* reporting interval (every N periods) */
	enum metric_level reporting_level; /* detail level for reporting */
};

static struct metrics_conf metrics_conf = {
	METRICS_SAMPLE_DEFAULT,
	METRICS_REPORT_DEFAULT,
	METRIC_LEVEL_DEFAULT
};

struct metrics_state {
	void (*reporting_cb)(void);
	struct ada_lock *lock;
	struct timer timer;
	struct metrics_bucket *curr_bucket;
	struct metrics_bucket *buckets;
	u32 current_period;	/* current sampling period */
	size_t bucket_cnt;	/* number of buckets in array */
	u8 unreported_cnt;	/* number of buckets since last report */
	u8 write_idx;		/* buckets index for writes */
	u8 read_idx;		/* buckets index for reads */
};

static struct metrics_state metrics_state;

static const struct ada_conf_item metrics_conf_items[] = {
	{ "metric/time", ATLV_UINT, &metrics_conf.sampling_period,
	    sizeof(metrics_conf.sampling_period)},
	{ "metric/interval", ATLV_UINT, &metrics_conf.reporting_interval,
	    sizeof(metrics_conf.reporting_interval)},
	{ "metric/log", ATLV_UINT, &metrics_conf.reporting_level,
	    sizeof(metrics_conf.reporting_level)},
	{ NULL }
};

struct metric_entry *metric_get(enum metric_id id, u16 instance, s16 code)
{
	const struct metric_info *info;
	u16 index = (u16)(id << 2) % METRICS_BUCKET_ENTRIES;
	u16 start_index = index;
	struct metric_entry *entry;
	struct metrics_state *state = &metrics_state;
	struct metrics_bucket *buckets = state->buckets;
	struct metrics_bucket *bucket;

	if (!buckets) {
		/* metrics not enabled */
		return NULL;
	}

	ASSERT(id < ARRAY_LEN(metrics_info));
	info = &metrics_info[id];

	/*
	 * Lock because multiple threads can update metrics.
	 *
	 * This code assumes that a bucket will not go from active to being
	 * recycled (zeroed) during the time period the metric entry pointer
	 * returned here is in use. It is okay for state->curr_bucket to be
	 * advanced while the returned entry is in use. Such an update will
	 * be applied to the older bucket, which is okay provided it happens
	 * before the bucket is recycled, which will be the case.
	 */
	ada_lock(state->lock);
	bucket = state->curr_bucket;
	do {
		entry = &bucket->metric[index];
		if (entry->info == info && entry->instance == instance &&
		    entry->code == code) {
			ada_unlock(state->lock);
			return entry;
		}
		if (!entry->info) {
			/* allocate unused entry */
			entry->info = info;
			entry->instance = instance;
			entry->code = code;
			ada_unlock(state->lock);
			return entry;
		}
		if (++index >= METRICS_BUCKET_ENTRIES) {
			index = 0;
		}
	} while (index != start_index);

	/* table is full */
	ada_unlock(state->lock);
	return NULL;
}

struct metric_cat_info *metrics_get_category_info(u16 cindex)
{
	if (cindex >= ARRAY_LEN(metric_categories)) {
		return NULL;
	}

	return &metric_categories[cindex];
}

static void metrics_next_bucket(void)
{
	struct metrics_bucket *old_bucket;
	struct metrics_bucket *new_bucket;
	u64 time_ms;
	struct metrics_state *state = &metrics_state;
	struct metrics_bucket *buckets = state->buckets;

	if (!buckets) {
		return;
	}
	old_bucket = &buckets[state->write_idx];

	if (++state->write_idx >= state->bucket_cnt) {
		state->write_idx = 0;
	}
	if (state->write_idx == state->read_idx) {
		if (++state->read_idx >= state->bucket_cnt) {
			state->read_idx = 0;
		}
	}

	new_bucket = &buckets[state->write_idx];
	time_ms = clock_total_ms();
	if (!time_ms) {
		time_ms = 1;
	}
	memset(new_bucket, 0, sizeof(struct metrics_bucket));
	new_bucket->hdr.start_time = time_ms;
	old_bucket->hdr.duration = (u32)(time_ms - old_bucket->hdr.start_time);
	ada_lock(state->lock);
	state->curr_bucket = new_bucket;
	ada_unlock(state->lock);
}

struct metrics_bucket *metrics_get_read_ptr(void)
{
	struct metrics_state *state = &metrics_state;
	struct metrics_bucket *buckets = state->buckets;
	struct metrics_bucket *bucket;

	if (!buckets) {
		return NULL;
	}
	bucket = &buckets[state->read_idx];
	if (!bucket->hdr.start_time) {
		return NULL;
	}
	return bucket;
}

struct metrics_bucket *metrics_adv_read_ptr(void)
{
	struct metrics_state *state = &metrics_state;
	struct metrics_bucket *buckets = state->buckets;
	struct metrics_bucket *bucket;

	if (!buckets || state->read_idx == state->write_idx) {
		state->unreported_cnt = 0;
		return NULL;
	}
	bucket = &buckets[state->read_idx];
	--state->unreported_cnt;
	bucket->hdr.start_time = 0;
	if (++state->read_idx >= state->bucket_cnt) {
		state->read_idx = 0;
	}
	bucket = &buckets[state->read_idx];
	if (!bucket->hdr.start_time) {
		return NULL;
	}
	return bucket;
}

static void metrics_timeout(struct timer *timer)
{
	struct metrics_state *state = &metrics_state;
	struct metrics_bucket *buckets = state->buckets;
	struct metrics_conf *mcf = &metrics_conf;

	if (!buckets || !mcf->reporting_interval) {
		return;
	}
	client_timer_set(&state->timer, mcf->sampling_period * 1000);
	metrics_next_bucket();

	if (++state->unreported_cnt >= mcf->reporting_interval) {
		if (state->reporting_cb) {
			state->reporting_cb();
		}
	}
}

/* set all categories to a level */
void metrics_set_category_levels(enum metric_level level)
{
	struct metric_cat_info *cinfo;
	int i;

	for (cinfo = metric_categories, i = 0;
	    i < ARRAY_LEN(metric_categories); cinfo++, i++) {
		cinfo->level = level;
	}
}

int metrics_set_category_level(const char *name, enum metric_level level)
{
	struct metric_cat_info *cinfo;
	int i;

	for (cinfo = metric_categories, i = 0;
	    i < ARRAY_LEN(metric_categories); cinfo++, i++) {
		if (!strcmp(name, cinfo->name)) {
			cinfo->level = level;
			return 1;
		}
	}

	return 0;
}

void metrics_conf_load(void)
{
	const struct ada_conf_item *item;
	struct metrics_conf *mcf = &metrics_conf;

	for (item = metrics_conf_items; item->name; item++) {
		ada_conf_get_item(item);
	}
	metrics_set_category_levels(mcf->reporting_level);
}

static enum conf_error
metrics_conf_set(int src, enum conf_token *token, size_t len,
    struct ayla_tlv *tlv)
{
	struct metrics_conf *mcf = &metrics_conf;
	u32 val;
	enum conf_error rc = CONF_ERR_PATH;

	if (len != 1) {
		goto err;
	}
	if (conf_access(CONF_OP_SS_METRIC | CONF_OP_WRITE | src)) {
		rc = CONF_ERR_PERM;
		goto err;
	}
	switch (token[0]) {
	case CT_time:
		val = conf_get_u32(tlv);
		if (val < METRICS_SAMPLE_MIN || val > METRICS_SAMPLE_MAX) {
			rc = CONF_ERR_RANGE;
			goto err;
		}
		mcf->sampling_period = val;
		break;
	case CT_interval:
		val = conf_get_u8(tlv);
		if (val > METRICS_REPORT_MAX) {
			rc = CONF_ERR_RANGE;
			goto err;
		}
		mcf->reporting_interval = val;
		break;
	case CT_log:
		val = conf_get_u8(tlv);
		if (val > METRIC_LEVEL_DEBUG) {
			rc = CONF_ERR_RANGE;
			goto err;
		}
		mcf->reporting_level = (enum metric_level)val;
		metrics_set_category_levels(mcf->reporting_level);
		break;
	default:
		goto err;
	}
	return CONF_ERR_NONE;
err:
	return rc;
}

static enum conf_error
metrics_conf_get(int src, enum conf_token *token, size_t len)
{
	struct metrics_conf *mcf = &metrics_conf;

	if (len != 1) {
		goto err;
	}
	if (conf_access(CONF_OP_SS_METRIC | CONF_OP_READ | src)) {
		return CONF_ERR_PERM;
	}
	switch (token[0]) {
	case CT_time:
		conf_resp_u32(mcf->sampling_period);
		break;
	case CT_interval:
		conf_resp_u32(mcf->reporting_interval);
		break;
	case CT_log:
		conf_resp_u32(mcf->reporting_level);
		break;
	default:
		goto err;
	}
	return CONF_ERR_NONE;
err:
	return CONF_ERR_PATH;
}

static void metrics_commit(int from_ui)
{
	struct metrics_state *state = &metrics_state;
	struct metrics_conf *mcf = &metrics_conf;
	struct metrics_bucket *buckets;

	if (!state->lock) {
		/* not initialized yet */
		return;
	}

	if (!mcf->reporting_interval) {
		if (state->current_period) {
			/* stop metrics reporting */
			state->current_period = 0;
			buckets = state->buckets;
			state->buckets = NULL;
			state->bucket_cnt = 0;
			client_lock();
			client_timer_cancel(&state->timer);
			client_unlock();
			al_os_mem_free(buckets);
			client_log(LOG_DEBUG "metrics stop");
		}
		return;
	}

	if (!state->current_period) {
		/* start metrics reporting */
		buckets = al_os_mem_calloc(METRICS_BUCKET_CNT *
		    sizeof(struct metrics_bucket));
		if (!buckets) {
			client_log(LOG_ERR "metrics alloc failed");
			return;
		}
		state->buckets = buckets;
		state->bucket_cnt = METRICS_BUCKET_CNT;
		state->unreported_cnt = 0;
		state->write_idx = state->bucket_cnt - 1;
		metrics_next_bucket();
		state->read_idx = state->write_idx;
		state->current_period = mcf->sampling_period;
		client_lock();
		client_timer_set(&state->timer, mcf->sampling_period * 1000);
		client_unlock();
		client_log(LOG_DEBUG
		    "metrics start sample %lu report %u level %u",
		    mcf->sampling_period, mcf->reporting_interval,
		    mcf->reporting_level);
		return;
	}

	if (mcf->sampling_period != state->current_period) {
		/* complete the current sample and switch to new period */
		state->current_period = mcf->sampling_period;
		client_lock();
		client_timer_cancel(&state->timer);
		metrics_timeout(&state->timer);
		client_unlock();
	}
}

static void metrics_conf_export(void)
{
	struct metrics_conf *mcf = &metrics_conf;

	conf_put_u32_default(CT_time, mcf->sampling_period,
	    METRICS_SAMPLE_DEFAULT);
	conf_put_u32_default(CT_interval, mcf->reporting_interval,
	    METRICS_REPORT_DEFAULT);
	conf_put_u32_default(CT_log, mcf->reporting_level,
	    METRIC_LEVEL_DEFAULT);
}

const struct conf_entry metrics_conf_entry = {
	.token = CT_metric,
	.export = metrics_conf_export,
	.set = metrics_conf_set,
	.get = metrics_conf_get,
	.commit = metrics_commit,
};

void metrics_set_controls(u32 sampling_period, u8 reporting_interval,
    enum metric_level reporting_level)
{
	struct metrics_conf *mcf = &metrics_conf;

	mcf->reporting_interval = reporting_interval;
	mcf->reporting_level = reporting_level;
	mcf->sampling_period = sampling_period;
}

void metrics_init(void (*reporting_cb)(void))
{
	struct metrics_state *state = &metrics_state;

	state->lock = ada_lock_create("metrics");
	state->reporting_cb = reporting_cb;

	timer_handler_init(&state->timer, metrics_timeout);

	metrics_commit(0);
}

static void metric_show(struct metric_entry *metric)
{
	const struct metric_info *minfo = metric->info;
	struct metric_sampled *smetric;
	struct metric_duration *dmetric;
	u32 u32value;
	s32 s32value;
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
			printcli("\t%s%s\t%lu", minfo->tag, suffix, u32value);
		}
		break;
	case METRIC_TYPE_SAMPLED:
		smetric = &metric->sampled;
		if (smetric->count > 0) {
			s32value = (s32)(smetric->sum / smetric->count);
			printcli("\t%s%s\t%ld", minfo->tag, suffix, s32value);
		}
		break;
	case METRIC_TYPE_DURATION:
		dmetric = &metric->duration;
		if (dmetric->count > 0) {
			s32value = (s32)(dmetric->sum / dmetric->count);
			printcli("\t%s%s\t%ld", minfo->tag, suffix, s32value);
		}
		break;
	default:
		/* unknown type, skip it */
		break;
	}
}

static void metrics_config_show(void)
{
	struct metrics_conf *mcf = &metrics_conf;

	printcli("metrics sample %lu report %u level %u",
	    mcf->sampling_period, mcf->reporting_interval,
	    mcf->reporting_level);
}

static void metrics_show(void)
{
	struct metrics_state *state = &metrics_state;
	struct metrics_bucket *buckets = state->buckets;
	const struct metric_info *minfo;
	struct metric_cat_info *cinfo;
	struct metrics_bucket *bucket;
	struct metric_entry *metric;
	int i;
	int j;
	int oldest_idx = -1;
	u64 oldest_time = MAX_U64;
	u32 duration;

	metrics_config_show();

	if (!buckets) {
		printcli("metrics disabled");
		return;
	}

	/* find the index of the oldest bucket */
	for (i = 0; i < state->bucket_cnt; ++i) {
		bucket = &buckets[i];
		if (bucket->hdr.start_time &&
		    bucket->hdr.start_time < oldest_time) {
			oldest_idx = i;
			oldest_time = bucket->hdr.start_time;
		}
	}

	if (oldest_idx < 0) {
		printcli("no samples available");
		return;
	}

	i = oldest_idx;
	do {
		bucket = &buckets[i];
		if (!bucket->hdr.start_time) {
			break;
		}

		duration = bucket->hdr.duration;
		if (!duration) {
			duration = (u32)(clock_total_ms() -
			    bucket->hdr.start_time);
		}
		duration = (duration + 500) / 1000;

		printcli("metrics sample start %lu duration %lu",
		    clock_ms_to_utc(bucket->hdr.start_time), duration);

		/* print individual metrics names and values */
		for (j = 0; j < ARRAY_LEN(bucket->metric); ++j) {
			metric = &bucket->metric[j];
			minfo = metric->info;
			if (minfo) {
				cinfo = metrics_get_category_info(
				    minfo->category);
				if (minfo->level <= cinfo->level) {
					metric_show(metric);
				}
			}

		}
		if (++i >= state->bucket_cnt) {
			i = 0;
		}
	} while (i != oldest_idx);
}

const char metrics_cli_help[] =
    "metrics [show|[sample <period>][report <periods>][level <level>]]";

void metrics_cli(int argc, char **argv)
{
	unsigned long val;
	struct metrics_conf *mcf = &metrics_conf;
	u32 sample = mcf->sampling_period;
	u8 report = mcf->reporting_interval;
	enum metric_level level = mcf->reporting_level;
	char *errptr;

	if (argc == 1) {
		metrics_config_show();
		return;
	} else if (argc == 2 && !strcmp(argv[1], "show")) {
		metrics_show();
		return;
	} else if (argc < 3) {
usage:
		printcli("usage: %s", metrics_cli_help);
		printcli(
		    "    sample - sampling period (seconds)\n"
		    "    report - reporting interval (sample periods)\n"
		    "    level - reporting level\n"
		    /* Note: disabled is not currently supported. Since only
		     * global settings are supported at this time, disabled
		     * doesn't make much sense. Once category overrides are
		     * supported, disabled will be useful at this level.
		     */
		    "        1 = summary\n"
		    "        2 = detail\n"
		    "        3 = debug");
		return;
	}
	argv++;
	argc--;
	while (argc) {
		if (argc < 2) {
			goto usage;
		}
		if (!strcmp(argv[0], "sample")) {
			val = strtoul(argv[1], &errptr, 10);
			if (*errptr != '\0' || val < METRICS_SAMPLE_MIN ||
			    val > METRICS_SAMPLE_MAX) {
				goto invalid;
			}
			sample = val;
		} else if (!strcmp(argv[0], "report")) {
			val = strtoul(argv[1], &errptr, 10);
			if (*errptr != '\0' ||
			    (long)val < METRICS_REPORT_MIN ||
			    val > METRICS_REPORT_MAX) {
				goto invalid;
			}
			report = val;
		} else if (!strcmp(argv[0], "level")) {
			val = strtoul(argv[1], &errptr, 10);
			if (*errptr != '\0' ||
			    /* Note: disabled not currently supported */
			    (long)val < METRIC_LEVEL_SUMMARY ||
			    val > METRIC_LEVEL_DEBUG) {
				goto invalid;
			}
			level = (enum metric_level)val;
		} else {
			goto usage;
		}
		argv += 2;
		argc -= 2;
	}
	metrics_set_category_levels(level);	/* only global level for now */
	metrics_set_controls(sample, report, level);
	metrics_commit(1);
	return;

invalid:
	printcli("invalid %s value", argv[0]);
}
#endif /* AYLA_METRICS_SUPPORT */
