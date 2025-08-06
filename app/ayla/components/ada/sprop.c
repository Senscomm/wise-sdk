/*
 * Copyright 2015-2016 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <ayla/assert.h>
#include <ayla/utypes.h>
#include <ayla/tlv.h>
#include <ayla/clock.h>
#include <ayla/log.h>
#include <ayla/timer.h>
#include <ayla/callback.h>

#include <ada/err.h>
#include <ada/prop.h>
#include <ada/prop_mgr.h>
#include <ada/sprop.h>
#include <ada/client.h>
#include <al/al_os_mem.h>
#include "client_lock.h"
#include "client_timer.h"
#include <ada/prop.h>
#include "prop_int.h"
#ifdef AYLA_BATCH_PROP_SUPPORT
#include "batch_int.h"
#endif

struct ada_sprop_entry {
	char name[SPROP_NAME_MAX_LEN];
	struct ada_sprop *table;
	unsigned int entries;
};

#if defined(AYLA_MSG_PROP_SUPPORT) & !defined(AYLA_FILE_PROP_SUPPORT)
#error AYLA_MSG_PROP_SUPPORT requires AYLA_FILE_PROP_SUPPORT
#endif

#ifdef AYLA_FILE_PROP_SUPPORT
static struct file_dp *sprop_file_dp_active;
#endif

static struct ada_sprop_entry ada_sprop_table[SPROP_TABLE_ENTRIES];
static struct prop_mgr ada_sprop_mgr;

u8 ada_sprop_dest_mask;

static enum ada_err sprop_send_opt(struct ada_sprop *, int echo, u8 dests,
				struct prop_dp_meta *metadata, u8 count);

static u32 ada_sprop_allocs;
static u32 ada_sprop_echos_pending;
static struct callback ada_sprop_ready_callback;

static struct ada_sprop *ada_sprop_lookup(const char *name)
{
	struct ada_sprop *sprop;
	int i;
	int j;

	for (i = 0; i < SPROP_TABLE_ENTRIES; i++) {
		for (j = 0; j < ada_sprop_table[i].entries; j++) {
			sprop = &ada_sprop_table[i].table[j];
			if (!strcmp(name, sprop->name)) {
				return sprop;
			}
		}
	}
	return NULL;
}

static enum ada_err ada_sprop_mgr_set(const char *name,
	enum ayla_tlv_type type, const void *val, size_t len,
	size_t *offset, u8 src, void *cb_arg,
	const char *ack_id, struct prop_dp_meta *metadata)
{
	struct ada_sprop *sprop;
	const size_t meta_size = sizeof(struct prop_dp_meta) * PROP_MAX_DPMETA;
	enum ada_err err;
	size_t ack_len;

	sprop = ada_sprop_lookup(name);
	if (!sprop) {
		return AE_NOT_FOUND;
	}
	if (!sprop->set) {
		return AE_RDONLY;
	}

	free(sprop->ack_id);
	sprop->ack_id = NULL;
	if (ack_id && ack_id[0]) {
		ack_len = strlen(ack_id) + 1;	/* include NUL termination */
		sprop->ack_id = al_os_mem_alloc(ack_len);
		if (sprop->ack_id) {
			memcpy(sprop->ack_id, ack_id, ack_len);
		}
	}

	if (metadata && !sprop->metadata) {
		sprop->metadata = al_os_mem_alloc(meta_size);
	}
	if (sprop->metadata) {
		if (metadata) {
			memcpy(sprop->metadata, metadata, meta_size);
		} else {
			memset(sprop->metadata, 0, meta_size);
		}
	}

	sprop->source_mask = src;
	err = sprop->set(sprop, val, len);
	if (!err && !(ada_sprop_dest_mask & NODES_ADS)) {
		/* ADS is down, remember to echo when ADS is back up */
		sprop->send_req = 1;
	}
	return err;
}

static void ada_sprop_prop_free(struct prop *prop)
{
	if (prop) {
		free(prop);
		ada_sprop_allocs--;
	}
}

static struct prop *ada_sprop_prop_alloc(size_t buf_len)
{
	struct prop *prop;

	/* TODO: consider enforcing limit on allocations, like PDA */
	prop = al_os_mem_alloc(sizeof(*prop) + buf_len);
	if (prop) {
		ada_sprop_allocs++;
		memset(prop, 0, sizeof(*prop));
		prop->mgr = &ada_sprop_mgr;
		prop->prop_mgr_done = ada_sprop_prop_free;
	}
	return prop;
}

static enum ada_err ada_sprop_mgr_get(const char *name,
	enum ada_err (*get_cb)(struct prop *, void *arg, enum ada_err),
	void *arg)
{
	struct prop *prop;
	struct ada_sprop *sprop;
	int ret;
	void *buf;
	size_t buf_len;

	sprop = ada_sprop_lookup(name);
	if (!sprop) {
		return AE_NOT_FOUND;
	}
	if (!sprop->get) {
		return AE_ERR;
	}
	buf_len = sprop->val_len + sizeof(u32);
	prop = ada_sprop_prop_alloc(buf_len);
	if (!prop) {
		return AE_ALLOC;
	}
	buf = prop + 1;

	prop->name = sprop->name;
	prop->type = sprop->type;
	prop->val = buf;

	ret = sprop->get(sprop, buf, buf_len);
	if (ret < 0) {
		ada_sprop_prop_free(prop);
		return (enum ada_err)ret;
	}
	prop->len = ret;

	ret = get_cb(prop, arg, AE_OK);
	if (ret != AE_IN_PROGRESS) {
		ada_sprop_prop_free(prop);
	}
	return (enum ada_err)ret;
}

