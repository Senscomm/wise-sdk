/*
 * Copyright 2014, 2015 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>

#include <ayla/utypes.h>
#include <ayla/assert.h>
#include <ada/err.h>
#include <ayla/base64.h>
#include <ayla/tlv.h>

#include <ayla/log.h>
#include <ayla/clock.h>
#include <ayla/timer.h>
#include <ayla/callback.h>
#include <ayla/conf.h>

#include <ayla/base64.h>
#include <ada/err.h>
#include <al/al_os_mem.h>
#include <ada/ada_conf.h>
#include <ada/client.h>
#include <ayla/mod_log.h>
#include <ayla/http.h>
#include <ada/prop.h>
#include <ada/prop_mgr.h>
#include "ada_lock.h"
#include "http_client.h"
#include "client_lock.h"
#include "client_timer.h"
#include <ada/prop.h>
#ifdef AYLA_BATCH_PROP_SUPPORT
#include "batch_int.h"
#endif
#ifdef AYLA_LAN_SUPPORT
#include "lan_int.h"
#endif
#include "prop_int.h"

/*
 * State bits
 */
#define PROP_MGR_ENABLED	1	/* listen enable */
#define PROP_MGR_FLOW_CTL	2	/* flow controlled */

/*
 * Private structure for list of all property managers.
 */
struct prop_mgr_node {
	const struct prop_mgr *mgr;
	struct prop_mgr_node *next;
	u8 state;
};

static struct prop_mgr_node *prop_mgrs;
static struct prop *prop_mgr_sending;
static struct prop_queue prop_mgr_send_queue =
		STAILQ_HEAD_INITIALIZER(prop_mgr_send_queue);
static struct callback prop_mgr_send_callback;
static u8 prop_mgr_dest_mask;
static struct ada_lock *prop_mgr_lock;
static struct callback prop_mgr_cont_callback;

#ifdef AYLA_FILE_PROP_SUPPORT
static size_t prop_dp_tot_len;
static size_t prop_dp_max_chunk_size;
static u32 prop_dp_offset;
static u8 prop_dp_eof;
#endif

static void prop_mgr_send_next(void *);
#ifdef AYLA_FILE_PROP_SUPPORT
static enum ada_err (*prop_dp_process_cb)(const char *location, u32 off,
	    void *buf, size_t len, u8 eof);
#endif

static void prop_mgr_cont_cb(void *arg)
{
	client_continue_recv(NULL);
}

void prop_mgr_init(void)
{
	callback_init(&prop_mgr_send_callback, prop_mgr_send_next, NULL);
	callback_init(&prop_mgr_cont_callback, prop_mgr_cont_cb, NULL);
	prop_mgr_lock = ada_lock_create("prop_mgr");
	ASSERT(prop_mgr_lock);
}

/*
 * Queue request for the callback.
 */
static void ada_prop_mgr_enqueue(struct prop *prop)
{
	struct prop_queue *qp = &prop_mgr_send_queue;

	ada_lock(prop_mgr_lock);
	STAILQ_INSERT_TAIL(qp, prop, list);
	ada_unlock(prop_mgr_lock);
	client_callback_pend(&prop_mgr_send_callback);
}

static struct prop_mgr_node *prop_mgr_node_lookup(const struct prop_mgr *pm)
{
	struct prop_mgr_node *pg;

	for (pg = prop_mgrs; pg; pg = pg->next) {
		if (pm == pg->mgr) {
			return pg;
		}
	}
	return NULL;
}

/*
 * If all property managers are now ready, enable listen to client.
 */
static void prop_mgr_listen_check(void)
{
	struct prop_mgr_node *pg;

	for (pg = prop_mgrs; pg; pg = pg->next) {
		if (!(pg->state & PROP_MGR_ENABLED)) {
			return;
		}
	}
	client_enable_ads_listen();
}

static void prop_mgr_report_status(struct prop *prop, enum prop_cb_status stat,
	u8 dest, void *send_done_arg)
{
	const struct prop_mgr *pm;
	if (!prop) {
		return;
	}
#ifdef AYLA_BATCH_PROP_SUPPORT
	if (prop->type == ATLV_BATCH) {
		batch_mgr_report_status(stat, dest, prop);
		return;
	}
#endif
	pm = prop->mgr;
	if (pm) {
		ASSERT(pm->send_done);
		pm->send_done(stat, dest, send_done_arg);
	} else if (prop->prop_mgr_done) {
		prop->prop_mgr_done(prop);
	}
}

