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
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <cli.h>

/* BLE header */

#include "nimble/ble.h"
#include "nimble/nimble_port.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "host/ble_att.h"

/* Mandatory services. */

#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

/* Application-specified header. */

#include "blecent.h"
#include "hal/cmsis/cmsis_os2.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define TEXT_BRIGHT_RED     "\x1B[1;31m"
#define TEXT_BRIGHT_GREEN   "\x1B[1;32m"
#define TEXT_BRIGHT_YELLOW  "\x1B[1;33m"
#define TEXT_BRIGHT_BLUE    "\x1B[1;34m"
#define TEXT_CTRL_RESET     "\x1B[0m"

#define TC_LOG(color, arg)  \
							printf(color); \
							printf(arg); \
							printf(TEXT_CTRL_RESET);
#define TC_INFO(...)        TC_LOG(TEXT_BRIGHT_GREEN, __VA_ARGS__)
#define TC_WARN(...)        TC_LOG(TEXT_BRIGHT_YELLOW, __VA_ARGS__)
#define TC_ERROR(...)       TC_LOG(TEXT_BRIGHT_RED, __VA_ARGS__)

static uint8_t g_dev_index;
static char *g_peer_dev_name;

static int g_read_verify_data;
static int g_write_verify_data;

/* 59462f12-9543-9999-12c8-58b459a2712d */

static const ble_uuid128_t gatt_svr_svc_sec_test_uuid =
BLE_UUID128_INIT(0x2d, 0x71, 0xa2, 0x59, 0xb4, 0x58, 0xc8, 0x12,
				 0x99, 0x99, 0x43, 0x95, 0x12, 0x2f, 0x46, 0x59);

/* 5c3a659e-897e-45e1-b016-007107c96df6 */

static const ble_uuid128_t gatt_svr_chr_sec_test_rand_uuid =
BLE_UUID128_INIT(0xf6, 0x6d, 0xc9, 0x07, 0x71, 0x00, 0x16, 0xb0,
				 0xe1, 0x45, 0x7e, 0x89, 0x9e, 0x65, 0x3a, 0x5c);

/* 5c3a659e-897e-45e1-b016-007107c96df7 */

static const ble_uuid128_t gatt_svr_chr_sec_test_static_uuid =
BLE_UUID128_INIT(0xf7, 0x6d, 0xc9, 0x07, 0x71, 0x00, 0x16, 0xb0,
				 0xe1, 0x45, 0x7e, 0x89, 0x9e, 0x65, 0x3a, 0x5c);

static int blecent_gap_event(struct ble_gap_event *event, void *arg);

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/**
 * Application callback.  Called when the read of the ANS Supported New Alert
 * Category characteristic has completed.
 */

static int
blecent_on_read(uint16_t conn_handle,
		const struct ble_gatt_error *error,
		struct ble_gatt_attr *attr,
		void *arg)
{
	MODLOG_DFLT(INFO, "Read complete; status=%d conn_handle=%d",
			error->status,
			conn_handle);

	if (error->status == 0)
	{
		MODLOG_DFLT(INFO, " attr_handle=%d value=", attr->handle);
		print_mbuf(attr->om);
		MODLOG_DFLT(INFO, "\n");

		if (!memcmp((void *)&g_read_verify_data, (void *)attr->om->om_data,
					attr->om->om_len))
		{
			TC_INFO("[TC4:GATT Read] Success\n");
		}
	}
	else
	{
		MODLOG_DFLT(INFO, "\n");
	}

	if (error->status == BLE_HS_ATT_ERR(BLE_ATT_ERR_INSUFFICIENT_ENC) ||
			error->status == BLE_HS_ATT_ERR(BLE_ATT_ERR_INSUFFICIENT_AUTHEN))
	{
		TC_WARN("[TC4:GATT Read] Need Pairing\n");
	}

	return 0;
}