/*
 * Send any pending echos if possible.
 * Return 0 if no echos are pending.
 * Currently only queues one per call.
 * More will be sent when that one finishes.
 */
static int ada_sprop_send_echos(void)
{
	struct ada_sprop *sprop;
	enum ada_err err;
	int i;
	int j;

	/*
	 * Send any required echos before enabling listen.
	 */
	for (i = 0; i < SPROP_TABLE_ENTRIES && !ada_sprop_echos_pending; i++) {
		for (j = 0; j < ada_sprop_table[i].entries; j++) {
			sprop = &ada_sprop_table[i].table[j];
			if (sprop->send_req) {
				sprop->send_req = 0;
				err = ada_sprop_send(sprop);
				if (err && err != AE_IN_PROGRESS) {
					sprop->send_req = 1;
					continue;
				}
				break;
			}
		}
	}
	return ada_sprop_echos_pending != 0;
}

/*
 * Call prop_mgr_ready.
 * This must be done in a callback to avoid recursively locking the client lock.
 */
static void ada_sprop_ready_cb(void *arg)
{
	if (!ada_sprop_echos_pending && (ada_sprop_dest_mask & NODES_ADS)) {
		client_unlock();
		ada_prop_mgr_ready(&ada_sprop_mgr);	/* gets client_lock */
		client_lock();
	}
}

#ifdef AYLA_FILE_PROP_SUPPORT
static void ada_sprop_file_done(struct file_dp *dp)
{
	if (dp && dp->sprop) {
		client_log(LOG_DEBUG "%s: name %s",
		    __func__, dp->sprop->name);
	}

	if (!dp) {
		return;
	}
	dp->state = FD_IDLE;
	if (sprop_file_dp_active == dp) {
		sprop_file_dp_active = NULL;
	}
	al_os_mem_free(dp->val_buf);
	dp->val_buf = NULL;
}

static void ada_sprop_file_result(struct file_dp *dp, enum ada_err err)
{
	if (dp && dp->sprop) {
		client_log(LOG_DEBUG "%s: name %s, err %d",
		    __func__, dp->sprop->name, err);
	}

	ada_sprop_file_done(dp);
	if (dp->file_result) {
		dp->file_result(dp->sprop, err);
	}
}

static enum ada_err ada_sprop_file_send(struct file_dp *dp)
{
	struct prop *prop;
	u8 eof = 0;
	int chunk_size = dp->chunk_size;
	enum ada_err ret;
	size_t len;

	if (!dp) {
		return AE_NOT_FOUND;
	}
	if ((dp->tot_len - dp->next_off) < chunk_size) {
		chunk_size = (dp->tot_len - dp->next_off);
		eof = 1;
	}

	al_os_mem_free(dp->val_buf);
	dp->val_buf = NULL;
	dp->val_buf = al_os_mem_alloc(chunk_size);
	if (!dp->val_buf) {
		return AE_ALLOC;
	}
	len = dp->file_get(dp->sprop, dp->next_off, dp->val_buf, chunk_size);
	len += sizeof(u32);
	prop = ada_sprop_prop_alloc(len);
	if (!prop) {
		return AE_ALLOC;
	}
	prop->name = dp->loc;
	prop->type = dp->sprop->type;
	prop->val = dp->val_buf;
	prop->len = chunk_size;

	ret = ada_prop_mgr_dp_put(&ada_sprop_mgr, prop,
	    dp->next_off, dp->tot_len, eof, prop);
	if (!ret || ret == AE_IN_PROGRESS) {
		ret = 0;
	} else {
		ada_sprop_prop_free(prop);
	}
	dp->next_off += chunk_size;
	return ret;
}

static struct ada_sprop *ada_sprop_allow_file_op(const char *name,
				enum ada_err *ret)
{
	struct ada_sprop *sp;

	*ret = AE_OK;
	if (sprop_file_dp_active) {
		*ret = AE_BUSY;
		return NULL;
	}
	sp = ada_sprop_lookup(name);
	if (!sp) {
		*ret = AE_NOT_FOUND;
		return NULL;
	}
#ifndef AYLA_MSG_PROP_SUPPORT
	if (sp->type != ATLV_LOC) {
		*ret = AE_INVAL_TYPE;
		return NULL;
	}
#endif
	if (!prop_type_is_file_or_msg(sp->type)) {
		*ret = AE_INVAL_TYPE;
		return NULL;
	}
	return sp;
}

/*
 * Begin file property upload with metadata.
 */