/*
 * Check the send list for any property updates that can no longer be sent
 * because all destinations specified have lost connectivity.
 */
static void prop_mgr_send_queue_check(u8 mask)
{
	struct prop_queue *qp = &prop_mgr_send_queue;
	struct prop_queue lost_q;
	struct prop *prop;
	struct prop *next;

	/*
	 * Look for props that can no longer be sent to any destination,
	 * take them off the list and put them on the local lost_q list.
	 * Then go through the local lost_q list and do the callbacks after.
	 * dropping the lock.
	 */
	STAILQ_INIT(&lost_q);
	ada_lock(prop_mgr_lock);
	for (prop = STAILQ_FIRST(qp); prop; prop = next) {
		next = STAILQ_NEXT(prop, list);
		if (!(prop->send_dest & mask)) {
			STAILQ_REMOVE(qp, prop, prop, list);
			STAILQ_INSERT_TAIL(&lost_q, prop, list);
		}
	}
	ada_unlock(prop_mgr_lock);
	for (;;) {
		prop = STAILQ_FIRST(&lost_q);
		if (!prop) {
			break;
		}
		STAILQ_REMOVE_HEAD(&lost_q, list);

		/*
		 * report all dests failed
		 */
		prop_mgr_report_status(prop, PROP_CB_CONN_ERR,
		    prop->send_dest, prop->send_done_arg);
	}
}

void prop_mgr_connect_sts(u8 mask)
{
	struct prop_mgr_node *pg;
	const struct prop_mgr *pm;
	u8 lost_dests;
	u8 added_dests;

	for (pg = prop_mgrs; pg; pg = pg->next) {
		pm = pg->mgr;
		if (pm->connect_status) {
			if (!(mask & NODES_ADS)) {
				pg->state &= ~PROP_MGR_ENABLED;
			}
			pm->connect_status(mask);
		}
	}
	if (mask & NODES_ADS) {
		prop_mgr_listen_check();
	}

	/*
	 * Determine which destinations have been lost and added.
	 */
	lost_dests = prop_mgr_dest_mask & ~mask;
	added_dests = ~prop_mgr_dest_mask & mask;
	prop_mgr_dest_mask = mask;

	/*
	 * If connectivity has improved, send pending properties.
	 */
	if (added_dests) {
		client_lock();
		prop_mgr_send_next(NULL);
		client_unlock();
	}

	/*
	 * If some connectivity has been lost, check queued property updates.
	 */
	if (lost_dests) {
		prop_mgr_send_queue_check(mask);
	}
}

void prop_mgr_event(enum prop_mgr_event event, void *event_specific_arg)
{
	struct prop_mgr_node *pg;
	const struct prop_mgr *pm;

	for (pg = prop_mgrs; pg; pg = pg->next) {
		pm = pg->mgr;

		/*
		 * PME_TIMEOUT events are sent only to the active prop_mgr.
		 * Other events go to all prop_mgrs.
		 */
		if (event == PME_TIMEOUT &&
		    (prop_mgr_sending && pm != prop_mgr_sending->mgr)) {
			continue;
		}
		if (pm->event) {
			pm->event(event, event_specific_arg);
		}
	}
}

void ada_prop_mgr_register(const struct prop_mgr *pm)
{
	struct prop_mgr_node *pg;

	log_put(LOG_DEBUG "%s: pm=%s\n", __func__, pm->name);

	pg = al_os_mem_calloc(sizeof(*pg));
	ASSERT(pg);
	pg->mgr = pm;
	pg->next = prop_mgrs;
	prop_mgrs = pg;
}

void ada_prop_mgr_ready(const struct prop_mgr *pm)
{
	struct prop_mgr_node *pg;

	log_put(LOG_DEBUG "%s: pm=%s\n", __func__, pm->name);

	pg = prop_mgr_node_lookup(pm);
	if (!pg || (pg->state & PROP_MGR_ENABLED)) {
		return;
	}
	pg->state |= PROP_MGR_ENABLED;
	prop_mgr_listen_check();
}