static int
blecent_on_write_after_read(uint16_t conn_handle,
		const struct ble_gatt_error *error,
		struct ble_gatt_attr *attr,
		void *arg)
{
	MODLOG_DFLT(INFO, "Read complete; status=%d conn_handle=%d",
			error->status,
			conn_handle);

	if (error->status == 0)
	{
		MODLOG_DFLT(INFO, " attr_handle=%d value=", attr->handle);
		print_mbuf(attr->om);
		MODLOG_DFLT(INFO, "\n");

		if (!memcmp((void *)&g_write_verify_data, (void *)attr->om->om_data,
					attr->om->om_len))
		{
			TC_INFO("[TC5:GATT Write] Success\n");

			struct ble_gap_upd_params params;
			int rc;

			params.itvl_min = le16toh(12);
			params.itvl_max = le16toh(12);
			params.latency = le16toh(0);
			params.supervision_timeout = le16toh(50);

			TC_INFO("[TC7:Connection Update] Start\n");
			rc = ble_gap_update_params(conn_handle, &params);
			if (rc)
			{
				MODLOG_DFLT(ERROR, "Error, update param; rc=%d\n", rc);
			}
		}
	}

	MODLOG_DFLT(INFO, "\n");

	return 0;
}

/**
 * Application callback.  Called when the write to the ANS Alert Notification
 * Control Point characteristic has completed.
 */

static int
blecent_on_write(uint16_t conn_handle,
		const struct ble_gatt_error *error,
		struct ble_gatt_attr *attr,
		void *arg)
{
	MODLOG_DFLT(INFO,
			"Write complete; status=%d conn_handle=%d attr_handle=%d\n",
			error->status, conn_handle, attr->handle);

	if (error->status == BLE_HS_ATT_ERR(BLE_ATT_ERR_INSUFFICIENT_ENC) ||
			error->status == BLE_HS_ATT_ERR(BLE_ATT_ERR_INSUFFICIENT_AUTHEN))
	{
		int rc;

		TC_WARN("[TC5:GATT Write] Need Pairing\n");

		TC_INFO("[TC6:Pairing] Start\n");

		rc = ble_gap_security_initiate(conn_handle);
		if (rc)
		{
			MODLOG_DFLT(ERROR, "Error, initiate security; rc=%d\n", rc);
		}
	}
	else if (error->status == 0)
	{
		int rc;

		rc = ble_gattc_read(conn_handle, attr->handle,
				blecent_on_write_after_read, NULL);

		if (rc != 0)
		{
			MODLOG_DFLT(ERROR, "Error: Failed to read characteristic; rc=%d\n",
					rc);
		}
	}

	return 0;
}

static void
blecent_read_write_subscribe(const struct peer *peer)
{
	const struct peer_chr *chr;
	uint8_t value[1];
	int rc;

	/* Read the supported-new-alert-category characteristic. */

	chr = peer_chr_find_uuid(peer,
			(const ble_uuid_t *)&gatt_svr_svc_sec_test_uuid,
			(const ble_uuid_t *)&gatt_svr_chr_sec_test_rand_uuid);
	if (chr == NULL)
	{
		MODLOG_DFLT(ERROR, "Error: Peer doesn't support test service\n");
		goto err;
	}

	TC_INFO("[TC4:GATT Read] Start\n");

	rc = ble_gattc_read(peer->conn_handle, chr->chr.val_handle,
			blecent_on_read, NULL);
	if (rc != 0)
	{
		MODLOG_DFLT(ERROR, "Error: Failed to read characteristic; rc=%d\n",
				rc);
		goto err;
	}

	chr = peer_chr_find_uuid(peer,
			(const ble_uuid_t *)&gatt_svr_svc_sec_test_uuid,
			(const ble_uuid_t *)&gatt_svr_chr_sec_test_static_uuid);

	TC_INFO("[TC5:GATT Write] Start\n");

	value[0] = g_write_verify_data;
	rc = ble_gattc_write_flat(peer->conn_handle, chr->chr.val_handle,
			value, 1, blecent_on_write, NULL);

	if (rc != 0)
	{
		MODLOG_DFLT(ERROR, "Error: Failed to write characteristic; rc=%d\n",
				rc);
	}

	return;

err:

	/* Terminate the connection. */

	ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
}