enum ada_err ada_sprop_file_start_send_with_meta(const char *name,
		size_t len, struct prop_dp_meta *meta, u8 meta_count)
{
	struct ada_sprop *sprop;
	struct file_dp *dp;
	enum ada_err err;

	sprop = ada_sprop_allow_file_op(name, &err);
	if (!sprop || !len) {
		return err;
	}

	dp = (struct file_dp *)sprop->val;
	dp->sprop = sprop;
	dp->metadata = meta;
	dp->meta_count = meta_count;
	if (dp->state == FD_IDLE) {
		dp->state = FD_CREATE;
		dp->tot_len = len;
		dp->next_off = 0;
	} else {
		return AE_BUSY;
	}
	sprop_file_dp_active = dp;
	return sprop_send_opt(sprop, 0, NODES_ADS, meta, meta_count);
}

/*
 * Begin file property upload.
 */
enum ada_err ada_sprop_file_start_send(const char *name, size_t len)
{
	return ada_sprop_file_start_send_with_meta(name, len, NULL, 0);
}

static enum ada_err ada_sprop_process_file(const char *location, u32 off,
				void *buf, size_t len, u8 eof)
{
	struct file_dp *dp = sprop_file_dp_active;
	enum ada_err err;

	if (dp->state != FD_RECV) {
		return AE_FILE;
	}
	if (strcmp(dp->loc, location)) {
		return AE_FILE;
	}
	err = dp->file_set(dp->sprop, off, buf, len, eof);
	if (err != AE_BUF && err != AE_OK) {
		return AE_FILE;
	}
	return err;
}

/*
 * Mark file datapoint fetched.
 */
enum ada_err ada_sprop_file_fetched(const char *name)
{
	struct ada_sprop *sprop;
	struct prop *prop;
	struct file_dp *dp;
	enum ada_err err = AE_OK;

	sprop = ada_sprop_lookup(name);
	if (!sprop) {
		return AE_NOT_FOUND;
	}
	if (sprop->type != ATLV_LOC) {
		return AE_INVAL_TYPE;
	}

	dp = (struct file_dp *)sprop->val;
	dp->state = FD_FETCHED;
	prop = ada_sprop_prop_alloc(0);
	if (!prop) {
		return AE_ALLOC;
	}
	sprop_file_dp_active = dp;
	prop->type = dp->sprop->type;

	err = ada_prop_mgr_dp_fetched(&ada_sprop_mgr, prop, dp->loc, prop);
	if (err == AE_IN_PROGRESS) {
		return AE_OK;
	}
	return err;
}

/*
 * Begin file property download.
 */
enum ada_err ada_sprop_file_start_recv(const char *name, const void *buf,
				size_t len, u32 off)
{
	struct ada_sprop *sprop;
	struct prop *prop;
	struct file_dp *dp;
	enum ada_err err = AE_OK;

	sprop = ada_sprop_allow_file_op(name, &err);
	if (!sprop || !buf || !len) {
		return err;
	}

	dp = (struct file_dp *)sprop->val;
	dp->sprop = sprop;
	if (dp->state != FD_IDLE) {
		return AE_BUSY;
	}

	dp->state = FD_RECV;
	dp->next_off = off;
	al_os_mem_free(dp->val_buf);
	dp->val_buf = NULL;
	dp->val_buf = al_os_mem_alloc(dp->chunk_size);
	if (!dp->val_buf) {
		client_log(LOG_ERR "%s: prop %s alloc chunk failed size %zu",
		    __func__, name, dp->chunk_size);
		return AE_ALLOC;
	}
	memcpy(dp->loc, buf, sizeof(dp->loc));
	dp->loc[len] = '\0';

	prop = ada_sprop_prop_alloc(dp->chunk_size);
	if (!prop) {
		client_log(LOG_ERR "%s: prop %s alloc prop failed size %zu",
		    __func__, name, sizeof(*prop) + dp->chunk_size);
		return AE_ALLOC;
	}
	sprop_file_dp_active = dp;

	prop->val = dp->val_buf;
	prop->type = dp->sprop->type;

	err = ada_prop_mgr_dp_get(&ada_sprop_mgr, prop, dp->loc,
	    dp->next_off, dp->chunk_size,
	    ada_sprop_process_file, prop);
	if (err == AE_IN_PROGRESS) {
		return AE_OK;
	}
	return err;
}

static enum ada_err ada_sprop_file_step(struct file_dp *dp,
	enum prop_cb_status status, u8 fail_mask, void *cb_arg)
{
	struct prop *prop = cb_arg;
	const char *name;

	if (!dp) {
		return AE_OK;
	}
	name = dp->sprop->name;

