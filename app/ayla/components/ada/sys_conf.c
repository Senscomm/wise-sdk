/*
 * Copyright 2011-2013 Ayla Networks, Inc.  All rights reserved.
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
#include <ada/err.h>
#include <ayla/log.h>
#include <ayla/conf_token.h>
#include <ayla/conf.h>
#include <ayla/json.h>
#include <ayla/clock.h>
#include <ayla/parse.h>
#include <ayla/uri_code.h>
#include <ada/ada_conf.h>
#include <ada/client.h>

#define CONFIG_PATH_LEN 100

/*
 * /sys configuration items.
 */
char conf_sys_model[CONF_MODEL_MAX];		/* Ayla module model */
char conf_sys_mfg_model[CONF_MODEL_MAX];
char conf_sys_mfg_serial[CONF_MFG_SN_MAX];
char conf_sys_serial[CONF_DEV_SN_MAX];
u32 conf_mfg_test_time;
u8 conf_test_mode;	/* 1 if test service ticket applied */
u8 conf_setup_mode_test; /* allow setup_mode enable without factory reset */

static enum conf_error conf_sys_get(int src, enum conf_token *, size_t);

/*
 * Export ID configuration items.
 *
 * These items are only exported for testing purposes. Exporting is only allowed
 * when not in mfg or setup mode such that a factory reset will revert any
 * exported changes.
 */
static void conf_id_export(void)
{
	const char *pub_key;

	if (!client_conf_server_change_en() && !conf_state.show_conf) {
		return;
	}

	conf_put_str(CT_dev_id, conf_sys_dev_id);
	pub_key = client_conf_pub_key_base64();
	conf_put(CT_key, ATLV_FILE, pub_key, strlen(pub_key));
}

static int conf_id_path_check(enum conf_token *token, size_t len)
{
	if (len != 1) {
		return -1;
	}
	switch (token[0]) {
	case CT_dev_id:
	case CT_key:
		return 0;
	default:
		break;
	}
	return -1;
}

/*
 * Set ID configuration items.
 */
static enum conf_error
conf_id_set_func(int src, enum conf_token *token, size_t len,
    struct ayla_tlv *tlv)
{
	if (conf_id_path_check(token, len)) {
		return CONF_ERR_PATH;
	}
	/*
	 * Changes to id items is only supported for testing with an alternate
	 * server. These changes are only allowed when changing the server
	 * is enabled and when the device is not in manufacturing mode, so the
	 * changes will not affect the factory configured values.
	 */
	if (!client_conf_server_change_en()) {
		return CONF_ERR_PERM;
	}
	switch (token[0]) {
	case CT_dev_id:
		conf_get(tlv, ATLV_UTF8, conf_sys_dev_id, CONF_DEV_ID_MAX - 1);
		break;
	case CT_key:
		break;
	default:
		goto err;
	}
	return CONF_ERR_NONE;
err:
	return CONF_ERR_PATH;
}

/*
 * Handle get of id config setting.
 */
static enum conf_error conf_id_get(int src, enum conf_token *token,
					size_t len)
{
	if (conf_id_path_check(token, len)) {
		return CONF_ERR_PATH;
	}
	if (conf_access(CONF_OP_SS_ID | CONF_OP_READ | src)) {
		return CONF_ERR_PERM;
	}
	switch (token[0]) {
	case CT_dev_id:
		conf_resp_str(conf_sys_dev_id);
		break;
	default:
		return CONF_ERR_PATH;
	}
	return CONF_ERR_NONE;
}

const struct conf_entry conf_id_entry = {
	.token = CT_id,
	.export = conf_id_export,
	.set = conf_id_set_func,
	.get = conf_id_get,
};

/*
 * Export system ID configuration items.
 */
static void conf_sys_id_export(void)
{
	conf_put_str(CT_model, conf_sys_model);
	conf_put_str(CT_mfg_model, conf_sys_mfg_model);
	conf_put_str(CT_mfg_serial, conf_sys_mfg_serial);
	conf_put_str(CT_serial, conf_sys_serial);
	conf_cd(CT_mfg_mode);
	conf_put_u32(CT_complete, conf_mfg_test_time);
}

static int conf_sys_id_path_check(enum conf_token *token, size_t len)
{
	if (len == 2 && token[0] == CT_mfg_mode && token[1] == CT_complete) {
		return 0;
	}
	if (len != 1) {
		return -1;
	}
	switch (token[0]) {
	case CT_serial:
	case CT_model:
	case CT_mfg_model:
	case CT_mfg_serial:
	case CT_dev_id:
		return 0;
	default:
		break;
	}
	return -1;
}

/*
 * Set system ID configuration items.
 */
