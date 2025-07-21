/*
 * Copyright 2019 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_METRICS_INT_H__
#define __AYLA_METRICS_INT_H__

#define METRICS_CAT_LEN_MAX	20	/* max length of category name */

struct metric_cat_info {
	const char *name;
	enum metric_level level;
};

struct metrics_bucket {
	struct metrics_bucket_hdr hdr;
	struct metric_entry metric[METRICS_BUCKET_ENTRIES];
};

void metrics_pages_init(void);
void metrics_conf_load(void);
void metrics_init(void (*reporting_cb)(void));
struct metrics_bucket *metrics_get_read_ptr(void);
struct metrics_bucket *metrics_adv_read_ptr(void);
struct metric_cat_info *metrics_get_category_info(u16 cindex);
void metrics_set_category_levels(enum metric_level level);
int metrics_set_category_level(const char *name, enum metric_level level);
void metrics_set_controls(u32 sampling_period, u8 reporting_interval,
    enum metric_level default_level);
#endif
