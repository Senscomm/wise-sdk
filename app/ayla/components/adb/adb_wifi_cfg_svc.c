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

#include <lwip/sys.h>

#include <ayla/assert.h>
#include <ayla/utypes.h>
#include <ayla/endian.h>
#include <ayla/wifi_error.h>
#include <ayla/log.h>
#include <ayla/mod_log.h>
#include <adw/wifi.h>
#include <ada/ada_wifi.h>
#include <ada/libada.h>
#include <adb/adb.h>
#include <al/al_bt.h>
#include <adb/adb_wifi_cfg_svc.h>

#include <wifi_int.h>


#ifdef AYLA_BLUETOOTH_SUPPORT

#define ADB_WIFI_CFG_SSID_LEN	32
#define ADB_WIFI_CFG_BSSID_LEN	6

/*
 * Ayla specific security type definitions
 */
enum wifi_conn_security {
	WIFI_SEC_NONE = 0,		/* Open Security */
	WIFI_SEC_WEP = 1,		/* WEP */
	WIFI_SEC_WPA = 2,		/* WPA */
	WIFI_SEC_WPA2_PERSONAL = 3,	/* WPA2-Personal */
	WIFI_SEC_WPA3_PERSONAL = 4,	/* WPA3-Personal */
	WIFI_SEC_END_LIST = 5,		/* end-of-list */
};

enum wifi_conn_state {
	WIFI_STATE_NA = 0,		/* N/A */
	WIFI_STATE_DISABLED = 1,	/* Disabled */
	WIFI_STATE_CONNECTING = 2,	/* Connecting to Wi-Fi */
	WIFI_STATE_NETWORK_CONNECTING = 3,/* Obtaining IP */
	WIFI_STATE_CLOUD_CONNECTING = 4,/* Connecting to Cloud */
	WIFI_STATE_UP = 5,		/* Up */
};

struct wifi_scan_result_msg {
	u8 idx;				/* Index of the scan result */
	u8 ssid[ADB_WIFI_CFG_SSID_LEN];	/* SSID of the AP */
	u8 ssid_len;			/* length of the SSID */
	u8 bssid[ADB_WIFI_CFG_BSSID_LEN];/* BSSID (optional) of the AP */
	s16 rssi;			/* signal strength */
	u8 security;			/* security */
} PACKED;

struct wifi_connect_request_msg {
	u8 ssid[ADB_WIFI_CFG_SSID_LEN];	/* SSID of the AP */
	u8 ssid_len;			/* Length of the SSID */
	u8 bssid[ADB_WIFI_CFG_BSSID_LEN];/* BSSID (optional) of the AP */
	u8 key[WIFI_MAX_KEY_LEN];	/* Passphrase */
	u8 key_len;			/* Length of the Passphrase */
	u8 security;			/* Security */
} PACKED;

struct wifi_connect_status_msg {
	u8 ssid[ADB_WIFI_CFG_SSID_LEN];	/* SSID of the AP */
	u8 ssid_len;			/* Length of the SSID */
	u8 error;			/* Error Code */
	u8 state;			/* State */
} PACKED;

static enum adb_att_err adb_wifi_cfg_wps_start_cb(u16 conn,
    const struct adb_attr *attr, u8 *buf, u16 length);
static enum adb_att_err adb_wifi_cfg_scan_start_cb(u16 conn,
    const struct adb_attr *attr, u8 *buf, u16 length);
static enum adb_att_err adb_wifi_cfg_connect_request_cb(u16 conn,
    const struct adb_attr *attr, u8 *buf, u16 length);
static u8 adb_wifi_cfg_connect_write_redact_cb(int offset, u8 c);
static enum adb_att_err adb_wifi_cfg_connect_status_read_cb(u16 conn,
    const struct adb_attr *attr, u8 *buf, u16 *length);

static const AL_BT_UUID128(wifi_connect_uuid,
	/* 1F80AF6A-2B71-4E35-94E5-00F854D8F16F */
	0x6f, 0xf1, 0xd8, 0x54, 0xf8, 0x00, 0xe5, 0x94,
	0x35, 0x4e, 0x71, 0x2b, 0x6a, 0xaf, 0x80, 0x1f);

