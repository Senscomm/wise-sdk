/*
 * Copyright 2013-2015 Ayla Networks, Inc.  All rights reserved.
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
#include <ada/err.h>
#include <ayla/tlv.h>
#include <ayla/base64.h>
#include <ayla/log.h>
#include <ayla/mod_log.h>
#include <ayla/conf_token.h>
#include <ayla/conf.h>
#include <ayla/json.h>
#include <ayla/clock.h>
#include <ayla/parse.h>
#include <ayla/timer.h>
#include <jsmn.h>
#include <ayla/jsmn_get.h>
#include <ayla/http.h>
#include <ada/client.h>
#include <ada/ada_conf.h>
#include <ada/client_ota.h>
#include <ada/prop.h>
#include <ada/prop_mgr.h>
#include <ada/server_req.h>
#include "client_int.h"
#include "client_lock.h"
#include "ada_lock.h"
#include "schedeval.h"
#ifdef AYLA_MATTER_SUPPORT
#include <al/al_matter.h>
#endif
#ifdef AYLA_BLUETOOTH_SUPPORT
#include <adb/adb.h>
#include <al/al_bt.h>
#endif

#define CONFIG_JSON_PUT_TOKENS	40
#define CONFIG_TOK_LEN 10
#define CONFIG_PATH_LEN 100
#define CONFIG_VAL_LEN 200

struct ada_conf ada_conf;
#if defined(AYLA_LAN_SUPPORT) || defined(AYLA_LOCAL_CONTROL_SUPPORT)
struct ada_lan_conf ada_lan_conf;
#endif

char conf_sys_dev_id[CONF_DEV_ID_MAX];
u8 ada_conf_reset_factory;
struct callback ada_conf_reset_cb;
u8 conf_was_reset = 1;

struct ada_conf_ctx {
	u8 busy:1;		/* 1 indicates in use */
	u8 dryrun:1;		/* 1 if this is just a dry run */
	u8 config_changed:1;	/* 1 if the conf was changed */
	u8 reset_req:1;		/* 1 if reset required after conf update */
	u8 reset_factory:1;	/* 1 if reset to factory settings requested */
	u8 time_update:1;	/* 1 if time settings were updated */
	u8 save_setup:1;	/* 1 if setup and mfg mode should be saved */
	u8 save_default:1;	/* 1 if server/default should be saved */
};
static struct ada_conf_ctx ada_conf_ctx;
static struct ada_lock *ada_conf_lock;
static char region_code[CLIENT_CONF_REGION_CODE_LEN + 1];

/*
 * Config items automatically loaded and updated by this module.
 * These names must match the names used by the cloud.
 * The type ATLV_UINT is used for all boolean values for compatibility.
 */
static const struct ada_conf_item ada_conf_items[] = {
	{ "id/dev_id", ATLV_UTF8, conf_sys_dev_id, sizeof(conf_sys_dev_id)},
	{ "sys/timezone", ATLV_INT,
	    &timezone_info.mins, sizeof(timezone_info.mins)},
	{ "sys/timezone_valid", ATLV_INT,
	    &timezone_info.valid, sizeof(timezone_info.valid)},
	{ "sys/dst_valid", ATLV_UINT,
	    &daylight_info.valid, sizeof(daylight_info.valid)},
	{ "sys/dst_active", ATLV_UINT,
	    &daylight_info.active, sizeof(daylight_info.active)},
	{ "sys/dst_change", ATLV_UINT,
	    &daylight_info.change, sizeof(daylight_info.change)},
	{ "sys/reset", ATLV_UINT, &conf_was_reset, sizeof(conf_was_reset)},
	{ "sys/setup_mode", ATLV_UINT,
	    &conf_setup_pending, sizeof(conf_setup_pending)},
	{ "sys/test", ATLV_UINT,
	    &conf_test_mode, sizeof(conf_test_mode)},
#if defined(AYLA_LAN_SUPPORT) || defined(AYLA_LOCAL_CONTROL_SUPPORT)
	{ "client/lan/enable", ATLV_UINT,
	    &ada_lan_conf.enable_mask, sizeof(ada_lan_conf.enable_mask) },
	{ "client/lan/private_key", ATLV_BIN,
	    &ada_lan_conf.lanip_key, sizeof(ada_lan_conf.lanip_key) },
	{ "client/lan/key", ATLV_UINT, &ada_lan_conf.lanip_key_id,
	    sizeof(ada_lan_conf.lanip_key_id) },
	{ "client/lan/poll_interval", ATLV_UINT, &ada_lan_conf.keep_alive,
	    sizeof(ada_lan_conf.keep_alive) },
	{ "client/lan/auto", ATLV_UINT, &ada_lan_conf.auto_echo,
	    sizeof(ada_lan_conf.auto_echo) },
#endif
	{ "client/reg/ready", ATLV_UINT, &ada_conf.reg_user,
	    sizeof(ada_conf.reg_user)},
	{ "client/server/default", ATLV_UINT, &ada_conf.conf_serv_override,
	    sizeof(ada_conf.conf_serv_override)},
	{ "client/server/region", ATLV_UTF8, &region_code,
	    sizeof(region_code)},
	{ "client/hostname", ATLV_UTF8, &ada_conf.conf_server,
	    sizeof(ada_conf.conf_server)},
	{ "client/port", ATLV_UINT, &ada_conf.conf_port,
	    sizeof(ada_conf.conf_port)},
	{ "sched/locale/n/1", ATLV_SOLAR_TABLE },
	{ "sched/locale/n/2", ATLV_SOLAR_TABLE },
	{ NULL }
};

