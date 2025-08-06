/*
 * Copyright 2011-2015 Ayla Networks, Inc.  All rights reserved.
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
#include <ayla/timer.h>
#include <ayla/tlv.h>
#include <ayla/clock.h>
#include <ayla/conf.h>
#include <ayla/crc.h>
#include <ayla/log.h>
#include <ayla/mod_log.h>
#include <ayla/http.h>
#include <ayla/base64.h>

#include <al/al_os_mem.h>
#include <ada/ada_conf.h>
#include <ada/prop.h>
#include <ada/prop_mgr.h>
#include <ada/client.h>
#include <ada/batch.h>
#include "batch_int.h"
#include "client_lock.h"
#include "client_timer.h"
#include "client_batch.h"

/*
 * Batch context and send timer
 */
static struct timer client_batch_send_timer;
static struct batch_ctx *client_batch_ctx;

/*
 * Batch configuration
 */
#define CLIENT_BATCH_ENABLED	1
#define CLIENT_BATCH_MAX_DPS	64
#define CLIENT_BATCH_MAX_SIZE	2000	/* bytes */
#define CLIENT_BATCH_SEND_DELAY	2000	/* ms */

struct client_batch_conf {
	u8 enabled;		/* 1 if batching to ADS is enabled */
	u16 max_dps;
	int max_size;
	unsigned long delay_ms;
};
static struct client_batch_conf client_batch_conf;

/*
 * Property manager to be notified of send completion
 */
static struct prop_mgr client_batch_prop_mgr;

static const struct ada_conf_item client_batch_conf_items[] = {
	{ "client/batch/enabled", ATLV_BOOL, &client_batch_conf.enabled,
	    sizeof(client_batch_conf.enabled)},
	{ "client/batch/max_dps", ATLV_UINT, &client_batch_conf.max_dps,
	    sizeof(client_batch_conf.max_dps)},
	{ "client/batch/max_sz", ATLV_UINT, &client_batch_conf.max_size,
	    sizeof(client_batch_conf.max_size)},
	{ "client/batch/dly_ms", ATLV_UINT, &client_batch_conf.delay_ms,
	    sizeof(client_batch_conf.delay_ms)},
	{ NULL }
};

/*
 * Client batch configuration: load configuration from flash
 */
void client_batch_conf_load(void)
{
	const struct ada_conf_item *item;
	struct client_batch_conf *cbf = &client_batch_conf;

	client_log(LOG_DEBUG "%s", __func__);

	cbf->enabled = CLIENT_BATCH_ENABLED;
	cbf->max_dps = CLIENT_BATCH_MAX_DPS;
	cbf->max_size = CLIENT_BATCH_MAX_SIZE;
	cbf->delay_ms = CLIENT_BATCH_SEND_DELAY;

	for (item = client_batch_conf_items; item->name; item++) {
		ada_conf_get_item(item);
	}
	client_batch_info();
}

/*
 * Client batch configuration: persist configuration to flash
 */
void client_batch_conf_export(void)
{
	const struct ada_conf_item *item;

	client_log(LOG_DEBUG "%s", __func__);

	for (item = client_batch_conf_items; item->name; item++) {
		ada_conf_export_item(item);
	}
}

/*
 * Client batch configuration: displaying client batch settings
 */
void client_batch_info(void)
{
	struct client_batch_conf *cbf = &client_batch_conf;

	printcli("client: batch max_dps=%d max_size=%d delay_ms=%ld %s",
	    cbf->max_dps, cbf->max_size, cbf->delay_ms,
	    cbf->enabled ? "enabled" : "disabled");
}

/*
 * Client batch configuration: CLI configuration
 */
enum ada_err client_batch_cli(int argc, char **argv)
{
	client_log(LOG_DEBUG "%s: argc=%d argv=0x%p", __func__, argc, argv);

	if (argc >= 2 && !strcmp(argv[1], "batch")) {
		struct client_batch_conf *cbf = &client_batch_conf;

		if (argc == 2) {
			client_batch_info();
			return AE_OK;
		}
		if (argc == 3) {
			if (!strcmp(argv[2], "enable")) {
				cbf->enabled = 1;
			} else if (!strcmp(argv[2], "disable")) {
				cbf->enabled = 0;
			} else {
				goto batch_usage;
			}
			return AE_OK;
		}
		if (argc == 4) {
			unsigned long val;
			char *errptr;

			val = strtoul(argv[3], &errptr, 10);
			if (*errptr != '\0') {
				goto batch_usage;
			}

			if (!strcmp(argv[2], "max_dps")) {
				cbf->max_dps = val;
			} else if (!strcmp(argv[2], "max_size")) {
				cbf->max_size = val;
			} else if (!strcmp(argv[2], "delay_ms")) {
				cbf->delay_ms = val;
			} else {
				goto batch_usage;
			}
			return AE_OK;
		}
batch_usage:
		printcli("usage:");
		printcli("client batch [enable|disable]");
		printcli("client batch max_dps <num>");
		printcli("client batch max_size <bytes>");
		printcli("client batch delay_ms <msec>");
		return AE_OK;
	}

	/*
	 * Returns non-zero if not consumed/processed
	 */
	return AE_NOT_FOUND;
}

/*
 * client_batch_send_batch_done
 */
static void client_batch_send_batch_done(size_t size_sent)
{
	client_log(LOG_DEBUG "%s: size_sent=%d", __func__, size_sent);

	/* Free the ctx here? */
}

/*
 * client_batch_add
 *
 * Adds the datapoint passed as arg to the batch. This routine should
 * only be called if client batching is enabled.
 *
 * Returns AE_OK on success, or specific error if datapoint was not
 * created or retained.
 */