static struct adb_chr_info wifi_connect_chr;

static const AL_BT_UUID128(wifi_wps_uuid,
	/* 1F80AF6B-2B71-4E35-94E5-00F854D8F16F */
	0x6f, 0xf1, 0xd8, 0x54, 0xf8, 0x00, 0xe5, 0x94,
	0x35, 0x4e, 0x71, 0x2b, 0x6b, 0xaf, 0x80, 0x1f);

static struct adb_chr_info wifi_wps_chr;

static const AL_BT_UUID128(wifi_connect_status_uuid,
	/* 1F80AF6C-2B71-4E35-94E5-00F854D8F16F */
	0x6f, 0xf1, 0xd8, 0x54, 0xf8, 0x00, 0xe5, 0x94,
	0x35, 0x4e, 0x71, 0x2b, 0x6c, 0xaf, 0x80, 0x1f);

static struct adb_chr_info wifi_connect_status_chr;

static const AL_BT_UUID128(wifi_scan_uuid,
	/* 1F80AF6D-2B71-4E35-94E5-00F854D8F16F */
	0x6f, 0xf1, 0xd8, 0x54, 0xf8, 0x00, 0xe5, 0x94,
	0x35, 0x4e, 0x71, 0x2b, 0x6d, 0xaf, 0x80, 0x1f);

static struct adb_chr_info wifi_scan_chr;

static u32 scan_result_token;

static const AL_BT_UUID128(wifi_scan_result_uuid,
	/* 1F80AF6E-2B71-4E35-94E5-00F854D8F16F */
	0x6f, 0xf1, 0xd8, 0x54, 0xf8, 0x00, 0xe5, 0x94,
	0x35, 0x4e, 0x71, 0x2b, 0x6e, 0xaf, 0x80, 0x1f);

static struct adb_chr_info wifi_scan_result_chr;

static const AL_BT_UUID128(wifi_cfg_svc_uuid,
	/* 1CF0FE66-3ECF-4D6E-A9FC-E287AB124B96 */
	0x96, 0x4b, 0x12, 0xab, 0x87, 0xe2, 0xfc, 0xa9,
	0x6e, 0x4d, 0xcf, 0x3e, 0x66, 0xfe, 0xf0, 0x1c);

static struct adb_service_info wifi_cfg_svc = {
	.is_primary = 1,
};

static const struct adb_attr wifi_cfg_svc_table[] = {
	ADB_SERVICE("wifi_svc", &wifi_cfg_svc_uuid, &wifi_cfg_svc),
	ADB_CHR_REDACT("wifi_connect", &wifi_connect_uuid,
	    AL_BT_AF_WRITE | AL_BT_AF_WRITE_ENC,
	    NULL, adb_wifi_cfg_connect_request_cb, &wifi_connect_chr,
	    NULL, adb_wifi_cfg_connect_write_redact_cb),
	ADB_CHR("wifi_wps", &wifi_wps_uuid,
	    AL_BT_AF_WRITE | AL_BT_AF_WRITE_ENC,
	    NULL, adb_wifi_cfg_wps_start_cb, &wifi_wps_chr),
	ADB_CHR("wifi_connect_status", &wifi_connect_status_uuid,
	    AL_BT_AF_READ | AL_BT_AF_READ_ENC | AL_BT_AF_NOTIFY,
	    adb_wifi_cfg_connect_status_read_cb, NULL,
	    &wifi_connect_status_chr),
	ADB_CHR("wifi_scan", &wifi_scan_uuid,
	    AL_BT_AF_WRITE | AL_BT_AF_WRITE_ENC,
	    NULL, adb_wifi_cfg_scan_start_cb, &wifi_scan_chr),
	ADB_CHR("wifi_scan_result", &wifi_scan_result_uuid,
	    AL_BT_AF_READ_ENC | AL_BT_AF_NOTIFY,
	    NULL, NULL, &wifi_scan_result_chr),
	ADB_SERVICE_END()
};