static const struct ada_conf_item *ada_conf_item_lookup(const char *name)
{
	const struct ada_conf_item *item;

	for (item = ada_conf_items; item->name; item++) {
		if (!strcmp(item->name, name)) {
			return item;
		}
	}
	conf_setup_pending = conf_setup_mode;
	return NULL;
}

/*
 * Store string representing numeric config item to in-memory buffer.
 * Values are not range-checked.  32-bit unsigned values won't work.
 * Name is used for error message only.
 */
static int ada_conf_load_num(const char *name, const char *buf,
				void *valp, size_t len)
{
	char *errptr;
	long val;

	val = strtol(buf, &errptr, 10);
	if (*errptr || errptr == buf) {
		conf_log(LOG_WARN "conf read of %s: invalid int value %s",
		    name, buf);
		return -1;
	}

	switch (len) {
	case sizeof(u8):
		*(u8 *)valp = (u8)val;
		break;
	case sizeof(u16):
		*(u16 *)valp = (u16)val;
		break;
	case sizeof(u32):
		*(u32 *)valp = (u32)val;
		break;
	default:
		ASSERT_NOTREACHED();
	}
	return 0;
}

int ada_conf_get(const char *name, void *buf, size_t len)
{
	char save[CONF_VAL_MAX];
	int rc;

	AYLA_ASSERT(len <= sizeof(save));
	memcpy(save, buf, len);
	rc = conf_persist_get(name, buf, len);
	if (rc < 0) {
		if (rc == AE_NOT_FOUND) {
			conf_log(LOG_DEBUG2 "ada_conf_get %s not found", name);
		} else {
			conf_log(LOG_ERR "ada_conf_get %s err %d", name, rc);
		}
		memcpy(buf, save, len);		/* restore value */
		return -1;
	}
	return rc;
}

/*
 * Get numeric config item.
 * Values are not range-checked.
 */
static int ada_conf_get_num(const char *name, void *valp, size_t len)
{
	char buf[30];
	int rc;

	rc = ada_conf_get(name, buf, sizeof(buf));
	if (rc < 0) {
		return rc;
	}
	if (rc >= sizeof(buf)) {
		return -1;
	}
	return ada_conf_load_num(name, buf, valp, len);
}

/*
 * Set solar table value from cloud in base64.
 */
static void ada_conf_solar_table_b64_set(const char *val)
{
	struct sched_solar_table table;
	size_t olen = sizeof(table);

	memset(&table, 0, sizeof(table));
	if (ayla_base64_decode(val, strlen(val), &table, &olen)) {
		conf_log(LOG_WARN "ada_conf: solar table decode failed");
		return;
	}
	if (olen < sizeof(table)) {
		conf_log(LOG_WARN "ada_conf: solar table only %zu bytes", olen);
	}
	sched_solar_table_set(&table);
}

/*
 * Get solar table from config.
 */
static int ada_conf_get_solar_table(const char *name)
{
	char buf[BASE64_LEN_EXPAND(sizeof(struct sched_solar_table)) + 20];
	int rc;

	rc = ada_conf_get(name, buf, sizeof(buf));
	if (rc < 0) {
		return rc;
	}
	if (rc >= sizeof(buf)) {
		return -1;
	}
	ada_conf_solar_table_b64_set(buf);
	return 0;
}