	switch (status) {
	case PROP_CB_BEGIN:
		switch (dp->state) {
		case FD_SEND:
		case FD_CREATE:
			if (cb_arg) {
				break;
			}
			/* try again after conn err */
			ada_sprop_file_done(dp);
			return ada_sprop_file_start_send_with_meta(name,
			    dp->tot_len, dp->metadata, dp->meta_count);
		case FD_RECV:
			if (cb_arg) {
				break;
			}
			/* try again after conn err */
			ada_sprop_file_done(dp);
			return ada_sprop_file_start_recv(name,
			    dp->loc, strlen(dp->loc), dp->next_off);
		case FD_FETCHED:
			if (cb_arg) {
				break;
			}
			/* try again after conn err */
			return ada_sprop_file_fetched(name);
		default:
			break;
		}
		break;
	case PROP_CB_DONE:
		switch (dp->state) {
		case FD_CREATE:
			if (prop) {
				memcpy(dp->loc, prop->val,
				    strlen(prop->val) + 1);
				dp->state = FD_SEND;
			}
			return ada_sprop_file_send(dp);
		case FD_SEND:
			client_log(LOG_DEBUG "%s: uploaded \"%s\"",
			    __func__, name);
			ada_sprop_file_result(dp, AE_OK);
			break;
		case FD_ABORT:
			client_log(LOG_DEBUG "%s: aborted \"%s\"",
			    __func__, name);
			ada_sprop_file_result(dp, AE_ABRT);
			break;
		case FD_RECV:
			client_log(LOG_DEBUG "%s: recv completed \"%s\"",
			    __func__, name);
			break;
		case FD_FETCHED:
			client_log(LOG_DEBUG "%s: fetched \"%s\"",
			    __func__, name);
			ada_sprop_file_result(dp, AE_OK);
			break;
		default:
			break;
		}
		break;
	case PROP_CB_CONTINUE:
		if (dp->state == FD_SEND) {
			/* continue sends */
			ada_sprop_prop_free(prop);
			return ada_sprop_file_send(dp);
		}
		break;
	case PROP_CB_CONN_ERR:
	case PROP_CB_CONN_ERR2:
		client_log(LOG_ERR "%s: transfer of \"%s\" failed. "
		   "status %d mask %x",
		    __func__, name, status, fail_mask);
		switch (dp->state) {
		case FD_SEND:
		case FD_CREATE:
		case FD_RECV:
		case FD_FETCHED:
			dp->sprop->send_req = 1;
		default:
			break;
		}
		ada_sprop_file_result(dp, AE_NOTCONN);
		break;
	default:
		client_log(LOG_ERR "%s: transfer of \"%s\" failed. "
		   "status %d mask %x",
		    __func__, name, status, fail_mask);
		ada_sprop_file_result(dp, AE_ERR);
		break;
	}

	ada_sprop_prop_free(prop);
	return 0;
}

/*
 * Abort ongoing file transfer.
 */
void ada_sprop_file_abort(void)
{
	if (sprop_file_dp_active) {
		sprop_file_dp_active->state = FD_ABORT;
		sprop_file_dp_active->aborted = 1;
		ada_prop_mgr_dp_abort(sprop_file_dp_active->loc);
	}
}
#endif

static void ada_sprop_mgr_send_done(enum prop_cb_status status,
	u8 fail_mask, void *cb_arg)
{
	struct prop *prop = cb_arg;
	struct ada_sprop *sprop;
	u8 dests = client_valid_dest_mask();

#ifdef AYLA_FILE_PROP_SUPPORT
	if (sprop_file_dp_active && prop_is_file_or_msg(prop)) {
		/*
		 * Handle file property completions. No echoes.
		 */
		ada_sprop_file_step(sprop_file_dp_active, status, fail_mask,
		    cb_arg);
		return;
	}
#endif

	if (status != PROP_CB_DONE) {
		client_log(LOG_ERR "%s: send of \"%s\" failed. "
		   "status %d mask %x",
		    __func__, prop->name, status, fail_mask);

		/*
		 * If we were echoing and have lost connectivity with ADS,
		 * re-pend the echo.  Otherwise, just let it fail.
		 */
		if (prop->echo && (fail_mask & NODES_ADS) &&
		    (status == PROP_CB_CONN_ERR ||
		    status == PROP_CB_CONN_ERR2)) {
			sprop = ada_sprop_lookup(prop->name);
			if (sprop) {
				sprop->send_req = 1;
			}
			dests &= ~fail_mask;
		}
	} else {
		client_log(LOG_DEBUG2 "%s: sent \"%s\"", __func__, prop->name);
	}

	if (prop->prop_mgr_echo) {
		ASSERT(ada_sprop_echos_pending);
		ada_sprop_echos_pending--;

		/*
		 * If there are more echos to send, send them, otherwise
		 * declare sprop ready so listen is enabled.
		 */
		if ((dests & NODES_ADS) && !ada_sprop_send_echos()) {
			client_callback_pend(&ada_sprop_ready_callback);
		}
	}

	ada_sprop_prop_free(prop);
}

static void ada_sprop_mgr_connect_sts(u8 mask)
{
	ada_sprop_dest_mask = mask;
	if ((mask & NODES_ADS) && !ada_sprop_send_echos()) {
		ada_prop_mgr_ready(&ada_sprop_mgr);
	}
}

static void ada_sprop_mgr_event(enum prop_mgr_event event, const void *arg)
{
	struct ada_sprop *sprop;
	u8 mask;
#ifdef AYLA_FILE_PROP_SUPPORT
	struct file_dp *dp = sprop_file_dp_active;
	const char *loc;
#endif

	switch (event) {
	case PME_DISCONNECT:
		mask = (u8)(ptrdiff_t)arg;
		ada_sprop_dest_mask &= ~mask;
		break;
	case PME_ECHO_FAIL:
		client_log(LOG_WARN "Failed echoing %s to ADS", (char *)arg);
		sprop = ada_sprop_lookup((const char *)arg);
		if (!sprop) {
			return;
		}
		sprop->send_req = 1;
		break;
#ifdef AYLA_FILE_PROP_SUPPORT
	case PME_FILE_DONE:
		/* only meant for file or message downloads */
		loc = (const char *)arg;
		if (loc && dp && !strcmp(dp->loc, loc)) {
			if (dp->sprop->type == ATLV_LOC) {
				ada_sprop_file_fetched(dp->sprop->name);
			} else {
				ada_sprop_file_result(dp, AE_OK);
			}
		}
		break;
#endif
	default:
		break;
	}
}