enum ada_err client_batch_add(const char *name, enum ayla_tlv_type type,
    const void *val, size_t val_len, u8 echo, struct prop_dp_meta *metadata)
{
	struct client_batch_conf *cbf = &client_batch_conf;
	struct prop *prop;
	int batch_id;
	enum ada_err err;

	/*
	 * Convert args to a prop to add to the batch
	 */
	prop = prop_create(name, type, val, val_len, echo, metadata);
	if (!prop) {
		client_log(LOG_ERR "%s: prop alloc failed", __func__);
		return AE_ALLOC;
	}
	prop->send_dest = NODES_ADS;
	prop->mgr = &client_batch_prop_mgr;
	prop->send_done_arg = prop;

	/*
	 * Create the batch if required
	 */
	if (!client_batch_ctx) {
		client_batch_ctx = ada_batch_create(cbf->max_dps,
		    cbf->max_size);
		if (!client_batch_ctx) {
			al_os_mem_free(prop);
			client_log(LOG_ERR "%s: batch alloc failed", __func__);
			return AE_ALLOC;
		}
	}

	/*
	 * Add the property to the batch.
	 */
retry:
	batch_id = batch_add_prop(client_batch_ctx, prop, 0);
	if (batch_id == AE_BUF) {
		/*
		 * No room. Send the batch and retry this property
		 */
		err = ada_batch_send(client_batch_ctx,
		    client_batch_send_batch_done);
		if (err == AE_OK) {
			client_timer_cancel(&client_batch_send_timer);
			client_log(LOG_DEBUG "%s: full batch send ok",
			    __func__);
			goto retry;
		} else {
			/*
			 * Failed to send; keep current batch but return error
			 * on current prop
			 */
			al_os_mem_free(prop);
			client_log(LOG_ERR "%s: full batch send failed",
			    __func__);
			return AE_BUF;
		}
	}

	/*
	 * If batch has max datapoints, send it
	 */
	if (ada_batch_is_full(client_batch_ctx)) {
		err = ada_batch_send(client_batch_ctx,
		    client_batch_send_batch_done);
		if (err == AE_OK) {
			client_timer_cancel(&client_batch_send_timer);
			ada_batch_destroy(client_batch_ctx);
			client_batch_ctx = NULL;
			client_log(LOG_DEBUG "%s: send ok", __func__);
		} else {
			client_log(LOG_ERR "%s: max dps send failed rc=%d",
			    __func__, err);
		}
		return err;
	} else {
		/*
		 * Restart the batch send timer
		 */
		client_timer_set(&client_batch_send_timer, cbf->delay_ms);
	}
	client_log(LOG_DEBUG "%s: add prop ok", __func__);
	return AE_OK;
}

u8 client_batch_enabled(void)
{
	client_log(LOG_DEBUG "%s: returned %d", __func__,
	    client_batch_conf.enabled == 1);
	return (client_batch_conf.enabled == 1);
}

/*
 * Reset client batching state
 */
void client_batch_reset(void)
{
	client_log(LOG_DEBUG "%s", __func__);

	client_timer_cancel(&client_batch_send_timer);
	if (client_batch_ctx) {
		ada_batch_destroy(client_batch_ctx);
		client_batch_ctx = NULL;
	}
}

static void client_batch_connect_sts(u8 mask)
{
	static u8 old_mask;

	client_log(LOG_DEBUG "%s: mask=0x%x old_mask=0x%x",
	    __func__, mask, old_mask);

	if ((mask ^ old_mask) & NODES_ADS) {
		if (mask & NODES_ADS) {
			ada_prop_mgr_ready(&client_batch_prop_mgr);
		} else {
			client_lock();
			client_batch_reset();
			client_unlock();
		}
	}
	old_mask = mask;
}

static void client_batch_send_done(enum prop_cb_status status,
    u8 fail_mask, void *cb_arg)
{
	struct prop *prop = (struct prop *)cb_arg;

	if (prop == NULL) {
		client_log(LOG_ERR "%s: NULL prop unexpected", __func__);
		return;
	}

	if (status != PROP_CB_DONE) {
		client_log(LOG_ERR "%s: send of \"%s\" failed. "
		    "status=%d mask=0x%x",
		    __func__, prop->name, status, fail_mask);
	} else {
		client_log(LOG_DEBUG "%s: send of \"%s\" done. "
		    "status=%d mask=0x%x",
		    __func__, prop->name, status, fail_mask);
	}
	al_os_mem_free(prop);
}

static struct prop_mgr client_batch_prop_mgr = {
	.name = "client_batch",
	.send_done = client_batch_send_done,
	.connect_status = client_batch_connect_sts,
};

static void client_batch_send_timeout(struct timer *tm)
{
	struct client_batch_conf *cbf = &client_batch_conf;
	enum ada_err err;

	client_log(LOG_DEBUG "%s", __func__);

	if (client_batch_ctx) {
		err = ada_batch_send(client_batch_ctx,
		    client_batch_send_batch_done);
		if (err == AE_OK) {
			ada_batch_destroy(client_batch_ctx);
			client_batch_ctx = NULL;
		} else {
			client_timer_set(&client_batch_send_timer,
			    cbf->delay_ms / 8);
		}
	}
}

/*
 * Initialize batching of ADA updates
 */
void client_batch_init(void)
{
	client_log(LOG_DEBUG "%s", __func__);

	timer_handler_init(&client_batch_send_timer, client_batch_send_timeout);
	ada_prop_mgr_register(&client_batch_prop_mgr);
}
