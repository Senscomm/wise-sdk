/*
 * Copyright 2018-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/****************************************************************************
 * Included Files
 ****************************************************************************/

#ifdef CONFIG_CMDLINE

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <cli.h>
#include "syscfg/syscfg.h"
#include "nimble/nimble_port.h"
#include "console/console.h"

/* BLE header */

#include "nimble/ble.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"

/* Application-specified header. */

#include "blehogp.h"
#include "blehogp_svc_bas.h"
#include "blehogp_svc_hids.h"
#include "hal/cmsis/cmsis_os2.h"

#define HOGP_ADV_RPA_ENABLE			0

char adv_name[32];
int adv_name_len;
uint16_t g_conn_handle = BLE_HS_CONN_HANDLE_NONE;

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

static int blehogp_gap_event(struct ble_gap_event *event, void *arg);

/**
 * Logs information about a connection to the console.
 */

static void
blehogp_print_conn_desc(struct ble_gap_conn_desc *desc)
{
	MODLOG_DFLT(INFO, "handle=%d our_ota_addr_type=%d our_ota_addr=",
			desc->conn_handle, desc->our_ota_addr.type);
	print_addr(desc->our_ota_addr.val);
	MODLOG_DFLT(INFO, " our_id_addr_type=%d our_id_addr=",
			desc->our_id_addr.type);
	print_addr(desc->our_id_addr.val);
	MODLOG_DFLT(INFO, " peer_ota_addr_type=%d peer_ota_addr=",
			desc->peer_ota_addr.type);
	print_addr(desc->peer_ota_addr.val);
	MODLOG_DFLT(INFO, " peer_id_addr_type=%d peer_id_addr=",
			desc->peer_id_addr.type);
	print_addr(desc->peer_id_addr.val);
	MODLOG_DFLT(INFO, " conn_itvl=%d conn_latency=%d supervision_timeout=%d "
			"encrypted=%d authenticated=%d bonded=%d\n",
			desc->conn_itvl, desc->conn_latency,
			desc->supervision_timeout,
			desc->sec_state.encrypted,
			desc->sec_state.authenticated,
			desc->sec_state.bonded);
}

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int
blehogp_keystore_iterator(int obj_type,
		union ble_store_value *val,
		void *cookie) {

	ble_addr_t *peer_addr = (ble_addr_t *)cookie;
	memcpy(peer_addr, &val->sec.peer_addr, sizeof(ble_addr_t));
	return 0;
};

/**
 * Enables advertising with the following parameters:
 *     o General discoverable mode.
 *     o Undirected connectable mode.
 */