static struct prop_mgr ada_sprop_mgr = {
	.name = "sprop",
	.prop_meta_recv = ada_sprop_mgr_set,
	.get_val = ada_sprop_mgr_get,
	.send_done = ada_sprop_mgr_send_done,
	.connect_status = ada_sprop_mgr_connect_sts,
	.event = ada_sprop_mgr_event,
};

/*
 * Get an ATLV_INT or ATLV_CENTS type property from the
 * sprop structure.
 */
ssize_t ada_sprop_get_int(struct ada_sprop *sprop, void *buf, size_t len)
{
	if (!sprop || !buf) {
		return AE_ERR;
	}
	if (sprop->type != ATLV_INT && sprop->type != ATLV_CENTS) {
		return AE_INVAL_TYPE;
	}
	if (len < sizeof(s32)) {
		return AE_LEN;
	}

	switch (sprop->val_len) {
	case 1:
		*(s32 *)buf = *(s8 *)sprop->val;
		break;
	case 2:
		*(s32 *)buf = *(s16 *)sprop->val;
		break;
	case 4:
		*(s32 *)buf = *(s32 *)sprop->val;
		break;
	default:
		return AE_ERR;
	}

	return sizeof(s32);
}

/*
 * Get an ATLV_UINT type property from the sprop structure.
 */
ssize_t ada_sprop_get_uint(struct ada_sprop *sprop, void *buf, size_t len)
{
	if (!sprop || !buf) {
		return AE_ERR;
	}
	if (sprop->type != ATLV_UINT) {
		return AE_INVAL_TYPE;
	}
	if (len < sizeof(u32)) {
		return AE_LEN;
	}

	switch (sprop->val_len) {
	case 1:
		*(u32 *)buf = *(u8 *)sprop->val;
		break;
	case 2:
		*(u32 *)buf = *(u16 *)sprop->val;
		break;
	case 4:
		*(u32 *)buf = *(u32 *)sprop->val;
		break;
	default:
		return AE_ERR;
	}

	return sizeof(u32);
}


/*
 * Get an ATLV_BOOL type property from the sprop structure.
 */
ssize_t ada_sprop_get_bool(struct ada_sprop *sprop, void *buf, size_t len)
{
	if (!sprop || !buf) {
		return AE_ERR;
	}
	if (sprop->type != ATLV_BOOL) {
		return AE_INVAL_TYPE;
	}
	if (len < sizeof(u8)) {
		return AE_LEN;
	}

	*(u8 *)buf = (*(u8 *)sprop->val != 0);
	return sizeof(u8);
}

/*
 * Get an ATLV_UTF8 type property from the sprop structure.
 */
ssize_t ada_sprop_get_string(struct ada_sprop *sprop, void *buf, size_t len)
{
	size_t val_len;

	if (!sprop) {
		return AE_ERR;
	}

	if (sprop->type != ATLV_UTF8) {
		return AE_INVAL_TYPE;
	}

	val_len = strlen(sprop->val);
	if (val_len + 1 > len) {
		return AE_LEN;
	}
	memcpy(buf, sprop->val, val_len + 1);	/* copy includes NUL term */
	return val_len;
}

/*
 * Set an ATLV_INT or ATLV_CENTS property value to the
 * value in *buf.
 */
enum ada_err ada_sprop_set_int(struct ada_sprop *sprop,
				const void *buf, size_t len)
{
	s32 val;

	if (!sprop || !buf) {
		return AE_ERR;
	}
	if (len != sizeof(s32)) {
		return AE_LEN;
	}
	if (sprop->type != ATLV_INT && sprop->type != ATLV_CENTS) {
		return AE_INVAL_TYPE;
	}

	val = *(s32 *)buf;

	switch (sprop->val_len) {
	case 1:
		if (val < MIN_S8 || val > MAX_S8) {
			return AE_INVAL_VAL;
		}
		*(s8 *)sprop->val = val;
		break;
	case 2:
		if (val < MIN_S16 || val > MAX_S16) {
			return AE_INVAL_VAL;
		}
		*(s16 *)sprop->val = val;
		break;
	case 4:
		*(s32 *)sprop->val = *(s32 *)buf;
		break;
	default:
		return AE_LEN;
	}
	return AE_OK;
}

/*
 * Set an ATLV_UINT property value to the value in *buf.
 */
enum ada_err ada_sprop_set_uint(struct ada_sprop *sprop,
				const void *buf, size_t len)
{
	u32 val;

	if (!sprop || !buf) {
		return AE_ERR;
	}
	if (sprop->type != ATLV_UINT) {
		return AE_INVAL_TYPE;
	}
	if (len != sizeof(u32)) {
		return AE_LEN;
	}
	val = *(u32 *)buf;

	switch (sprop->val_len) {
	case 1:
		if (val > MAX_U8) {
			return AE_INVAL_VAL;
		}
		*(u8 *)sprop->val = val;
		break;
	case 2:
		if (val > MAX_U16) {
			return AE_INVAL_VAL;
		}
		*(u16 *)sprop->val = val;
		break;
	case 4:
		*(u32 *)sprop->val = val;
		break;
	default:
		return AE_LEN;
	}
	return AE_OK;
}