static enum ada_err prop_mgr_cb_done(enum prop_cb_status stat,
				u8 dests)
{
	struct prop *prop = prop_mgr_sending;

	prop_mgr_sending = NULL;
	prop_mgr_report_status(prop, stat, dests, prop->send_done_arg);
	client_callback_pend(&prop_mgr_send_callback);
	return AE_OK;
}

/*
 * Callback from client state machine to send a property or a request.
 */
static enum ada_err prop_mgr_send_cb(enum prop_cb_status stat, void *arg)
{
	struct prop *prop = prop_mgr_sending;
	struct prop_ack *ack;
	enum ada_err err = AE_OK;

	ASSERT(client_locked);
	ASSERT(prop);

	switch (stat) {
	case PROP_CB_BEGIN:
		ack = prop->ack;
		if (prop->type == ATLV_ACK_ID && ack) {
			err = client_send_prop_ack(prop);
			break;
		}
		if (!prop->val) {	/* request property from ADS */
			err = client_get_prop_val(prop->name);
			if (err == AE_BUSY) {
				ada_prop_mgr_enqueue(prop);
			}
			break;
		}
		err = client_send_data(prop);
		break;

	case PROP_CB_DONE:
		if (prop->val) {		/* not a GET */
			prop_mgr_event(PME_PROP_SET, (void *)prop->name);
		}
		/* fall through */
	default:
		err = prop_mgr_cb_done(stat, client_get_failed_dests());
		break;
	}
	return err;
}

#ifdef AYLA_FILE_PROP_SUPPORT
/*
 * Callback from client state machine to send a file datapoint loc req.
 */
static enum ada_err prop_mgr_dp_loc_req_send(enum prop_cb_status stat,
	void *arg)
{
	struct prop *prop = prop_mgr_sending;
	enum ada_err err;

	ASSERT(client_locked);
	ASSERT(prop);

	switch (stat) {
	case PROP_CB_BEGIN:
		/* request s3 file loc from ADS */
		err = client_send_dp_loc_req(prop->name, prop->dp_meta);
		if (err == AE_BUSY) {
			ada_prop_mgr_enqueue(prop);
		}
		break;
	case PROP_CB_DONE:
		if (!arg) {
			stat = PROP_CB_CONN_ERR;
			goto def_case;
		}
		prop->val = arg;
		/* fall through */
	default:
def_case:
		err = prop_mgr_cb_done(stat, client_get_failed_dests());
		break;
	}
	return err;
}

/*
 * Callback from client state machine to close file dp upload.
 */
enum ada_err prop_mgr_dp_close(enum prop_cb_status stat, void *arg)
{
	struct prop *prop = prop_mgr_sending;
	enum ada_err err = AE_OK;
	const char *loc = NULL;

	ASSERT(client_locked);
	ASSERT(prop);

	switch (stat) {
	case PROP_CB_BEGIN:
		err = client_close_dp_put(prop->name);
		if (err == AE_BUSY) {
			ada_prop_mgr_enqueue(prop);
		}
		break;
	case PROP_CB_DONE:
		loc = prop->name;
		goto def_case;
	default:
		stat = PROP_CB_CONN_ERR;
def_case:
		err = prop_mgr_cb_done(stat, client_get_failed_dests());
		if (stat == PROP_CB_DONE) {
			prop_mgr_event(PME_FILE_DONE, (void *)loc);
		}
		break;
	}
	return err;
}

const char *prop_dp_put_get_loc(void)
{
	struct prop *prop = prop_mgr_sending;

	return prop->name;
}

/*
 * Callback from client state machine to do file dp upload.
 */
enum ada_err prop_mgr_dp_put(enum prop_cb_status stat, void *arg)
{
	struct prop *prop = prop_mgr_sending;
	enum ada_err err = AE_OK;
	u8 dests;

	ASSERT(client_locked);
	ASSERT(prop);

	switch (stat) {
	case PROP_CB_BEGIN:
		err = client_send_dp_put(prop->val, prop->len, prop->name,
		    prop_dp_offset, prop_dp_tot_len, prop_dp_eof);
		if (err == AE_BUSY) {
			ada_prop_mgr_enqueue(prop);
		}
		break;
	case PROP_CB_DONE:
		prop_dp_tot_len -= prop->len;
		dests = prop->send_dest;
		if (!prop_dp_tot_len) {
			if (prop->type != ATLV_LOC) {
				goto def_case;
			}
			client_unlock();
			client_send_callback_set(prop_mgr_dp_close,
			    prop->send_dest);
			client_lock();
		} else {
			stat = PROP_CB_CONTINUE;
			goto def_case;
		}
		break;
	default:
		dests = client_get_failed_dests();
		prop_dp_tot_len = 0;
def_case:
		err = prop_mgr_cb_done(stat, dests);
		break;
	}
	return err;
}

