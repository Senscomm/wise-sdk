/*
 * Copyright 2020 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include <ada/libada.h>
#include <ada/client.h>
#include <ayla/log.h>
#include <adb/adb.h>
#include <al/al_bt.h>
#include <adb/adb_conn_svc.h>


#ifdef AYLA_BLUETOOTH_SUPPORT

static enum adb_att_err adb_conn_svc_setup_token_write_cb(u16 conn,
    const struct adb_attr *attr, u8 *buf, u16 length);

static const AL_BT_UUID128(setup_token_uuid,
	/* 7E9869ED-4DB3-4520-88EA-1C21EF1BA834 */
	0x34, 0xa8, 0x1b, 0xef, 0x21, 0x1c, 0xea, 0x88,
	0x20, 0x45, 0xb3, 0x4d, 0xed, 0x69, 0x98, 0x7e);

static struct adb_chr_info setup_token;

static const AL_BT_UUID128(conn_svc_uuid,
	/* 1CF0FE66-3ECF-4D6E-A9FC-E287AB124B96 */
	0xdc, 0x9a, 0xd5, 0x5b, 0xb2, 0xfa, 0x36, 0xae,
	0x73, 0x48, 0xb6, 0x59, 0x41, 0xec, 0xe3, 0xfc);

static struct adb_service_info conn_svc = {
	.is_primary = 1,
};

static const struct adb_attr conn_svc_table[] = {
	ADB_SERVICE("conn_svc", &conn_svc_uuid, &conn_svc),
	ADB_CHR("setup_token", &setup_token_uuid,
	    AL_BT_AF_WRITE /*| AL_BT_AF_WRITE_ENC*/,
	    NULL, adb_conn_svc_setup_token_write_cb, &setup_token),
	ADB_SERVICE_END()
};

#define HYD_AVOID_AYLA_MULTI_REGISTER	1
#if HYD_AVOID_AYLA_MULTI_REGISTER
char first_setup_token[CLIENT_SETUP_TOK_LEN];
struct ada_conf_item token_item = {
	.name = "prop/sim", 
	.type = ATLV_UTF8, 
	.val = &(first_setup_token),
	.len = sizeof(first_setup_token)
};

static void setup_token_conf_save(void *arg)
{
	conf_delete(CT_sim);
	conf_put_s32(CT_sim, *(s32 *)(token_item.val));
	conf_put_str(CT_sim, (char *)(token_item.val));
    conf_cd_parent();
}
#endif
static enum adb_att_err adb_conn_svc_setup_token_write_cb(u16 conn,
    const struct adb_attr *attr, u8 *buf, u16 length)
{
	char setup_token[CLIENT_SETUP_TOK_LEN]; /* connection setup token */

	if (length > CLIENT_SETUP_TOK_LEN - 1) {
		length = CLIENT_SETUP_TOK_LEN - 1;
	}

	memcpy(setup_token, buf, length);
	setup_token[length] = '\0';
#if HYD_AVOID_AYLA_MULTI_REGISTER
	{
		bool exist = ada_conf_get_item(&token_item) > 0 ? true : false;
		if (!exist) {
			printf("Input setup_token: %s, len:%d\n", buf, length);
			memcpy(first_setup_token, buf, length);
			first_setup_token[length] = '\0';
			conf_persist(CT_prop, setup_token_conf_save, NULL);
		} else {
			printf("Setup_token: cur - %s, recv - %s\n", first_setup_token, setup_token);
			if (strcmp(first_setup_token, setup_token) != 0) {
				printf("Setup token is not same, use a trash vaule to register!\n");
				strcpy(setup_token, "Trash");
			}
		}
		printf("Register setup_token:%s\n", setup_token);
	}
#endif
	client_set_setup_token(setup_token);

	return ADB_ATT_SUCCESS;
}

const void *adb_conn_svc_get_uuid(void)
{
	return &conn_svc_uuid;
}

int adb_conn_svc_register(const struct adb_attr **service)
{
	if (service) {
		*service = conn_svc_table;
	}
	return al_bt_register_service(conn_svc_table);
}