/*
 * Set an ATLV_BOOL property value to the value in *buf.
 */
enum ada_err ada_sprop_set_bool(struct ada_sprop *sprop,
				const void *buf, size_t len)
{
	u32 val;

	if (!sprop || !buf) {
		return AE_ERR;
	}
	if (sprop->type != ATLV_BOOL) {
		return AE_INVAL_TYPE;
	}
	if (len != sizeof(u32)) {
		return AE_LEN;
	}
	val = *(u32 *)buf;
	if (val > 1) {
		return AE_INVAL_VAL;
	}
	if (sprop->val_len != sizeof(u8)) {
		return AE_LEN;
	}
	*(u8 *)sprop->val = val;
	return AE_OK;
}

/*
 * Set an ATLV_UTF8 property value to the value in *buf.
 */
enum ada_err ada_sprop_set_string(struct ada_sprop *sprop,
				const void *buf, size_t len)
{
	size_t val_len;

	if (!sprop || !buf) {
		return AE_ERR;
	}
	if (sprop->type != ATLV_UTF8) {
		return AE_INVAL_TYPE;
	}
	val_len = strnlen(buf, len);
	if (val_len + 1 > sprop->val_len || val_len > TLV_MAX_STR_LEN) {
		return AE_LEN;
	}
	memcpy(sprop->val, buf, val_len);
	((char *)sprop->val)[val_len] = '\0';
	return AE_OK;
}

/*
 * Send Property Ack.
 * The ack goes to the source which delivered the ack_id, either ADS or LAN.
 */
enum ada_err ada_sprop_send_ack(struct ada_sprop *sprop, u8 status,
				int ack_message)
{
	struct prop *prop;
	enum ada_err err;

	if (!sprop) {
		return AE_INVAL_STATE;
	}

	if (!sprop->ack_id) {
		return AE_NOT_FOUND;
	}

	prop = ada_sprop_prop_alloc(sizeof(struct prop_ack));
	if (!prop) {
		return AE_ALLOC;
	}
	prop->ack = (struct prop_ack *)(prop + 1);

	prop->name = sprop->name;
	prop->type = ATLV_ACK_ID;
	prop->send_done_arg = prop;
	prop->send_dest = sprop->source_mask;
	prop->ack->src = sprop->source_mask;
	prop->ack->status = status;
	prop->ack->msg = ack_message;

	snprintf(prop->ack->id, sizeof(prop->ack->id), "%s", sprop->ack_id);

	err = ada_prop_mgr_prop_send(prop);
	if (err != AE_OK && err != AE_IN_PROGRESS) {
		ada_sprop_prop_free(prop);
		return err;
	}
	return AE_OK;
}

/*
 * Send property update, possibly for echo.
 */
static enum ada_err sprop_send_opt(struct ada_sprop *sprop, int echo, u8 dests,
				struct prop_dp_meta *metadata, u8 meta_count)
{
	struct prop *prop;
	size_t len;
	ssize_t ret;
	enum ada_err err;
	u8 connected_dests = ada_sprop_dest_mask;

	if (!sprop) {
		return AE_INVAL_STATE;
	}
	if (!sprop->get && !prop_type_is_file_or_msg(sprop->type)) {
		return AE_ERR;
	}

	/*
	 * Ignore LANs and local control clients that aren't connected.
	 */
	dests &= (connected_dests & (NODES_LOCAL)) | NODES_ADS;

	/*
	 * Properties with a set handler should be to-device properties.
	 * Set the echo flag for them.
	 */
	if (sprop->set) {
		echo = 1;
	}

	/*
	 * If sending an echo to ADS, but ADS is not connected, set the
	 * send_req flag.  We'll send the echo when we connect, and not
	 * queue the current value.
	 */
	if (echo) {
		if ((dests & NODES_ADS) && !(connected_dests & NODES_ADS)) {
			sprop->send_req = 1;
			if (!(dests & (NODES_LOCAL))) {
				return AE_IN_PROGRESS;
			}
		}
		if (!dests) {
			return AE_OK;
		}
	}

	len = sprop->val_len + sizeof(u32);
	if (metadata) {
		if (meta_count > PROP_MAX_DPMETA || meta_count == 0) {
			client_log(LOG_ERR "%s: count \"%u\" is invalid",
			    __func__, meta_count);
			return AE_INVAL_VAL;
		}
		len += sizeof(struct prop_dp_meta) * PROP_MAX_DPMETA;
	}
	prop = ada_sprop_prop_alloc(len);
	if (!prop) {
		return AE_ALLOC;
	}
	prop->name = sprop->name;
	prop->type = sprop->type;
	prop->val = prop + 1;