/*
 * Callback from client state machine to do mark downloaded
 * file dp as fetched.
 */
static enum ada_err prop_mgr_dp_fetched(enum prop_cb_status stat, void *arg)
{
	struct prop *prop = prop_mgr_sending;
	enum ada_err err = AE_OK;

	ASSERT(client_locked);
	ASSERT(prop);

	switch (stat) {
	case PROP_CB_BEGIN:
		err = client_send_dp_fetched(prop->name);
		if (err == AE_BUSY) {
			ada_prop_mgr_enqueue(prop);
		}
		break;
	default:
		prop_dp_eof = 0;
		prop_dp_max_chunk_size = 0;
		err = prop_mgr_cb_done(stat, client_get_failed_dests());
		break;
	}
	return err;
}

/*
 * Callback from client state machine to do file dp download.
 */
enum ada_err prop_mgr_dp_req(enum prop_cb_status stat, void *arg)
{
	struct prop *prop = prop_mgr_sending;
	struct recv_payload *recv;
	struct http_client *hc;
	enum ada_err err = AE_OK;
	int block_size;
	const char *loc = NULL;

	ASSERT(client_locked);
	ASSERT(prop);

	switch (stat) {
	case PROP_CB_BEGIN:
		err = client_get_dp_req(prop->name, prop_dp_offset,
		    (prop_dp_offset + LONG_GET_REQ_SIZE - 1));
		if (err == AE_BUSY) {
			ada_prop_mgr_enqueue(prop);
		}
		break;
	case PROP_CB_CONTINUE:
		if (!arg) {
			stat = PROP_CB_CONN_ERR;
			goto def_case;
		}
		recv = (struct recv_payload *)arg;
		while (recv->len != 0) {
			block_size = (recv->len < prop_dp_max_chunk_size) ?
			    recv->len : prop_dp_max_chunk_size;
			err = prop_dp_process_cb(prop->name, prop_dp_offset,
			    recv->data, block_size, 0);
			if (err == AE_BUF) {
				/* application unable to process fast enough */
				break;
			}
			recv->len -= block_size;
			recv->data += block_size;
			recv->consumed += block_size;
			prop_dp_offset += block_size;
		}
		break;
	case PROP_CB_DONE:
		if (!arg) {
			stat = PROP_CB_CONN_ERR;
			goto def_case;
		}
		hc = (struct http_client *)arg;
		/* more chunks to fetch */
		if (!hc->range_given || prop_dp_offset >= hc->range_bytes) {
			err = prop_dp_process_cb(prop->name, prop_dp_offset,
			    NULL, 0, 1);
			goto def_case;
		} else {
			client_unlock();
			client_send_callback_set(prop_mgr_dp_req,
			    prop->send_dest);
			client_lock();
			err = AE_IN_PROGRESS;
		}
		break;
	default:
def_case:
		loc = prop->name;
		err = prop_mgr_cb_done(stat, client_get_failed_dests());
		if (stat == PROP_CB_DONE) {
			prop_mgr_event(PME_FILE_DONE, (void *)loc);
		}
		break;
	}
	if (err == AE_BUF) {
		client_callback_pend(&prop_mgr_cont_callback);
	}
	return err;
}

/*
 * Callback from client state machine to request S3 location
 * for file download.
 */
static enum ada_err prop_mgr_dp_loc_req(enum prop_cb_status stat, void *arg)
{
	struct prop *prop = prop_mgr_sending;
	enum ada_err err = AE_OK;

	ASSERT(client_locked);
	ASSERT(prop);

	switch (stat) {
	case PROP_CB_BEGIN:
		err = client_get_dp_loc_req(prop->name);
		if (err == AE_BUSY) {
			ada_prop_mgr_enqueue(prop);
		}
		break;
	case PROP_CB_DONE:
		client_unlock();
		client_send_callback_set(prop_mgr_dp_req, NODES_ADS);
		client_lock();
		break;
	default:
		err = prop_mgr_cb_done(stat, client_get_failed_dests());
		break;
	}
	return err;
}
#endif

