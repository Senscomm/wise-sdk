/*
 * Copyright 2019 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_ADA_BATCH_INT_H__
#define __AYLA_ADA_BATCH_INT_H__

#include <ada/batch.h>

/**
 * @file
 * Internal interfaces of batch.
 */

/**
 * Structure of data point in sending state.
 */
struct batch_dp {
	struct prop *prop; /* dynamic */
	int batch_id;
	s64 time_stamp;
	size_t tx_size;
	int err; /* <0: enum ada_err, =0: AE_OK, >0: http_status  */
};

/**
 * Structure of batch buffer.
 */
struct batch {
	u32 age_time;	/* UTC, in seconds */
	u16 dps_max;	/* data points max */
	int dps_cnt;	/* data-points added */
	int dps_tx;	/* data-points sent */
	int len_max;	/* in bytes in memory */
	int len_cnt;	/* len of all all data buffered in memory */
	void (*done_cb)(size_t size_sent);
	struct batch_dp dps[1];
};

struct batch_ctx {
	u16 dps_max;	/* data points max */
	int len_max;	/* in bytes */
	struct batch *batch;
};

/**
 * Add a data-point to the specified batch-buffer.
 *
 * \param handle is the handle of a batch.
 * \param prop is the property to be added.
 * \param t_stamp is time stamp (in millisecond)
 *
 * Notice: metadata is contained in prop.
 *
 * \returns the batch-id, 0 on error (alloc-fail or buffer full).
 */
int batch_add_prop(struct batch_ctx *handle, struct prop *prop, s64 t_stamp);

/**
 * A callback called on a batch is done.
 *
 * \param status is prop_cb_status.
 * \param fail_mask is failure mask.
 * \param cb_arg is argument for the callback.
 */
void batch_mgr_report_status(enum prop_cb_status status,
		u8 fail_mask, void *cb_arg);

/* A function used to check if all data points in the batch are sent.
 *
 * \param batch is the batch buffer.
 */
u8 batch_is_all_dps_sent(struct batch *batch);

#endif /* __AYLA_ADA_BATCH_INT_H__ */