static void
blehogp_advertise(void)
{
	uint8_t own_addr_type;
	struct ble_gap_adv_params adv_params;
	struct ble_hs_adv_fields fields;
	int rc;

	ble_svc_gap_device_name_set(adv_name);
	ble_svc_gap_device_appearance_set(BLE_HOGP_APPEARANCE);

	/* Figure out address to use while advertising (no privacy for now) */

	rc = ble_hs_id_infer_auto(0, &own_addr_type);
	if (rc != 0)
	{
		MODLOG_DFLT(ERROR, "error determining address type; rc=%d\n", rc);
		return;
	}

	/* force the address type */
#if HOGP_ADV_RPA_ENABLE
	own_addr_type = BLE_OWN_ADDR_RPA_RANDOM_DEFAULT;
#else
	own_addr_type = BLE_OWN_ADDR_RANDOM;
#endif

	/**
	 *  Set the advertisement data included in our advertisements:
	 *     o Flags (indicates advertisement type and other general info).
	 *     o Device name.
	 *     o Appearance.
	 *     o 16-bit service UUIDs (alert notifications).
	 */

	memset(&fields, 0, sizeof fields);

	/* Advertise two flags:
	 *     o Discoverability in forthcoming advertisement (general)
	 *     o BLE-only (BR/EDR unsupported).
	 */

	fields.flags = BLE_HS_ADV_F_DISC_GEN |
		BLE_HS_ADV_F_BREDR_UNSUP;

	fields.name = (uint8_t *)adv_name;
	fields.name_len = adv_name_len;
	fields.name_is_complete = 1;

	fields.appearance_is_present = 1;
	fields.appearance = BLE_HOGP_APPEARANCE;

	fields.uuids16 = (ble_uuid16_t[])
	{
		BLE_UUID16_INIT(BLE_SVC_HIDS_UUID16)
	};

	fields.num_uuids16 = 1;
	fields.uuids16_is_complete = 1;

	rc = ble_gap_adv_set_fields(&fields);
	if (rc != 0)
	{
		MODLOG_DFLT(ERROR, "error setting advertisement data; rc=%d\n", rc);
		return;
	}

	memset(&fields, 0, sizeof fields);

	fields.mfg_data = (const uint8_t *)"ABCD";
	fields.mfg_data_len = 4;

	rc = ble_gap_adv_rsp_set_fields(&fields);
	if (rc != 0)
	{
		MODLOG_DFLT(ERROR, "error setting scan response data; rc=%d\n", rc);
		return;
	}

	/* Begin advertising. */

	memset(&adv_params, 0, sizeof adv_params);
	adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
	adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
	adv_params.itvl_min = BLE_GAP_ADV_ITVL_MS(60);
	adv_params.itvl_max = BLE_GAP_ADV_ITVL_MS(60);

#if HOGP_ADV_RPA_ENABLE
	ble_addr_t peer_addr;

	memset(&peer_addr, 0, sizeof(peer_addr));
	ble_store_iterate(BLE_STORE_OBJ_TYPE_PEER_SEC, &blehogp_keystore_iterator, &peer_addr);

	rc = ble_gap_adv_start(own_addr_type, &peer_addr, BLE_HS_FOREVER,
			&adv_params, blehogp_gap_event, NULL);
#else
	rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER,
			&adv_params, blehogp_gap_event, NULL);
#endif
	if (rc != 0)
	{
		MODLOG_DFLT(ERROR, "error enabling advertisement; rc=%d\n", rc);
		return;
	}

#if HOGP_ADV_RPA_ENABLE
	MODLOG_DFLT(INFO, "advertising started for %02x:%02x:%02x:%02x:%02x:%02x\n",
		peer_addr.val[5],
		peer_addr.val[4],
		peer_addr.val[3],
		peer_addr.val[2],
		peer_addr.val[1],
		peer_addr.val[0]);
#else
	MODLOG_DFLT(INFO, "advertising started\n");
#endif
}

/**
 * The nimble host executes this callback when a GAP event occurs.
 * The application associates a GAP event callback with each connection
 * that forms.
 * blehogp uses the same callback for all connections.
 *
 * @param event                 The type of event being signalled.
 * @param ctxt                  Various information pertaining to the event.
 * @param arg                   Application-specified argument; unuesd by
 *                              blehogp.
 *
 * @return                      0 if the application successfully handled the
 *                              event; nonzero on failure.  The semantics
 *                              of the return code is specific to the
 *                              particular GAP event being signalled.
 */