static const struct adb_attr *scan_result_chr;
static const struct adb_attr *connect_status_chr;

static u8 adb_wifi_cfg_ct_security_to_msg_security(
    enum conf_token ct_security)
{
	u8 security;

	switch (ct_security) {
	case CT_WEP:
		security = WIFI_SEC_WEP;
		break;
	case CT_WPA:
		security = WIFI_SEC_WPA;
		break;
	case CT_WPA2_Personal:
		security = WIFI_SEC_WPA2_PERSONAL;
		break;
	case CT_WPA3_Personal:
		security = WIFI_SEC_WPA3_PERSONAL;
		break;
	case CT_none:
	default:
		security = WIFI_SEC_NONE;
		break;
	}
	return security;
}

static enum conf_token adb_wifi_cfg_msg_security_to_ct_security(u8 security)
{
	enum conf_token ct_security;

	switch (security) {
	case WIFI_SEC_WEP:
		ct_security = CT_WEP;
		break;
	case WIFI_SEC_WPA:
		ct_security = CT_WPA;
		break;
	case WIFI_SEC_WPA2_PERSONAL:
		ct_security = CT_WPA2_Personal;
		break;
	case WIFI_SEC_WPA3_PERSONAL:
		ct_security = CT_WPA3_Personal;
		break;
	case WIFI_SEC_NONE:
	default:
		ct_security = CT_none;
		break;
	}
	return ct_security;
}

static void adb_wifi_cfg_encode_connect_status_msg(const char *ssid,
    size_t ssid_len, enum wifi_conn_state conn_state, enum wifi_error error,
    struct wifi_connect_status_msg *connect_status_msg)
{
	memset(connect_status_msg, 0, sizeof(struct wifi_connect_status_msg));

	if (ssid_len > sizeof(connect_status_msg->ssid)) {
		ssid_len = sizeof(connect_status_msg->ssid);
	}
	connect_status_msg->ssid_len = ssid_len;
	memcpy(connect_status_msg->ssid, ssid, ssid_len);
	connect_status_msg->state = conn_state;
	connect_status_msg->error = (u8)error;
}

static void adb_wifi_cfg_encode_scan_result_msg(int index,
    const struct al_wifi_scan_result *scan,
    struct wifi_scan_result_msg *scan_result_msg)
{
	memset(scan_result_msg, 0, sizeof(struct wifi_scan_result_msg));

	scan_result_msg->idx = index + 1;

	if (!scan) {
		/* end-of-list, marker is ssid_len = 0 */
		return;
	}

	scan_result_msg->ssid_len = scan->ssid.len;
	if (scan_result_msg->ssid_len > sizeof(scan_result_msg->ssid)) {
		scan_result_msg->ssid_len = sizeof(scan_result_msg->ssid);
	}
	memcpy(scan_result_msg->ssid, scan->ssid.id,
	    scan_result_msg->ssid_len);

	memcpy(scan_result_msg->bssid, scan->bssid,
	    sizeof(scan_result_msg->bssid));

	put_ua_le16((u16 *)&scan_result_msg->rssi, (u16)scan->rssi);

	scan_result_msg->security = adb_wifi_cfg_ct_security_to_msg_security(
	    adw_wifi_sec_import(scan->wmi_sec));
}

static void adb_wifi_cfg_scan_done_handler(void)
{
	struct al_wifi_scan_result scan_buf;
	struct al_wifi_scan_result *scan = &scan_buf;
	u8 index = 0;	/* index for indications */
	struct wifi_scan_result_msg msg;

	scan_result_token = 0;

	while (scan) {
		scan = adw_wifi_get_scan(index, &scan_result_token, scan);
		/* only report infrastructure networks that have an SSID */
		if (scan &&
		    (scan->type != AL_WIFI_BT_INFRASTRUCTURE ||
		    !scan->ssid.len)) {
			continue;
		}
		adb_wifi_cfg_encode_scan_result_msg(index, scan, &msg);
		al_bt_notify(scan_result_chr, (u8 *)&msg, (u16)sizeof(msg));
		index++;
		/*
		 * Add a short delay to avoid over-running the receiver
		 * with notifications
		 */
		sys_msleep(30);
	}
}

