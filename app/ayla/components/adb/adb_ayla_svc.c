/*
 * Copyright 2020 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <stddef.h>
#include <string.h>

#include <ada/libada.h>
#include <ayla/log.h>
#include <al/al_random.h>
#include <al/al_hmac_sha256.h>
#include <ada/ada_conf.h>

#include <adb/adb.h>
#include <al/al_bt.h>
#include <adb/adb_ayla_svc.h>

#ifdef AYLA_BLUETOOTH_SUPPORT

#define ADB_AYLA_SVC_UUID16	0xfe28

static void (*identify_cb)(void);

/*
 * Service data that can be put into BLE advertisements. Includes the
 * recoverable id for the device, which is generated from random data
 * and a signature hash of the data using the LAN IP key.
 */
struct __attribute__((packed)) adb_ayla_svc_data {
	u8 uuid[2];
	u8 random[4];
	u8 signature[4];
};

#ifdef AYLA_LAN_SUPPORT
static struct adb_ayla_svc_data ayla_svc_data = {
	.uuid = { (u8)ADB_AYLA_SVC_UUID16, (u8)(ADB_AYLA_SVC_UUID16 >> 8) },
};

static u8 ayla_svc_data_valid;
static char ayla_id_str[18];
#endif
static u32 ayla_passkey = MAX_U32;

static enum adb_att_err adb_ayla_svc_identify_write_cb(u16 conn,
    const struct adb_attr *attr, u8 *buf, u16 length)
{
	if (identify_cb) {
		identify_cb();
	}
	return ADB_ATT_SUCCESS;
}

static const AL_BT_UUID128(duid_uuid,
	/* 00000001-FE28-435B-991A-F1B21BB9BCD0 */
	0xd0, 0xbc, 0xb9, 0x1b, 0xb2, 0xf1, 0x1a, 0x99,
	0x5b, 0x43, 0x28, 0xfe, 0x01, 0x00, 0x00, 0x00);

static struct adb_chr_info duid_chr = {
	.value = conf_sys_dev_id,
};

static const AL_BT_UUID128(oem_id_uuid,
	/* 00000002-FE28-435B-991A-F1B21BB9BCD0 */
	0xd0, 0xbc, 0xb9, 0x1b, 0xb2, 0xf1, 0x1a, 0x99,
	0x5b, 0x43, 0x28, 0xfe, 0x02, 0x00, 0x00, 0x00);

static struct adb_chr_info oem_id_chr = {
	.value = oem,
};

static const AL_BT_UUID128(oem_model_uuid,
	/* 00000003-FE28-435B-991A-F1B21BB9BCD0 */
	0xd0, 0xbc, 0xb9, 0x1b, 0xb2, 0xf1, 0x1a, 0x99,
	0x5b, 0x43, 0x28, 0xfe, 0x03, 0x00, 0x00, 0x00);

static struct adb_chr_info oem_model_chr = {
	.value = oem_model,
};

static const AL_BT_UUID128(template_ver_uuid,
	/* 00000004-FE28-435B-991A-F1B21BB9BCD0 */
	0xd0, 0xbc, 0xb9, 0x1b, 0xb2, 0xf1, 0x1a, 0x99,
	0x5b, 0x43, 0x28, 0xfe, 0x04, 0x00, 0x00, 0x00);

static struct adb_chr_info template_ver_chr = {
	.value = template_version,
};

static const AL_BT_UUID128(identify_uuid,
	/* 00000005-FE28-435B-991A-F1B21BB9BCD0 */
	0xd0, 0xbc, 0xb9, 0x1b, 0xb2, 0xf1, 0x1a, 0x99,
	0x5b, 0x43, 0x28, 0xfe, 0x05, 0x00, 0x00, 0x00);

static struct adb_chr_info identify_chr;

static const AL_BT_UUID128(display_name_uuid,
	/* 00000006-FE28-435B-991A-F1B21BB9BCD0 */
	0xd0, 0xbc, 0xb9, 0x1b, 0xb2, 0xf1, 0x1a, 0x99,
	0x5b, 0x43, 0x28, 0xfe, 0x06, 0x00, 0x00, 0x00);

static struct adb_chr_info display_name_chr = {
	.max_val_len = sizeof(ada_conf.host_symname) - 1,
	.value = &ada_conf.host_symname,
};

static const AL_BT_UUID16(ayla_svc_uuid, ADB_AYLA_SVC_UUID16);

static struct adb_service_info ayla_svc = {
	.is_primary = 1,
};