static int
blehogp_gap_event(struct ble_gap_event *event, void *arg)
{
	struct ble_gap_conn_desc desc;
	int rc;

	switch (event->type)
	{
		case BLE_GAP_EVENT_CONNECT:

			/* A new connection was established or a connection attempt failed. */

			MODLOG_DFLT(INFO, "connection %s; status=%d ",
					event->connect.status == 0 ? "established" : "failed",
					event->connect.status);
			if (event->connect.status == 0)
			{
				rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
				assert(rc == 0);
				blehogp_print_conn_desc(&desc);

#if MYNEWT_VAL(BLEPRPH_LE_PHY_SUPPORT)
				phy_conn_changed(event->connect.conn_handle);
#endif
			}

			MODLOG_DFLT(INFO, "\n");

			if (event->connect.status != 0)
			{
				/* Connection failed; resume advertising. */

				blehogp_advertise();
				g_conn_handle = BLE_HS_CONN_HANDLE_NONE;
			}
			else {
				g_conn_handle = event->connect.conn_handle;
			}

			return 0;

		case BLE_GAP_EVENT_DISCONNECT:
			MODLOG_DFLT(INFO, "disconnect; reason=%d ",
					event->disconnect.reason);
			blehogp_print_conn_desc(&event->disconnect.conn);
			MODLOG_DFLT(INFO, "\n");

#if MYNEWT_VAL(BLEPRPH_LE_PHY_SUPPORT)
			phy_conn_changed(CONN_HANDLE_INVALID);
#endif

			/* Connection terminated; resume advertising. */

			blehogp_advertise();
			g_conn_handle = BLE_HS_CONN_HANDLE_NONE;
			return 0;

		case BLE_GAP_EVENT_CONN_UPDATE:

			/* The central has updated the connection parameters. */

			MODLOG_DFLT(INFO, "connection updated; status=%d ",
					event->conn_update.status);
			rc = ble_gap_conn_find(event->conn_update.conn_handle, &desc);
			assert(rc == 0);
			blehogp_print_conn_desc(&desc);
			MODLOG_DFLT(INFO, "\n");
			return 0;

		case BLE_GAP_EVENT_ADV_COMPLETE:
			MODLOG_DFLT(INFO, "advertise complete; reason=%d",
					event->adv_complete.reason);
			blehogp_advertise();
			return 0;

		case BLE_GAP_EVENT_ENC_CHANGE:

			/* Encryption has been enabled or disabled for this connection. */

			MODLOG_DFLT(INFO, "encryption change event; status=%d ",
					event->enc_change.status);
			rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
			assert(rc == 0);
			blehogp_print_conn_desc(&desc);
			MODLOG_DFLT(INFO, "\n");
			return 0;

		case BLE_GAP_EVENT_SUBSCRIBE:
			MODLOG_DFLT(INFO, "subscribe event; conn_handle=%d attr_handle=%d "
					"reason=%d prevn=%d curn=%d previ=%d curi=%d\n",
					event->subscribe.conn_handle,
					event->subscribe.attr_handle,
					event->subscribe.reason,
					event->subscribe.prev_notify,
					event->subscribe.cur_notify,
					event->subscribe.prev_indicate,
					event->subscribe.cur_indicate);

			return 0;

		case BLE_GAP_EVENT_MTU:
			MODLOG_DFLT(INFO, "mtu update event; conn_handle=%d cid=%d mtu=%d\n",
					event->mtu.conn_handle,
					event->mtu.channel_id,
					event->mtu.value);
			return 0;

		case BLE_GAP_EVENT_REPEAT_PAIRING:

			/* We already have a bond with the peer, but it is attempting to
			 * establish a new secure link.  This app sacrifices security for
			 * convenience: just throw away the old bond and accept the new link.
			 */

			/* Delete the old bond. */

			rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
			assert(rc == 0);
			ble_store_util_delete_peer(&desc.peer_id_addr);

			/* Return BLE_GAP_REPEAT_PAIRING_RETRY to indicate that the host
			 * should continue with the pairing operation.
			 */

			return BLE_GAP_REPEAT_PAIRING_RETRY;

		case BLE_GAP_EVENT_PHY_UPDATE_COMPLETE:
			MODLOG_DFLT(INFO, "PHY updated; status=%d\n",
					event->phy_updated.status);
			if (event->phy_updated.status == 0)
			{
				MODLOG_DFLT(INFO, "TX PHY %d, RX PHY %d\n",
						event->phy_updated.tx_phy,
						event->phy_updated.rx_phy);
			}
			return 0;
	}

	return 0;
}

static void
blehogp_on_reset(int reason)
{
	MODLOG_DFLT(ERROR, "Resetting state; reason=%d\n", reason);
}