/**
 * Called when service discovery of the specified peer has completed.
 */

static void
blecent_on_disc_complete(const struct peer *peer, int status, void *arg)
{
	if (status != 0)
	{
		/* Service discovery failed.  Terminate the connection. */

		MODLOG_DFLT(ERROR, "Error: Service discovery failed; status=%d "
				"conn_handle=%d\n", status, peer->conn_handle);
		ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
		return;
	}

	TC_INFO("[TC3:Service Discovery] Success\n");

	/* Service discovery has completed successfully.  Now we have a complete
	 * list of services, characteristics, and descriptors that the peer
	 * supports.
	 */

	MODLOG_DFLT(ERROR, "Service discovery complete; status=%d "
			"conn_handle=%d\n", status, peer->conn_handle);

	/* Now perform three concurrent GATT procedures against the peer: read,
	 * write, and subscribe to notifications.
	 */

	blecent_read_write_subscribe(peer);
}

/**
 * Initiates the GAP general discovery procedure.
 */

static void
blecent_scan(void)
{
	uint8_t own_addr_type;
	struct ble_gap_disc_params disc_params;
	int rc;

	/* Figure out address to use while advertising (no privacy for now) */

	rc = ble_hs_id_infer_auto(0, &own_addr_type);
	if (rc != 0)
	{
		MODLOG_DFLT(ERROR, "error determining address type; rc=%d\n", rc);
		return;
	}

	/* Tell the controller to filter duplicates; we don't want to process
	 * repeated advertisements from the same device.
	 */

	disc_params.filter_duplicates = 1;

	/**
	 * Perform a passive scan.  I.e., don't send follow-up scan requests to
	 * each advertiser.
	 */

	disc_params.passive = 1;

	/* Use defaults for the rest of the parameters. */

	disc_params.itvl = 0;
	disc_params.window = 0;
	disc_params.filter_policy = 0;
	disc_params.limited = 0;

	TC_INFO("[TC1:Scanning] Start\n");

	rc = ble_gap_disc(own_addr_type, BLE_HS_FOREVER, &disc_params,
			blecent_gap_event, NULL);
	if (rc != 0)
	{
		MODLOG_DFLT(ERROR, "Error initiating GAP discovery procedure; rc=%d\n",
				rc);
		return;
	}

	MODLOG_DFLT(INFO, "scanning started\n");
}

/**
 * Indicates whether we should try to connect to the sender of the specified
 * advertisement.  The function returns a positive result if the device
 * advertises connectability and support for the Alert Notification service.
 */

static int
blecent_should_connect(const struct ble_gap_disc_desc *disc)
{
	struct ble_hs_adv_fields fields;
	int rc;

	/* The device has to be advertising connectability. */

	if (disc->event_type != BLE_HCI_ADV_RPT_EVTYPE_ADV_IND &&
			disc->event_type != BLE_HCI_ADV_RPT_EVTYPE_DIR_IND)
	{
		return 0;
	}

	rc = ble_hs_adv_parse_fields(&fields, disc->data, disc->length_data);
	if (rc != 0)
	{
		return rc;
	}

	/* connect expected name device */

	if (strlen(g_peer_dev_name) == fields.name_len)
	{
		if (!strncmp(g_peer_dev_name, (void *)fields.name, fields.name_len))
		{
			return 1;
		}
	}

	return 0;
}

/**
 * Connects to the sender of the specified advertisement of it looks
 * interesting.  A device is "interesting" if it advertises connectability
 * and support for the Alert Notification service.
 */