static void adb_wifi_cfg_send_connect_status_msg(
    enum wifi_conn_state conn_state, enum wifi_error error)
{
	static struct wifi_connect_status_msg connect_status_msg;
	int ssid_len;
	char ssid[ADB_WIFI_CFG_SSID_LEN];

	ssid_len = adap_wifi_get_ssid(ssid, sizeof(ssid));
	if (ssid_len < 0) {
		ssid_len = 0;
	}
	adb_wifi_cfg_encode_connect_status_msg(ssid, ssid_len, conn_state,
	    error, &connect_status_msg);
	al_bt_notify(connect_status_chr, (u8 *)&connect_status_msg,
	    (u16)sizeof(connect_status_msg));
}

static void adb_wifi_cfg_wifi_event_handler(enum adw_wifi_event_id id,
    void *arg)
{
	enum wifi_conn_state conn_state = WIFI_STATE_NA;
	enum wifi_error error;

	switch (id) {
	case ADW_EVID_DISABLE:
	case ADW_EVID_STA_DOWN:
		conn_state = WIFI_STATE_DISABLED;
		break;
	case ADW_EVID_ASSOCIATING:
		conn_state = WIFI_STATE_CONNECTING;
		break;
	case ADW_EVID_STA_UP:
		conn_state = WIFI_STATE_NETWORK_CONNECTING;
		break;
	case ADW_EVID_STA_DHCP_UP:
		conn_state = WIFI_STATE_CLOUD_CONNECTING;
		break;
	case ADW_EVID_SCAN_DONE:
		adb_wifi_cfg_scan_done_handler();
		break;
	case ADW_EVID_STATUS:
		break;
	default:
		/*
		 * Regarding to any unexpected event, it thinks that it is
		 * disabled.
		 */
		conn_state = WIFI_STATE_DISABLED;
		break;
	}

	if (conn_state != WIFI_STATE_NA) {
		error = adw_wifi_get_error();
		adb_wifi_cfg_send_connect_status_msg(conn_state, error);
	}
}

static void adb_wifi_cfg_client_event_handler(void *arg, enum ada_err err)
{
	static u8 client_up;
	enum wifi_error error;

	if (err) {
		client_up = 0;
		error = adw_wifi_get_error();
		adb_wifi_cfg_send_connect_status_msg(WIFI_STATE_UP, error);
		return;
	}

	if (!client_up) {
		client_up = 1;
		adb_wifi_cfg_send_connect_status_msg(WIFI_STATE_UP,
		    WIFI_ERR_NONE);
	}
}

static enum adb_att_err adb_wifi_cfg_wps_start_cb(u16 conn,
    const struct adb_attr *attr, u8 *buf, u16 length)
{
	/* virtual WPS push button */
#ifdef WIFI_WPS
	if (adw_wifi_configured() || !adw_wifi_in_ap_mode() ||
	    adw_wifi_start_wps()) {
		adb_log(LOG_WARN "WPS request rejected");
		return ADB_ATT_UNLIKELY;
	}

	adb_log(LOG_INFO "WPS started");
	return ADB_ATT_SUCCESS;
#else
	adb_log(LOG_WARN "WPS not supported");
	return ADB_ATT_UNLIKELY;
#endif
}

static enum adb_att_err adb_wifi_cfg_scan_start_cb(u16 conn,
    const struct adb_attr *attr, u8 *buf, u16 length)
{
	if (adw_wifi_configured()) {
		adb_log(LOG_WARN
		    "scan start rejected, Wi-Fi already configured");
		return ADB_ATT_UNLIKELY;
	}
	adw_wifi_start_scan(0);

	return ADB_ATT_SUCCESS;
}