int ble_hs_pvcy_set_our_irk(const uint8_t *irk);
int ble_hs_hci_util_rand(void *dst, int len);
int ble_hs_misc_restore_irks(void);
char nibble_to_char(uint8_t nibble);

static void
blehogp_on_sync(void)
{
	int rc;
	ble_addr_t addr;
	int fd;

	rc = gatt_svr_init();
	assert(rc == 0);

	/* check or generate random address */

	fd = os_open("/ble_hs/rand_addr", O_RDONLY);
	if (fd > 0) {
		addr.type = BLE_ADDR_RANDOM;
		os_read(fd, addr.val, 6);
		os_close(fd);
	} else {

		/* generate new random address */

		rc = ble_hs_id_gen_rnd(0, &addr);
		assert(rc == 0);

		fd = os_open("/ble_hs/rand_addr", O_CREAT | O_WRONLY);
		if (fd > 0) {
			os_write(fd, addr.val, 6);
			os_close(fd);
		}
	}

	/* set generated address */

	rc = ble_hs_id_set_rnd(addr.val);
	assert(rc == 0);

	/* check or generate irk */
	uint8_t irk[16];
	fd = os_open("/ble_hs/irk", O_RDONLY);
	if (fd > 0) {
		os_read(fd, irk, 16);
		os_close(fd);
	} else {

		/* generate new irk */
		ble_hs_hci_util_rand(irk, 16);

		fd = os_open("/ble_hs/irk", O_CREAT | O_WRONLY);
		if (fd > 0) {
			os_write(fd, irk, 16);
			os_close(fd);
		}
	}

	/* set irk */
	ble_hs_pvcy_set_our_irk(irk);

	/* restore irks */
	ble_hs_misc_restore_irks();

	strcpy(adv_name, BLE_HOGP_DEVICE_NAME);
	adv_name_len = strlen(adv_name);
	adv_name[adv_name_len++] = ' ';
	adv_name[adv_name_len++] = nibble_to_char(addr.val[5] >> 4);
	adv_name[adv_name_len++] = nibble_to_char(addr.val[5] & 0xf);
	adv_name[adv_name_len++] = nibble_to_char(addr.val[4] >> 4);
	adv_name[adv_name_len++] = nibble_to_char(addr.val[4] & 0xf);
	adv_name[adv_name_len++] = nibble_to_char(addr.val[3] >> 4);
	adv_name[adv_name_len++] = nibble_to_char(addr.val[3] & 0xf);

	/* Make sure we have proper identity address set (public preferred) */

	rc = ble_hs_util_ensure_addr(0);
	assert(rc == 0);

	MODLOG_DFLT(INFO, "random addr ");
	print_addr(addr.val);
	MODLOG_DFLT(INFO, "\n");

	ble_hs_id_copy_addr(BLE_ADDR_PUBLIC, addr.val, NULL);

	MODLOG_DFLT(INFO, "public addr ");
	print_addr(addr.val);
	MODLOG_DFLT(INFO, "\n");


	/* Begin advertising. */

	blehogp_advertise();
}