int prop_type_is_file_or_msg(enum ayla_tlv_type type)
{
	return type == ATLV_LOC || type == ATLV_MSG_BIN ||
	    type == ATLV_MSG_JSON || type == ATLV_MSG_UTF8;
}

int prop_is_file_or_msg(struct prop *prop)
{
	return prop_type_is_file_or_msg(prop->type);
}

/*
 * Start send for next property on the queue.
 * This is called through a callback so it may safely drop the client_lock.
 */
static void prop_mgr_send_next(void *arg)
{
	struct prop_queue *qp = &prop_mgr_send_queue;
	struct prop *prop;
	enum ada_err (*callback)(enum prop_cb_status stat, void *arg);

	ASSERT(client_locked);
	for (;;) {
		if (!prop_mgr_dest_mask) {
			break;
		}
		if (prop_mgr_sending) {
			break;
		}
		ada_lock(prop_mgr_lock);
		prop = STAILQ_FIRST(qp);
		if (!prop) {
			ada_unlock(prop_mgr_lock);
			break;
		}
		STAILQ_REMOVE_HEAD(qp, list);
#ifdef AYLA_FILE_PROP_SUPPORT
		if (client_ongoing_file() && !prop_is_file_or_msg(prop)) {
			/* non-file prop to be processed later */
			ada_unlock(prop_mgr_lock);
			ada_prop_mgr_enqueue(prop);
			break;
		}
#endif
		prop_mgr_sending = prop;
		ada_unlock(prop_mgr_lock);
		client_unlock();

		callback = prop_mgr_send_cb;
#ifdef AYLA_FILE_PROP_SUPPORT
		if (prop_is_file_or_msg(prop)) {
			if (prop->is_dl) {
				if (prop->type == ATLV_LOC) {
					/* file download */
					callback = prop_dp_eof ?
					    prop_mgr_dp_fetched :
					    prop_mgr_dp_loc_req;
				} else {
					/* message download */
					callback = prop_mgr_dp_req;
				}
			} else {
				/* file or message upload */
				callback = prop_dp_tot_len ?
				    prop_mgr_dp_put : prop_mgr_dp_loc_req_send;
			}
		}
#endif
		client_send_callback_set(callback, prop->send_dest);

		client_lock();
	}
}

struct prop *prop_mgr_get_send_prop(void)
{
	return prop_mgr_sending;
}

enum ada_err ada_prop_mgr_prop_send(struct prop *prop)
{
	ada_prop_mgr_enqueue(prop);
	return AE_IN_PROGRESS;
}

/*
 * Send a property.
 * The prop structure must be available until (*send_cb)() is called indicating
 * the value has been completely sent.
 */
enum ada_err ada_prop_mgr_send(const struct prop_mgr *pm, struct prop *prop,
			u8 dest_mask, void *cb_arg)
{
	if (!dest_mask) {
		return AE_INVAL_STATE;
	}
	prop->mgr = pm;
	prop->send_dest = dest_mask;
	prop->send_done_arg = cb_arg;
	return ada_prop_mgr_prop_send(prop);
}

#ifdef AYLA_FILE_PROP_SUPPORT
/*
 * Put a file or message property upload to S3 server.
 */
enum ada_err ada_prop_mgr_dp_put(const struct prop_mgr *pm, struct prop *prop,
	u32 off, size_t tot_len, u8 eof, void *cb_arg)
{
#ifndef AYLA_MSG_PROP_SUPPORT
	if (prop->type != ATLV_LOC) {
		return AE_INVAL_TYPE;
	}
#endif
	if (!off) {
		prop_dp_tot_len = tot_len;
	}
	if (prop->len > prop_dp_tot_len) {
		return AE_INVAL_OFF;
	}
	prop_dp_eof = eof;
	prop_dp_offset = off;
	return ada_prop_mgr_send(pm, prop, NODES_ADS, cb_arg);
}

/*
 * Do a file or message property download.
 */