	/*
	 * For proper alignment, put metadata first after prop struct.
	 */
	if (metadata) {
		prop->dp_meta = (struct prop_dp_meta *)(prop + 1);
		prop->val = (char *)(prop + 1) +
		    sizeof(struct prop_dp_meta) * PROP_MAX_DPMETA;
		memcpy(prop->dp_meta, metadata,
		    sizeof(struct prop_dp_meta) * meta_count);
		memset(prop->dp_meta + meta_count, 0,
		    sizeof(struct prop_dp_meta) *
		    (PROP_MAX_DPMETA - meta_count));
	}

	if (!prop_type_is_file_or_msg(sprop->type)) {
		ret = sprop->get(sprop, prop->val, len);
		if (ret < 0) {
			ada_sprop_prop_free(prop);
			return (enum ada_err)ret;
		}
		ASSERT(ret <= len);
		prop->len = ret;
	} else {
		prop->val = NULL;
		dests = NODES_ADS;
	}

	if (echo) {
		prop->echo = 1;
		prop->prop_mgr_echo = 1;
		ada_sprop_echos_pending++;
	}

	err = ada_prop_mgr_send(&ada_sprop_mgr, prop, dests, prop);
	if (err != AE_OK && err != AE_IN_PROGRESS) {
		if (echo) {
			ASSERT(ada_sprop_echos_pending);
			ada_sprop_echos_pending--;
		}
		ada_sprop_prop_free(prop);
		return err;
	}
	return AE_OK;
}

enum ada_err ada_sprop_send(struct ada_sprop *sprop)
{
#ifdef AYLA_FILE_PROP_SUPPORT
	if (prop_type_is_file_or_msg(sprop->type)) {
		return ada_sprop_file_step(sprop_file_dp_active,
		    PROP_CB_BEGIN, 0, NULL);
	}
#endif
	return sprop_send_opt(sprop, 0, NODES_ALL, NULL, 0);
}

enum ada_err ada_sprop_send_with_meta(struct ada_sprop *sprop,
		struct prop_dp_meta *metadata, u8 count)
{
	if (!metadata) {
		return AE_ERR;
	}
	return sprop_send_opt(sprop, 0, NODES_ALL, metadata, count);
}

/*
 * Send to specified destinations only.
 */
enum ada_err ada_sprop_send_to(struct ada_sprop *sprop, u8 dests)
{
	return sprop_send_opt(sprop, 0, dests, NULL, 0);
}

/*
 * Send echo to all specified destinations except the source of the last set.
 */
enum ada_err ada_sprop_send_echo(struct ada_sprop *sprop, u8 dests)
{
	if (sprop->source_mask == NODES_SCHED) {
		return AE_OK;		/* handled separately by sprop_set() */
	}
	dests &= ~sprop->source_mask & NODES_ALL;
	if (~dests) {
		return AE_OK;
	}
	if (!sprop->set) {
		return AE_RDONLY;
	}
	return sprop_send_opt(sprop, 1, dests, NULL, 0);
}

/*
 * Send echo to specified destinations only.
 */
enum ada_err ada_sprop_send_echo_by_name(const char *name, u8 dests)
{
	struct ada_sprop *sprop;

	sprop = ada_sprop_lookup(name);
	if (!sprop) {
		return AE_NOT_FOUND;
	}
	return ada_sprop_send_echo(sprop, dests);
}

/*
 * Send to specified destinations only.
 */
enum ada_err ada_sprop_send_to_by_name(const char *name, u8 dests)
{
	struct ada_sprop *sprop;

	sprop = ada_sprop_lookup(name);
	if (!sprop) {
		return AE_NOT_FOUND;
	}
	return sprop_send_opt(sprop, 0, dests, NULL, 0);
}

/*
 * Send by name.
 */
enum ada_err ada_sprop_send_by_name(const char *name)
{
	return ada_sprop_send_to_by_name(name, NODES_ALL);
}

enum ada_err ada_sprop_send_by_name_with_meta(const char *name,
			struct prop_dp_meta *metadata, u8 count)
{
	struct ada_sprop *sprop;

	sprop = ada_sprop_lookup(name);
	if (!sprop) {
		return AE_NOT_FOUND;
	}
	return ada_sprop_send_with_meta(sprop, metadata, count);
}

static int ada_sprop_table_check(const char *table_name,
		const struct ada_sprop *table, unsigned int entries)
{
	const struct ada_sprop *sprop = table;
	unsigned int i;
	int rc = 0;

	for (i = 0; i < entries; i++, sprop++) {
		if (!sprop->name) {
			client_log(LOG_ERR "sprop: table %s: missing prop name "
			   "entry %u", table_name, i);
			rc = -1;
		}
		if (!prop_type_is_file_or_msg(sprop->type) && !sprop->get) {
			client_log(LOG_ERR "sprop: table %s prop %s: "
			   "no get function", table_name, sprop->name);
			rc = -1;
		}
	}
	return rc;
}

/*
 * Register a table of properties to the sprop prop_mgr.
 */