static struct ble_npl_task s_task_host;
void ble_store_config_init(void);
static void *ble_host_task(void *param)
{
	nimble_port_run();
	return NULL;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/**
 * main
 *
 * The main task for the project. This function initializes the packages,
 * then starts serving events from default event queue.
 *
 * @return int NOTE: this function should never return!
 */

int
blehogp_main(int argc, char *argv[])
{
	int rc;

	if (ble_hs_is_enabled()) {
		printf("already enabled\n");
		return CMD_RET_FAILURE;
	}

	/* Initialize the NimBLE host configuration. */

	ble_hs_cfg.reset_cb = blehogp_on_reset;
	ble_hs_cfg.sync_cb = blehogp_on_sync;
	ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;
	ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

	ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;
	ble_hs_cfg.sm_mitm = 0;
	ble_hs_cfg.sm_bonding = 1;
	ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
	ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;

	ble_store_config_init();

	ble_npl_task_init(&s_task_host, "ble_host", ble_host_task,
			NULL, 0, 0,
			NULL, 0);

	return CMD_RET_SUCCESS;
}

CMD(blehogp, blehogp_main, "BLE HIDS over GATT Profile", "");

int do_hogp_cmd(int argc, char *argv[])
{
	const struct cli_cmd *start, *end, *cmd;

	argc--;
	argv++;

	start = HOGP_CMD_START();
	end = HOGP_CMD_END();

	for (cmd = start; cmd < end; cmd++) {
		if (strcmp(cmd->name, argv[0]) == 0) {
			return cmd->handler(argc, argv);
		}
	}

	for (cmd = start; cmd < end; cmd++) {
		printf("%-16s - %s\n", cmd->name, cmd->usage);
	}

	return -1;
}

CMD(hogp, do_hogp_cmd, "BLE HOGP command", "hogp cmd param");

static int
cmd_keystore_iterator(int obj_type,
		union ble_store_value *val,
		void *cookie) {

	switch (obj_type) {
		case BLE_STORE_OBJ_TYPE_PEER_SEC:
		case BLE_STORE_OBJ_TYPE_OUR_SEC:
			console_printf("Key: ");
			if (ble_addr_cmp(&val->sec.peer_addr, BLE_ADDR_ANY) == 0) {
				console_printf("ediv=%u ", val->sec.ediv);
				console_printf("ediv=%llu ", val->sec.rand_num);
			} else {
				console_printf("addr_type=%u ", val->sec.peer_addr.type);
				print_addr(val->sec.peer_addr.val);
			}
			console_printf("\n");

			if (val->sec.ltk_present) {
				console_printf("    LTK: ");
				print_bytes(val->sec.ltk, 16);
				console_printf("\n");
			}
			if (val->sec.irk_present) {
				console_printf("    IRK: ");
				print_bytes(val->sec.irk, 16);
				console_printf("\n");
			}
			if (val->sec.csrk_present) {
				console_printf("    CSRK: ");
				print_bytes(val->sec.csrk, 16);
				console_printf("\n");
			}
			break;
		case BLE_STORE_OBJ_TYPE_CCCD:
			console_printf("Key: ");
			console_printf("addr_type=%u ", val->cccd.peer_addr.type);
			print_addr(val->cccd.peer_addr.val);
			console_printf("\n");

			console_printf("    char_val_handle: %d\n", val->cccd.chr_val_handle);
			console_printf("    flags:           0x%02x\n", val->cccd.flags);
			console_printf("    changed:         %d\n", val->cccd.value_changed);
			break;
	}
	return 0;
}

int do_ble_key_show(int argc, char *argv[])
{
	printf("---- peer sec\n");
	ble_store_iterate(BLE_STORE_OBJ_TYPE_PEER_SEC, &cmd_keystore_iterator, NULL);
	printf("---- our sec\n");
	ble_store_iterate(BLE_STORE_OBJ_TYPE_OUR_SEC, &cmd_keystore_iterator, NULL);
	printf("---- cccd\n");
	ble_store_iterate(BLE_STORE_OBJ_TYPE_CCCD, &cmd_keystore_iterator, NULL);

	return 0;
}

CMD(blekey, do_ble_key_show, "BLE key info", "show ble key");

int do_ble_key_reset(int argc, char *argv[])
{
	remove("/ble_hs/our_sec");
	remove("/ble_hs/peer_sec");
	remove("/ble_hs/cccd");

	return 0;
}

CMD(blekeyreset, do_ble_key_reset, "BLE key reset", "reset ble key");

int do_ble_addr_reset(int argc, char *argv[])
{
	remove("/ble_hs/rand_addr");

	return 0;
}

CMD(bleaddrreset, do_ble_addr_reset, "BLE random address reset", "reset ble random address");

int do_ble_irk_reset(int argc, char *argv[])
{
	remove("/ble_hs/irk");

	return 0;
}

CMD(bleirkreset, do_ble_irk_reset, "BLE IRK reset", "reset ble IRK");

#endif