static void
blecent_connect_if_interesting(const struct ble_gap_disc_desc *disc)
{
	uint8_t own_addr_type;
	int rc;

	/* Don't do anything if we don't care about this advertiser. */

	if (!blecent_should_connect(disc))
	{
		return;
	}

	TC_INFO("[TC1:Scanning] Success\n");

	/* Scanning must be stopped before a connection can be initiated. */

	rc = ble_gap_disc_cancel();
	if (rc != 0)
	{
		MODLOG_DFLT(DEBUG, "Failed to cancel scan; rc=%d\n", rc);
		return;
	}

	/* Figure out address to use for connect (no privacy for now) */

	rc = ble_hs_id_infer_auto(0, &own_addr_type);
	if (rc != 0)
	{
		MODLOG_DFLT(ERROR, "error determining address type; rc=%d\n", rc);
		return;
	}

	/* Try to connect the the advertiser.  Allow 30 seconds (30000 ms) for
	 * timeout.
	 */

	TC_INFO("[TC2:Connecting] Start\n");

	rc = ble_gap_connect(own_addr_type, &disc->addr, 30000, NULL,
			blecent_gap_event, NULL);
	if (rc != 0)
	{
		MODLOG_DFLT(ERROR, "Error: Failed to connect to device; addr_type=%d "
				"addr=%s\n; rc=%d",
				disc->addr.type, addr_str(disc->addr.val), rc);
		return;
	}
}

/**
 * The nimble host executes this callback when a GAP event occurs.  The
 * application associates a GAP event callback with each connection that is
 * established.  blecent uses the same callback for all connections.
 *
 * @param event                 The event being signalled.
 * @param arg                   Application-specified argument; unused by
 *                              blecent.
 *
 * @return                      0 if the application successfully handled the
 *                              event; nonzero on failure.  The semantics
 *                              of the return code is specific to the
 *                              particular GAP event being signalled.
 */

static int
blecent_mtu_ext(uint16_t conn_handle, const struct ble_gatt_error *error,
		uint16_t mtu, void *arg)
{
	int rc;

	switch (error->status)
	{
		case 0:
			MODLOG_DFLT(INFO, "MTU size : %d\n", mtu);
			TC_INFO("[TC8:MTU exchange] Success\n");

			TC_INFO("[TC9:2M PHY set] Start\n");
			rc = ble_gap_set_prefered_le_phy(conn_handle,
					BLE_HCI_LE_PHY_2M_PREF_MASK,
					BLE_HCI_LE_PHY_2M_PREF_MASK,
					BLE_HCI_LE_PHY_CODED_ANY);
			if (rc)
			{
				MODLOG_DFLT(ERROR, "Error, set phy; rc=%d\n", rc);
			}

			break;

		default:
			MODLOG_DFLT(ERROR, "Error: MTU exchange; rc=%d\n", error->status);
			break;
	}

	return 0;
}

