/*
 * Copyright 2024-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

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

#include "blecomp.h"
#include "hal/cmsis/cmsis_os2.h"

static int blecomp_gap_event(struct ble_gap_event *event, void *arg);

static int
blecomp_display_passkey(uint16_t conn_handle)
{
	struct ble_sm_io pk;
	int rc;

	pk.passkey = 123456;
	pk.action = BLE_SM_IOACT_DISP;

	rc = ble_sm_inject_io(conn_handle, &pk);

	printf("The passkey notify number: 123456\n");

	return rc;
}

static void
blecomp_advertise(void)
{
	uint8_t own_addr_type;
	struct ble_gap_adv_params adv_params;
	struct ble_hs_adv_fields fields;
	const char *name;
	int rc;

	ble_svc_gap_device_name_set("BLE_COMP_TEST");

	/* Figure out address to use while advertising (no privacy for now) */

	rc = ble_hs_id_infer_auto(0, &own_addr_type);
	if (rc != 0)
	{
		MODLOG_DFLT(ERROR, "error determining address type; rc=%d\n", rc);
		return;
	}

	/* force the address type */
	own_addr_type = BLE_OWN_ADDR_PUBLIC;

	/**
	 *  Set the advertisement data included in our advertisements:
	 *     o Flags (indicates advertisement type and other general info).
	 *     o Advertising tx power.
	 *     o Device name.
	 *     o 16-bit service UUIDs (alert notifications).
	 */

	memset(&fields, 0, sizeof fields);

	/* Advertise two flags:
	 *     o Discoverability in forthcoming advertisement (general)
	 *     o BLE-only (BR/EDR unsupported).
	 */

	fields.flags = BLE_HS_ADV_F_DISC_GEN |
		BLE_HS_ADV_F_BREDR_UNSUP;

	rc = ble_gap_adv_set_fields(&fields);
	if (rc != 0)
	{
		MODLOG_DFLT(ERROR, "error setting advertisement data; rc=%d\n", rc);
		return;
	}

	memset(&fields, 0, sizeof fields);

	name = ble_svc_gap_device_name();
	fields.name = (uint8_t *)name;
	fields.name_len = strlen(name);
	fields.name_is_complete = 1;

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
	adv_params.itvl_min = BLE_GAP_ADV_ITVL_MS(40);
	adv_params.itvl_max = BLE_GAP_ADV_ITVL_MS(40);
	rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER,
			&adv_params, blecomp_gap_event, NULL);
	if (rc != 0)
	{
		MODLOG_DFLT(ERROR, "error enabling advertisement; rc=%d\n", rc);
		return;
	}

	printf("(0) ***** advertising start successfully *****\n");
}


static int
blecomp_gap_event(struct ble_gap_event *event, void *arg)
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
				print_conn_desc(&desc);

#if MYNEWT_VAL(BLEPRPH_LE_PHY_SUPPORT)
				phy_conn_changed(event->connect.conn_handle);
#endif
			}

			MODLOG_DFLT(INFO, "\n");

			if (event->connect.status != 0)
			{
				/* Connection failed; resume advertising. */

				blecomp_advertise();
			}

			return 0;

		case BLE_GAP_EVENT_DISCONNECT:
			MODLOG_DFLT(INFO, "disconnect; reason=%d ",
					event->disconnect.reason);
			print_conn_desc(&event->disconnect.conn);
			MODLOG_DFLT(INFO, "\n");

#if MYNEWT_VAL(BLEPRPH_LE_PHY_SUPPORT)
			phy_conn_changed(CONN_HANDLE_INVALID);
#endif

            gatt_svt_val_clear();

			/* Connection terminated; resume advertising. */

			blecomp_advertise();
			return 0;

		case BLE_GAP_EVENT_CONN_UPDATE:

			/* The central has updated the connection parameters. */

			MODLOG_DFLT(INFO, "connection updated; status=%d ",
					event->conn_update.status);
			rc = ble_gap_conn_find(event->conn_update.conn_handle, &desc);
			assert(rc == 0);
			print_conn_desc(&desc);
			MODLOG_DFLT(INFO, "\n");
			return 0;

		case BLE_GAP_EVENT_ADV_COMPLETE:
			MODLOG_DFLT(INFO, "advertise complete; reason=%d",
					event->adv_complete.reason);
			blecomp_advertise();
			return 0;

		case BLE_GAP_EVENT_ENC_CHANGE:

			/* Encryption has been enabled or disabled for this connection. */

			MODLOG_DFLT(INFO, "encryption change event; status=%d ",
					event->enc_change.status);
			rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
			assert(rc == 0);
			print_conn_desc(&desc);
			MODLOG_DFLT(INFO, "\n");

			if (event->enc_change.status == 0) {
				printf("(1) ***** pair status = success *****\n");
			}

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

			if (event->subscribe.cur_notify) {
				gatt_svr_notify(event->subscribe.conn_handle);
			}

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

		case BLE_GAP_EVENT_PASSKEY_ACTION:
			rc = blecomp_display_passkey(event->passkey.conn_handle);
			return rc;
	}

	return 0;
}

static void
blecomp_on_reset(int reason)
{
	MODLOG_DFLT(ERROR, "Resetting state; reason=%d\n", reason);
}

static void
blecomp_on_sync(void)
{
	int rc;
	ble_addr_t addr;
	int fd;

	rc = gatt_svr_init();
	assert(rc == 0);

	fd = open("/ble_hs/rand_addr", O_RDONLY);
	if (fd > 0) {
		printf("random address stored\n");
		addr.type = BLE_ADDR_RANDOM;
		os_read(fd, addr.val, 6);
		os_close(fd);
	} else {

		/* generate new random address */

		rc = ble_hs_id_gen_rnd(0, &addr);
		assert(rc == 0);

		printf("random address generated\n");
		fd = open("/ble_hs/rand_addr", O_CREAT | O_WRONLY);
		if (fd > 0) {
			printf("random address saved\n");
			os_write(fd, addr.val, 6);
			os_close(fd);
		}
	}

	/* set generated address */

	rc = ble_hs_id_set_rnd(addr.val);
	assert(rc == 0);

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

	blecomp_advertise();
}

static struct ble_npl_task s_task_host;
void ble_store_config_init(void);
static void *ble_host_task(void *param)
{
	nimble_port_run();
	return NULL;
}

int
blecomp_main(int arc, char *argv[])
{
	if (ble_hs_is_enabled()) {
		printf("already enabled\n");
		return CMD_RET_FAILURE;
	}

	/* Initialize the NimBLE host configuration. */
	ble_hs_cfg.reset_cb = blecomp_on_reset;
	ble_hs_cfg.sync_cb = blecomp_on_sync;
	ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;
	ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

	ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_DISP_ONLY;
	ble_hs_cfg.sm_mitm = 1;
	ble_hs_cfg.sm_bonding = 1;
	ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC;
	ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ID;

	ble_store_config_init();

	ble_npl_task_init(&s_task_host, "ble_host", ble_host_task,
			NULL, 0, 0,
			NULL, 0);

	return CMD_RET_SUCCESS;
}

CMD(blecomp, blecomp_main, "CLI command for BLE compatibility test", "");
