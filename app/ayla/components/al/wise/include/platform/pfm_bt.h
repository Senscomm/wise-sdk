/*
 * Copyright 2020 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_AL_PFM_BT_H__
#define __AYLA_AL_PFM_BT_H__

#if defined(AYLA_BLUETOOTH_SUPPORT) && \
    (!defined(CONFIG_BLE) || !defined(CONFIG_BLE_NIMBLE_HOST))
/* don't build Bluetooth support unless both BT and nimble are enabled */
#undef AYLA_BLUETOOTH_SUPPORT
#endif

#ifdef AYLA_BLUETOOTH_SUPPORT

#include <stdbool.h>

#include "wise_err.h"

/**
 * Attribute access control flags
 */
#define AL_BT_AF_BROADCAST \
    0x0001	/**< broadcast support */
#define AL_BT_AF_READ \
    0x0002		/**< read support */
#define AL_BT_AF_WRITE_NR \
    0x0004	/**< no-resp write support */
#define AL_BT_AF_WRITE \
    0x0008	/**< write support */
#define AL_BT_AF_NOTIFY \
    0x0010	/**< notification support */
#define AL_BT_AF_INDICATE \
    0x0020	/**< indication support */
#define AL_BT_AF_AUTH_SIGN \
    0x0040 /**< signed write support */
#define AL_BT_AF_READ_ENC \
    0x0200	/**< encryption required for reads */
#define AL_BT_AF_READ_AUTHEN \
    0x0400	/**< authentication required for reads */
#define AL_BT_AF_WRITE_ENC \
    0x1000	/**< encryption required for writes */
#define AL_BT_AF_WRITE_AUTHEN \
    0x2000	/**< authentication required for writes */

/** Type of UUID */
enum {
    /** 16-bit UUID (BT SIG assigned) */
    AL_BLE_UUID_TYPE_16 = 16,

    /** 32-bit UUID (BT SIG assigned) */
    AL_BLE_UUID_TYPE_32 = 32,

    /** 128-bit UUID */
    AL_BLE_UUID_TYPE_128 = 128,
};

typedef struct {
    /** Type of the UUID */
    uint8_t type;
} al_ble_uuid_t;

typedef struct {
    al_ble_uuid_t u;
    uint16_t value;
} al_ble_uuid16_t;

typedef struct {
    al_ble_uuid_t u;
    uint8_t value[16];
} al_ble_uuid128_t;

#define AL_BLE_DEV_ADDR_LEN            (6)     /* bytes */

typedef struct {
    uint8_t type;
    uint8_t val[6];
} al_ble_addr_t;


/**
 * UUID in 16 bit representation declaration macro.
 *
 * \param name is the variable name for the UUID.
 * \param uuid is the 16 bit UUID value.
 */
#define AL_BT_UUID16(name, uuid)		\
	al_ble_uuid16_t name = {			\
		.u.type = AL_BLE_UUID_TYPE_16,	\
		.value = uuid,			\
	}

/**
 * UUID in 128 bit representation declaration macro.
 *
 * \param name is the variable name for the UUID.
 * \param uuid is the 128 bit UUID value specified
 *	as 16 bytes.
 */
#define AL_BT_UUID128(name, uuid...)		\
	al_ble_uuid128_t name = {			\
		.u.type = AL_BLE_UUID_TYPE_128,	\
		.value = { uuid },		\
	}

/**
 * Bluetooth CLI help string.
 */
extern const char al_bt_cli_help[];

/**
 * Bluetooth CLI.
 */
wise_err_t al_bt_cli(int argc, char **argv);

/*
 * Structures and exports missing from nimble header files.
 *
 * WARNING: The structures must be kept in sync with private structures
 * in ble_hs_resolv_priv.h.
 */
struct ble_hs_peer_sec {
    al_ble_addr_t peer_addr;
    uint8_t irk[16];
    uint8_t irk_present:1;
};

struct ble_hs_dev_records {
    bool rec_used;
    uint8_t rand_addr_type;
    uint8_t pseudo_addr[AL_BLE_DEV_ADDR_LEN];
    uint8_t rand_addr[AL_BLE_DEV_ADDR_LEN];
    uint8_t identity_addr[AL_BLE_DEV_ADDR_LEN];
    struct ble_hs_peer_sec peer_sec;
};

void ble_store_config_init(void);

#endif /* AYLA_BLUETOOTH_SUPPORT */

#endif