static const struct adb_attr ayla_svc_table[] = {
	ADB_SERVICE("ayla_svc", &ayla_svc_uuid, &ayla_svc),
	ADB_CHR("duid", &duid_uuid,
	    AL_BT_AF_READ | AL_BT_AF_READ_ENC,
	    adb_chr_read_str, NULL, &duid_chr),
	ADB_CHR("oem_id", &oem_id_uuid,
	    AL_BT_AF_READ, adb_chr_read_str, NULL, &oem_id_chr),
	ADB_CHR("oem_model", &oem_model_uuid,
	    AL_BT_AF_READ, adb_chr_read_str, NULL, &oem_model_chr),
	ADB_CHR("template_version", &template_ver_uuid,
	    AL_BT_AF_READ, adb_chr_read_str, NULL, &template_ver_chr),
	ADB_CHR("identify", &identify_uuid,
	    AL_BT_AF_WRITE, NULL, adb_ayla_svc_identify_write_cb,
	    &identify_chr),
	ADB_CHR("display_name", &display_name_uuid,
	    AL_BT_AF_READ | AL_BT_AF_READ_ENC |
	    AL_BT_AF_WRITE | AL_BT_AF_WRITE_ENC,
	    adb_chr_read_str, adb_chr_write_str, &display_name_chr),
	ADB_SERVICE_END()
};

const void *adb_ayla_svc_uuid_get(void)
{
	return &ayla_svc_uuid;
}

int adb_ayla_svc_identify_cb_set(void (*cb)(void))
{
	identify_cb = cb;
	return 0;
}

int adb_ayla_svc_register(const struct adb_attr **service)
{
	if (service) {
		*service = ayla_svc_table;
	}
	return al_bt_register_service(ayla_svc_table);
}

const char *adb_ayla_svc_ayla_id_get(void)
{
#ifdef AYLA_LAN_SUPPORT
	if (!ayla_svc_data_valid) {
		adb_ayla_svc_random_id_update();
		if (!ayla_svc_data_valid) {
			return NULL;
		}
	}
	return ayla_id_str;
#else
	return NULL;
#endif
}

#ifdef AYLA_LAN_SUPPORT
enum ada_err adb_ayla_svc_random_id_update(void)
{
	struct ada_lan_conf *lcf = &ada_lan_conf;
	u8 hash[32];
	struct al_hmac_ctx *ctx;
	char *seed;

	if (!CLIENT_LANIP_HAS_KEY(lcf)) {
		/* LAN IP key must be set to generate id */
		ayla_svc_data_valid = 0;
		ayla_passkey = MAX_U32;
		return AE_INVAL_STATE;
	}

	/* set random portion of id */
	al_random_fill(ayla_svc_data.random, sizeof(ayla_svc_data.random));

	/* set signature portion of id */
	seed = "random_id";

	ctx = al_hmac_sha256_new();
	if (!ctx) {
		return AE_ALLOC;
	}
	al_hmac_sha256_init(ctx, lcf->lanip_key, strlen(lcf->lanip_key));
	al_hmac_sha256_update(ctx, seed, strlen(seed));
	al_hmac_sha256_update(ctx, ayla_svc_data.random,
	    sizeof(ayla_svc_data.random));
	al_hmac_sha256_final(ctx, hash);

	memcpy(ayla_svc_data.signature, hash, sizeof(ayla_svc_data.signature));

	ayla_svc_data_valid = 1;

	snprintf(ayla_id_str, sizeof(ayla_id_str),
	    "%02x%02x%02x%02x:%02x%02x%02x%02x",
	    ayla_svc_data.random[0], ayla_svc_data.random[1],
	    ayla_svc_data.random[2], ayla_svc_data.random[3],
	    ayla_svc_data.signature[0], ayla_svc_data.signature[1],
	    ayla_svc_data.signature[2], ayla_svc_data.signature[3]);

	adb_log(LOG_DEBUG "Ayla id set to %s using lanip key id %u",
	    ayla_id_str, lcf->lanip_key_id);

	/* generate a new passkey derived from the id and LAN IP key */
	seed = "passkey";
	al_hmac_sha256_init(ctx, lcf->lanip_key, strlen(lcf->lanip_key));
	al_hmac_sha256_update(ctx, seed, strlen(seed));
	al_hmac_sha256_update(ctx, &ayla_svc_data, sizeof(ayla_svc_data));
	al_hmac_sha256_final(ctx, hash);

	ayla_passkey = (((u32)hash[0] << 24) + ((u32)hash[1] << 16) +
	    ((u32)hash[2] << 8) + (u32)hash[3]) % (ADB_PASSKEY_MAX + 1);

	al_hmac_sha256_free(ctx);
	return AE_OK;
}
#endif

u32 adb_ayla_svc_passkey_get(void)
{
	return ayla_passkey;
}

const u8 *adb_ayla_svc_data_get(u8 *length)
{
#ifdef AYLA_LAN_SUPPORT
	struct ada_lan_conf *lcf = &ada_lan_conf;

	if (!CLIENT_LANIP_HAS_KEY(lcf)) {
		/* LAN IP key not set, no service data to advertise */
		goto not_valid;
	}

	if (!ayla_svc_data_valid) {
		/* try to create valid service data */
		if (adb_ayla_svc_random_id_update()) {
			goto not_valid;
		}
	}

	*length = (u8)sizeof(ayla_svc_data);
	return (u8 *)&ayla_svc_data;

not_valid:
	ayla_svc_data_valid = 0;
	ayla_passkey = MAX_U32;
#endif
	*length = 0;
	return NULL;
}

#endif /* AYLA_BLUETOOTH_SUPPORT */