/* Put Customer's GATT Service Here for easy porting */
/* HYD BLE GATT Service */
static const AL_BT_UUID16(hyd_svc_uuid, 0xFFF0);
static const AL_BT_UUID16(hyd_inbox_uuid, 0xFFF3);
static const AL_BT_UUID16(hyd_outbox_uuid, 0xFFF4);

static struct adb_service_info hyd_svc = {.is_primary = 1,};
static struct adb_chr_info hyd_inbox_info;
static struct adb_chr_info hyd_outbox_info;

/* al_bt_gatt_chr_access_cb ===> bt_chr->write_cb = adb_hyd_svc_write_cb */
extern void wlt_ble_app_control(u8 *buf, u16 length);

static enum adb_att_err adb_hyd_svc_write_cb(u16 conn, const struct adb_attr *attr, u8 *buf, u16 length)
{
	/* Note: Update the MTU. No need to incorporate the logic for fragment assembly. */
#if 0
	printf("write %s conn %d\n", attr->name, conn);
	printf("Recv HYD Demo Protocol Pkts: \n");
	for (int i = 0; i < length; i++) 
	{
		//printf("%02x ", buf[i]);
	}
	printf("\n");
#endif
	// todo : Implement your own protocols logic by sync or async ways.

	
	uint32_t current_tick = osKernelGetTickCount();
	uint32_t current_ms = current_tick * 1000 / osKernelGetTickFreq();
	static  uint32_t last_time_ms;
	// 检查是否时间间隔是否小于过滤阈值
	//int is_same_addr = !memcmp(event->disc.addr.val, scan_filter_ctx.last_addr.val, 6);
	
	if ((current_ms - last_time_ms) > 120)
	{
		//printf("(current_ms - last_time_ms ) == %d is small !!!\n",(current_ms - last_time_ms));
		//放到iotalink_control处理	
		wlt_ble_app_control(buf,  length);
		last_time_ms = current_ms;
		return ADB_ATT_SUCCESS;
	}

	 // 短时间重复包，跳过


	return ADB_ATT_SUCCESS;
}

static enum adb_att_err adb_hyd_svc_subscribe_cb(u16 conn, u8 notify, u8 indicate)
{
	if (indicate || notify) {
		// do sth when receive BLE_GAP_EVENT_SUBSCRIBE call attr->subscribe_cb
		// or do nothing 
		// or set adb_hyd_svc_subscribe_cb to NULL in hyd_svc_table
	} 
	return ADB_ATT_SUCCESS;
}

static const struct adb_attr *hyd_notify_chr;
static void adb_hyd_notify_demo(void)
{
	// u16 len = 64;
	// void *msg = malloc(len);
	// construct your own msg, notify to master or central
	// al_bt_notify(hyd_notify_chr, (u8 *)msg, len);
	// free(msg);
}
//雷达数据实时上报
void wlt_radar_notify( u8 * msg, u8 len)
{
	 al_bt_notify(hyd_notify_chr, msg, len);
}


static const struct adb_attr hyd_svc_table[] = {
	ADB_SERVICE("hyd_svc", &hyd_svc_uuid, &hyd_svc),
	/* dynamic means need indicate or notify maybe.... */
	ADB_CHR("hyd_inbox", &hyd_inbox_uuid, AL_BT_AF_READ | AL_BT_AF_WRITE_NR ,NULL, adb_hyd_svc_write_cb, &hyd_inbox_info),
	ADB_CHR_SUB("hyd_outbox", &hyd_outbox_uuid, AL_BT_AF_NOTIFY , NULL, NULL, adb_hyd_svc_subscribe_cb, &hyd_outbox_info),
	ADB_SERVICE_END()
};

int adb_hyd_svc_register(const struct adb_attr **service)
{
	if (service) {
		*service = hyd_svc_table;
	}

	hyd_notify_chr = al_bt_find_attr_by_uuid(hyd_svc_table, &hyd_outbox_uuid);

	return al_bt_register_service(hyd_svc_table);
}

#endif /* AYLA_BLUETOOTH_SUPPORT */