static enum conf_error
conf_sys_id_set(int src, enum conf_token *token, size_t len,
    struct ayla_tlv *tlv)
{
	if (conf_sys_id_path_check(token, len)) {
		return CONF_ERR_PATH;
	}
	if (token[0] == CT_mfg_mode && token[1] == CT_complete) {
		conf_mfg_test_time = conf_get_u32(tlv);
		return CONF_ERR_NONE;
	}
	if (conf_access(CONF_OP_SS_ID | CONF_OP_WRITE | src)) {
		return CONF_ERR_PERM;
	}
	switch (token[0]) {
	case CT_serial:
		conf_get(tlv, ATLV_UTF8,
		    conf_sys_serial, sizeof(conf_sys_serial) - 1);
		break;
	case CT_mfg_model:
		conf_get(tlv, ATLV_UTF8,
		    conf_sys_mfg_model, sizeof(conf_sys_mfg_model) - 1);
		break;
	case CT_mfg_serial:
		conf_get(tlv, ATLV_UTF8,
		    conf_sys_mfg_serial, sizeof(conf_sys_mfg_serial) - 1);
		break;
	case CT_dev_id:
		/*
		 * CT_dev_id is readable under CT_sys for compatibility with
		 * the MCU interface despite it being under CT_id.
		 */
		conf_get(tlv, ATLV_UTF8, conf_sys_dev_id, CONF_DEV_ID_MAX - 1);
		break;
	default:
		goto err;
	}
	return CONF_ERR_NONE;
err:
	return CONF_ERR_PATH;
}

/*
 * Handle get of sys or id config setting.
 */
static enum conf_error conf_sys_id_get(int src, enum conf_token *token,
					size_t len)
{
	if (conf_sys_id_path_check(token, len)) {
		return CONF_ERR_PATH;
	}
	if (len == 2 && token[0] == CT_mfg_mode && token[1] == CT_complete) {
		conf_resp_s32(conf_mfg_test_time);
		return CONF_ERR_NONE;
	}
	if (conf_access(CONF_OP_SS_ID | CONF_OP_READ | src)) {
		return CONF_ERR_PERM;
	}
	switch (token[0]) {
	case CT_reset:		/* return non-ID sys/reset */
		conf_resp_bool(conf_was_reset);
		break;
	case CT_serial:
		conf_resp_str(conf_sys_serial);
		break;
	case CT_model:
		conf_resp_str(conf_sys_model);
		break;
	case CT_mfg_model:
		conf_resp_str(conf_sys_mfg_model);
		break;
	case CT_mfg_serial:
		conf_resp_str(conf_sys_mfg_serial);
		break;
	case CT_dev_id:
		conf_resp_str(conf_sys_dev_id);
		break;
	default:
		return CONF_ERR_PATH;
	}
	return CONF_ERR_NONE;
}

const struct conf_entry conf_sys_id_entry = {
	.token = CT_sys,
	.export = conf_sys_id_export,
	.set = conf_sys_id_set,
	.get = conf_sys_id_get,
};

static void conf_sys_persist_reset(void *arg)
{
	conf_put_u32(CT_reset, conf_was_reset);
}

void ada_conf_persist_reset(void)
{
	conf_persist(CT_sys, conf_sys_persist_reset, NULL);
}

static void conf_sys_persist_setup(void *arg)
{
	conf_factory_start();
	conf_put_u32(CT_mfg_mode, conf_mfg_pending);
	conf_put_u32(CT_setup_mode, conf_setup_pending);
	conf_factory_stop();
}

void ada_conf_persist_setup(void)
{
	conf_persist(CT_sys, conf_sys_persist_setup, NULL);
}


static void conf_sys_persist_timezone(void *arg)
{
	conf_put_u32(CT_timezone_valid, timezone_info.valid);
	conf_put_s32(CT_timezone, timezone_info.mins);
	conf_put_u32(CT_dst_active, daylight_info.active);
	conf_put_u32(CT_dst_change, daylight_info.change);
	conf_put_u32(CT_dst_valid, daylight_info.valid);
}

void ada_conf_persist_timezone(void)
{
	conf_persist(CT_sys, conf_sys_persist_timezone, NULL);
}

static void conf_sys_persist_test(void *arg)
{
	conf_put_s32_nz(CT_test, conf_test_mode);
}

void conf_sys_test_mode_update(u8 enable)
{
	conf_test_mode = enable ? 1 : 0;
}

u8 conf_sys_test_mode_get(void)
{
	return conf_test_mode;
}

/*
 * Export system configuration items.
 */
static void conf_sys_export(void)
{
	conf_sys_persist_setup(NULL);
	conf_sys_persist_reset(NULL);
	conf_sys_persist_timezone(NULL);
	conf_sys_persist_test(NULL);
}

static int conf_sys_path_check(enum conf_token *token, size_t len)
{
	if (len == 2 && token[0] == CT_time) {
		if (token[1] == CT_source) {
			return 0;
		}
		if (token[1] == CT_pri) {
			return 0;
		}
		return -1;
	}
	if (len != 1) {
		return -1;
	}
	switch (token[0]) {
	case CT_mfg_mode:
	case CT_reset:
	case CT_setup_mode:
	case CT_time:
	case CT_timezone_valid:
	case CT_timezone:
	case CT_dst_active:
	case CT_dst_change:
	case CT_dst_valid:
		return 0;
	default:
		break;
	}
	return -1;
}

/*
 * Set system configuration items.
 */