/*
 * Update in-memory copy of config variable.
 */
static void ada_conf_refresh(const char *name, const char *val)
{
	const struct ada_conf_item *item;
	size_t len;

	item = ada_conf_item_lookup(name);
	if (!item) {
		return;
	}

	switch (item->type) {
	case ATLV_INT:
	case ATLV_UINT:
		ada_conf_load_num(name, val, item->val, item->len);
		break;
	case ATLV_SOLAR_TABLE:
		ada_conf_solar_table_b64_set(val);
		break;
	default:
		len = strlen(val) + 1;	/* include NUL */
		if (len > item->len) {
			conf_log(LOG_WARN "conf set of %s: value too long",
			    name);
			return;
		}
		memcpy(item->val, val, len);
		break;
	}

	/*
	 * Special case for timezone.valid.  Needs to be set if timezone is set.
	 */
	if (!strcmp(name, "sys/timezone")) {
		timezone_info.valid = 1;
	}
}

/*
 * Export an individual config item.
 *
 * This routine may write this value to flash or simply display it,
 * depending on the configuration system state.
 *
 * Note that config items are always exported as strings.
 */
int ada_conf_export_item(const struct ada_conf_item *item)
{
	char save[CONF_VAL_MAX];
	int rc;

	switch (item->type) {
	case ATLV_INT:
		switch (item->len) {
		case sizeof(s8):
			rc = snprintf(save, CONF_VAL_MAX, "%d",
			    *(s8 *)item->val);
			break;
		case sizeof(s16):
			rc = snprintf(save, CONF_VAL_MAX, "%d",
			    *(s16 *)item->val);
			break;
		case sizeof(s32):
		default:
			rc = snprintf(save, CONF_VAL_MAX, "%ld",
			    *(s32 *)item->val);
			break;
		}
		conf_persist_export(item->name, save, rc);
		break;
	case ATLV_UINT:
		switch (item->len) {
		case sizeof(u8):
			rc = snprintf(save, CONF_VAL_MAX, "%u",
			    *(u8 *)item->val);
			break;
		case sizeof(u16):
			rc = snprintf(save, CONF_VAL_MAX, "%u",
			    *(u16 *)item->val);
			break;
		case sizeof(u32):
		default:
			rc = snprintf(save, CONF_VAL_MAX, "%lu",
			    *(u32 *)item->val);
			break;
		}
		conf_persist_export(item->name, save, rc);
		break;

	case ATLV_BOOL:
		rc = snprintf(save, CONF_VAL_MAX, "%u", *(u8 *)item->val != 0);
		conf_persist_export(item->name, save, rc);
		break;

	default:
		conf_persist_export(item->name, item->val, item->len);
		rc = item->len;
	}
	return rc;
}

/*
 * Load an individual config item.
 */
int ada_conf_get_item(const struct ada_conf_item *item)
{
	int len = 0;

	switch (item->type) {
	case ATLV_INT:
	case ATLV_UINT:
	case ATLV_BOOL:
		len = ada_conf_get_num(item->name, item->val, item->len);
		len = len == 0 ? item->len : len;
		break;
	case ATLV_SOLAR_TABLE:
		ada_conf_get_solar_table(item->name);
		break;
	default:
		len = ada_conf_get(item->name, item->val, item->len);
		break;
	}
	return len;
}

void ada_conf_reset(int factory)
{
	if (factory) {
		conf_reset_factory();
#ifdef AYLA_MATTER_SUPPORT
		al_matter_config_reset();
#endif
#ifdef AYLA_BLUETOOTH_SUPPORT
		al_bt_conf_factory_reset();
#endif
	}
	adap_conf_reset(factory);
	ASSERT_NOTREACHED();
}

static void ada_conf_reset_callback(void *arg)
{
	ada_conf_reset(ada_conf_reset_factory);
}

/*
 * Load configuration for client and associated systems.
 */
void ada_conf_load(void)
{
	const struct ada_conf_item *item;

	ada_conf_init();
	for (item = ada_conf_items; item->name; item++) {
		ada_conf_get_item(item);
	}
	client_set_region(region_code);
}

void conf_lock(void)
{
	ada_lock(ada_conf_lock);
}

void conf_unlock(void)
{
	ada_unlock(ada_conf_lock);
}

/*
 * Initialize config subsystem.
 */