enum ada_err ada_prop_mgr_dp_get(const struct prop_mgr *pm, struct prop *prop,
	const char *location, u32 off, size_t max_chunk_size,
	enum ada_err (*prop_mgr_dp_process_cb)(const char *location, u32 off,
	    void *buf, size_t len, u8 eof), void *cb_arg)
{
#ifndef AYLA_MSG_PROP_SUPPORT
	if (prop->type != ATLV_LOC) {
		return AE_INVAL_TYPE;
	}
#endif
	if (!location) {
		return AE_INVAL_VAL;
	}
	prop_dp_offset = off;
	prop_dp_eof = 0;
	prop_dp_max_chunk_size = max_chunk_size;
	prop_dp_process_cb = prop_mgr_dp_process_cb;
	prop->name = location;
	prop->is_dl = 1;
	return ada_prop_mgr_send(pm, prop, NODES_ADS, cb_arg);
}

/*
 * Mark a file property datapoint fetched.
 */
enum ada_err ada_prop_mgr_dp_fetched(const struct prop_mgr *pm,
	struct prop *prop, const char *location, void *cb_arg)
{
	if (!location) {
		return AE_INVAL_VAL;
	}
	prop_dp_eof = 1;
	prop->name = location;
	prop->is_dl = 1;
	return ada_prop_mgr_send(pm, prop, NODES_ADS, cb_arg);
}

/*
 * Abort an ongoing file transfer.
 */
void ada_prop_mgr_dp_abort(const char *location)
{
	struct prop_queue *qp = &prop_mgr_send_queue;
	struct prop *prop;

	client_unlock();
	client_abort_file_operation();
	client_lock();
	prop_dp_tot_len = 0;
	prop_dp_max_chunk_size = 0;
	prop_dp_process_cb = NULL;
	prop_dp_eof = 0;
	prop_mgr_cb_done(PROP_CB_DONE, 0);

	/* remove the file prop from prop queue */
	ada_lock(prop_mgr_lock);
	STAILQ_FOREACH(prop, qp, list) {
		if (prop->type == ATLV_LOC &&
		    !strcmp(prop->name, location)) {
			STAILQ_REMOVE(qp, prop, prop, list);
			break;
		}
	}
	ada_unlock(prop_mgr_lock);
}
#endif

/*
 * Get a prop using a property manager.
 */
enum ada_err ada_prop_mgr_get(const char *name,
		enum ada_err (*get_cb)(struct prop *, void *arg, enum ada_err),
		void *arg)
{
	struct prop_mgr_node *pg;
	const struct prop_mgr *pm;
	enum ada_err err;

	if (!prop_name_valid(name)) {
		return AE_INVAL_NAME;
	}

	for (pg = prop_mgrs; pg; pg = pg->next) {
		pm = pg->mgr;
		if (!pm->get_val) {
			continue;
		}
		err = pm->get_val(name, get_cb, arg);
		if (err != AE_NOT_FOUND) {
			return err;
		}
	}
	return AE_NOT_FOUND;
}

static void ada_prop_mgr_prop_free(struct prop *prop)
{
	al_os_mem_free(prop);
}

/*
 * Create a property from name/value/metadata
 */
struct prop *prop_create(const char *name, enum ayla_tlv_type type,
    const void *val, size_t val_len, u8 echo, struct prop_dp_meta *metadata)
{
	struct prop *prop;
	size_t len;
	size_t name_len;	/* length of name including null terminator */
	u8 *ptr;
	size_t meta_len = sizeof(struct prop_dp_meta) * PROP_MAX_DPMETA;

	name_len = strlen(name) + 1;
	len = val_len + name_len;
	if (type == ATLV_UTF8) {
		len++;
	}
	if (metadata) {
		len += meta_len;
	}
	prop = al_os_mem_alloc(sizeof(*prop) + len);
	if (!prop) {
		return NULL;
	}
	memset(prop, 0, sizeof(*prop));

	prop->type = type;
	prop->echo = echo;
	ptr = (u8 *)(prop + 1);

	/*
	 * Copy metadata, if present, after prop struct, for good alignment.
	 */
	if (metadata) {
		prop->dp_meta = (struct prop_dp_meta *)ptr;
		memcpy(prop->dp_meta, metadata, meta_len);
		ptr += meta_len;
	}

	/*
	 * Copy name to location after metadata.
	 */
	memcpy(ptr, name, name_len);
	prop->name = (const char *)ptr;

	/*
	 * Copy value to location after name.
	 */
	ptr += name_len;
	memcpy(ptr, val, val_len);
	prop->val = ptr;
	prop->len = val_len;
	if (type == ATLV_UTF8) {
		*(ptr + val_len) = '\0';
	}

	return prop;
}