enum ada_err ada_sprop_mgr_register(char *name, struct ada_sprop *table,
		unsigned int entries)
{
	static int i;

	if (!name || !table || ada_sprop_table_check(name, table, entries)) {
		return AE_ERR;
	}

	if (i >= SPROP_TABLE_ENTRIES) {
		client_log(LOG_ERR "sprop table limit reached");
		return AE_ERR;
	}

	if (i == 0) {
		callback_init(&ada_sprop_ready_callback,
		    ada_sprop_ready_cb, NULL);
		ada_prop_mgr_register(&ada_sprop_mgr);
	}

	strncpy(ada_sprop_table[i].name, name, SPROP_NAME_MAX_LEN);
	ada_sprop_table[i].table = table;
	ada_sprop_table[i].entries = entries;
	i++;

	return AE_OK;
}

#ifdef AYLA_FILE_PROP_SUPPORT
void ada_sprop_file_init_v2(struct file_dp *dp,
	size_t (*file_get)(struct ada_sprop *sprop, size_t off,
	    void *buf, size_t len),
	enum ada_err (*file_set)(struct ada_sprop *sprop, size_t off,
	    void *buf, size_t len, u8 eof),
	void (*file_result)(struct ada_sprop *sprop, enum ada_err err),
	size_t chunk_size)
{
	dp->state = FD_IDLE;
	dp->file_get = file_get;
	dp->file_set = file_set;
	dp->file_result = file_result;
	dp->chunk_size = chunk_size ? chunk_size : SPROP_DEF_FILECHUNK_SIZE;
	dp->val_buf = NULL;
}

/*
 * Deprecated interface kept for source compatibility.
 */
void ada_sprop_file_init(struct file_dp *dp,
	size_t (*file_get)(struct ada_sprop *sprop, size_t off,
	    void *buf, size_t len),
	enum ada_err (*file_set)(struct ada_sprop *sprop, size_t off,
	    void *buf, size_t len, u8 eof),
	size_t chunk_size)
{
	ada_sprop_file_init_v2(dp, file_get, file_set, NULL, chunk_size);
}

enum ada_err ada_sprop_file_alloc(const char *name,
	size_t (*file_get)(struct ada_sprop *sprop, size_t off,
	    void *buf, size_t len),
	enum ada_err (*file_set)(struct ada_sprop *sprop, size_t off,
	    void *buf, size_t len, u8 eof),
	void (*file_result)(struct ada_sprop *sprop, enum ada_err err),
	size_t chunk_size)
{
	struct ada_sprop *sprop;
	struct file_dp *dp;

	sprop = ada_sprop_lookup(name);
	if (!sprop) {
		return AE_NOT_FOUND;
	}

	if (!prop_type_is_file_or_msg(sprop->type) || sprop->val) {
		return AE_INVAL_VAL;
	}

	dp = al_os_mem_calloc(sizeof(*dp));
	if (!dp) {
		return AE_ALLOC;
	}

	sprop->val = dp;
	ada_sprop_file_init_v2(dp, file_get, file_set, file_result, chunk_size);

	return AE_OK;
}
#endif

#ifdef AYLA_BATCH_PROP_SUPPORT
int ada_batch_add_prop(struct batch_ctx *ctx, struct ada_sprop *sprop,
	s64 t_stamp)
{
	struct prop *prop;
	size_t len;
	ssize_t ret;
	int echo = 0;
	u8 dest = ada_sprop_dest_mask;
	int batch_id;

	if (!ctx) {
		return AE_INVAL_VAL;
	}
	if (!sprop) {
		return AE_INVAL_VAL;
	}
	if (prop_type_is_file_or_msg(sprop->type)) {
		return AE_INVAL_TYPE;
	}
	if (sprop->val == NULL) {
		/* getting prop is not supported */
		return AE_ERR;
	}
	if (!sprop->get) {
		return AE_ERR;
	}

	if (sprop->set) {
		echo = 1;
	}
	if (echo && !(dest & NODES_ADS)) {
		sprop->send_req = 1;
		if (!dest) {
			return AE_ERR;
		}
	}
	dest |= NODES_ADS;

	if (sprop->type == ATLV_UTF8) {
		len = strlen(sprop->val)  + sizeof(u32);
	} else {
		len = sprop->val_len + sizeof(u32);
	}
	prop = ada_sprop_prop_alloc(len);
	if (!prop) {
		return AE_ALLOC;
	}
	prop->name = sprop->name;
	prop->type = sprop->type;
	prop->val = prop + 1;
	prop->echo = echo;
	prop->send_dest = dest;
	prop->mgr = &ada_sprop_mgr;
	prop->send_done_arg = prop;

	ret = sprop->get(sprop, prop->val, len);
	if (ret < 0) {
		ada_sprop_prop_free(prop);
		return AE_ERR;
	}
	ASSERT(ret <= len);
	prop->len = ret;
	batch_id = batch_add_prop(ctx, prop, t_stamp);
	if (batch_id <= 0) {
		ada_sprop_prop_free(prop);
	}
	return batch_id;
}

int ada_batch_add_prop_by_name(struct batch_ctx *ctx, const char *name,
	s64 t_stamp)
{
	struct ada_sprop *sprop;

	if (!ctx) {
		return AE_INVAL_VAL;
	}
	if (!name) {
		return AE_INVAL_VAL;
	}
	sprop = ada_sprop_lookup(name);
	if (!sprop) {
		return AE_NOT_FOUND;
	}
	return ada_batch_add_prop(ctx, sprop, t_stamp);
}
#endif /* AYLA_BATCH_PROP_SUPPORT */

