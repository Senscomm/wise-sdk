/*
 * Copyright 2019 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_METRICS_H__
#define __AYLA_METRICS_H__

#ifdef AYLA_WIFI_SUPPORT
#define METRICS_WIFI
#endif
#define METRICS_HTTP
#define METRICS_PROP

#define METRICS_BUCKET_ENTRIES	37	/* number of entries in a bucket */
#define METRICS_BUCKET_CNT	3	/* number of buckets to reserve */

#define METRICS_SAMPLE_MIN	15		/* seconds */
#define METRICS_SAMPLE_MAX	(24 * 60 * 60)	/* 1 day */
#define METRICS_SAMPLE_DEFAULT	(15 * 60)	/* 15 minutes */

#define METRICS_REPORT_MIN	0	/* zero = off */
#define METRICS_REPORT_MAX	(METRICS_BUCKET_CNT - 1)
#define METRICS_REPORT_DEFAULT	0	/* default metrics reporting to off */

#define METRIC_LEVEL_DEFAULT	METRIC_LEVEL_SUMMARY

enum metric_category {
#ifdef METRICS_WIFI
	METRICS_CAT_WIFI,
#endif
#ifdef METRICS_HTTP
	METRICS_CAT_HTTP,
#endif
#ifdef METRICS_PROP
	METRICS_CAT_PROP,
#endif
};

enum metric_type {
	METRIC_TYPE_COUNTER,
	METRIC_TYPE_SAMPLED,
	METRIC_TYPE_DURATION,
};

enum metric_level {
	METRIC_LEVEL_DISABLED = 0,
	METRIC_LEVEL_SUMMARY = 1,
	METRIC_LEVEL_DETAIL = 2,
	METRIC_LEVEL_DEBUG = 3,
};

enum metric_id {
#ifdef METRICS_WIFI
	M_WIFI_CONNECT_ATTEMPTS,
	M_WIFI_CONNECT_SUCCESSES,
	M_WIFI_CONNECT_FAILURES,
	M_WIFI_CONNECT_LATENCY,
	M_WIFI_CONNECT_ERRORS,
	M_WIFI_RSSI,
#endif
#ifdef METRICS_HTTP
	M_HTTP_CONNECT_ATTEMPTS,
	M_HTTP_CONNECT_SUCCESSES,
	M_HTTP_CONNECT_FAILURES,
	M_HTTP_CONNECT_LATENCY,
	M_HTTP_REQUEST_ATTEMPTS,
	M_HTTP_REQUEST_SUCCESSES,
	M_HTTP_REQUEST_FAILURES,
	M_HTTP_REQUEST_LATENCY,
	M_HTTP_STATUS,
#endif
#ifdef METRICS_PROP
	M_PROP_SEND_ATTEMPTS,
	M_PROP_SEND_SUCCESSES,
	M_PROP_SEND_FAILURES,
	M_PROP_SEND_LATENCY,
	M_PROP_SEND_STATUS,
#endif
};

struct metric_info {
	enum metric_category category;
	const char *tag;
	enum metric_type type;
	enum metric_level level;
};

struct metrics_bucket_hdr {
	u64 start_time;
	u32 duration;
};

struct metric_counter {
	s32 count;
};

struct metric_sampled {
	s64 sum;
	s32 count;
};

struct metric_duration {
	s64 sum;
	s32 count;
	u32 start;
};

struct metric_entry {
	const struct metric_info *info;
	u16 instance;
	s16 code;
	union {
		struct metric_counter counter;
		struct metric_sampled sampled;
		struct metric_duration duration;
	};
};

/* Increment a counter metric */
#ifdef AYLA_METRICS_SUPPORT
#define METRIC_INCR(metric, instance, code) \
	do { \
		struct metric_entry *entry = metric_get((metric), (instance), \
		    (code)); \
		if (entry) { \
			entry->counter.count++; \
		} \
	} while (0)
#else
#define METRIC_INCR(metric, instance, code) \
	do { \
		(void)(instance); \
		(void)(code); \
	} while (0)
#endif

/* Update a sampled metric */
#ifdef AYLA_METRICS_SUPPORT
#define METRIC_SAMPLE(metric, instance, value) \
	do { \
		struct metric_entry *entry = metric_get((metric), (instance), \
		    0); \
		if (entry) { \
			entry->sampled.count++; \
			entry->sampled.sum += (value); \
		} \
	} while (0)
#else
#define METRIC_SAMPLE(metric, instance, value)
#endif

/* Record the start time for a duration metric */
#ifdef AYLA_METRICS_SUPPORT
#define METRIC_OP_BEGIN(metric, instance) \
	do { \
		struct metric_entry *__entry = metric_get((metric), \
		    (instance), 0); \
		if (__entry) { \
			__entry->duration.start = (u32)clock_total_ms(); \
		} \
	} while (0)
#else
#define METRIC_OP_BEGIN(metric, instance)
#endif

/* Record the end time for a duration metric */
#ifdef AYLA_METRICS_SUPPORT
#define METRIC_OP_END(metric, instance) \
	do { \
		struct metric_entry *__entry = metric_get((metric), \
		    (instance),  0); \
		if (__entry && __entry->duration.start) { \
			s32 __duration = (s32)((u32)clock_total_ms() - \
			    __entry->duration.start); \
			__entry->duration.start = 0; \
			METRIC_SAMPLE((metric), (instance), __duration); \
		} \
	} while (0)
#else
#define METRIC_OP_END(metric, instance)
#endif

struct metric_entry *metric_get(enum metric_id id, u16 instance, s16 code);
void metrics_cli(int argc, char **argv);
extern const char metrics_cli_help[];

#endif