/*
 * Echo a received property update.
 */
static enum ada_err prop_mgr_echo(const char *name,
    enum ayla_tlv_type type, const void *val, size_t val_len, u8 src,
    struct prop_dp_meta *metadata)
{
	struct prop *prop;
	u8 dests;
	enum ada_err err;

	switch (type) {
	case ATLV_LOC:
	case ATLV_MSG_BIN:
	case ATLV_MSG_JSON:
	case ATLV_MSG_UTF8:
	case ATLV_SCHED:
		/* don't echo these types of properties */
		return AE_OK;
	default:
		break;
	}

	/*
	 * Echo to all connected local nodes and ADS.
	 */
	dests = prop_mgr_dest_mask & ~src;
	if (!dests) {
		/* no other dests connected */
		return AE_OK;
	}

	prop = prop_create(name, type, val, val_len, 1, metadata);
	if (!prop) {
		return AE_ALLOC;
	}

	prop->prop_mgr_done = ada_prop_mgr_prop_free;

	err = ada_prop_mgr_send(NULL, prop, dests, prop);
	if (!err || err == AE_IN_PROGRESS) {
		return AE_OK;
	}
	al_os_mem_free(prop);
	return err;
}

/*
 * Set a prop using a property manager.
 */
enum ada_err ada_prop_mgr_set(const char *name, enum ayla_tlv_type type,
			const void *val, size_t val_len,
			size_t *offset, u8 src, void *set_arg)
{
	return ada_prop_mgr_meta_set(name, type, val, val_len, offset,
	    src, set_arg, NULL, NULL);
}

/*
 * Set a prop using a property manager including ack ID and metadata.
 */
enum ada_err ada_prop_mgr_meta_set(const char *name, enum ayla_tlv_type type,
			const void *val, size_t val_len,
			size_t *offset, u8 src, void *set_arg,
			const char *ack_id, struct prop_dp_meta *metadata)
{
	struct prop_mgr_node *pg;
	const struct prop_mgr *pm;
	enum ada_err err = AE_NOT_FOUND;
#if defined(AYLA_LAN_SUPPORT) || defined(AYLA_LOCAL_CONTROL_SUPPORT)
	struct ada_lan_conf *lcf = &ada_lan_conf;
#endif

	if (!prop_name_valid(name)) {
		return AE_INVAL_NAME;
	}

	for (pg = prop_mgrs; pg; pg = pg->next) {
		pm = pg->mgr;
		if (pm->prop_meta_recv) {
			err = pm->prop_meta_recv(name, type, val, val_len,
			    offset, src, set_arg, ack_id, metadata);
		} else if (pm->prop_recv) {
			err = pm->prop_recv(name, type, val, val_len,
			    offset, src, set_arg);
		} else {
			continue;
		}
		if (err != AE_NOT_FOUND) {
			/*
			 * AE_ABRT is an indication of success but to abort
			 * subsequent processing (echos) of this property.
			 */
			if (err == AE_OK || err == AE_IN_PROGRESS ||
			    err == AE_BUF || err == AE_ABRT) {
				/*
				 * Automatically echo props on behalf of
				 * scheduler and local control clients.
				 */
				if (err != AE_ABRT &&
				    ((src & NODES_SCHED) ||
#if defined(AYLA_LAN_SUPPORT) || defined(AYLA_LOCAL_CONTROL_SUPPORT)
				    (lcf->auto_echo && (src & NODES_LC)) ||
#endif
				    0)) {

					prop_mgr_echo(name, type, val, val_len,
					    src, metadata);
				}
				prop_mgr_event(PME_PROP_SET, (void *)name);
			}
			break;
		}
	}
	return err;
}

/*
 * Returns 1 if the property type means the value has to be in quotes
 */
int prop_type_is_str(enum ayla_tlv_type type)
{
	return type == ATLV_UTF8 || type == ATLV_BIN || type == ATLV_SCHED ||
	    type == ATLV_LOC;
}

/*
 * Format a property value.
 * Returns the number of bytes placed into the value buffer.
 */
