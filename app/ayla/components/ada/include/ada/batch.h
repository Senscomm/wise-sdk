/*
 * Copyright 2019 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_ADA_BATCH_H__
#define __AYLA_ADA_BATCH_H__

#include <ada/sprop.h>

/**
 * @file
 * Interfaces of batch-buffer manager.
 *
 * Usage of batch interface:
 *
 * (1). Use ada_batch_create() to create a batch buffer. A handle is
 *	returned.
 * (2). Use ada_batch_add_prop() or ada_batch_add_prop_by_name() to add a
 *	property to the batch buffer. If it returns AE_BUF, app needs
 *	to call ada_batch_send() to send the batch to cloud first and then
 *	re-add the property to the batch.
 *	Notice: after batch is sent, data-points in the batch are cleared,
 *	but the batch handler is still valid.
 * (3). If all data-points in a batch become useless, call ada_batch_discard()
 *	to free its data-points. After this call, the handle of batch is still
 *	valid.
 * (4). If a batch handle is not required any more, call ada_batch_destroy().
 *	After this call, the handle becomes invalid.
 *
 * Features:
 *
 * (1). A batch handle is always valid after it is created, and can be
 *	used for adding-dada-point/sending-batch repeatedly, unless it is
 *	destroyed.
 * (2). Memories allocated by batch manager will be freed by batch manager or
 *	the related prop-mgr. Caller not not take care about it.
 * (3). When app calls ada_batch_send(), a paramter done_cb can be specified.
 *	When the batch is sent, callback done_cb() will be called.
 * (4). In batch sending, properties errors are parsed from the HTTP response,
 *	and stored for each data-points. At last, batch_mgr_report_status()
 *	will be called by prop-mgr to show or log each data-point's batch-id,
 *	name, error-code. You can call ada_batch_set_err_cb() to customize
 *	the error handling.
 * (5). Function ada_batch_send() can be called at any time, even in the time
 *	that another property is in sending or pending state.
 *
 * Restriction:
 *
 * (1). Properties are managed by different property-manager, for example,
 *	ada_sprop_mgr, sched_prop_mgr. Properties under different property-
 *	manager may have different tx direction (to device or from device),
 *	different cloud destination. For the first data-point added to a batch
 *	, if it's dest is NODES_ADS, all other data-points latter added must
 *	be with the same dest. Properties with different dest should be
 *	put into batches with different batch handles.
 */

/**
 * Structure of batch buffer.
 */
struct batch_ctx;

/**
 * Create a batch buffer.
 *
 * \param max_dps is the max numbers of data-points can be buffered.
 * \param max_size is the max buffer-size in bytes.
 * \return handle of the batch created.
 */
struct batch_ctx *ada_batch_create(u16 max_dps, u16 max_size);

/**
 * Free memory of all data points in the batch buffer. Memory of the
 * batch itsself is also freed.
 *
 * \param ctx is the handle of a batch.
 */
void ada_batch_destroy(struct batch_ctx *ctx);

/**
 * Discard all data points in the batch-buffer. The memory of data points
 * are freed. The handle of the batch is still valid.
 *
 * \param ctx is the handle of a batch.
 */
void ada_batch_discard(struct batch_ctx *ctx);

/**
 * Add a property to the batch by property name.
 *
 * \param ctx is handle of the batch.
 * \param prop_name is the property name.
 * \param t_stamp is time stamp.
 *
 * \returns a positive batch-id, 0 or negative on error.
 */
int ada_batch_add_prop_by_name(struct batch_ctx *ctx,
	const char *prop_name, s64 t_stamp);

/**
 * Add a property to the batch.
 *
 * \param ctx is handle of the batch.
 * \param sprop is the property.
 * \param t_stamp is time stamp.
 *
 * \returns a positive batch-id, 0 or negative on error.
 */
int ada_batch_add_prop(struct batch_ctx *ctx,
	struct ada_sprop *sprop, s64 t_stamp);

/**
 * Send a specified batch to cloud, and the memory of all data points
 * are freed. Handle of the batch keeps valid.
 *
 * \param ctx is the handle of a batch.
 * \param done_cb is a callback which is called when sending batch is done.
 *	argument size_sent of done_cb() is the HTTP json payload of all batch
 * data-points sent to cloud.
 * \returns 0 on success.
 */
enum ada_err ada_batch_send(struct batch_ctx *ctx,
		void (*done_cb)(size_t size_sent));

/**
 * Set an error callback to batch-manager.
 *
 * \param err_cb is a callback for reporting error.
 *
 * Parameters for err_cb():
 *	batch_id is the batch-id of a datapoint.
 *	err is error code, 0 and negative are enum ada_err, positive is
 * http status code.
 */
void ada_batch_set_err_cb(void (*err_cb)(int batch_id, int err));

/**
 * Returns 1 if batch contains max number of datapoints allowed
 *
 * \param ctx is the batch context returned by ada_batch_create()
 * \returns 1 if max datapoints reached, 0 otherwise.
 */
u8 ada_batch_is_full(struct batch_ctx *ctx);

#endif /* __AYLA_ADA_BATCH_H__ */