void ada_conf_init(void)
{
	ada_conf_lock = ada_lock_create("conf_lock");
	callback_init(&ada_conf_reset_cb, ada_conf_reset_callback, NULL);
}

struct ada_conf_ctx *ada_conf_dryrun_new(void)
{
	struct ada_conf_ctx *ctx = &ada_conf_ctx;

	ASSERT(!ctx->busy);
	memset(ctx, 0, sizeof(*ctx));
	ctx->busy = 1;
	ctx->dryrun = 1;
	return ctx;
}

void ada_conf_dryrun_off(struct ada_conf_ctx *ctx)
{
	ctx->dryrun = 0;
}

/*
 * Set config items.
 * Called from config.json PUT handler.
 * Persists all items (after the dryrun).
 */
int ada_conf_set(struct ada_conf_ctx *ctx, const char *name, const char *val)
{
	char name_buf[CONFIG_PATH_LEN];
	enum conf_token tk[CONFIG_TOK_LEN];
	int tok_len;
	u8 conf_setup_prev = conf_setup_pending;

	/*
	 * Convert name to tokens.
	 * Copy name first since conf_str_to_tokens() modifies its input.
	 */
	snprintf(name_buf, sizeof(name_buf), "%s", name);
	tok_len = conf_str_to_tokens(name_buf, tk, sizeof(tk));
	if (tok_len <= 0) {
		conf_log(LOG_WARN "conf_str_to_tokens failed \"%s\"", name_buf);
		return -1;
	}
	if (ctx && ctx->dryrun) {
		return 0;
	}

	/*
	 * Save the config item in persistent storage.
	 */
	conf_log(LOG_DEBUG "set %s = %s", name, val);
	if (conf_persist_set(name, val, strlen(val))) {
		conf_log(LOG_WARN "conf set of %s to %s failed", name, val);
		return -1;
	}
	ada_conf_refresh(name, val);

	/*
	 * See if we've done a change that requires a reset or time update.
	 */
	switch (tk[0]) {
	case CT_sys:
		switch (tk[1]) {
		case CT_setup_mode:
			if (ctx && conf_setup_pending != conf_setup_prev) {
				ctx->save_setup = 1;
				if (conf_setup_pending) {
					ctx->reset_req = 1;
					ctx->reset_factory = 1;
				}
			}
			break;
		case CT_dst_valid:
		case CT_dst_active:
		case CT_dst_change:
		case CT_timezone:
			if (ctx) {
				ctx->time_update = 1;
			}
			break;
		default:
			break;
		}
		break;
	case CT_sched:
		switch (tk[1]) {
		case CT_locale:		/* solar table change */
			if (ctx) {
				ctx->time_update = 1;
			}
			break;
		default:
			break;
		}
		break;
	case CT_client:
		if (ctx && tk[1] == CT_server && tk[2] == CT_default) {
			conf_log(LOG_DEBUG "set save_default 1");
			ctx->save_default = 1;
		}
		break;
	default:
		break;
	}
	return 0;
}

int ada_conf_commit(struct ada_conf_ctx *ctx)
{
	conf_commit();
	return 0;
}

void ada_conf_close(struct ada_conf_ctx *ctx)
{
	if (ctx->time_update) {
		prop_mgr_event(PME_TIME, NULL);
		conf_log(LOG_INFO "time info change");
		ada_conf_persist_timezone();
	}
	if (ctx->save_default) {
		conf_log(LOG_INFO "client/server/default info change");
		ada_conf_persist_default();
	}
	if (ctx->save_setup) {
		ada_conf_persist_setup();
	}
	if (ctx->reset_req) {
		ada_conf_reset(ctx->reset_factory);
	}
	ctx->busy = 0;
}

void ada_conf_abort(struct ada_conf_ctx *ctx)
{
	ctx->busy = 0;
}

void ada_conf_setup_mode(int enable)
{
	if (!enable && !mfg_or_setup_mode_ok()) {
		return;
	}
	if (enable == conf_setup_mode && enable == conf_setup_pending) {
		return;
	}
	if (enable && !conf_setup_mode_test && !conf_is_factory_fresh()) {
		printcli("enabling setup_mode requires factory reset");
		printcli("use the command \"reset factory\" first");
		return;
	}
	conf_setup_pending = enable;
	if (!enable) {
		conf_was_reset = 1;
	}
	conf_mfg_pending = enable;
	if (!conf_save_config()) {
		conf_log(LOG_INFO "configuration saved");
	}
}