size_t prop_fmt(char *buf, size_t len, enum ayla_tlv_type type,
		void *val, size_t val_len, char **out_val)
{
	int rc;
	size_t i;
	s32 ones;
	unsigned int tenths;
	unsigned int cents;
	char *sign;

	*out_val = buf;
	switch (type) {
	case ATLV_INT:
		rc = snprintf(buf, len, "%ld", *(s32 *)val);
		break;

	case ATLV_UINT:
		rc = snprintf(buf, len, "%lu", *(u32 *)val);
		break;

	case ATLV_BOOL:
		rc = snprintf(buf, len, "%u", *(u8 *)val != 0);
		break;

	case ATLV_UTF8:
		/* for strings, use the original buffer */
		ASSERT(val_len <= TLV_MAX_STR_LEN);
		*out_val = val;
		return val_len;

	case ATLV_CENTS:
		ones = *(s32 *)val;
		sign = "";
		if (ones < 0 && ones != 0x80000000) {
			sign = "-";
			ones = -ones;
		}
		tenths = (unsigned)ones;
		cents = tenths % 10;
		tenths = (tenths / 10) % 10;
		ones /= 100;
		rc = snprintf(buf, len, "%s%ld.%u%u",
		    sign, ones, tenths, cents);
		break;

	case ATLV_BIN:
	case ATLV_SCHED:
		rc = ayla_base64_encode((u8 *)val, val_len, buf, &len);
		if (rc < 0) {
			log_put("prop_fmt: enc fail rc %d", rc);
			buf[0] = '\0';
			return 0;
		}
		return len;

	case ATLV_LOC:
	case ATLV_MSG_BIN:
	case ATLV_MSG_JSON:
	case ATLV_MSG_UTF8:
		ASSERT_NOTREACHED();
		break;

	default:
		rc = 0;
		ASSERT(val);
		for (i = 0; i < val_len && rc < len; i++) {
			rc += snprintf(buf + rc, len - rc, "%2.2x ",
			    *((unsigned char *)val + i));
		}
		break;
	}
	if (rc >= len) {
		log_put("prop_fmt: value too big for buffer, len %d", rc);
		buf[0] = '\0';
		return 0;
	}
	return rc;
}

/*
 * Return non-zero if property name is valid.
 * Valid names need no XML or JSON encoding.
 * Valid names include: alphabetic chars numbers, hyphen and underscore.
 * The first character must be alphabetic.  The max length is 27.
 *
 * This could use ctypes, but doesn't due to library license.
 */
int prop_name_valid(const char *name)
{
	const char *cp = name;
	char c;
	size_t len = 0;

	while ((c = *cp++) != '\0') {
		len++;
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
			continue;
		}
		if (len > 1) {
			if (c >= '0' && c <= '9') {
				continue;
			}
			if (c == '_' || c == '-') {
				continue;
			}
		}
		return 0;
	}
	if (len == 0 || len >= PROP_NAME_LEN) {
		return 0;
	}
	return 1;
}

static void ada_prop_mgr_request_done(struct prop *prop)
{
	al_os_mem_free(prop);
}

/*
 * Request a property (or all properties, if name is NULL).
 * This version supplies a callback which should free the property structure.
 */
enum ada_err ada_prop_mgr_request_get(const char *name,
		void (*cb)(struct prop *prop))
{
	struct prop *prop;

	if (name && !prop_name_valid(name)) {
		return AE_INVAL_NAME;
	}

	prop = al_os_mem_calloc(sizeof(*prop));
	if (!prop) {
		return AE_ALLOC;
	}

	/*
	 * GET request is indicated by the NULL val pointer.
	 * A NULL or empty name will indicate all to-device properties.
	 */
	prop->name = name;
	prop->send_dest = NODES_ADS;
	prop->prop_mgr_done = cb;

	ada_prop_mgr_enqueue(prop);
	return AE_OK;
}

/*
 * Request a property (or all properties if name is NULL).
 */
enum ada_err ada_prop_mgr_request(const char *name)
{
	return ada_prop_mgr_request_get(name, ada_prop_mgr_request_done);
}

/*
 * Indicate to agent that property recv is complete.
 * After a property manager's prop_recv() handler returns AE_BUF, receive is
 * paused until this is called.
 * The src_mask indicates which client should be unpaused.
 */
void ada_prop_mgr_recv_done(u8 src_mask)
{
	if (src_mask == NODES_ADS) {
		client_callback_pend(&prop_mgr_cont_callback);
#ifdef AYLA_LAN_SUPPORT
	} else if ((src_mask & NODES_LAN) != 0) {
		client_lan_recv_done(src_mask);
#endif
	}
}