static int
blecent_gap_event(struct ble_gap_event *event, void *arg)
{
	struct ble_gap_conn_desc desc;
	struct ble_hs_adv_fields fields;
	int rc;

	switch (event->type)
	{
		case BLE_GAP_EVENT_DISC:
			rc = ble_hs_adv_parse_fields(&fields, event->disc.data,
					event->disc.length_data);
			if (rc != 0)
			{
				return 0;
			}

			/* An advertisement report was received during GAP discovery. */

			print_adv_fields(&fields);

			/* Try to connect to the advertiser if it looks interesting. */

			blecent_connect_if_interesting(&event->disc);

			return 0;

		case BLE_GAP_EVENT_CONNECT:

			/* A new connection was established or a connection attempt failed. */

			if (event->connect.status == 0)
			{
				/* Connection successfully established. */

				MODLOG_DFLT(INFO, "Connection established ");

				rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
				assert(rc == 0);
				print_conn_desc(&desc);
				MODLOG_DFLT(INFO, "\n");

				TC_INFO("[TC2:Connecting] Success\n");

				/* Remember peer. */

				rc = peer_add(event->connect.conn_handle);
				if (rc != 0)
				{
					MODLOG_DFLT(ERROR, "Failed to add peer; rc=%d\n", rc);
					return 0;
				}

				/* Perform service discovery. */

				TC_INFO("[TC3:Service Discovery] Start\n");

				rc = peer_disc_all(event->connect.conn_handle,
						blecent_on_disc_complete, NULL);
				if (rc != 0)
				{
					MODLOG_DFLT(ERROR, "Failed to discover services; rc=%d\n",
							rc);
					return 0;
				}
			}
			else
			{
				/* Connection attempt failed; resume scanning. */

				MODLOG_DFLT(ERROR, "Error: Connection failed; status=%d\n",
						event->connect.status);
				blecent_scan();
			}

			return 0;

		case BLE_GAP_EVENT_DISCONNECT:

			/* Connection terminated. */

			MODLOG_DFLT(INFO, "disconnect; reason=%d ",
					event->disconnect.reason);
			print_conn_desc(&event->disconnect.conn);
			MODLOG_DFLT(INFO, "\n");

			/* Forget about peer. */

			peer_delete(event->disconnect.conn.conn_handle);

			/* Resume scanning. */

			blecent_scan();
			return 0;

		case BLE_GAP_EVENT_DISC_COMPLETE:
			MODLOG_DFLT(INFO, "discovery complete; reason=%d\n",
					event->disc_complete.reason);
			return 0;

		case BLE_GAP_EVENT_ENC_CHANGE:

			/* Encryption has been enabled or disabled for this connection. */

			MODLOG_DFLT(INFO, "encryption change event; status=%d ",
					event->enc_change.status);

			rc = ble_gap_conn_find(event->enc_change.conn_handle, &desc);
			assert(rc == 0);
			print_conn_desc(&desc);

			TC_INFO("[TC6:Pairing] Success\n");

			TC_INFO("[TC3:Service Discovery] Start\n");

			rc = peer_disc_all(event->connect.conn_handle,
					blecent_on_disc_complete, NULL);

			if (rc != 0)
			{
				MODLOG_DFLT(ERROR, "Failed to discover services; rc=%d\n", rc);
				return 0;
			}

			return 0;

		case BLE_GAP_EVENT_NOTIFY_RX:

			/* Peer sent us a notification or indication. */

			MODLOG_DFLT(INFO, "received %s; conn_handle=%d attr_handle=%d "
					"attr_len=%d\n",
					event->notify_rx.indication ?
					"indication" :
					"notification",
					event->notify_rx.conn_handle,
					event->notify_rx.attr_handle,
					OS_MBUF_PKTLEN(event->notify_rx.om));

			/* Attribute data is contained in event->notify_rx.attr_data. */

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

		case BLE_GAP_EVENT_CONN_UPDATE:

			/* The central has updated the connection parameters. */

			MODLOG_DFLT(INFO, "connection updated; status=%d ",
					event->conn_update.status);
			rc = ble_gap_conn_find(event->conn_update.conn_handle, &desc);
			assert(rc == 0);
			print_conn_desc(&desc);
			MODLOG_DFLT(INFO, "\n");

			if (event->conn_update.status == 0)
			{
				TC_INFO("[TC7:Connection Update] Success\n");

				TC_INFO("[TC8:MTU exchange] Start\n");

				rc = ble_gattc_exchange_mtu(event->conn_update.conn_handle,
						blecent_mtu_ext, NULL);
				if (rc)
				{
					MODLOG_DFLT(ERROR, "Error, exchange mtu; rc=%d\n", rc);
				}
			}

			return 0;

		case BLE_GAP_EVENT_PHY_UPDATE_COMPLETE:
			MODLOG_DFLT(INFO, "PHY updated; status=%d\n",
					event->phy_updated.status);
			if (event->phy_updated.status == 0)
			{
				if (event->phy_updated.tx_phy ==  BLE_GAP_LE_PHY_2M &&
						event->phy_updated.rx_phy == BLE_GAP_LE_PHY_2M)
				{
					TC_INFO("[TC9:2M PHY set] Success\n");

					TC_INFO("[TC10:Coded PHY set] Start\n");
					ble_gap_set_prefered_le_phy(event->phy_updated.conn_handle,
							BLE_HCI_LE_PHY_CODED_PREF_MASK,
							BLE_HCI_LE_PHY_CODED_PREF_MASK,
							BLE_HCI_LE_PHY_CODED_ANY);
				}

				if (event->phy_updated.tx_phy == BLE_GAP_LE_PHY_CODED &&
						event->phy_updated.rx_phy == BLE_GAP_LE_PHY_CODED)
				{
					TC_INFO("[TC10:Coded PHY set] Success\n");
					TC_INFO("===== Test Complete ====\n");
				}
			}
			else if (event->phy_updated.status == BLE_ERR_UNSUPP_REM_FEATURE)
			{
				TC_WARN("[TC9:2M PHY set] not supported\n");
				TC_WARN("[TC10:Coded PHY set] not supported\n");
				TC_INFO("===== Test Complete ====\n");
			}
			else
			{
				TC_ERROR("[TC9:2M PHY set] error\n");
				TC_ERROR("[TC10:Coded PHY set] error\n");
			}

			return 0;
		default:
			return 0;
	}
}