static enum conf_error
conf_sys_set(int src, enum conf_token *token, size_t len, struct ayla_tlv *tlv)
{
	enum clock_src source;

	if (conf_sys_path_check(token, len)) {
		return CONF_ERR_PATH;
	}
	/*
	 * Special case to allow MCU to clear setup mode.
	 */
	if (token[0] == CT_setup_mode && src == CONF_OP_SRC_MCU &&
	    tlv->type == ATLV_BOOL && *(u8 *)TLV_VAL(tlv) == 0) {
		conf_setup_pending = 0;
		return CONF_ERR_NONE;
	}
	if (conf_access(CONF_OP_SS_MODE | CONF_OP_WRITE | src)) {
		return CONF_ERR_PERM;
	}
	switch (token[0]) {
	case CT_mfg_mode:
		conf_mfg_pending = conf_get_bit(tlv);
		break;
	case CT_setup_mode:
		conf_setup_pending = conf_get_bit(tlv);
		break;
	case CT_reset:
		conf_was_reset = conf_get_bit(tlv);
		break;
	case CT_time:
		if (src == CONF_OP_SRC_ADS) {
			source = CS_SERVER;
		} else if (src == CONF_OP_SRC_MCU) {
			/*
			 * server time beats mcu time unless MCU is
			 * setting /sys/time/pri
			 */
			source = CS_MCU_LO;
			if (len == 2) {
				source = CS_MCU_HI;
			}
		} else if (src == CONF_OP_SRC_SERVER) {
			source = CS_LOCAL;
		} else {
			source = CS_MIN;
		}
		client_clock_set(conf_get_u32(tlv), source);
		break;
	case CT_timezone_valid:
		timezone_info.valid = conf_get_bit(tlv);
		break;
	case CT_timezone:
		timezone_info.mins = conf_get_s32(tlv);
		if (timezone_info.mins) {
			timezone_info.valid = 1;
		}
		break;
	case CT_dst_active:
		if (src == CONF_OP_SRC_MCU) {
			/* automatically assume daylight is valid */
			daylight_info.valid = 1;
		}
		daylight_info.active = conf_get_bit(tlv);
		break;
	case CT_dst_change:
		if (src == CONF_OP_SRC_MCU) {
			/* automatically assume daylight is valid */
			daylight_info.valid = 1;
		}
		daylight_info.change = conf_get_u32(tlv);
		break;
	case CT_dst_valid:
		daylight_info.valid = conf_get_bit(tlv);
		break;
	default:
		return CONF_ERR_PATH;
	}
	return CONF_ERR_NONE;
}

/*
 * Handle get of sys or id config setting.
 */
static enum conf_error conf_sys_get(int src, enum conf_token *token, size_t len)
{
	if (conf_sys_path_check(token, len)) {
		return CONF_ERR_PATH;
	}
	if (conf_access(CONF_OP_SS_ID | CONF_OP_READ | src)) {
		return CONF_ERR_PERM;
	}
	switch (token[0]) {
	case CT_mfg_mode:
		conf_resp_bool(conf_mfg_mode);
		break;
	case CT_setup_mode:
		conf_resp_bool(conf_setup_mode);
		break;
	case CT_reset:
		conf_resp_bool(conf_was_reset);
		break;
	case CT_time:
		if (len == 2) {
			if (token[1] == CT_source) {
				conf_resp_s32(clock_source());
			} else if (token[1] == CT_pri) {
				conf_resp_s32(clock_utc());
			} else {
				return CONF_ERR_PATH;
			}
			break;
		}
		conf_resp_s32(clock_utc());
		break;
	case CT_timezone_valid:
		conf_resp_bool(timezone_info.valid);
		break;
	case CT_timezone:
		conf_resp_s32(timezone_info.mins);
		break;
	case CT_dst_active:
		conf_resp_bool(daylight_info.active);
		break;
	case CT_dst_change:
		conf_resp_s32(daylight_info.change);
		break;
	case CT_dst_valid:
		conf_resp_bool(daylight_info.valid);
		break;
	case CT_version:
		conf_resp_str(adap_conf_sw_build());
		break;
	default:
		return CONF_ERR_PATH;
	}
	return CONF_ERR_NONE;
}

static void conf_sys_commit(int from_ui)
{
	conf_mfg_mode = conf_mfg_pending;
	conf_setup_mode = conf_setup_pending;
	if (!conf_setup_mode) {
		conf_mfg_mode = 0;
		conf_mfg_pending = 0;
	}
}

const struct conf_entry conf_sys_conf_entry = {
	.token = CT_sys,
	.export = conf_sys_export,
	.set = conf_sys_set,
	.get = conf_sys_get,
	.commit = conf_sys_commit,
};

/*
 * configuration table.
 */
const struct conf_entry * const conf_table[] = {
	&conf_id_entry,
	&conf_sys_id_entry,
	&conf_sys_conf_entry,
	&client_conf_entry,
	&conf_oem_entry,
	&log_conf_entry,
#ifdef AYLA_METRICS_SUPPORT
	&metrics_conf_entry,
#endif
	NULL
};