static enum adb_att_err adb_wifi_cfg_connect_request_cb(u16 conn,
    const struct adb_attr *attr, u8 *buf, u16 length)
{
	struct wifi_connect_request_msg *conn_req =
	    (struct wifi_connect_request_msg *)buf;
	struct al_wifi_ssid ssid;
	enum wifi_error error;

	if (adw_wifi_configured()) {
		adb_log(LOG_WARN
		    "connect request rejected, Wi-Fi already configured");
		return ADB_ATT_UNLIKELY;
	}

	if (length < sizeof(struct wifi_connect_request_msg) ||
	    conn_req->ssid_len > sizeof(ssid.id) ||
	    conn_req->key_len > WIFI_MAX_KEY_LEN ||
	    conn_req->security >= WIFI_SEC_END_LIST) {
		/* invalid message */
		adb_log(LOG_WARN "invalid connect request");
		return ADB_ATT_UNLIKELY;
	}

	ssid.len = conn_req->ssid_len;
	memcpy(ssid.id, conn_req->ssid, ssid.len);
	error = adw_wifi_connect(&ssid, (char *)conn_req->key,
	    conn_req->key_len,
	    adb_wifi_cfg_msg_security_to_ct_security(conn_req->security), 0);
	if (error) {
		adb_log(LOG_ERR "wifi connect request failed error %d", error);
		return ADB_ATT_UNLIKELY;
	}

	return ADB_ATT_SUCCESS;
}

static u8 adb_wifi_cfg_connect_write_redact_cb(int offset, u8 c)
{
	if (offset < offsetof(struct wifi_connect_request_msg, key) ||
		offset > offsetof(struct wifi_connect_request_msg, key_len)) {
		return c;
	}

	return (u8)'*';
}

static enum adb_att_err adb_wifi_cfg_connect_status_read_cb(u16 conn,
    const struct adb_attr *attr, u8 *buf, u16 *length)
{
	struct wifi_connect_status_msg connect_status_msg;
	enum wifi_conn_state conn_state;
	int ssid_len;
	char ssid[ADB_WIFI_CFG_SSID_LEN];
	enum adw_wifi_conn_state wifi_conn_state = adw_wifi_get_state();
	enum wifi_error error = adw_wifi_get_error();

	switch (wifi_conn_state) {
	case WS_DISABLED:
		conn_state = WIFI_STATE_DISABLED;
		break;
	case WS_JOIN:
		conn_state = WIFI_STATE_CONNECTING;
		break;
	case WS_DHCP:
		conn_state = WIFI_STATE_NETWORK_CONNECTING;
		break;
	case WS_WAIT_CLIENT:
		conn_state = WIFI_STATE_CLOUD_CONNECTING;
		break;
	case WS_UP:
		conn_state = WIFI_STATE_UP;
		break;
	default:
		conn_state = WIFI_STATE_NA;
		break;
	}

	ssid_len = adap_wifi_get_ssid(ssid, sizeof(ssid));
	if (ssid_len < 0) {
		ssid_len = 0;
	}
	adb_wifi_cfg_encode_connect_status_msg(ssid, ssid_len, conn_state,
	    error, &connect_status_msg);

	if (*length > sizeof(connect_status_msg)) {
		*length = sizeof(connect_status_msg);
	}

	memcpy(buf, &connect_status_msg, *length);

	return ADB_ATT_SUCCESS;
}

const void *adb_wifi_cfg_svc_get_uuid(void)
{
	return &wifi_cfg_svc_uuid;
}

int adb_wifi_cfg_svc_register(const struct adb_attr **service)
{
	if (service) {
		*service = wifi_cfg_svc_table;
	}

	scan_result_chr = al_bt_find_attr_by_uuid(wifi_cfg_svc_table,
	    &wifi_scan_result_uuid);
	connect_status_chr = al_bt_find_attr_by_uuid(wifi_cfg_svc_table,
	    &wifi_connect_status_uuid);

	adw_wifi_event_register(adb_wifi_cfg_wifi_event_handler, NULL);
	ada_client_event_register(adb_wifi_cfg_client_event_handler, NULL);

	return al_bt_register_service(wifi_cfg_svc_table);
}

#endif /* AYLA_BLUETOOTH_SUPPORT */