static void
blecent_on_reset(int reason)
{
	MODLOG_DFLT(ERROR, "Resetting state; reason=%d\n", reason);
}

static void
blecent_on_sync(void)
{
	int rc;
	ble_addr_t addr;
	int fd;

	ble_svc_gap_device_name_set("nimble-blecent");

	fd = os_open("/ble_hs/rand_addr", O_RDONLY);
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
		fd = os_open("/ble_hs/rand_addr", O_CREAT | O_WRONLY);
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

	/* Begin scanning for a peripheral to connect to. */

	blecent_scan();
}

static struct ble_npl_task s_task_host;
void ble_store_config_init(void);
static void *ble_host_task(void *param)
{
	nimble_port_run();
	return NULL;
}

static void show_usage(const char *progname)
{
	fprintf(stderr, "USAGE:\n");
	fprintf(stderr, "\t%s <-n peer advname>\n", progname);
	fprintf(stderr, "where:\n:");
	fprintf(stderr, "\t\tpeer advname: advertising name (default: xiaohu)\n");
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/**
 * main
 *
 * All application logic and NimBLE host work is performed in default task.
 *
 * @return int NOTE: this function should never return!
 */

int
blecent_main(int argc, char *argv[])
{
	int rc;
	int opt;
	bool badarg = false;

	g_peer_dev_name = "xiaohu";
	g_read_verify_data = 0x04030201;
	g_write_verify_data = 0xca;

	while ((opt = getopt(argc, argv, "n:")) != -1)
	{
		switch (opt)
		{
			case 'n':
				g_peer_dev_name = optarg;
				break;
			default:
				fprintf(stderr, "<unknown parameter '-%c'>\n\n", opt);

				/* fall through */

			case '?':
			case ':':
				badarg = true;
		}
	}

	if (badarg)
	{
		show_usage(argv[0]);
		return CMD_RET_FAILURE;
	}

	if (ble_hs_is_enabled()) {
		printf("already enabled\n");
		return CMD_RET_FAILURE;
	}

	/* Configure the host. */
	ble_hs_cfg.reset_cb = blecent_on_reset;
	ble_hs_cfg.sync_cb = blecent_on_sync;
	ble_hs_cfg.gatts_register_cb = NULL;
	ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

	ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;
	ble_hs_cfg.sm_mitm = 0;
	ble_hs_cfg.sm_bonding = 1;
	ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
	ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;

	/* Initialize data structures to track connected peers. */

	rc = peer_init(MYNEWT_VAL(BLE_MAX_CONNECTIONS), 64, 64, 64);
	assert(rc == 0);

	ble_store_config_init();

	ble_npl_task_init(&s_task_host, "ble_host", ble_host_task,
			NULL, 0, 0,
			NULL, 0);

	return CMD_RET_SUCCESS;
}

CMD(blecent, blecent_main, "CLI command for BLE Central", "");
#endif
