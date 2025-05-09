/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <assert.h>
#include <inttypes.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <cli.h>
#include <hal/kernel.h>

#include "nimble/ble.h"
#include "nimble/nimble_opt.h"
#include "nimble/hci_common.h"
#include "nimble/nimble_port.h"
#include "host/ble_gap.h"
#include "host/ble_hs_adv.h"
#include "host/ble_sm.h"
#include "host/ble_eddystone.h"
#include "host/ble_hs_id.h"
#include "host/ble_dtm.h"
#include "services/gatt/ble_svc_gatt.h"
#include "../src/ble_hs_priv.h"

/* Syscfg. */
#include "syscfg/syscfg.h"

#include "cmd.h"
#include "btshell.h"
#include "cmd_gatt.h"
#include "cmd_l2cap.h"

#define bssnz_t

#ifdef CONFIG_NIMBLE_PTS
static struct ble_npl_callout btshell_addr_timer;
#endif

struct shell_param {
	const char *param;
	const char *help;
};

struct shell_cmd_help {
	const char *summary;
	const char *usage;
	const struct shell_param *params;
};

#ifdef CONFIG_NIMBLE_PTS
static void
btshell_addr_timer_cb(struct ble_npl_event *ev)
{
	struct ble_gap_adv_params params;
	ble_addr_t addr;
	int rc;

	btshell_adv_stop();

	rc = ble_hs_id_gen_rnd(0, &addr);
	assert(rc == 0);

	addr.val[5] &= 0x3F;

	rc = ble_hs_id_set_rnd(addr.val);
	assert(rc == 0);

	memset(&params, 0, sizeof(struct ble_gap_adv_params));
	params.conn_mode = BLE_GAP_CONN_MODE_NON;
	params.disc_mode = BLE_GAP_DISC_MODE_NON;

	btshell_adv_start(1, NULL, INT32_MAX, &params, 0);

	ble_npl_callout_reset(&btshell_addr_timer, 5000);
}
#endif

int
cmd_parse_conn_start_end(uint16_t *out_conn, uint16_t *out_start,
		uint16_t *out_end)
{
	int rc;

	*out_conn = parse_arg_uint16("conn", &rc);
	if (rc != 0) {
		return rc;
	}

	*out_start = parse_arg_uint16("start", &rc);
	if (rc != 0) {
		return rc;
	}

	*out_end = parse_arg_uint16("end", &rc);
	if (rc != 0) {
		return rc;
	}

	return 0;
}

static const struct kv_pair cmd_own_addr_types[] = {
	{ "public",     BLE_OWN_ADDR_PUBLIC },
	{ "random",     BLE_OWN_ADDR_RANDOM },
	{ "rpa_pub",    BLE_OWN_ADDR_RPA_PUBLIC_DEFAULT },
	{ "rpa_rnd",    BLE_OWN_ADDR_RPA_RANDOM_DEFAULT },
	{ NULL }
};

static const struct kv_pair cmd_peer_addr_types[] = {
	{ "public",     BLE_ADDR_PUBLIC },
	{ "random",     BLE_ADDR_RANDOM },
	{ "public_id",  BLE_ADDR_PUBLIC_ID },
	{ "random_id",  BLE_ADDR_RANDOM_ID },
	{ NULL }
};

static const struct kv_pair cmd_addr_type[] = {
	{ "public",     BLE_ADDR_PUBLIC },
	{ "random",     BLE_ADDR_RANDOM },
	{ NULL }
};

#ifdef CONFIG_NIMBLE_PTS
static int
parse_pts_addr(ble_addr_t *addr)
{
	int	rc;
	int fd;

	rc = parse_arg_find_idx("pts");
	if (rc == -1) {
		return ENOENT;
	}

	fd = open("/ble_hs/pts_addr", O_RDONLY);
	if (fd >= 0) {
		ble_addr_t pts_addr;
		int ret = read(fd, &pts_addr, sizeof(pts_addr));
		if (ret == sizeof(pts_addr)) {
			memcpy(addr, &pts_addr, sizeof(pts_addr));
			rc = 0;
		} else {
			rc = EINVAL;
		}
		close(fd);
	} else {
		rc = EINVAL;
	}
	return rc;
}
#endif

static int
parse_dev_addr(const char *prefix, const struct kv_pair *addr_types,
		ble_addr_t *addr)
{
	char name[32];
	int rc;
	int written = 0;

	if (!prefix) {
		name[0] = '\0';
	} else {
		written = snprintf(name, sizeof(name), "%s", prefix);
		if (written >= sizeof(name) || written < 0) {
			return EINVAL;
		}
	}

	rc = snprintf(name + written, sizeof(name) - written, "%s", "addr");
	if (rc >= sizeof(name) - written || rc < 0) {
		return EINVAL;
	}
	written += rc;

	rc = parse_arg_addr(name, addr);
	if (rc == ENOENT) {
		/* not found */
		return rc;
	} else if (rc == EAGAIN) {
		/* address found, but no type provided */
		rc = written;
		written = snprintf(name + written, sizeof(name) - written, "%s", "_type");
		if (written >= sizeof(name) - rc || written < 0) {
			return EINVAL;
		}
		addr->type = parse_arg_kv(name, addr_types, &rc);
		if (rc == ENOENT) {
			addr->type = BLE_ADDR_PUBLIC;
		} else if (rc != 0) {
			return rc;
		}
	} else if (rc != 0) {
		/* error parsing address */
		return rc;
	} else {
		/* full address found, but let's just make sure there is no type arg */
		rc = written;
		written = snprintf(name + written, sizeof(name) - written, "%s", "_type");
		if (written >= sizeof(name) - rc || written < 0) {
			return EINVAL;
		}
		if (parse_arg_extract(name)) {
			return E2BIG;
		}
	}

	return 0;
}

/*****************************************************************************
 * $advertise                                                                *
 *****************************************************************************/
static const struct kv_pair cmd_adv_filt_types[] = {
	{ "none", BLE_HCI_ADV_FILT_NONE },
	{ "scan", BLE_HCI_ADV_FILT_SCAN },
	{ "conn", BLE_HCI_ADV_FILT_CONN },
	{ "both", BLE_HCI_ADV_FILT_BOTH },
	{ NULL }
};

#if MYNEWT_VAL(BLE_EXT_ADV)
static struct kv_pair cmd_ext_adv_phy_opts[] = {
	{ "1M",          0x01 },
	{ "2M",          0x02 },
	{ "coded",       0x03 },
	{ NULL }
};

static int
cmd_advertise_configure(int argc, char **argv)
{
	struct ble_gap_ext_adv_params params = {0};
	int8_t selected_tx_power;
	uint8_t instance;
	int rc;

	rc = parse_arg_all(argc - 1, argv + 1);
	if (rc != 0) {
		return rc;
	}

	instance = parse_arg_uint8_dflt("instance", 0, &rc);
	if (rc != 0 || instance >= BLE_ADV_INSTANCES) {
		console_printf("invalid instance\n");
		return rc;
	}

	memset(&params, 0, sizeof(params));

	params.legacy_pdu = parse_arg_bool_dflt("legacy", 0, &rc);
	if (rc != 0) {
		console_printf("invalid 'legacy' parameter\n");
		return rc;
	}

	if (params.legacy_pdu) {
		params.connectable = 1;
		params.scannable = 1;
	}

	params.connectable = parse_arg_bool_dflt("connectable", params.connectable, &rc);
	if (rc != 0) {
		console_printf("invalid 'connectable' parameter\n");
		return rc;
	}

	params.scannable = parse_arg_bool_dflt("scannable", params.scannable, &rc);
	if (rc != 0) {
		console_printf("invalid 'scannable' parameter\n");
		return rc;
	}

	params.high_duty_directed = parse_arg_bool_dflt("high_duty", 0, &rc);
	if (rc != 0) {
		console_printf("invalid 'high_duty' parameter\n");
		return rc;
	}

	params.anonymous = parse_arg_bool_dflt("anonymous", 0, &rc);
	if (rc != 0) {
		console_printf("invalid 'anonymous' parameter\n");
		return rc;
	}

	params.include_tx_power = parse_arg_bool_dflt("include_tx_power", 0, &rc);
	if (rc != 0) {
		console_printf("invalid 'include_tx_power' parameter\n");
		return rc;
	}

	params.scan_req_notif = parse_arg_bool_dflt("scan_req_notif", 0, &rc);
	if (rc != 0) {
		console_printf("invalid 'scan_req_notif' parameter\n");
		return rc;
	}

	rc = parse_dev_addr("peer_", cmd_peer_addr_types, &params.peer);
	if (rc == 0) {
		params.directed = 1;
	} else if (rc == ENOENT) {
		/* skip, no peer address provided */
	} else {
		console_printf("invalid 'peer_addr' parameter\n");
		return rc;
	}


	params.directed = parse_arg_bool_dflt("directed", params.directed, &rc);
	if (rc != 0) {
		console_printf("invalid 'directed' parameter\n");
		return rc;
	}

	if (params.directed && params.legacy_pdu) {
		params.scannable = 0;
	}

	params.own_addr_type = parse_arg_kv_dflt("own_addr_type",
			cmd_own_addr_types,
			BLE_OWN_ADDR_PUBLIC, &rc);
	if (rc != 0) {
		console_printf("invalid 'own_addr_type' parameter\n");
		return rc;
	}

	params.channel_map = parse_arg_uint8_dflt("channel_map", 0, &rc);
	if (rc != 0) {
		console_printf("invalid 'channel_map' parameter\n");
		return rc;
	}

	params.filter_policy = parse_arg_kv_dflt("filter", cmd_adv_filt_types,
			BLE_HCI_ADV_FILT_NONE, &rc);
	if (rc != 0) {
		console_printf("invalid 'filter' parameter\n");
		return rc;
	}

	params.itvl_min = parse_arg_time_dflt("interval_min", 625, 0, &rc);
	if (rc != 0) {
		console_printf("invalid 'interval_min' parameter\n");
		return rc;
	}

	params.itvl_max = parse_arg_time_dflt("interval_max", 625, 0, &rc);
	if (rc != 0) {
		console_printf("invalid 'interval_max' parameter\n");
		return rc;
	}

	params.tx_power = parse_arg_long_bounds_dflt("tx_power",
			-127, 127, 127, &rc);
	if (rc != 0) {
		console_printf("invalid 'tx_power' parameter\n");
		return rc;
	}

	params.primary_phy = parse_arg_kv_dflt("primary_phy", cmd_ext_adv_phy_opts,
			1, &rc);
	if (rc != 0) {
		console_printf("invalid 'primary_phy' parameter\n");
		return rc;
	}

	params.secondary_phy = parse_arg_kv_dflt("secondary_phy",
			cmd_ext_adv_phy_opts,
			params.primary_phy, &rc);
	if (rc != 0) {
		console_printf("invalid 'secondary_phy' parameter\n");
		return rc;
	}

	params.sid = parse_arg_uint8_dflt("sid", 0, &rc);
	if (rc != 0) {
		console_printf("invalid 'sid' parameter\n");
		return rc;
	}

	rc = btshell_ext_adv_configure(instance, &params, &selected_tx_power);
	if (rc) {
		console_printf("failed to configure advertising instance\n");
		return rc;
	}

	console_printf("Instance %u configured (selected tx power: %d)\n",
			instance, selected_tx_power);

	return 0;
}

static int
cmd_advertise_set_addr(int argc, char **argv)
{
	ble_addr_t addr;
	uint8_t instance;
	int rc;

	rc = parse_arg_all(argc - 1, argv + 1);
	if (rc != 0) {
		return rc;
	}

	instance = parse_arg_uint8_dflt("instance", 0, &rc);
	if (rc != 0 || instance >= BLE_ADV_INSTANCES) {
		console_printf("invalid instance\n");
		return rc;
	}

	rc = parse_arg_mac("addr", addr.val);
	if (rc != 0) {
		console_printf("invalid 'addr' parameter\n");
		return rc;
	}

	addr.type = BLE_ADDR_RANDOM;

	rc = ble_gap_ext_adv_set_addr(instance, &addr);
	if (rc) {
		console_printf("failed to start advertising instance\n");
		return rc;
	}

	return 0;
}

static int
cmd_advertise_start(int argc, char **argv)
{
	int max_events;
	uint8_t instance;
	int duration;
	bool restart;
	int rc;

	rc = parse_arg_all(argc - 1, argv + 1);
	if (rc != 0) {
		return rc;
	}

	instance = parse_arg_uint8_dflt("instance", 0, &rc);
	if (rc != 0 || instance >= BLE_ADV_INSTANCES) {
		console_printf("invalid instance\n");
		return rc;
	}

	duration = parse_arg_uint16_dflt("duration", 0, &rc);
	if (rc != 0) {
		console_printf("invalid 'duration' parameter\n");
		return rc;
	}

	max_events = parse_arg_uint8_dflt("max_events", 0, &rc);
	if (rc != 0) {
		console_printf("invalid 'max_events' parameter\n");
		return rc;
	}

	restart = parse_arg_bool_dflt("restart", 0, &rc);
	if (rc != 0) {
		console_printf("invalid 'restart' parameter\n");
		return rc;
	}

	rc = btshell_ext_adv_start(instance, duration, max_events, restart);
	if (rc) {
		console_printf("failed to start advertising instance\n");
		return rc;
	}

	return 0;
}

static int
cmd_advertise_stop(int argc, char **argv)
{
	uint8_t instance;
	int rc;

	rc = parse_arg_all(argc - 1, argv + 1);
	if (rc != 0) {
		return rc;
	}

	instance = parse_arg_uint8_dflt("instance", 0, &rc);
	if (rc != 0 || instance >= BLE_ADV_INSTANCES) {
		console_printf("invalid instance\n");
		return rc;
	}

	rc = btshell_ext_adv_stop(instance);
	if (rc) {
		console_printf("failed to stop advertising instance\n");
		return rc;
	}

	return 0;
}

static int
cmd_advertise_remove(int argc, char **argv)
{
	uint8_t instance;
	int rc;

	rc = parse_arg_all(argc - 1, argv + 1);
	if (rc != 0) {
		return rc;
	}

	instance = parse_arg_uint8_dflt("instance", 0, &rc);
	if (rc != 0 || instance >= BLE_ADV_INSTANCES) {
		console_printf("invalid instance\n");
		return rc;
	}

	rc = ble_gap_ext_adv_remove(instance);
	if (rc) {
		console_printf("failed to remove advertising instance\n");
		return rc;
	}

	return 0;
}

static const struct shell_param advertise_configure_params[] = {
	{"instance", "default: 0"},
	{"connectable", "connectable advertising, usage: =[0-1], default: 0"},
	{"scannable", "scannable advertising, usage: =[0-1], default: 0"},
	{"directed", "directed advertising, usage: =[0-1], default: 0"},
	{"peer_addr_type", "usage: =[public|random|public_id|random_id], default: public"},
	{"peer_addr", "usage: =[XX:XX:XX:XX:XX:XX]"},
	{"own_addr_type", "usage: =[public|random|rpa_pub|rpa_rnd], default: public"},
	{"channel_map", "usage: =[0x00-0xff], default: 0"},
	{"filter", "usage: =[none|scan|conn|both], default: none"},
	{"interval_min", "usage: =[0-UINT32_MAX], default: 0"},
	{"interval_max", "usage: =[0-UINT32_MAX], default: 0"},
	{"tx_power", "usage: =[-127-127], default: 127"},
	{"primary_phy", "usage: =[1M|coded], default: 1M"},
	{"secondary_phy", "usage: =[1M|2M|coded], default: primary_phy"},
	{"sid", "usage: =[0-UINT8_MAX], default: 0"},
	{"high_duty", "usage: =[0-1], default: 0"},
	{"anonymous", "enable anonymous advertising, usage: =[0-1], default: 0"},
	{"legacy", "use legacy PDUs, usage: =[0-1], default: 0"},
	{"include_tx_power", "include TX power in PDU, usage: =[0-1], default: 0"},
	{"scan_req_notif", "enable Scan Request notification usage: =[0-1], default: 0"},
	{NULL, NULL}
};

static const struct shell_cmd_help advertise_configure_help = {
	.summary = "configure new advertising instance",
	.usage = NULL,
	.params = advertise_configure_params,
};

static const struct shell_param advertise_set_addr_params[] = {
	{"instance", "default: 0"},
	{"addr", "usage: =[XX:XX:XX:XX:XX:XX]"},
	{NULL, NULL}
};

static const struct shell_cmd_help advertise_set_addr_help = {
	.summary = "set advertising instance random address",
	.usage = NULL,
	.params = advertise_set_addr_params,
};

static const struct shell_param advertise_start_params[] = {
	{"instance", "default: 0"},
	{"duration", "advertising duration in 10ms units, default: 0 (forever)"},
	{"max_events", "max number of advertising events, default: 0 (no limit)"},
	{"restart", "restart advertising after disconnect, usage: =[0-1], default: 0"},
	{NULL, NULL}
};

static const struct shell_cmd_help advertise_start_help = {
	.summary = "start advertising instance",
	.usage = NULL,
	.params = advertise_start_params,
};

static const struct shell_param advertise_stop_params[] = {
	{"instance", "default: 0"},
	{NULL, NULL}
};

static const struct shell_cmd_help advertise_stop_help = {
	.summary = "stop advertising instance",
	.usage = NULL,
	.params = advertise_stop_params,
};

static const struct shell_param advertise_remove_params[] = {
	{"instance", "default: 0"},
	{NULL, NULL}
};

static const struct shell_cmd_help advertise_remove_help = {
	.summary = "remove advertising instance",
	.usage = NULL,
	.params = advertise_remove_params,
};

#else

static const struct kv_pair cmd_adv_conn_modes[] = {
	{ "non", BLE_GAP_CONN_MODE_NON },
	{ "und", BLE_GAP_CONN_MODE_UND },
	{ "dir", BLE_GAP_CONN_MODE_DIR },
	{ NULL }
};

static const struct kv_pair cmd_adv_disc_modes[] = {
	{ "non", BLE_GAP_DISC_MODE_NON },
	{ "ltd", BLE_GAP_DISC_MODE_LTD },
	{ "gen", BLE_GAP_DISC_MODE_GEN },
	{ NULL }
};

static int
cmd_advertise(int argc, char **argv)
{
	struct ble_gap_adv_params params;
	int32_t duration_ms;
	ble_addr_t peer_addr;
	ble_addr_t *peer_addr_param = &peer_addr;
	uint8_t own_addr_type;
	bool restart;
	int rc;
#ifdef CONFIG_NIMBLE_PTS
	uint8_t non_resolve;
#endif

	rc = parse_arg_all(argc - 1, argv + 1);
	if (rc != 0) {
		printf("error %d\n", rc);
		return rc;
	}

	if (argc > 1 && strcmp(argv[1], "stop") == 0) {
		rc = btshell_adv_stop();
		if (rc != 0) {
			console_printf("advertise stop fail: %d\n", rc);
			return rc;
		}

		return 0;
	}

	params.conn_mode = parse_arg_kv_dflt("conn", cmd_adv_conn_modes,
			BLE_GAP_CONN_MODE_UND, &rc);
	if (rc != 0) {
		console_printf("invalid 'conn' parameter\n");
		return rc;
	}

	params.disc_mode = parse_arg_kv_dflt("discov", cmd_adv_disc_modes,
			BLE_GAP_DISC_MODE_GEN, &rc);
	if (rc != 0) {
		console_printf("invalid 'discov' parameter\n");
		return rc;
	}

	rc = parse_dev_addr("peer_", cmd_peer_addr_types, &peer_addr);
	if (rc == ENOENT) {

#ifdef CONFIG_NIMBLE_PTS
		rc = parse_pts_addr(&peer_addr);
		if (rc != 0) {
			peer_addr_param = NULL;
		}
#else
		peer_addr_param = NULL;
#endif
	} else if (rc != 0) {
		console_printf("invalid 'peer_addr' parameter\n");
		return rc;
	}

	restart = parse_arg_bool_dflt("restart", 0, &rc);
	if (rc != 0) {
		console_printf("invalid 'restart' parameter\n");
		return rc;
	}

	own_addr_type = parse_arg_kv_dflt("own_addr_type", cmd_own_addr_types,
			BLE_OWN_ADDR_PUBLIC, &rc);
	if (rc != 0) {
		console_printf("invalid 'own_addr_type' parameter\n");
		return rc;
	}

	params.channel_map = parse_arg_uint8_dflt("channel_map", 0, &rc);
	if (rc != 0) {
		console_printf("invalid 'channel_map' parameter\n");
		return rc;
	}

	params.filter_policy = parse_arg_kv_dflt("filter", cmd_adv_filt_types,
			BLE_HCI_ADV_FILT_NONE, &rc);
	if (rc != 0) {
		console_printf("invalid 'filter' parameter\n");
		return rc;
	}

	params.itvl_min = parse_arg_time_dflt("interval_min", 625, 0, &rc);
	if (rc != 0) {
		console_printf("invalid 'interval_min' parameter\n");
		return rc;
	}

	params.itvl_max = parse_arg_time_dflt("interval_max", 625, 0, &rc);
	if (rc != 0) {
		console_printf("invalid 'interval_max' parameter\n");
		return rc;
	}

	params.high_duty_cycle = parse_arg_bool_dflt("high_duty", 0, &rc);
	if (rc != 0) {
		console_printf("invalid 'high_duty' parameter\n");
		return rc;
	}

	duration_ms = parse_arg_long_bounds_dflt("duration", 1, INT32_MAX,
			BLE_HS_FOREVER, &rc);
	if (rc != 0) {
		console_printf("invalid 'duration' parameter\n");
		return rc;
	}


#ifdef CONFIG_NIMBLE_PTS
	non_resolve = parse_arg_bool_dflt("non_resolve", 0, &rc);
	if (rc != 0 ) {
		console_printf("invalid 'non_resolve' parameter\n");
		return rc;
	}

	if (non_resolve) {
		ble_addr_t addr;

		rc = ble_hs_id_gen_rnd(0, &addr);
		assert(rc == 0);

		addr.val[5] &= 0x3F;

		rc = ble_hs_id_set_rnd(addr.val);
		assert(rc == 0);

		ble_npl_callout_init(&btshell_addr_timer, nimble_port_get_dflt_eventq(),
				btshell_addr_timer_cb, NULL);

		ble_npl_callout_reset(&btshell_addr_timer, 5000);
	}
#endif

	rc = btshell_adv_start(own_addr_type, peer_addr_param, duration_ms,
			&params, restart);
	if (rc != 0) {
		console_printf("advertise fail: %d\n", rc);
		return rc;
	}

	return 0;
}

static const struct shell_param advertise_params[] = {
#ifdef CONFIG_NIMBLE_PTS
	{"pts", "usage: =[any] will auto fill pts address/type"},
#endif
	{"stop", "stop advertising procedure"},
	{"conn", "connectable mode, usage: =[non|und|dir], default: und"},
	{"discov", "discoverable mode, usage: =[non|ltd|gen], default: gen"},
	{"peer_addr_type", "usage: =[public|random|public_id|random_id], default: public"},
	{"peer_addr", "usage: =[XX:XX:XX:XX:XX:XX]"},
	{"own_addr_type", "usage: =[public|random|rpa_pub|rpa_rnd], default: public"},
	{"channel_map", "usage: =[0x00-0xff], default: 0"},
	{"filter", "usage: =[none|scan|conn|both], default: none"},
	{"interval_min", "usage: =[0-UINT16_MAX], default: 0"},
	{"interval_max", "usage: =[0-UINT16_MAX], default: 0"},
	{"high_duty", "usage: =[0-1], default: 0"},
	{"duration", "usage: =[1-INT32_MAX], default: INT32_MAX"},
	{"restart", "restart advertising after disconnect, usage: =[0-1], default: 0"},
#ifdef CONFIG_NIMBLE_PTS
	{"non_resolve", "usage: =[0-1], default: 0"},
#endif
	{NULL, NULL}
};

static const struct shell_cmd_help advertise_help = {
	.summary = "start/stop advertising with specific parameters",
	.usage = NULL,
	.params = advertise_params,
};
#endif

/*****************************************************************************
 * $connect                                                                  *
 *****************************************************************************/

static struct kv_pair cmd_ext_conn_phy_opts[] = {
	{ "none",        0x00 },
	{ "1M",          0x01 },
	{ "coded",       0x02 },
	{ "both",        0x03 },
	{ "all",         0x04 },
	{ NULL }
};

static int
cmd_connect(int argc, char **argv)
{
	struct ble_gap_conn_params phy_1M_params = {0};
	struct ble_gap_conn_params phy_coded_params = {0};
	struct ble_gap_conn_params phy_2M_params = {0};
	uint8_t ext;
	int32_t duration_ms;
	ble_addr_t peer_addr;
	ble_addr_t *peer_addr_param = &peer_addr;
	int own_addr_type;
	int rc;

	rc = parse_arg_all(argc - 1, argv + 1);
	if (rc != 0) {
		return rc;
	}

	if (argc > 1 && strcmp(argv[1], "cancel") == 0) {
		rc = btshell_conn_cancel();
		if (rc != 0) {
			console_printf("connection cancel fail: %d\n", rc);
			return rc;
		}

		return 0;
	}

	ext = parse_arg_kv_dflt("extended", cmd_ext_conn_phy_opts, 0, &rc);
	if (rc != 0) {
		console_printf("invalid 'extended' parameter\n");
		return rc;
	}

	rc = parse_dev_addr("peer_", cmd_peer_addr_types, &peer_addr);
	if (rc == ENOENT) {
#ifdef CONFIG_NIMBLE_PTS
		rc = parse_pts_addr(&peer_addr);
		if (rc != 0) {
			/* With no "peer_addr" specified we'll use white list */
			peer_addr_param = NULL;
		}
#else
		/* With no "peer_addr" specified we'll use white list */
		peer_addr_param = NULL;
#endif
	} else if (rc != 0) {
		console_printf("invalid 'peer_addr' parameter\n");
		return rc;
	}

	own_addr_type = parse_arg_kv_dflt("own_addr_type", cmd_own_addr_types,
			BLE_OWN_ADDR_PUBLIC, &rc);
	if (rc != 0) {
		console_printf("invalid 'own_addr_type' parameter\n");
		return rc;
	}

	duration_ms = parse_arg_long_bounds_dflt("duration", 1, INT32_MAX, 0, &rc);
	if (rc != 0) {
		console_printf("invalid 'duration' parameter\n");
		return rc;
	}

	phy_1M_params.scan_itvl = parse_arg_time_dflt("scan_interval", 625, 0x0010, &rc);
	if (rc != 0) {
		console_printf("invalid 'scan_interval' parameter\n");
		return rc;
	}

	phy_1M_params.scan_window = parse_arg_time_dflt("scan_window", 625, 0x0010, &rc);
	if (rc != 0) {
		console_printf("invalid 'scan_window' parameter\n");
		return rc;
	}

	phy_1M_params.itvl_min = parse_arg_time_dflt("interval_min", 1250,
			BLE_GAP_INITIAL_CONN_ITVL_MIN,
			&rc);
	if (rc != 0) {
		console_printf("invalid 'interval_min' parameter\n");
		return rc;
	}

	phy_1M_params.itvl_max = parse_arg_time_dflt("interval_max", 1250,
			BLE_GAP_INITIAL_CONN_ITVL_MAX,
			&rc);
	if (rc != 0) {
		console_printf("invalid 'interval_max' parameter\n");
		return rc;
	}

	phy_1M_params.latency = parse_arg_uint16_dflt("latency", 0, &rc);
	if (rc != 0) {
		console_printf("invalid 'latency' parameter\n");
		return rc;
	}

	phy_1M_params.supervision_timeout = parse_arg_time_dflt("timeout", 10000,
			0x0100, &rc);
	if (rc != 0) {
		console_printf("invalid 'timeout' parameter\n");
		return rc;
	}

	phy_1M_params.min_ce_len = parse_arg_time_dflt("min_conn_event_len", 625,
			0x0010, &rc);
	if (rc != 0) {
		console_printf("invalid 'min_conn_event_len' parameter\n");
		return rc;
	}

	phy_1M_params.max_ce_len = parse_arg_time_dflt("max_conn_event_len", 625,
			0x0300, &rc);
	if (rc != 0) {
		console_printf("invalid 'max_conn_event_len' parameter\n");
		return rc;
	}

	if (ext == 0x00) {
		rc = btshell_conn_initiate(own_addr_type, peer_addr_param, duration_ms,
				&phy_1M_params);
		if (rc) {
			console_printf("error connecting; rc=%d\n", rc);
		}
		return rc;
	}

	if (ext == 0x01) {
		rc = btshell_ext_conn_initiate(own_addr_type, peer_addr_param,
				duration_ms, &phy_1M_params,
				NULL, NULL);
		if (rc) {
			console_printf("error connecting; rc=%d\n", rc);
		}
		return rc;
	}

	/* Get coded params */
	phy_coded_params.scan_itvl = parse_arg_time_dflt("coded_scan_interval",
			625, 0x0010, &rc);
	if (rc != 0) {
		console_printf("invalid 'coded_scan_interval' parameter\n");
		return rc;
	}

	phy_coded_params.scan_window = parse_arg_time_dflt("coded_scan_window",
			625, 0x0010, &rc);
	if (rc != 0) {
		console_printf("invalid 'coded_scan_window' parameter\n");
		return rc;
	}

	phy_coded_params.itvl_min = parse_arg_time_dflt("coded_interval_min", 1250,
			BLE_GAP_INITIAL_CONN_ITVL_MIN,
			&rc);
	if (rc != 0) {
		console_printf("invalid 'coded_interval_min' parameter\n");
		return rc;
	}

	phy_coded_params.itvl_max = parse_arg_time_dflt("coded_interval_max", 1250,
			BLE_GAP_INITIAL_CONN_ITVL_MAX,
			&rc);
	if (rc != 0) {
		console_printf("invalid 'coded_interval_max' parameter\n");
		return rc;
	}

	phy_coded_params.latency =
		parse_arg_uint16_dflt("coded_latency", 0, &rc);
	if (rc != 0) {
		console_printf("invalid 'coded_latency' parameter\n");
		return rc;
	}

	phy_coded_params.supervision_timeout =
		parse_arg_time_dflt("coded_timeout", 10000, 0x0100, &rc);

	if (rc != 0) {
		console_printf("invalid 'coded_timeout' parameter\n");
		return rc;
	}

	phy_coded_params.min_ce_len =
		parse_arg_time_dflt("coded_min_conn_event", 625, 0x0010, &rc);
	if (rc != 0) {
		console_printf("invalid 'coded_min_conn_event' parameter\n");
		return rc;
	}

	phy_coded_params.max_ce_len = parse_arg_time_dflt("coded_max_conn_event",
			625, 0x0300, &rc);
	if (rc != 0) {
		console_printf("invalid 'coded_max_conn_event' parameter\n");
		return rc;
	}

	/* Get 2M params */
	phy_2M_params.itvl_min = parse_arg_time_dflt("2M_interval_min", 1250,
			BLE_GAP_INITIAL_CONN_ITVL_MIN,
			&rc);
	if (rc != 0) {
		console_printf("invalid '2M_interval_min' parameter\n");
		return rc;
	}

	phy_2M_params.itvl_max = parse_arg_time_dflt("2M_interval_max", 1250,
			BLE_GAP_INITIAL_CONN_ITVL_MAX, &rc);
	if (rc != 0) {
		console_printf("invalid '2M_interval_max' parameter\n");
		return rc;
	}

	phy_2M_params.latency =
		parse_arg_uint16_dflt("2M_latency", 0, &rc);
	if (rc != 0) {
		console_printf("invalid '2M_latency' parameter\n");
		return rc;
	}

	phy_2M_params.supervision_timeout = parse_arg_time_dflt("2M_timeout", 10000,
			0x0100, &rc);

	if (rc != 0) {
		console_printf("invalid '2M_timeout' parameter\n");
		return rc;
	}

	phy_2M_params.min_ce_len = parse_arg_time_dflt("2M_min_conn_event", 625,
			0x0010, &rc);
	if (rc != 0) {
		console_printf("invalid '2M_min_conn_event' parameter\n");
		return rc;
	}

	phy_2M_params.max_ce_len = parse_arg_time_dflt("2M_max_conn_event", 625,
			0x0300, &rc);
	if (rc != 0) {
		console_printf("invalid '2M_max_conn_event' parameter\n");
		return rc;
	}

	if (ext == 0x02) {
		rc = btshell_ext_conn_initiate(own_addr_type, peer_addr_param,
				duration_ms, NULL, NULL, &phy_coded_params);
		return rc;
	}

	if (ext == 0x03) {
		rc = btshell_ext_conn_initiate(own_addr_type, peer_addr_param,
				duration_ms, &phy_1M_params, NULL,
				&phy_coded_params);
		return rc;
	}

	rc = btshell_ext_conn_initiate(own_addr_type, peer_addr_param,
			duration_ms, &phy_1M_params,
			&phy_2M_params,
			&phy_coded_params);
	return rc;
}

static const struct shell_param connect_params[] = {
	{"cancel", "cancel connection procedure"},
	{"extended", "usage: =[none|1M|coded|both|all], default: none"},
#ifdef CONFIG_NIMBLE_PTS
	{"pts", "usage: =[any] will auto fill pts address/type"},
#endif
	{"peer_addr_type", "usage: =[public|random|public_id|random_id], default: public"},
	{"peer_addr", "usage: =[XX:XX:XX:XX:XX:XX]"},
	{"own_addr_type", "usage: =[public|random|rpa_pub|rpa_rnd], default: public"},
	{"duration", "usage: =[1-INT32_MAX], default: 0"},
	{"scan_interval", "usage: =[0-UINT16_MAX], default: 0x0010"},
	{"scan_window", "usage: =[0-UINT16_MAX], default: 0x0010"},
	{"interval_min", "usage: =[0-UINT16_MAX], default: 30"},
	{"interval_max", "usage: =[0-UINT16_MAX], default: 50"},
	{"latency", "usage: =[UINT16], default: 0"},
	{"timeout", "usage: =[UINT16], default: 0x0100"},
	{"min_conn_event_len", "usage: =[UINT16], default: 0x0010"},
	{"max_conn_event_len", "usage: =[UINT16], default: 0x0300"},
	{"coded_scan_interval", "usage: =[0-UINT16_MAX], default: 0x0010"},
	{"coded_scan_window", "usage: =[0-UINT16_MAX], default: 0x0010"},
	{"coded_interval_min", "usage: =[0-UINT16_MAX], default: 30"},
	{"coded_interval_max", "usage: =[0-UINT16_MAX], default: 50"},
	{"coded_latency", "usage: =[UINT16], default: 0"},
	{"coded_timeout", "usage: =[UINT16], default: 0x0100"},
	{"coded_min_conn_event_len", "usage: =[UINT16], default: 0x0010"},
	{"coded_max_conn_event_len", "usage: =[UINT16], default: 0x0300"},
	{"2M_interval_min", "usage: =[0-UINT16_MAX], default: 30"},
	{"2M_interval_max", "usage: =[0-UINT16_MAX], default: 50"},
	{"2M_latency", "usage: =[UINT16], default: 0"},
	{"2M_timeout", "usage: =[UINT16], default: 0x0100"},
	{"2M_min_conn_event_len", "usage: =[UINT16], default: 0x0010"},
	{"2M_max_conn_event_len", "usage: =[UINT16], default: 0x0300"},
	{NULL, NULL}
};

static const struct shell_cmd_help connect_help = {
	.summary = "start/stop connection procedure with specific parameters",
	.usage = NULL,
	.params = connect_params,
};

/*****************************************************************************
 * $disconnect                                                               *
 *****************************************************************************/

static int
cmd_disconnect(int argc, char **argv)
{
	uint16_t conn_handle;
	uint8_t reason;
	int rc;

	rc = parse_arg_all(argc - 1, argv + 1);
	if (rc != 0) {
		return rc;
	}

	conn_handle = parse_arg_uint16("conn", &rc);
	if (rc != 0) {
		console_printf("invalid 'conn' parameter\n");
		return rc;
	}

	reason = parse_arg_uint8_dflt("reason", BLE_ERR_REM_USER_CONN_TERM, &rc);
	if (rc != 0) {
		console_printf("invalid 'reason' parameter\n");
		return rc;
	}

	rc = btshell_term_conn(conn_handle, reason);
	if (rc != 0) {
		console_printf("error terminating connection; rc=%d\n", rc);
		return rc;
	}

	return 0;
}

static int
cmd_show_conn(int argc, char **argv)
{
	struct ble_gap_conn_desc conn_desc;
	struct btshell_conn *conn;
	int rc;
	int i;

	for (i = 0; i < btshell_num_conns; i++) {
		conn = btshell_conns + i;

		rc = ble_gap_conn_find(conn->handle, &conn_desc);
		if (rc == 0) {
			print_conn_desc(&conn_desc);
		}
	}

	return 0;
}


static int
cmd_show_addr(int argc, char **argv)
{
	uint8_t id_addr[6];
	int rc;

	console_printf("public_id_addr=");
	rc = ble_hs_id_copy_addr(BLE_ADDR_PUBLIC, id_addr, NULL);
	if (rc == 0) {
		print_addr(id_addr);
	} else {
		console_printf("none");
	}

	console_printf(" random_id_addr=");
	rc = ble_hs_id_copy_addr(BLE_ADDR_RANDOM, id_addr, NULL);
	if (rc == 0) {
		print_addr(id_addr);
	} else {
		console_printf("none");
	}
	console_printf("\n");

	return 0;
}

static const struct shell_param disconnect_params[] = {
	{"conn", "connection handle parameter, usage: =<UINT16>"},
	{"reason", "disconnection reason, usage: =[UINT8], default: 19 (remote user terminated connection)"},
	{NULL, NULL}
};

static const struct shell_cmd_help disconnect_help = {
	.summary = "disconnect command",
	.usage = NULL,
	.params = disconnect_params,
};

/*****************************************************************************
 * $set-scan-opts                                                            *
 *****************************************************************************/

static struct btshell_scan_opts g_scan_opts = {
	.limit = UINT16_MAX,
	.ignore_legacy = 0,
	.periodic_only = 0,
	.raw_data_display = 0,
	.name_filter_len = 0,
	.peer = {
		.type = 0,
		.val = {0, 0, 0, 0, 0, 0},
	}
};

static int
cmd_set_scan_opts(int argc, char **argv)
{
	char *name_filter;
	int rc;

	rc = parse_arg_all(argc - 1, argv + 1);
	if (rc != 0) {
		return rc;
	}

	g_scan_opts.limit = parse_arg_uint16_dflt("decode_limit", UINT16_MAX, &rc);
	if (rc != 0) {
		console_printf("invalid 'decode_limit' parameter\n");
		return rc;
	}

#if MYNEWT_VAL(BLE_EXT_ADV)
	g_scan_opts.ignore_legacy = parse_arg_bool_dflt("ignore_legacy", 0, &rc);
	if (rc != 0) {
		console_printf("invalid 'ignore_legacy' parameter\n");
		return rc;
	}
#endif

	g_scan_opts.raw_data_display = parse_arg_bool_dflt("raw_data_display", 0, &rc);
	if (rc != 0) {
		console_printf("invalid 'raw_data_display' parameter\n");
		return rc;
	}

	name_filter = parse_arg_extract("name_filter");
	if (name_filter) {
		strncpy(g_scan_opts.name_filter, name_filter, NAME_FILTER_LEN_MAX);
		g_scan_opts.name_filter[NAME_FILTER_LEN_MAX - 1] = '\0';
	} else {
		g_scan_opts.name_filter[0] = '\0';
	}
	g_scan_opts.name_filter_len = strlen(g_scan_opts.name_filter);

#ifdef CONFIG_NIMBLE_PTS
	rc = parse_pts_addr(&g_scan_opts.peer);
	if (rc != 0) {
		memset(&g_scan_opts.peer, 0, sizeof(ble_addr_t));
		rc = 0;
	}
#endif

	return rc;
}

static const struct shell_param set_scan_opts_params[] = {
	{"decode_limit", "usage: =[0-UINT16_MAX], default: UINT16_MAX"},
#if MYNEWT_VAL(BLE_EXT_ADV)
	{"ignore_legacy", "usage: =[0-1], default: 0"},
#endif
	{"name_filter", "usage: =name, default: {none}"},
#ifdef CONFIG_NIMBLE_PTS
	{"pts", "usage: =[any] will auto fill pts address/type"},
#endif
	{NULL, NULL}
};

static const struct shell_cmd_help set_scan_opts_help = {
	.summary = "set scan options",
	.usage = NULL,
	.params = set_scan_opts_params,
};

/*****************************************************************************
 * $scan                                                                     *
 *****************************************************************************/

static const struct kv_pair cmd_scan_filt_policies[] = {
	{ "no_wl", BLE_HCI_SCAN_FILT_NO_WL },
	{ "use_wl", BLE_HCI_SCAN_FILT_USE_WL },
	{ "no_wl_inita", BLE_HCI_SCAN_FILT_NO_WL_INITA },
	{ "use_wl_inita", BLE_HCI_SCAN_FILT_USE_WL_INITA },
	{ NULL }
};

static struct kv_pair cmd_scan_ext_types[] = {
	{ "none",       0x00 },
	{ "1M",         0x01 },
	{ "coded",      0x02 },
	{ "both",       0x03 },
	{ NULL }
};

static struct btshell_scan_opts g_scan_opts;

static int
cmd_scan(int argc, char **argv)
{
	struct ble_gap_disc_params params = {0};
	struct ble_gap_ext_disc_params uncoded = {0};
	struct ble_gap_ext_disc_params coded = {0};
	uint8_t extended;
	int32_t duration_ms;
	uint8_t own_addr_type;
	uint16_t duration;
	uint16_t period;
	int rc;

	rc = parse_arg_all(argc - 1, argv + 1);
	if (rc != 0) {
		return rc;
	}

	if (argc > 1 && strcmp(argv[1], "cancel") == 0) {
		rc = btshell_scan_cancel();
		if (rc != 0) {
			console_printf("scan cancel fail: %d\n", rc);
			return rc;
		}
		return 0;
	}

	extended = parse_arg_kv_dflt("extended", cmd_scan_ext_types, 0, &rc);
	if (rc != 0) {
		console_printf("invalid 'extended' parameter\n");
		return rc;
	}

	duration_ms = parse_arg_time_dflt("duration", 10000, BLE_HS_FOREVER, &rc);
	if (rc != 0) {
		console_printf("invalid 'duration' parameter\n");
		return rc;
	}

	params.limited = parse_arg_bool_dflt("limited", 0, &rc);
	if (rc != 0) {
		console_printf("invalid 'limited' parameter\n");
		return rc;
	}

#ifdef CONFIG_NIMBLE_PTS
	params.observation = parse_arg_bool_dflt("observation", 0, &rc);
	if (rc != 0) {
		console_printf("invalid 'observation' parameter\n");
		return rc;
	}
#endif

	params.passive = parse_arg_bool_dflt("passive", 0, &rc);
	if (rc != 0) {
		console_printf("invalid 'passive' parameter\n");
		return rc;
	}

	params.itvl = parse_arg_time_dflt("interval", 625, 0, &rc);
	if (rc != 0) {
		console_printf("invalid 'interval' parameter\n");
		return rc;
	}

	params.window = parse_arg_time_dflt("window", 625, 0, &rc);
	if (rc != 0) {
		console_printf("invalid 'window' parameter\n");
		return rc;
	}

	params.filter_policy = parse_arg_kv_dflt("filter", cmd_scan_filt_policies,
			BLE_HCI_SCAN_FILT_NO_WL, &rc);
	if (rc != 0) {
		console_printf("invalid 'filter' parameter\n");
		return rc;
	}

	params.filter_duplicates = parse_arg_bool_dflt("nodups", 0, &rc);
	if (rc != 0) {
		console_printf("invalid 'nodups' parameter\n");
		return rc;
	}

	own_addr_type = parse_arg_kv_dflt("own_addr_type", cmd_own_addr_types,
			BLE_OWN_ADDR_PUBLIC, &rc);
	if (rc != 0) {
		console_printf("invalid 'own_addr_type' parameter\n");
		return rc;
	}

	if (extended == 0) {
		rc = btshell_scan(own_addr_type, duration_ms, &params, &g_scan_opts);
		if (rc != 0) {
			console_printf("error scanning; rc=%d\n", rc);
			return rc;
		}

		return 0;
	}

	/* Copy above parameters to uncoded params */
	uncoded.passive = params.passive;
	uncoded.itvl = params.itvl;
	uncoded.window = params.window;

	duration = parse_arg_time_dflt("extended_duration", 10000, 0, &rc);
	if (rc != 0) {
		console_printf("invalid 'extended_duration' parameter\n");
		return rc;
	}

	period = parse_arg_time_dflt("extended_period", 1280000, 0, &rc);
	if (rc != 0) {
		console_printf("invalid 'extended_period' parameter\n");
		return rc;
	}

	coded.itvl = parse_arg_time_dflt("longrange_interval", 625, 0, &rc);
	if (rc != 0) {
		console_printf("invalid 'longrange_interval' parameter\n");
		return rc;
	}

	coded.window = parse_arg_time_dflt("longrange_window", 625, 0, &rc);
	if (rc != 0) {
		console_printf("invalid 'longrange_window' parameter\n");
		return rc;
	}

	coded.passive = parse_arg_uint16_dflt("longrange_passive", 0, &rc);
	if (rc != 0) {
		console_printf("invalid 'longrange_passive' parameter\n");
		return rc;
	}

	switch (extended) {
		case 0x01:
			rc = btshell_ext_scan(own_addr_type, duration, period,
					params.filter_duplicates, params.filter_policy,
					params.limited, &uncoded, NULL,
					&g_scan_opts);
			break;
		case 0x02:
			rc = btshell_ext_scan(own_addr_type, duration, period,
					params.filter_duplicates, params.filter_policy,
					params.limited, NULL, &coded,
					&g_scan_opts);
			break;
		case 0x03:
			rc = btshell_ext_scan(own_addr_type, duration, period,
					params.filter_duplicates, params.filter_policy,
					params.limited, &uncoded, &coded,
					&g_scan_opts);
			break;
		default:
			assert(0);
			break;
	}

	if (rc != 0) {
		console_printf("error scanning; rc=%d\n", rc);
	}

	return rc;
}

static const struct shell_param scan_params[] = {
	{"cancel", "cancel scan procedure"},
	{"extended", "usage: =[none|1M|coded|both], default: none"},
	{"duration", "usage: =[1-INT32_MAX], default: INT32_MAX"},
	{"limited", "usage: =[0-1], default: 0"},
#ifdef CONFIG_NIMBLE_PTS
	{"observation", "usage: =[0-1], default: 0"},
#endif
	{"passive", "usage: =[0-1], default: 0"},
	{"interval", "usage: =[0-UINT16_MAX], default: 0"},
	{"window", "usage: =[0-UINT16_MAX], default: 0"},
	{"filter", "usage: =[no_wl|use_wl|no_wl_inita|use_wl_inita], default: no_wl"},
	{"nodups", "usage: =[0-1], default: 0"},
	{"own_addr_type", "usage: =[public|random|rpa_pub|rpa_rnd], default: public"},
	{"extended_duration", "usage: =[0-UINT16_MAX], default: 0"},
	{"extended_period", "usage: =[0-UINT16_MAX], default: 0"},
	{"longrange_interval", "usage: =[0-UINT16_MAX], default: 0"},
	{"longrange_window", "usage: =[0-UINT16_MAX], default: 0"},
	{"longrange_passive", "usage: =[0-1], default: 0"},
	{NULL, NULL}
};

static const struct shell_cmd_help scan_help = {
	.summary = "start/stop scan procedure with specific parameters",
	.usage = NULL,
	.params = scan_params,
};

/*****************************************************************************
 * $set                                                                      *
 *****************************************************************************/

static int
cmd_set_pts_addr(void)
{
	ble_addr_t addr;
	int rc;
	int fd;
	int ret;

	rc = parse_dev_addr("pts_", cmd_addr_type, &addr);
	if (rc != 0) {
		console_printf("invalid 'addr' parameter\n");
		return rc;
	}

	fd = open("/ble_hs/pts_addr", O_CREAT | O_WRONLY);
	if (fd < 0) {
		return EINVAL;
	}

	ret = write(fd, &addr, sizeof(addr));
	if (ret == sizeof(addr)) {
		rc = 0;
	} else {
		rc = EINVAL;
	}

	close(fd);

	return rc;
}

static int
cmd_set_addr(void)
{
	ble_addr_t addr;
	int rc;

	rc = parse_dev_addr(NULL, cmd_addr_type, &addr);
	if (rc != 0) {
		console_printf("invalid 'addr' parameter\n");
		return rc;
	}

	switch (addr.type) {
#if MYNEWT_VAL(BLE_CONTROLLER)
		case BLE_ADDR_PUBLIC:
			/* We shouldn't be writing to the controller's address (g_dev_addr).
			 * There is no standard way to set the local public address, so this is
			 * our only option at the moment.
			 */
			memcpy(g_dev_addr, addr.val, 6);
			ble_hs_id_set_pub(g_dev_addr);
			break;
#endif

		case BLE_ADDR_RANDOM:
			rc = ble_hs_id_set_rnd(addr.val);
			if (rc != 0) {
				return rc;
			}
			break;

		default:
			return BLE_HS_EUNKNOWN;
	}

	return 0;
}

static int
cmd_set(int argc, char **argv)
{
	uint16_t mtu;
	uint8_t irk[16];
	int good = 0;
	int rc;

	rc = parse_arg_all(argc - 1, argv + 1);
	if (rc != 0) {
		return rc;
	}

	rc = parse_arg_find_idx("addr");
	if (rc != -1) {
		rc = cmd_set_addr();
		if (rc != 0) {
			return rc;
		}
		good = 1;
	}

#ifdef CONFIG_NIMBLE_PTS
	rc = parse_arg_find_idx("pts_addr");
	if (rc != -1) {
		rc = cmd_set_pts_addr();
		if (rc != 0) {
			return rc;
		}
		good = 1;
	}
#endif

	mtu = parse_arg_uint16("mtu", &rc);
	if (rc == 0) {
		rc = ble_att_set_preferred_mtu(mtu);
		if (rc == 0) {
			good = 1;
		}
	} else if (rc != ENOENT) {
		console_printf("invalid 'mtu' parameter\n");
		return rc;
	}

	rc = parse_arg_byte_stream_exact_length("irk", irk, 16);
	if (rc == 0) {
		good = 1;
		ble_hs_pvcy_set_our_irk(irk);
	} else if (rc != ENOENT) {
		console_printf("invalid 'irk' parameter\n");
		return rc;
	}

	if (!good) {
		console_printf("Error: no valid settings specified\n");
		return -1;
	}

	return 0;
}

static const struct shell_param set_params[] = {
	{"addr", "set device address, usage: =[XX:XX:XX:XX:XX:XX]"},
	{"addr_type", "set device address type, usage: =[public|random], default: public"},
	{"pts_addr", "set device address, usage: =[XX:XX:XX:XX:XX:XX]"},
	{"pts_addr_type", "set device address type, usage: =[public|random], default: public"},
	{"mtu", "Maximum Transimssion Unit, usage: =[0-UINT16_MAX]"},
	{"irk", "Identity Resolving Key, usage: =[XX:XX...], len=16 octets"},
	{NULL, NULL}
};

static const struct shell_cmd_help set_help = {
	.summary = "set device parameters",
	.usage = NULL,
	.params = set_params,
};

/*****************************************************************************
 * $set-adv-data                                                             *
 *****************************************************************************/

#define CMD_ADV_DATA_MAX_UUIDS16                8
#define CMD_ADV_DATA_MAX_UUIDS32                8
#define CMD_ADV_DATA_MAX_UUIDS128               2
#define CMD_ADV_DATA_MAX_PUBLIC_TGT_ADDRS       8
#define CMD_ADV_DATA_SVC_DATA_UUID16_MAX_LEN    BLE_HS_ADV_MAX_FIELD_SZ
#define CMD_ADV_DATA_SVC_DATA_UUID32_MAX_LEN    BLE_HS_ADV_MAX_FIELD_SZ
#define CMD_ADV_DATA_SVC_DATA_UUID128_MAX_LEN   BLE_HS_ADV_MAX_FIELD_SZ
#define CMD_ADV_DATA_URI_MAX_LEN                BLE_HS_ADV_MAX_FIELD_SZ
#define CMD_ADV_DATA_MFG_DATA_MAX_LEN           BLE_HS_ADV_MAX_FIELD_SZ

#if MYNEWT_VAL(BLE_EXT_ADV)
static void
update_pattern(uint8_t *buf, int counter)
{
	int i;

	for (i = 0; i < 10; i += 2) {
		counter += 2;
		buf[i] = (counter / 1000) << 4 | (counter / 100 % 10);
		buf[i + 1] = (counter / 10 % 10) << 4 | (counter % 10);
	}
}
#endif

static int
cmd_set_adv_data_or_scan_rsp(int argc, char **argv, bool scan_rsp,
		bool periodic)
{
	static bssnz_t ble_uuid16_t uuids16[CMD_ADV_DATA_MAX_UUIDS16];
	static bssnz_t ble_uuid32_t uuids32[CMD_ADV_DATA_MAX_UUIDS32];
	static bssnz_t ble_uuid128_t uuids128[CMD_ADV_DATA_MAX_UUIDS128];
	static bssnz_t uint8_t
		public_tgt_addrs[CMD_ADV_DATA_MAX_PUBLIC_TGT_ADDRS]
		[BLE_HS_ADV_PUBLIC_TGT_ADDR_ENTRY_LEN];
	static bssnz_t uint8_t slave_itvl_range[BLE_HS_ADV_SLAVE_ITVL_RANGE_LEN];
	static bssnz_t uint8_t
		svc_data_uuid16[CMD_ADV_DATA_SVC_DATA_UUID16_MAX_LEN];
	static bssnz_t uint8_t
		svc_data_uuid32[CMD_ADV_DATA_SVC_DATA_UUID32_MAX_LEN];
	static bssnz_t uint8_t
		svc_data_uuid128[CMD_ADV_DATA_SVC_DATA_UUID128_MAX_LEN];
	static bssnz_t uint8_t uri[CMD_ADV_DATA_URI_MAX_LEN];
	static bssnz_t uint8_t mfg_data[CMD_ADV_DATA_MFG_DATA_MAX_LEN];
	struct ble_hs_adv_fields adv_fields;
	uint32_t uuid32;
	uint16_t uuid16;
	uint8_t uuid128[16];
	uint8_t public_tgt_addr[BLE_HS_ADV_PUBLIC_TGT_ADDR_ENTRY_LEN];
	uint8_t eddystone_url_body_len;
	uint8_t eddystone_url_suffix;
	uint8_t eddystone_url_scheme;
	int8_t eddystone_measured_power = 0;
	char eddystone_url_body[BLE_EDDYSTONE_URL_MAX_LEN];
	char *eddystone_url_full;
	int svc_data_uuid16_len;
	int svc_data_uuid32_len;
	int svc_data_uuid128_len;
	int uri_len;
	int mfg_data_len;
	int tmp;
	int rc;
#if MYNEWT_VAL(BLE_EXT_ADV)
	uint8_t instance;
	uint8_t extra_data[10];
	uint16_t counter;
	uint16_t extra_data_len;
	struct os_mbuf *adv_data;
#endif

	/* cannot set scan rsp for periodic */
	if (scan_rsp && periodic) {
		return -1;
	}

	memset(&adv_fields, 0, sizeof adv_fields);

	rc = parse_arg_all(argc - 1, argv + 1);
	if (rc != 0) {
		return rc;
	}

#if MYNEWT_VAL(BLE_EXT_ADV)
	instance = parse_arg_uint8_dflt("instance", 0, &rc);
	if (rc != 0 || instance >= BLE_ADV_INSTANCES) {
		console_printf("invalid instance\n");
		return rc;
	}
#endif

#ifdef CONFIG_NIMBLE_PTS
	uint8_t raw_data[BLE_HCI_MAX_ADV_DATA_LEN];
	int raw_data_len;
	rc = parse_arg_byte_stream("raw_data", BLE_HCI_MAX_ADV_DATA_LEN,
			raw_data, &raw_data_len);
	if (rc == 0) {
		if (scan_rsp) {
			rc = ble_gap_adv_rsp_set_data(raw_data, raw_data_len);
		} else {
			rc = ble_gap_adv_set_data(raw_data, raw_data_len);
		}
		return rc;
	}
#endif

	tmp = parse_arg_uint8("flags", &rc);
	if (rc == 0) {
		adv_fields.flags = tmp;
	} else if (rc != ENOENT) {
		console_printf("invalid 'flags' parameter\n");
		return rc;
	}

	while (1) {
		uuid16 = parse_arg_uint16("uuid16", &rc);
		if (rc == 0) {
			if (adv_fields.num_uuids16 >= CMD_ADV_DATA_MAX_UUIDS16) {
				console_printf("invalid 'uuid16' parameter\n");
				return EINVAL;
			}
			uuids16[adv_fields.num_uuids16] = (ble_uuid16_t) BLE_UUID16_INIT(uuid16);
			adv_fields.num_uuids16++;
		} else if (rc == ENOENT) {
			break;
		} else {
			console_printf("invalid 'uuid16' parameter\n");
			return rc;
		}
	}
	if (adv_fields.num_uuids16 > 0) {
		adv_fields.uuids16 = uuids16;
	}

	tmp = parse_arg_bool_dflt("uuids16_is_complete", 0, &rc);
	if (rc != 0) {
		console_printf("invalid 'uuids16_is_complete' parameter\n");
		return rc;
	}

	if (tmp) {
		adv_fields.uuids16_is_complete = 1;
	}

	while (1) {
		uuid32 = parse_arg_uint32("uuid32", &rc);
		if (rc == 0) {
			if (adv_fields.num_uuids32 >= CMD_ADV_DATA_MAX_UUIDS32) {
				console_printf("invalid 'uuid32' parameter\n");
				return EINVAL;
			}
			uuids32[adv_fields.num_uuids32] = (ble_uuid32_t) BLE_UUID32_INIT(uuid32);
			adv_fields.num_uuids32++;
		} else if (rc == ENOENT) {
			break;
		} else {
			console_printf("invalid 'uuid32' parameter\n");
			return rc;
		}
	}
	if (adv_fields.num_uuids32 > 0) {
		adv_fields.uuids32 = uuids32;
	}

	tmp = parse_arg_bool_dflt("uuids32_is_complete", 0, &rc);
	if (rc != 0) {
		console_printf("invalid 'uuids32_is_complete' parameter\n");
		return rc;
	}

	if (tmp) {
		adv_fields.uuids32_is_complete = 1;
	}

	while (1) {
		rc = parse_arg_byte_stream_exact_length("uuid128", uuid128, 16);
		if (rc == 0) {
			if (adv_fields.num_uuids128 >= CMD_ADV_DATA_MAX_UUIDS128) {
				console_printf("invalid 'uuid128' parameter\n");
				return EINVAL;
			}
			ble_uuid_init_from_buf((ble_uuid_any_t *) &uuids128[adv_fields.num_uuids128],
					uuid128, 16);
			adv_fields.num_uuids128++;
		} else if (rc == ENOENT) {
			break;
		} else {
			console_printf("invalid 'uuid128' parameter\n");
			return rc;
		}
	}
	if (adv_fields.num_uuids128 > 0) {
		adv_fields.uuids128 = uuids128;
	}

	tmp = parse_arg_bool_dflt("uuids128_is_complete", 0, &rc);
	if (rc != 0) {
		console_printf("invalid 'uuids128_is_complete' parameter\n");
		return rc;
	}

	if (tmp) {
		adv_fields.uuids128_is_complete = 1;
	}

	adv_fields.name = (uint8_t *)parse_arg_extract("name");
	if (adv_fields.name != NULL) {
		adv_fields.name_len = strlen((char *)adv_fields.name);
	}

	tmp = parse_arg_bool_dflt("name_is_complete", 0, &rc);
	if (rc != 0) {
		console_printf("invalid 'name_is_complete' parameter\n");
		return rc;
	}

	if (tmp) {
		adv_fields.name_is_complete = 1;
	}

	tmp = parse_arg_long_bounds("tx_power_level", INT8_MIN, INT8_MAX, &rc);
	if (rc == 0) {
		adv_fields.tx_pwr_lvl = tmp;
		adv_fields.tx_pwr_lvl_is_present = 1;
	} else if (rc != ENOENT) {
		console_printf("invalid 'tx_power_level' parameter\n");
		return rc;
	}

	rc = parse_arg_byte_stream_exact_length("slave_interval_range",
			slave_itvl_range,
			BLE_HS_ADV_SLAVE_ITVL_RANGE_LEN);
	if (rc == 0) {
		adv_fields.slave_itvl_range = slave_itvl_range;
	} else if (rc != ENOENT) {
		console_printf("invalid 'slave_interval_range' parameter\n");
		return rc;
	}

	rc = parse_arg_byte_stream("service_data_uuid16",
			CMD_ADV_DATA_SVC_DATA_UUID16_MAX_LEN,
			svc_data_uuid16, &svc_data_uuid16_len);
	if (rc == 0) {
		adv_fields.svc_data_uuid16 = svc_data_uuid16;
		adv_fields.svc_data_uuid16_len = svc_data_uuid16_len;
	} else if (rc != ENOENT) {
		console_printf("invalid 'service_data_uuid16' parameter\n");
		return rc;
	}

	while (1) {
		rc = parse_arg_byte_stream_exact_length(
				"public_target_address", public_tgt_addr,
				BLE_HS_ADV_PUBLIC_TGT_ADDR_ENTRY_LEN);
		if (rc == 0) {
			if (adv_fields.num_public_tgt_addrs >=
					CMD_ADV_DATA_MAX_PUBLIC_TGT_ADDRS) {

				console_printf("invalid 'public_target_address' parameter\n");
				return EINVAL;
			}
			memcpy(public_tgt_addrs[adv_fields.num_public_tgt_addrs],
					public_tgt_addr, BLE_HS_ADV_PUBLIC_TGT_ADDR_ENTRY_LEN);
			adv_fields.num_public_tgt_addrs++;
		} else if (rc == ENOENT) {
			break;
		} else {
			console_printf("invalid 'public_target_address' parameter\n");
			return rc;
		}
	}
	if (adv_fields.num_public_tgt_addrs > 0) {
		adv_fields.public_tgt_addr = (void *)public_tgt_addrs;
	}

	adv_fields.appearance = parse_arg_uint16("appearance", &rc);
	if (rc == 0) {
		adv_fields.appearance_is_present = 1;
	} else if (rc != ENOENT) {
		console_printf("invalid 'appearance' parameter\n");
		return rc;
	}

	adv_fields.adv_itvl = parse_arg_uint16("advertising_interval", &rc);
	if (rc == 0) {
		adv_fields.adv_itvl_is_present = 1;
	} else if (rc != ENOENT) {
		console_printf("invalid 'advertising_interval' parameter\n");
		return rc;
	}

	rc = parse_arg_byte_stream("service_data_uuid32",
			CMD_ADV_DATA_SVC_DATA_UUID32_MAX_LEN,
			svc_data_uuid32, &svc_data_uuid32_len);
	if (rc == 0) {
		adv_fields.svc_data_uuid32 = svc_data_uuid32;
		adv_fields.svc_data_uuid32_len = svc_data_uuid32_len;
	} else if (rc != ENOENT) {
		console_printf("invalid 'service_data_uuid32' parameter\n");
		return rc;
	}

	rc = parse_arg_byte_stream("service_data_uuid128",
			CMD_ADV_DATA_SVC_DATA_UUID128_MAX_LEN,
			svc_data_uuid128, &svc_data_uuid128_len);
	if (rc == 0) {
		adv_fields.svc_data_uuid128 = svc_data_uuid128;
		adv_fields.svc_data_uuid128_len = svc_data_uuid128_len;
	} else if (rc != ENOENT) {
		console_printf("invalid 'service_data_uuid128' parameter\n");
		return rc;
	}

	rc = parse_arg_byte_stream("uri", CMD_ADV_DATA_URI_MAX_LEN, uri, &uri_len);
	if (rc == 0) {
		adv_fields.uri = uri;
		adv_fields.uri_len = uri_len;
	} else if (rc != ENOENT) {
		console_printf("invalid 'uri' parameter\n");
		return rc;
	}

	rc = parse_arg_byte_stream("mfg_data", CMD_ADV_DATA_MFG_DATA_MAX_LEN,
			mfg_data, &mfg_data_len);
	if (rc == 0) {
		adv_fields.mfg_data = mfg_data;
		adv_fields.mfg_data_len = mfg_data_len;
	} else if (rc != ENOENT) {
		console_printf("invalid 'mfg_data' parameter\n");
		return rc;
	}

	tmp = parse_arg_long_bounds("eddystone_measured_power", -100, 20, &rc);
	if (rc == 0) {
		eddystone_measured_power = tmp;
	} else if (rc != ENOENT) {
		console_printf("invalid 'eddystone_measured_power' parameter\n");
		return rc;
	}

	eddystone_url_full = parse_arg_extract("eddystone_url");
	if (eddystone_url_full != NULL) {
		rc = parse_eddystone_url(eddystone_url_full, &eddystone_url_scheme,
				eddystone_url_body,
				&eddystone_url_body_len,
				&eddystone_url_suffix);
		if (rc != 0) {
			goto done;
		}

		rc = ble_eddystone_set_adv_data_url(&adv_fields, eddystone_url_scheme,
				eddystone_url_body,
				eddystone_url_body_len,
				eddystone_url_suffix,
				eddystone_measured_power);
	} else {
#if MYNEWT_VAL(BLE_EXT_ADV)
		/* Default to legacy PDUs size, mbuf chain will be increased if needed
		*/
		adv_data = os_msys_get_pkthdr(BLE_HCI_MAX_ADV_DATA_LEN, 0);
		if (!adv_data) {
			rc = ENOMEM;
			goto done;
		}

		rc = ble_hs_adv_set_fields_mbuf(&adv_fields, adv_data);
		if (rc) {
			os_mbuf_free_chain(adv_data);
			goto done;
		}

		/* Append some extra data, if requested */
		extra_data_len = parse_arg_uint16("extra_data_len", &rc);
		if (rc == 0) {
			counter = 0;
			extra_data_len = min(extra_data_len, 1650);
			while (counter < extra_data_len) {
				update_pattern(extra_data, counter);

				rc = os_mbuf_append(adv_data, extra_data,
						min(extra_data_len - counter, 10));
				if (rc) {
					os_mbuf_free_chain(adv_data);
					goto done;
				}

				counter += 10;
			}
		}

		if (scan_rsp) {
			rc = ble_gap_ext_adv_rsp_set_data(instance, adv_data);
		} else {
			rc = ble_gap_ext_adv_set_data(instance, adv_data);
		}
#else
		if (scan_rsp) {
			rc = ble_gap_adv_rsp_set_fields(&adv_fields);
		} else {
			rc = ble_gap_adv_set_fields(&adv_fields);
		}
#endif
	}
done:
	if (rc != 0) {
		console_printf("error setting advertisement data; rc=%d\n", rc);
		return rc;
	}

	return 0;
}

static int
cmd_set_adv_data(int argc, char **argv)
{
	return cmd_set_adv_data_or_scan_rsp(argc, argv, false, false);
}

static int
cmd_set_scan_rsp(int argc, char **argv)
{
	return cmd_set_adv_data_or_scan_rsp(argc, argv, true, false);
}

static const struct shell_param set_adv_data_params[] = {
	{"instance", "default: 0"},
	{"flags", "usage: =[0-UINT8_MAX]"},
	{"uuid16", "usage: =[UINT16]"},
	{"uuid16_is_complete", "usage: =[0-1], default=0"},
	{"uuid32", "usage: =[UINT32]"},
	{"uuid32_is_complete", "usage: =[0-1], default=0"},
	{"uuid128", "usage: =[XX:XX...], len=16 octets"},
	{"uuid128_is_complete", "usage: =[0-1], default=0"},
	{"tx_power_level", "usage: =[INT8_MIN-INT8_MAX]"},
	{"slave_interval_range", "usage: =[XX:XX:XX:XX]"},
	{"public_target_address", "usage: =[XX:XX:XX:XX:XX:XX]"},
	{"appearance", "usage: =[UINT16]"},
	{"name", "usage: =[string]"},
	{"name_is_complete", "usage: =[0-1], default=0"},
	{"advertising_interval", "usage: =[UINT16]"},
	{"service_data_uuid16", "usage: =[XX:XX...]"},
	{"service_data_uuid32", "usage: =[XX:XX...]"},
	{"service_data_uuid128", "usage: =[XX:XX...]"},
	{"uri", "usage: =[XX:XX...]"},
	{"mfg_data", "usage: =[XX:XX...]"},
	{"measured_power", "usage: =[-100-20]"},
	{"eddystone_url", "usage: =[string]"},
#if MYNEWT_VAL(BLE_EXT_ADV)
	{"extra_data_len", "usage: =[UINT16]"},
#endif
#ifdef CONFIG_NIMBLE_PTS
	{"raw_data", "usage: =<octets>"},
#endif
	{NULL, NULL}
};

static const struct shell_cmd_help set_adv_data_help = {
	.summary = "set advertising data",
	.usage = NULL,
	.params = set_adv_data_params,
};

static const struct shell_cmd_help set_scan_rsp_help = {
	.summary = "set scan response",
	.usage = NULL,
	.params = set_adv_data_params,
};

/*****************************************************************************
 * $set-priv-mode                                                            *
 *****************************************************************************/

static int
cmd_set_priv_mode(int argc, char **argv)
{
	ble_addr_t addr;
	uint8_t priv_mode;
	int rc;

	rc = parse_arg_all(argc - 1, argv + 1);
	if (rc != 0) {
		return rc;
	}

	rc = parse_dev_addr(NULL, cmd_addr_type, &addr);
	if (rc != 0) {
		console_printf("invalid 'addr' parameter\n");
		return rc;
	}

	priv_mode = parse_arg_uint8("mode", &rc);
	if (rc != 0) {
		console_printf("missing mode\n");
		return rc;
	}

	return ble_gap_set_priv_mode(&addr, priv_mode);
}

static const struct shell_param set_priv_mode_params[] = {
	{"addr", "set priv mode for device address, usage: =[XX:XX:XX:XX:XX:XX]"},
	{"addr_type", "set priv mode for device address type, usage: =[public|random], default: public"},
	{"mode", "set priv mode, usage: =[0-UINT8_MAX]"},
	{NULL, NULL}
};

static const struct shell_cmd_help set_priv_mode_help = {
	.summary = "set priv mode",
	.usage = NULL,
	.params = set_priv_mode_params,
};

/*****************************************************************************
 * $white-list                                                               *
 *****************************************************************************/

#define CMD_WL_MAX_SZ   8

static int
cmd_white_list(int argc, char **argv)
{
	static ble_addr_t addrs[CMD_WL_MAX_SZ];
	int addrs_cnt;
	int rc;

	rc = parse_arg_all(argc - 1, argv + 1);
	if (rc != 0) {
		return rc;
	}

	addrs_cnt = 0;

#ifdef CONFIG_NIMBLE_PTS
	rc = parse_pts_addr(&addrs[addrs_cnt]);
	if (rc == 0) {
		addrs_cnt = 1;
	}
#endif

	while (1) {
		if (addrs_cnt >= CMD_WL_MAX_SZ) {
			return EINVAL;
		}

		rc = parse_dev_addr(NULL, cmd_addr_type, &addrs[addrs_cnt]);
		if (rc == ENOENT) {
			break;
		} else if (rc != 0) {
			console_printf("invalid 'addr' parameter #%d\n", addrs_cnt + 1);
			return rc;
		}

		addrs_cnt++;
	}

	if (addrs_cnt == 0) {
		return EINVAL;
	}

	btshell_wl_set(addrs, addrs_cnt);

	return 0;
}

static const struct shell_param white_list_params[] = {
	{"addr", "white-list device addresses, usage: =[XX:XX:XX:XX:XX:XX]"},
	{"addr_type", "white-list address types, usage: =[public|random]"},
	{NULL, NULL}
};

static const struct shell_cmd_help white_list_help = {
	.summary = "set white-list addresses",
	.usage = NULL,
	.params = white_list_params,
};

/*****************************************************************************
 * $conn-rssi                                                                *
 *****************************************************************************/

static int
cmd_conn_rssi(int argc, char **argv)
{
	uint16_t conn_handle;
	int8_t rssi;
	int rc;

	rc = parse_arg_all(argc - 1, argv + 1);
	if (rc != 0) {
		return rc;
	}

	conn_handle = parse_arg_uint16("conn", &rc);
	if (rc != 0) {
		console_printf("invalid 'conn' parameter\n");
		return rc;
	}

	rc = btshell_rssi(conn_handle, &rssi);
	if (rc != 0) {
		console_printf("error reading rssi; rc=%d\n", rc);
		return rc;
	}

	console_printf("conn=%d rssi=%d\n", conn_handle, rssi);

	return 0;
}

static const struct shell_param conn_rssi_params[] = {
	{"conn", "connection handle parameter, usage: =<UINT16>"},
	{NULL, NULL}
};

static const struct shell_cmd_help conn_rssi_help = {
	.summary = "check connection rssi",
	.usage = NULL,
	.params = conn_rssi_params,
};

/*****************************************************************************
 * $conn-update-params                                                       *
 *****************************************************************************/

static int
cmd_conn_update_params(int argc, char **argv)
{
	struct ble_gap_upd_params params;
	uint16_t conn_handle;
	int rc;

	rc = parse_arg_all(argc - 1, argv + 1);
	if (rc != 0) {
		return rc;
	}

	conn_handle = parse_arg_uint16("conn", &rc);
	if (rc != 0) {
		console_printf("invalid 'conn' parameter\n");
		return rc;
	}

	params.itvl_min = parse_arg_time_dflt("interval_min", 1250,
			BLE_GAP_INITIAL_CONN_ITVL_MIN,
			&rc);
	if (rc != 0) {
		console_printf("invalid 'interval_min' parameter\n");
		return rc;
	}

	params.itvl_max = parse_arg_time_dflt("interval_max", 1250,
			BLE_GAP_INITIAL_CONN_ITVL_MAX,
			&rc);
	if (rc != 0) {
		console_printf("invalid 'interval_max' parameter\n");
		return rc;
	}

	params.latency = parse_arg_uint16_dflt("latency", 0, &rc);
	if (rc != 0) {
		console_printf("invalid 'latency' parameter\n");
		return rc;
	}

	params.supervision_timeout = parse_arg_time_dflt("timeout", 10000, 0x0100,
			&rc);
	if (rc != 0) {
		console_printf("invalid 'timeout' parameter\n");
		return rc;
	}

	params.min_ce_len = parse_arg_time_dflt("min_conn_event_len", 625,
			0x0010, &rc);
	if (rc != 0) {
		console_printf("invalid 'min_conn_event_len' parameter\n");
		return rc;
	}

	params.max_ce_len = parse_arg_time_dflt("max_conn_event_len", 625,
			0x0300, &rc);
	if (rc != 0) {
		console_printf("invalid 'max_conn_event_len' parameter\n");
		return rc;
	}

	rc = btshell_update_conn(conn_handle, &params);
	if (rc != 0) {
		console_printf("error updating connection; rc=%d\n", rc);
		return rc;
	}

	return 0;
}

static const struct shell_param conn_update_params_params[] = {
	{"conn", "conn_update_paramsion handle, usage: =<UINT16>"},
	{"interval_min", "usage: =[0-UINT16_MAX], default: 30"},
	{"interval_max", "usage: =[0-UINT16_MAX], default: 50"},
	{"latency", "usage: =[UINT16], default: 0"},
	{"timeout", "usage: =[UINT16], default: 0x0100"},
	{"min_conn_event_len", "usage: =[UINT16], default: 0x0010"},
	{"max_conn_event_len", "usage: =[UINT16], default: 0x0300"},
	{NULL, NULL}
};

static const struct shell_cmd_help conn_update_params_help = {
	.summary = "update connection parameters (time uint:ms)",
	.usage = "conn_update_params usage",
	.params = conn_update_params_params,
};

/*****************************************************************************
 * $conn-datalen                                                             *
 *****************************************************************************/

static int
cmd_conn_datalen(int argc, char **argv)
{
	uint16_t conn_handle;
	uint16_t tx_octets;
	uint16_t tx_time;
	int rc;

	rc = parse_arg_all(argc - 1, argv + 1);
	if (rc != 0) {
		return rc;
	}

	conn_handle = parse_arg_uint16("conn", &rc);
	if (rc != 0) {
		console_printf("invalid 'conn' parameter\n");
		return rc;
	}

	tx_octets = parse_arg_uint16("octets", &rc);
	if (rc != 0) {
		console_printf("invalid 'octets' parameter\n");
		return rc;
	}

	tx_time = parse_arg_uint16("time", &rc);
	if (rc != 0) {
		console_printf("invalid 'time' parameter\n");
		return rc;
	}

	rc = btshell_datalen(conn_handle, tx_octets, tx_time);
	if (rc != 0) {
		console_printf("error setting data length; rc=%d\n", rc);
		return rc;
	}

	return 0;
}

static const struct shell_param conn_datalen_params[] = {
	{"conn", "Connection handle, usage: =<UINT16>"},
	{"octets", "Max payload size to include in LL Data PDU, "
		"range=<27-251>, usage: =<UINT16>"},
	{"time", "Max number of microseconds the controller should use to tx "
		"single LL packet, range=<328-17040>, usage: =<UINT16>"},
	{NULL, NULL}
};

static const struct shell_cmd_help conn_datalen_help = {
	.summary = "set data length parameters for connection",
	.usage = NULL,
	.params = conn_datalen_params,
};

/*****************************************************************************
 * keystore                                                                  *
 *****************************************************************************/

static const struct kv_pair cmd_keystore_entry_type[] = {
	{ "msec",       BLE_STORE_OBJ_TYPE_PEER_SEC },
	{ "ssec",       BLE_STORE_OBJ_TYPE_OUR_SEC },
	{ "cccd",       BLE_STORE_OBJ_TYPE_CCCD },
	{ NULL }
};

static int
cmd_keystore_parse_keydata(int argc, char **argv, union ble_store_key *out,
		int *obj_type)
{
	int rc;

	memset(out, 0, sizeof(*out));
	*obj_type = parse_arg_kv("type", cmd_keystore_entry_type, &rc);
	if (rc != 0) {
		console_printf("invalid 'type' parameter\n");
		return rc;
	}

	switch (*obj_type) {
		case BLE_STORE_OBJ_TYPE_PEER_SEC:
		case BLE_STORE_OBJ_TYPE_OUR_SEC:
			rc = parse_dev_addr(NULL, cmd_addr_type, &out->sec.peer_addr);
			if (rc != 0) {
				console_printf("invalid 'addr' parameter\n");
				return rc;
			}

			out->sec.ediv = parse_arg_uint16("ediv", &rc);
			if (rc != 0) {
				console_printf("invalid 'ediv' parameter\n");
				return rc;
			}

			out->sec.rand_num = parse_arg_uint64("rand", &rc);
			if (rc != 0) {
				console_printf("invalid 'rand' parameter\n");
				return rc;
			}
			return 0;

		default:
			return EINVAL;
	}
}

static int
cmd_keystore_parse_valuedata(int argc, char **argv,
		int obj_type,
		union ble_store_key *key,
		union ble_store_value *out)
{
	int rc;
	int valcnt = 0;
	memset(out, 0, sizeof(*out));

	switch (obj_type) {
		case BLE_STORE_OBJ_TYPE_PEER_SEC:
		case BLE_STORE_OBJ_TYPE_OUR_SEC:
			rc = parse_arg_byte_stream_exact_length("ltk", out->sec.ltk, 16);
			if (rc == 0) {
				out->sec.ltk_present = 1;
				swap_in_place(out->sec.ltk, 16);
				valcnt++;
			} else if (rc != ENOENT) {
				console_printf("invalid 'ltk' parameter\n");
				return rc;
			}
			rc = parse_arg_byte_stream_exact_length("irk", out->sec.irk, 16);
			if (rc == 0) {
				out->sec.irk_present = 1;
				swap_in_place(out->sec.irk, 16);
				valcnt++;
			} else if (rc != ENOENT) {
				console_printf("invalid 'irk' parameter\n");
				return rc;
			}
			rc = parse_arg_byte_stream_exact_length("csrk", out->sec.csrk, 16);
			if (rc == 0) {
				out->sec.csrk_present = 1;
				swap_in_place(out->sec.csrk, 16);
				valcnt++;
			} else if (rc != ENOENT) {
				console_printf("invalid 'csrk' parameter\n");
				return rc;
			}
			out->sec.peer_addr = key->sec.peer_addr;
			out->sec.ediv = key->sec.ediv;
			out->sec.rand_num = key->sec.rand_num;
			break;
	}

	if (valcnt) {
		return 0;
	}
	return -1;
}

/*****************************************************************************
 * keystore-add                                                              *
 *****************************************************************************/

static int
cmd_keystore_add(int argc, char **argv)
{
	union ble_store_key key;
	union ble_store_value value;
	int obj_type;
	int rc;

	rc = parse_arg_all(argc - 1, argv + 1);
	if (rc != 0) {
		return rc;
	}

	rc = cmd_keystore_parse_keydata(argc, argv, &key, &obj_type);

	if (rc) {
		return rc;
	}

	rc = cmd_keystore_parse_valuedata(argc, argv, obj_type, &key, &value);

	if (rc) {
		return rc;
	}

	switch(obj_type) {
		case BLE_STORE_OBJ_TYPE_PEER_SEC:
			rc = ble_store_write_peer_sec(&value.sec);
			break;
		case BLE_STORE_OBJ_TYPE_OUR_SEC:
			rc = ble_store_write_our_sec(&value.sec);
			break;
		case BLE_STORE_OBJ_TYPE_CCCD:
			rc = ble_store_write_cccd(&value.cccd);
			break;
		default:
			rc = ble_store_write(obj_type, &value);
	}
	return rc;
}

static const struct shell_param keystore_add_params[] = {
	{"type", "entry type, usage: =<msec|ssec|cccd>"},
	{"addr_type", "usage: =<public|random>"},
	{"addr", "usage: =<XX:XX:XX:XX:XX:XX>"},
	{"ediv", "usage: =<UINT16>"},
	{"rand", "usage: =<UINT64>"},
	{"ltk", "usage: =<XX:XX:...>, len=16 octets"},
	{"irk", "usage: =<XX:XX:...>, len=16 octets"},
	{"csrk", "usage: =<XX:XX:...>, len=16 octets"},
	{NULL, NULL}
};

static const struct shell_cmd_help keystore_add_help = {
	.summary = "add data to keystore",
	.usage = NULL,
	.params = keystore_add_params,
};

/*****************************************************************************
 * keystore-del                                                              *
 *****************************************************************************/

static int
cmd_keystore_del(int argc, char **argv)
{
	union ble_store_key key;
	int obj_type;
	int rc;

	rc = parse_arg_all(argc - 1, argv + 1);
	if (rc != 0) {
		return rc;
	}

	rc = cmd_keystore_parse_keydata(argc, argv, &key, &obj_type);

	if (rc) {
		return rc;
	}
	rc = ble_store_delete(obj_type, &key);
	return rc;
}

static const struct shell_param keystore_del_params[] = {
	{"type", "entry type, usage: =<msec|ssec|cccd>"},
	{"addr_type", "usage: =<public|random>"},
	{"addr", "usage: =<XX:XX:XX:XX:XX:XX>"},
	{"ediv", "usage: =<UINT16>"},
	{"rand", "usage: =<UINT64>"},
	{NULL, NULL}
};

static const struct shell_cmd_help keystore_del_help = {
	.summary = "remove data from keystore",
	.usage = NULL,
	.params = keystore_del_params,
};

/*****************************************************************************
 * keystore-show                                                             *
 *****************************************************************************/

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

static int
cmd_keystore_show(int argc, char **argv)
{
	int type;
	int rc;

	rc = parse_arg_all(argc - 1, argv + 1);
	if (rc != 0) {
		return rc;
	}

	type = parse_arg_kv("type", cmd_keystore_entry_type, &rc);
	if (rc != 0) {
		console_printf("invalid 'type' parameter\n");
		return rc;
	}

	ble_store_iterate(type, &cmd_keystore_iterator, NULL);
	return 0;
}

static const struct shell_param keystore_show_params[] = {
	{"type", "entry type, usage: =<msec|ssec|cccd>"},
	{NULL, NULL}
};

static const struct shell_cmd_help keystore_show_help = {
	.summary = "show data in keystore",
	.usage = NULL,
	.params = keystore_show_params,
};

#if NIMBLE_BLE_SM
extern struct ble_sm_sc_oob_data oob_data_local;
extern struct ble_sm_sc_oob_data oob_data_remote;

/*****************************************************************************
 * $set-roob-sc                                                              *
 *****************************************************************************/

#ifdef CONFIG_NIMBLE_PTS
static int
cmd_set_remote_oob_sc(int argc, char **argv)
{
	int rc;

	rc = parse_arg_all(argc - 1, argv + 1);
	if (rc != 0) {
		return rc;
	}

	rc = parse_pts_oob_sc("c", oob_data_remote.c, 16);
	if (rc != 0 && rc != ENOENT) {
		console_printf("invalid 'r' parameter\n");
		return rc;
	}

	rc = parse_pts_oob_sc("r", oob_data_remote.r, 16);
	if (rc != 0 && rc != ENOENT) {
		console_printf("invalid 'r' parameter\n");
		return rc;
	}

	return rc;
}

static const struct shell_param set_remote_oob_sc_params[] = {
	{"c", "remote oob confirmation, usage: =<hex string>"},
	{"r", "remote oob random, usage: =<hex string>"},
	{NULL, NULL}
};

static const struct shell_cmd_help set_remote_oob_sc_help = {
	.summary = "set secure connection oob data",
	.usage = NULL,
	.params = set_remote_oob_sc_params,
};
#endif

/*****************************************************************************
 * $show-oob-sc                                                              *
 *****************************************************************************/

static int
cmd_show_oob_sc(int argc, char **argv)
{
	console_printf("Local OOB Data: r=");
	print_bytes(oob_data_local.r, 16);
	console_printf(" c=");
	print_bytes(oob_data_local.c, 16);
	console_printf("\n");

	console_printf("Remote OOB Data: r=");
	print_bytes(oob_data_remote.r, 16);
	console_printf(" c=");
	print_bytes(oob_data_remote.c, 16);
	console_printf("\n");

#ifdef CONFIG_NIMBLE_PTS
	int i = 15;

	printf("\nfor PTS\n");

	printf("local confirmation\n");
	for (i = 15; i >=0; i--) {
		console_printf("%02x", oob_data_local.c[i]);
	}
	console_printf("\n");

	printf("local random\n");
	for (i = 15; i >=0; i--) {
		console_printf("%02x", oob_data_local.r[i]);
	}
	console_printf("\n");

	printf("remote confirmation\n");
	for (i = 15; i >=0; i--) {
		console_printf("%02x", oob_data_remote.c[i]);
	}
	console_printf("\n");

	printf("remote random\n");
	for (i = 15; i >=0; i--) {
		console_printf("%02x", oob_data_remote.r[i]);
	}
	console_printf("\n");
#endif

	return 0;
}

/*****************************************************************************
 * $auth-passkey                                                             *
 *****************************************************************************/

static int
cmd_auth_passkey(int argc, char **argv)
{
	uint16_t conn_handle;
	struct ble_sm_io pk;
	char *yesno;
	int rc;

	rc = parse_arg_all(argc - 1, argv + 1);
	if (rc != 0) {
		return rc;
	}

	conn_handle = parse_arg_uint16("conn", &rc);
	if (rc != 0) {
		console_printf("invalid 'conn' parameter\n");
		return rc;
	}

	pk.action = parse_arg_uint16("action", &rc);
	if (rc != 0) {
		console_printf("invalid 'action' parameter\n");
		return rc;
	}

	switch (pk.action) {
		case BLE_SM_IOACT_INPUT:
		case BLE_SM_IOACT_DISP:
			/* passkey is 6 digit number */
			pk.passkey = parse_arg_long_bounds("key", 0, 999999, &rc);
			if (rc != 0) {
				console_printf("invalid 'key' parameter\n");
				return rc;
			}
			break;

		case BLE_SM_IOACT_OOB:
			rc = parse_arg_byte_stream_exact_length("oob", pk.oob, 16);
			if (rc != 0) {
				console_printf("invalid 'oob' parameter\n");
				return rc;
			}
			break;

		case BLE_SM_IOACT_NUMCMP:
			yesno = parse_arg_extract("yesno");
			if (yesno == NULL) {
				console_printf("invalid 'yesno' parameter\n");
				return EINVAL;
			}

			switch (yesno[0]) {
				case 'y':
				case 'Y':
					pk.numcmp_accept = 1;
					break;
				case 'n':
				case 'N':
					pk.numcmp_accept = 0;
					break;

				default:
					console_printf("invalid 'yesno' parameter\n");
					return EINVAL;
			}
			break;

		case BLE_SM_IOACT_OOB_SC:
#ifndef CONFIG_NIMBLE_PTS
			rc = parse_pts_oob_sc("r", oob_data_remote.r, 16);
			if (rc != 0 && rc != ENOENT) {
				console_printf("invalid 'r' parameter\n");
				return rc;
			}
			rc = parse_pts_oob_sc("c", oob_data_remote.c, 16);
			if (rc != 0 && rc != ENOENT) {
				console_printf("invalid 'r' parameter\n");
				return rc;
			}
#endif
			pk.oob_sc_data.local = &oob_data_local;
			if (ble_hs_cfg.sm_oob_data_flag) {
				pk.oob_sc_data.remote = &oob_data_remote;
			} else {
				pk.oob_sc_data.remote = NULL;
			}
			break;

		default:
			console_printf("invalid passkey action action=%d\n", pk.action);
			return EINVAL;
	}

	rc = ble_sm_inject_io(conn_handle, &pk);
	if (rc != 0) {
		console_printf("error providing passkey; rc=%d\n", rc);
		return rc;
	}

	return 0;
}

static const struct shell_param auth_passkey_params[] = {
	{"conn", "connection handle, usage: =<UINT16>"},
	{"action", "auth action type, usage: =<UINT16:0-NON, 1-OOB, 2-INPUT, 3-DISAPLY, 4-NUM COMPARE, 5-OOB SC>"},
	{"key", "usage: =[0-999999]"},
	{"oob", "usage: =[XX:XX...], len=16 octets"},
	{"yesno", "usage: =[string]"},
	{NULL, NULL}
};

static const struct shell_cmd_help auth_passkey_help = {
	.summary = "set authorization passkey options",
	.usage = NULL,
	.params = auth_passkey_params,
};

/*****************************************************************************
 * $security-pair                                                            *
 *****************************************************************************/

static int
cmd_security_pair(int argc, char **argv)
{
	uint16_t conn_handle;
	int rc;

	rc = parse_arg_all(argc - 1, argv + 1);
	if (rc != 0) {
		return rc;
	}

	conn_handle = parse_arg_uint16("conn", &rc);
	if (rc != 0) {
		console_printf("invalid 'conn' parameter\n");
		return rc;
	}

	rc = btshell_sec_pair(conn_handle);
	if (rc != 0) {
		console_printf("error initiating pairing; rc=%d\n", rc);
		return rc;
	}

	return 0;
}

static const struct shell_param security_pair_params[] = {
	{"conn", "connection handle, usage: =<UINT16>"},
	{NULL, NULL}
};

static const struct shell_cmd_help security_pair_help = {
	.summary = "start pairing procedure for connection",
	.usage = NULL,
	.params = security_pair_params,
};

/*****************************************************************************
 * $security-unpair                                                            *
 *****************************************************************************/

static int
cmd_security_unpair(int argc, char **argv)
{
	ble_addr_t peer;
	int rc;
	int oldest;

	rc = parse_arg_all(argc - 1, argv + 1);
	if (rc != 0) {
		return rc;
	}

	rc = parse_arg_bool_dflt("oldest", 0, &oldest);
	if (rc != 0) {
		console_printf("invalid 'oldest' parameter\n");
		return rc;
	}

	if (oldest) {
		rc = ble_gap_unpair_oldest_peer();
		console_printf("Unpair oldest status: 0x%02x\n", rc);
		return 0;
	}

	rc = parse_dev_addr("peer_", cmd_peer_addr_types, &peer);
	if (rc != 0) {
		console_printf("invalid peer address\n");
		return rc;
	}

	rc = ble_gap_unpair(&peer);
	if (rc != 0) {
		console_printf("error unpairing; rc=%d\n", rc);
		return rc;
	}

	return 0;
}

static const struct shell_param security_unpair_params[] = {
	{"oldest", "usage: =[true|false], default: false"},
	{"peer_addr_type", "usage: =[public|random|public_id|random_id], default: public"},
	{"peer_addr", "usage: =[XX:XX:XX:XX:XX:XX]"},
	{NULL, NULL}
};

static const struct shell_cmd_help security_unpair_help = {
	.summary = "unpair a peer device",
	.usage = NULL,
	.params = security_unpair_params,
};

/*****************************************************************************
 * $security-start                                                           *
 *****************************************************************************/

static int
cmd_security_start(int argc, char **argv)
{
	uint16_t conn_handle;
	int rc;

	rc = parse_arg_all(argc - 1, argv + 1);
	if (rc != 0) {
		return rc;
	}

	conn_handle = parse_arg_uint16("conn", &rc);
	if (rc != 0) {
		console_printf("invalid 'conn' parameter\n");
		return rc;
	}

	rc = btshell_sec_start(conn_handle);
	if (rc != 0) {
		console_printf("error starting security; rc=%d\n", rc);
		return rc;
	}

	return 0;
}

static const struct shell_param security_start_params[] = {
	{"conn", "connection handle, usage: =<UINT16>"},
	{NULL, NULL}
};

static const struct shell_cmd_help security_start_help = {
	.summary = "start security procedure for connection",
	.usage = NULL,
	.params = security_start_params,
};

/*****************************************************************************
 * $security-encryption                                                      *
 *****************************************************************************/

static int
cmd_security_encryption(int argc, char **argv)
{
	uint16_t conn_handle;
	uint16_t ediv;
	uint64_t rand_val;
	uint8_t ltk[16];
	uint8_t key_size;
	int rc;
	int auth;

	rc = parse_arg_all(argc - 1, argv + 1);
	if (rc != 0) {
		return rc;
	}

	conn_handle = parse_arg_uint16("conn", &rc);
	if (rc != 0) {
		console_printf("invalid 'conn' parameter\n");
		return rc;
	}

	ediv = parse_arg_uint16("ediv", &rc);
	if (rc == ENOENT) {
		rc = btshell_sec_restart(conn_handle, 0, NULL, 0, 0, 0);
	} else {
		rand_val = parse_arg_uint64("rand", &rc);
		if (rc != 0) {
			console_printf("invalid 'rand' parameter\n");
			return rc;
		}

		auth = parse_arg_bool("auth", &rc);
		if (rc != 0) {
			console_printf("invalid 'auth' parameter\n");
			return rc;
		}

		key_size = parse_arg_uint8("key_size", &rc);
		if (rc != 0) {
			console_printf("invalid 'key_size' parameter\n");
			return rc;
		}

		rc = parse_arg_byte_stream_exact_length("ltk", ltk, 16);
		if (rc != 0) {
			console_printf("invalid 'ltk' parameter\n");
			return rc;
		}

		rc = btshell_sec_restart(conn_handle, key_size,
				ltk, ediv, rand_val, auth);
	}

	if (rc != 0) {
		console_printf("error initiating encryption; rc=%d\n", rc);
		return rc;
	}

	return 0;
}

static const struct shell_param security_encryption_params[] = {
	{"conn", "connection handle, usage: =<UINT16>"},
	{"ediv", "usage: =[UINT16]"},
	{"rand", "usage: =[UINT64]"},
	{"auth", "usage: =[0-1]"},
	{"ltk", "usage: =[XX:XX...], len=16 octets"},
	{NULL, NULL}
};

static const struct shell_cmd_help security_encryption_help = {
	.summary = "start encryption procedure for connection",
	.usage = NULL,
	.params = security_encryption_params,
};

/*****************************************************************************
 * $security-set-data                                                        *
 *****************************************************************************/

static int
cmd_security_set_data(int argc, char **argv)
{
	uint8_t tmp;
	int good;
	int rc;

	good = 0;

	rc = parse_arg_all(argc - 1, argv + 1);
	if (rc != 0) {
		return rc;
	}

	tmp = parse_arg_bool("oob_flag", &rc);
	if (rc == 0) {
		ble_hs_cfg.sm_oob_data_flag = tmp;
		good++;
	} else if (rc != ENOENT) {
		console_printf("invalid 'oob_flag' parameter\n");
		return rc;
	}

	tmp = parse_arg_bool("mitm_flag", &rc);
	if (rc == 0) {
		good++;
		ble_hs_cfg.sm_mitm = tmp;
	} else if (rc != ENOENT) {
		console_printf("invalid 'mitm_flag' parameter\n");
		return rc;
	}

	tmp = parse_arg_uint8("io_capabilities", &rc);
	if (rc == 0) {
		good++;
		ble_hs_cfg.sm_io_cap = tmp;
	} else if (rc != ENOENT) {
		console_printf("invalid 'io_capabilities' parameter\n");
		return rc;
	}

	tmp = parse_arg_uint8("our_key_dist", &rc);
	if (rc == 0) {
		good++;
		ble_hs_cfg.sm_our_key_dist = tmp;
	} else if (rc != ENOENT) {
		console_printf("invalid 'our_key_dist' parameter\n");
		return rc;
	}

	tmp = parse_arg_uint8("their_key_dist", &rc);
	if (rc == 0) {
		good++;
		ble_hs_cfg.sm_their_key_dist = tmp;
	} else if (rc != ENOENT) {
		console_printf("invalid 'their_key_dist' parameter\n");
		return rc;
	}

	tmp = parse_arg_bool("bonding", &rc);
	if (rc == 0) {
		good++;
		ble_hs_cfg.sm_bonding = tmp;
	} else if (rc != ENOENT) {
		console_printf("invalid 'bonding' parameter\n");
		return rc;
	}

	tmp = parse_arg_bool("sc", &rc);
	if (rc == 0) {
		good++;
		ble_hs_cfg.sm_sc = tmp;
	} else if (rc != ENOENT) {
		console_printf("invalid 'sc' parameter\n");
		return rc;
	}

#ifdef CONFIG_NIMBLE_PTS
	tmp = parse_arg_bool("sc_only", &rc);
	if (rc == 0) {
		good++;
		ble_hs_cfg.sm_sc_only = tmp;
	} else if (rc != ENOENT) {
		console_printf("invalid 'sc_only' parameter\n");
		return rc;
	}
#endif

	if (!good) {
		console_printf("Error: no valid settings specified\n");
		return -1;
	}

	return 0;
}

static const struct shell_param security_set_data_params[] = {
	{"oob_flag", "usage: =[0-1]"},
	{"mitm_flag", "usage: =[0-1]"},
	{"io_capabilities", "usage: =[UINT8:0-DISPALY ONLY, 2-KEYBOARD ONLY, 3-NO IO, 4-DISPLAY & KEYBOARD]"},
	{"our_key_dist", "usage: =[UINT8:0-non, 0b-LTK, 1b-IRK, 2b-CSRK]"},
	{"their_key_dist", "usage: =[UINT8::0-non, 0b-LTK, 1b-IRK, 2b-CSRK]"},
	{"bonding", "usage: =[0-1]"},
	{"sc", "usage: =[0-1]"},
#ifdef CONFIG_NIMBLE_PTS
	{"sc_only", "usage: =[0-1]"},
#endif
	{NULL, NULL}
};

static const struct shell_cmd_help security_set_data_help = {
	.summary = "set security data",
	.usage = NULL,
	.params = security_set_data_params,
};
#endif

/*****************************************************************************
 * $test-tx                                                                  *
 *                                                                           *
 * Command to transmit 'num' packets of size 'len' at rate 'r' to
 * handle 'h' Note that length must be <= 251. The rate is in msecs.
 *
 *****************************************************************************/

static int
cmd_test_tx(int argc, char **argv)
{
	int rc;
	uint16_t conn;
	uint16_t len;
	uint16_t rate;
	uint16_t num;
	uint8_t stop;

	rc = parse_arg_all(argc - 1, argv + 1);
	if (rc != 0) {
		return rc;
	}

	stop = parse_arg_uint8_dflt("stop", 0, &rc);
	if (rc != 0) {
		console_printf("invalid 'stop' parameter\n");
		return rc;
	}

	if (stop) {
		btshell_tx_stop();
		return 0;
	}

	conn = parse_arg_uint16("conn", &rc);
	if (rc != 0) {
		console_printf("invalid 'conn' parameter\n");
		return rc;
	}

	len = parse_arg_uint16("length", &rc);
	if (rc != 0) {
		console_printf("invalid 'length' parameter\n");
		return rc;
	}
	if ((len > 251) || (len < 4)) {
		console_printf("error: len must be between 4 and 251, inclusive");
	}

	rate = parse_arg_uint16_dflt("rate", 1, &rc);
	if (rc != 0) {
		console_printf("invalid 'rate' parameter\n");
		return rc;
	}

	num = parse_arg_uint16_dflt("num", 1, &rc);
	if (rc != 0) {
		console_printf("invalid 'num' parameter\n");
		return rc;
	}

	rc = btshell_tx_start(conn, len, rate, num);
	return rc;
}

static const struct shell_param test_tx_params[] = {
	{"conn", "handle to tx to, usage: =<UINT16>"},
	{"length", "size of packet, usage: =<UINT16>"},
	{"rate", "rate of tx, usage: =<UINT16>, default=1"},
	{"num", "number of packets, usage: =<UINT16>, default=1"},
	{"stop", "stop sending, usage: 1 to stop, default 0"},
	{NULL, NULL}
};

static const struct shell_cmd_help test_tx_help = {
	.summary = "test packet transmission",
	.usage = NULL,
	.params = test_tx_params,
};

/*****************************************************************************
 * $phy-set                                                                  *
 *****************************************************************************/

static int
cmd_phy_set(int argc, char **argv)
{
	uint16_t conn;
	uint8_t tx_phys_mask;
	uint8_t rx_phys_mask;
	uint16_t phy_opts;
	int rc;

	rc = parse_arg_all(argc - 1, argv + 1);
	if (rc != 0) {
		return rc;
	}

	conn = parse_arg_uint16("conn", &rc);
	if (rc != 0) {
		console_printf("invalid 'conn' parameter\n");
		return rc;
	}

	tx_phys_mask = parse_arg_uint8("tx_phys_mask", &rc);
	if (rc != 0) {
		console_printf("invalid 'tx_phys_mask' parameter\n");
		return rc;
	}

	rx_phys_mask = parse_arg_uint8("rx_phys_mask", &rc);
	if (rc != 0) {
		console_printf("invalid 'rx_phys_mask' parameter\n");
		return rc;
	}

	phy_opts = parse_arg_uint16("phy_opts", &rc);
	if (rc != 0) {
		console_printf("invalid 'phy_opts' parameter\n");
		return rc;
	}

	return ble_gap_set_prefered_le_phy(conn, tx_phys_mask, rx_phys_mask,
			phy_opts);
}

static const struct shell_param phy_set_params[] = {
	{"conn", "connection handle, usage: =<UINT16>"},
	{"tx_phys_mask", "usage: =<UINT8:0b-1M, 1b-2M, 3b-coded>"},
	{"rx_phys_mask", "usage: =<UINT8:0b-1M, 1b-2M, 3b-coded>"},
	{"phy_opts", "usage: =<UINT16:0-ANY, 1-S2(125Kb), 2-S8(500Kb)>"},
	{NULL, NULL}
};

static const struct shell_cmd_help phy_set_help = {
	.summary = "set preferred PHYs",
	.usage = NULL,
	.params = phy_set_params,
};

/*****************************************************************************
 * $phy-set-default                                                          *
 *****************************************************************************/

static int
cmd_phy_set_default(int argc, char **argv)
{
	uint8_t tx_phys_mask;
	uint8_t rx_phys_mask;
	int rc;

	rc = parse_arg_all(argc - 1, argv + 1);
	if (rc != 0) {
		return rc;
	}

	tx_phys_mask = parse_arg_uint8("tx_phys_mask", &rc);
	if (rc != 0) {
		console_printf("invalid 'tx_phys_mask' parameter\n");
		return rc;
	}

	rx_phys_mask = parse_arg_uint8("rx_phys_mask", &rc);
	if (rc != 0) {
		console_printf("invalid 'rx_phys_mask' parameter\n");
		return rc;
	}

	return ble_gap_set_prefered_default_le_phy(tx_phys_mask, rx_phys_mask);
}

static const struct shell_param phy_set_default_params[] = {
	{"tx_phys_mask", "usage: =<UINT8:0b-1M, 1b-2M, 3b-coded>"},
	{"rx_phys_mask", "usage: =<UINT8:0b-1M, 1b-2M, 3b-coded>"},
	{NULL, NULL}
};

static const struct shell_cmd_help phy_set_default_help = {
	.summary = "set preferred default PHYs",
	.usage = NULL,
	.params = phy_set_default_params,
};

/*****************************************************************************
 * $phy-read                                                                 *
 *****************************************************************************/

static int
cmd_phy_read(int argc, char **argv)
{
	uint16_t conn = 0;
	uint8_t tx_phy;
	uint8_t rx_phy;
	int rc;

	rc = parse_arg_all(argc - 1, argv + 1);
	if (rc != 0) {
		return rc;
	}

	conn = parse_arg_uint16("conn", &rc);
	if (rc != 0) {
		console_printf("invalid 'conn' parameter\n");
		return rc;
	}

	rc = ble_gap_read_le_phy(conn, &tx_phy, &rx_phy);
	if (rc != 0) {
		console_printf("Could not read PHY error: %d\n", rc);
		return rc;
	}

	console_printf("TX_PHY: %d\n", tx_phy);
	console_printf("RX_PHY: %d\n", tx_phy);

	return 0;
}

static const struct shell_param phy_read_params[] = {
	{"conn", "connection handle, usage: =<UINT16>"},
	{NULL, NULL}
};

static const struct shell_cmd_help phy_read_help = {
	.summary = "read PHYs",
	.usage = NULL,
	.params = phy_read_params,
};

/*****************************************************************************
 * $dtm-rx 																	 *
 *****************************************************************************/

static int
cmd_dtm_rx(int argc, char **argv)
{
	struct ble_dtm_rx_params params;
	uint8_t channel;
	uint8_t phy;
	int rc;

	rc = parse_arg_all(argc - 1, argv + 1);
	if (rc != 0) {
		return rc;
	}

	params.channel = parse_arg_uint8_dflt("channel", 0, &rc);
	if (rc != 0 || params.channel > 39) {
		console_printf("invalid channel\n");
		return rc;
	}

	params.phy = parse_arg_uint8_dflt("phy", 1, &rc);
	if (rc != 0 || params.phy == 0 || params.phy > 3) {
		console_printf("invalid phy\n");
		return rc;
	}

	/* ignored by controller */
	params.modulation_index = 0;

	rc = ble_dtm_rx_start(&params);
	if (rc) {
		console_printf("failed to start dtm rx\n");
		return rc;
	}

	console_printf("start dtm rx (ch:%d, phy:%d)\n", params.channel, params.phy);

	return 0;
}

static const struct shell_param dtm_rx_params[] = {
	{"channel", "test rx channel, usage: =<UINT8>, default: 0"},
	{"phy", "test rx phy, usage: =<UINT8:1-1M, 2-2M, 3-coded>, default: 1"},
	{NULL, NULL}
};

static const struct shell_cmd_help dtm_rx_help = {
	.summary = "start ble dtm rx",
	.usage = NULL,
	.params = dtm_rx_params,
};

/*****************************************************************************
 * $dtm-tx																	 *
 *****************************************************************************/

static int
cmd_dtm_tx(int argc, char **argv)
{
	struct ble_dtm_tx_params params;
	uint8_t channel;
	uint8_t len;
	uint8_t payload;
	uint8_t phy;
	int rc;

	rc = parse_arg_all(argc - 1, argv + 1);
	if (rc != 0) {
		return rc;
	}

	params.channel = parse_arg_uint8_dflt("channel", 0, &rc);
	if (rc != 0 || params.channel > 39) {
		console_printf("invalid channel\n");
		return rc;
	}

	params.test_data_len = parse_arg_uint8_dflt("len", 37, &rc);
	if (rc != 0) {
		console_printf("invalid data len\n");
		return rc;
	}

	params.payload = parse_arg_uint8_dflt("payload", 0, &rc);
	if (rc != 0 || params.payload > 7) {
		console_printf("invalid payload\n");
		return rc;
	}

	params.phy = parse_arg_uint8_dflt("phy", 1, &rc);
	if (rc != 0 || params.phy == 0 || params.phy > 4) {
		console_printf("invalid phy\n");
		return rc;
	}

	rc = ble_dtm_tx_start(&params);
	if (rc) {
		console_printf("failed to start dtm tx\n");
		return rc;
	}

	console_printf("start dtm tx (ch:%d, len:%d, payload:%d, phy:%d)\n",
			params.channel,
			params.test_data_len,
			params.payload,
			params.phy);

	return 0;
}

static const struct shell_param dtm_tx_params[] = {
	{"channel", "test tx channel, usage: =<UINT8>, default: 0"},
	{"len", "test payload len, usage: =<UINT8>, default: 37"},
	{"payload", "test payload, usage: =<UINT8:0-prbs9, 1-0x0F, 2-0x55, 3-prbs15, 4-0xFF, 5-0x00, 6-0xF0, 7-0xAA>, default: 0"},
	{"phy", "test tx phy, usage: =<UINT8:1-1M, 2-2M, 3-coded s8, 4-coded s2>, default: 1"},
	{NULL, NULL}
};

static const struct shell_cmd_help dtm_tx_help = {
	.summary = "start ble dtm tx",
	.usage = NULL,
	.params = dtm_tx_params,
};

/*****************************************************************************
 * $dtm-end																	 *
 *****************************************************************************/

static const struct shell_cmd_help dtm_end_help = {
	.summary = "start ble dtm end",
	.usage = NULL,
	.params = NULL,
};

static int
cmd_dtm_end(int argc, char **argv)
{
	uint16_t num_packets;
	int rc;

	rc = ble_dtm_stop(&num_packets);
	if (rc) {
		console_printf("failed to start dtm end\n");
		return rc;
	}

	console_printf("number of packets : %d\n", num_packets);


	return 0;
}

/*****************************************************************************
 * $init                                                                     *
 *****************************************************************************/

static int
cmd_init(int argc, char **argv)
{
	btshell_init();

	return 0;
}

/*****************************************************************************
 * $gatt-discover                                                            *
 *****************************************************************************/

static const struct shell_param gatt_discover_characteristic_params[] = {
	{"conn", "connection handle, usage: =<UINT16>"},
	{"uuid", "discover by uuid, usage: =[UUID]"},
	{"start", "start handle, usage: =<UINT16>"},
	{"end", "end handle, usage: =<UINT16>"},
	{NULL, NULL}
};

static const struct shell_cmd_help gatt_discover_characteristic_help = {
	.summary = "perform characteristic discovery procedure",
	.usage = NULL,
	.params = gatt_discover_characteristic_params,
};

static const struct shell_param gatt_discover_descriptor_params[] = {
	{"conn", "connection handle, usage: =<UINT16>"},
	{"start", "start handle, usage: =<UINT16>"},
	{"end", "end handle, usage: =<UINT16>"},
	{NULL, NULL}
};

static const struct shell_cmd_help gatt_discover_descriptor_help = {
	.summary = "perform descriptor discovery procedure",
	.usage = NULL,
	.params = gatt_discover_descriptor_params,
};

static const struct shell_param gatt_discover_service_params[] = {
	{"conn", "connection handle, usage: =<UINT16>"},
	{"uuid", "discover by uuid, usage: =[UUID]"},
	{NULL, NULL}
};

static const struct shell_cmd_help gatt_discover_service_help = {
	.summary = "perform service discovery procedure",
	.usage = NULL,
	.params = gatt_discover_service_params,
};

static const struct shell_param gatt_discover_full_params[] = {
	{"conn", "connection handle, usage: =<UINT16>"},
	{NULL, NULL}
};

static const struct shell_cmd_help gatt_discover_full_help = {
	.summary = "perform full discovery procedure",
	.usage = NULL,
	.params = gatt_discover_full_params,
};

/*****************************************************************************
 * $gatt-exchange-mtu                                                        *
 *****************************************************************************/

static const struct shell_param gatt_exchange_mtu_params[] = {
	{"conn", "connection handle, usage: =<UINT16>"},
	{NULL, NULL}
};

static const struct shell_cmd_help gatt_exchange_mtu_help = {
	.summary = "perform mtu exchange procedure",
	.usage = NULL,
	.params = gatt_exchange_mtu_params,
};

/*****************************************************************************
 * $gatt-find-included-services                                              *
 *****************************************************************************/

static const struct shell_param gatt_find_included_services_params[] = {
	{"conn", "connection handle, usage: =<UINT16>"},
	{"start", "start handle, usage: =<UINT16>"},
	{"end", "end handle, usage: =<UINT16>"},
	{NULL, NULL}
};

static const struct shell_cmd_help gatt_find_included_services_help = {
	.summary = "perform find included services procedure",
	.usage = NULL,
	.params = gatt_find_included_services_params,
};

/*****************************************************************************
 * $gatt-notify                                                                *
 *****************************************************************************/

static const struct shell_param gatt_notify_params[] = {
	{"attr", "attribute handle, usage: =<UINT16>"},
	{NULL, NULL}
};

static const struct shell_cmd_help gatt_notify_help = {
	.summary = "notify about attribute value changed",
	.usage = NULL,
	.params = gatt_notify_params,
};

/*****************************************************************************
 * $gatt-read                                                                *
 *****************************************************************************/

static const struct shell_param gatt_read_params[] = {
	{"conn", "connection handle, usage: =<UINT16>"},
	{"long", "is read long, usage: =[0-1], default=0"},
	{"attr", "attribute handle, usage: =<UINT16>"},
	{"offset", "offset value, usage: =<UINT16>"},
	{"uuid", "read by uuid, usage: =[UUID]"},
	{"start", "start handle, usage: =<UINT16>"},
	{"end", "end handle, usage: =<UINT16>"},
	{NULL, NULL}
};

static const struct shell_cmd_help gatt_read_help = {
	.summary = "perform gatt read procedure",
	.usage = NULL,
	.params = gatt_read_params,
};

/*****************************************************************************
 * $gatt-service-changed                                                     *
 *****************************************************************************/

static const struct shell_param gatt_service_changed_params[] = {
	{"start", "start handle, usage: =<UINT16>"},
	{"end", "end handle, usage: =<UINT16>"},
	{NULL, NULL}
};

static const struct shell_cmd_help gatt_service_changed_help = {
	.summary = "send service changed indication",
	.usage = NULL,
	.params = gatt_service_changed_params,
};

/*****************************************************************************
 * $gatt-service-visibility                                                  *
 *****************************************************************************/

static const struct shell_param gatt_service_visibility_params[] = {
	{"handle", "usage: =<UINT16>"},
	{"visibility", "usage: =<0-1>"},
	{NULL, NULL}
};

static const struct shell_cmd_help gatt_service_visibility_help = {
	.summary = "change service visibility",
	.usage = NULL,
	.params = gatt_service_visibility_params,
};

/*****************************************************************************
 * $gatt-show                                                                *
 *****************************************************************************/

static const struct shell_param gatt_show_params[] = {
	{NULL, NULL}
};

static const struct shell_cmd_help gatt_show_help = {
	.summary = "show discovered gatt database",
	.usage = NULL,
	.params = gatt_show_params,
};

static const struct shell_cmd_help gatt_show_local_help = {
	.summary = "show local gatt database",
	.usage = NULL,
	.params = gatt_show_params,
};

static const struct shell_cmd_help gatt_show_addr_help = {
	.summary = "show device address",
	.usage = NULL,
	.params = gatt_show_params,
};

static const struct shell_cmd_help gatt_show_conn_help = {
	.summary = "show connections information",
	.usage = NULL,
	.params = gatt_show_params,
};

/*****************************************************************************
 * $gatt-write                                                                *
 *****************************************************************************/

static const struct shell_param gatt_write_params[] = {
	{"conn", "connection handle, usage: =<UINT16>"},
	{"no_rsp", "write without response, usage: =[0-1], default=0"},
	{"long", "is write long, usage: =[0-1], default=0"},
	{"attr", "attribute handle, usage: =<UINT16>"},
	{"offset", "attribute handle, usage: =<UINT16>"},
	{"value", "usage: =<octets>"},
#ifdef CONFIG_NIMBLE_PTS
	{"bytes", "number of bytes to send, usage: =<UINT16>"},
#endif
	{NULL, NULL}
};

static const struct shell_cmd_help gatt_write_help = {
	.summary = "perform gatt write procedure",
	.usage = NULL,
	.params = gatt_write_params,
};

#ifdef CONFIG_NIMBLE_PTS
/*****************************************************************************
 * $gatt-svr                                                                 *
 *****************************************************************************/
static const struct shell_param gatt_svr_params[] = {
	{"db", "database selection, usage: =<UINT8: 0-small, 1-large1, 2-large2, 3-large3>"},
	{NULL, NULL}
};

static const struct shell_cmd_help gatt_svr_help = {
	.summary = "change gatt server database",
	.usage = NULL,
	.params = gatt_svr_params,
};
#endif

#if MYNEWT_VAL(BLE_L2CAP_COC_MAX_NUM)
/*****************************************************************************
 * $l2cap-update                                                             *
 *****************************************************************************/

static const struct shell_param l2cap_update_params[] = {
	{"conn", "connection handle, usage: =<UINT16>"},
	{"interval_min", "usage: =[0-UINT16_MAX], default: 30"},
	{"interval_max", "usage: =[0-UINT16_MAX], default: 50"},
	{"latency", "usage: =[UINT16], default: 0"},
	{"timeout", "usage: =[UINT16], default: 0x0100"},
	{NULL, NULL}
};

static const struct shell_cmd_help l2cap_update_help = {
	.summary = "update l2cap parameters for connection",
	.usage = NULL,
	.params = l2cap_update_params,
};

/*****************************************************************************
 * $l2cap-create-server                                                      *
 *****************************************************************************/

static const struct shell_param l2cap_create_server_params[] = {
	{"psm", "usage: =<UINT16>"},
	{"mtu", "usage: =<UINT16> not more than BTSHELL_COC_MTU, default BTSHELL_COC_MTU"},
	{"error", "usage: used for PTS testing:"},
	{"", "0 - always accept"},
	{"", "1 - reject with insufficient authentication"},
	{"", "2 - reject with insufficient authorization"},
	{"", "3 - reject with insufficient key size"},
	{"", "4 - reject with insufficient encryption"},
	{NULL, NULL}
};

static const struct shell_cmd_help l2cap_create_server_help = {
	.summary = "create l2cap server",
	.usage = NULL,
	.params = l2cap_create_server_params,
};

/*****************************************************************************
 * $l2cap-connect                                                            *
 *****************************************************************************/

static const struct shell_param l2cap_connect_params[] = {
	{"conn", "connection handle, usage: =<UINT16>"},
	{"psm", "usage: =<UINT16>"},
	{"num", "usage: number of connection created in a row: [1-5]"},
	{"mtu", "usage: =<UINT16> not more than BTSHELL_COC_MTU, default BTSHELL_COC_MTU"},
	{NULL, NULL}
};

static const struct shell_cmd_help l2cap_connect_help = {
	.summary = "perform l2cap connect procedure",
	.usage = NULL,
	.params = l2cap_connect_params,
};

/*****************************************************************************
 * $l2cap-disconnect                                                         *
 *****************************************************************************/

static const struct shell_param l2cap_disconnect_params[] = {
	{"conn", "connection handle, usage: =<UINT16>"},
	{"idx", "usage: =<UINT16>"},
	{NULL, NULL}
};

static const struct shell_cmd_help l2cap_disconnect_help = {
	.summary = "perform l2cap disconnect procedure",
	.usage = "use gatt-show-coc to get the parameters",
	.params = l2cap_disconnect_params,
};

/*****************************************************************************
 * $l2cap-reconfig                                                           *
 *****************************************************************************/

static const struct shell_param l2cap_reconfig_params[] = {
	{"conn", "connection handle, usage: =<UINT16>"},
	{"mtu", "new mtu, usage: =<UINT16>, default: 0 (no change)"},
	{"idxs", "list of channel indexes, usage: idxs=1,3"},
	{NULL, NULL}
};

static const struct shell_cmd_help l2cap_reconfig_help = {
	.summary = "perform l2cap reconfigure procedure",
	.usage = "use gatt-show-coc to get the parameters",
	.params = l2cap_reconfig_params,
};

/*****************************************************************************
 * $l2cap-send                                                               *
 *****************************************************************************/

static const struct shell_param l2cap_send_params[] = {
	{"conn", "connection handle, usage: =<UINT16>"},
	{"idx", "usage: =<UINT16>"},
	{"bytes", "number of bytes to send, usage: =<UINT16>"},
	{NULL, NULL}
};

static const struct shell_cmd_help l2cap_send_help = {
	.summary = "perform l2cap send procedure",
	.usage = "use l2cap-show-coc to get the parameters",
	.params = l2cap_send_params,
};

/*****************************************************************************
 * $l2cap-show-coc                                                           *
 *****************************************************************************/

static const struct shell_param l2cap_show_coc_params[] = {
	{NULL, NULL}
};

static const struct shell_cmd_help l2cap_show_coc_help = {
	.summary = "show coc information",
	.usage = NULL,
	.params = l2cap_show_coc_params,
};

#endif

static const struct cli_cmd ble_cmd[] = {
	CMDENTRY(init, cmd_init, "", NULL),
#if MYNEWT_VAL(BLE_EXT_ADV)
	CMDENTRY(advertise-configure, cmd_advertise_configure, "", (const char *)&advertise_configure_help),
	CMDENTRY(advertise-set-addr, cmd_advertise_set_addr, "", (const char *)&advertise_set_addr_help),
	CMDENTRY(advertise-set-adv-data, cmd_set_adv_data, "", (const char *)&set_adv_data_help),
	CMDENTRY(advertise-set-scan-rsp, cmd_set_scan_rsp, "", (const char *)&set_scan_rsp_help),
	CMDENTRY(advertise-start, cmd_advertise_start, "", (const char *)&advertise_start_help),
	CMDENTRY(advertise-stop, cmd_advertise_stop, "", (const char *)&advertise_stop_help),
	CMDENTRY(advertise-remove, cmd_advertise_remove, "", (const char *)&advertise_remove_help),
#else
	CMDENTRY(advertise, cmd_advertise, "", (const char *)&advertise_help),
#endif
	CMDENTRY(connect, cmd_connect, "", (const char *)&connect_help),
	CMDENTRY(disconnect, cmd_disconnect, "", (const char *)&disconnect_help),
	CMDENTRY(show-addr, cmd_show_addr, "", (const char *)&gatt_show_addr_help),
	CMDENTRY(show-conn, cmd_show_conn, "", (const char *)&gatt_show_conn_help),
	CMDENTRY(set-scan-opts, cmd_set_scan_opts, "", (const char *)&set_scan_opts_help),
	CMDENTRY(scan, cmd_scan, "", (const char *)&scan_help),
	CMDENTRY(set, cmd_set, "", (const char *)&set_help),
#if !MYNEWT_VAL(BLE_EXT_ADV)
	CMDENTRY(set-adv-data, cmd_set_adv_data, "", (const char *)&set_adv_data_help),
	CMDENTRY(set-scan-rsp, cmd_set_scan_rsp, "", (const char *)&set_scan_rsp_help),
#endif
	CMDENTRY(set-priv-mode, cmd_set_priv_mode, "", (const char *)&set_priv_mode_help),
	CMDENTRY(white-list, cmd_white_list, "", (const char *)&white_list_help),
	CMDENTRY(conn-rssi, cmd_conn_rssi, "", (const char *)&conn_rssi_help),
	CMDENTRY(conn-update-params, cmd_conn_update_params, "", (const char *)&conn_update_params_help),
	CMDENTRY(conn-datalen, cmd_conn_datalen, "", (const char *)&conn_datalen_help),
	CMDENTRY(gatt-discover-characteristic, cmd_gatt_discover_characteristic, "", (const char *)&gatt_discover_characteristic_help),
	CMDENTRY(gatt-discover-descriptor, cmd_gatt_discover_descriptor, "", (const char *)&gatt_discover_descriptor_help),
	CMDENTRY(gatt-discover-service, cmd_gatt_discover_service, "", (const char *)&gatt_discover_service_help),
	CMDENTRY(gatt-discover-full, cmd_gatt_discover_full, "", (const char *)&gatt_discover_full_help),
	CMDENTRY(gatt-find-included-services, cmd_gatt_find_included_services, "", (const char *)&gatt_find_included_services_help),
	CMDENTRY(gatt-exchange-mtu, cmd_gatt_exchange_mtu, "", (const char *)&gatt_exchange_mtu_help),
	CMDENTRY(gatt-read, cmd_gatt_read, "", (const char *)&gatt_read_help),
	CMDENTRY(gatt-notify, cmd_gatt_notify, "", (const char *)&gatt_notify_help),
	CMDENTRY(gatt-service-changed, cmd_gatt_service_changed, "", (const char *)&gatt_service_changed_help),
	CMDENTRY(gatt-service-visibility, cmd_gatt_service_visibility, "", (const char *)&gatt_service_visibility_help),
	CMDENTRY(gatt-show, cmd_gatt_show, "", (const char *)&gatt_show_help),
	CMDENTRY(gatt-show-local, cmd_gatt_show_local, "", (const char *)&gatt_show_local_help),
	CMDENTRY(gatt-write, cmd_gatt_write, "", (const char *)&gatt_write_help),
#ifdef CONFIG_NIMBLE_PTS
	CMDENTRY(gatt-svr, cmd_gatt_svr, "", (const char *)&gatt_svr_help),
#endif
#if MYNEWT_VAL(BLE_L2CAP_COC_MAX_NUM)
	CMDENTRY(local-update, cmd_l2cap_update, "", (const char *)&l2cap_update_help),
	CMDENTRY(l2cap-create-server, cmd_l2cap_create_server, "", (const char *)&l2cap_create_server_help),
	CMDENTRY(l2cap-connect, cmd_l2cap_connect, "", (const char *)&l2cap_connect_help),
	CMDENTRY(l2cap-reconfig, cmd_l2cap_reconfig, "", (const char *)&l2cap_reconfig_help),
	CMDENTRY(l2cap-disconnect, cmd_l2cap_disconnect, "", (const char *)&l2cap_disconnect_help),
	CMDENTRY(l2cap-send, cmd_l2cap_send, "", (const char *)&l2cap_send_help),
	CMDENTRY(l2cap-show-coc, cmd_l2cap_show_coc, "", (const char *)&l2cap_show_coc_help),
#endif
	CMDENTRY(keystore-add, cmd_keystore_add, "", (const char *)&keystore_add_help),
	CMDENTRY(keystore-del, cmd_keystore_del, "", (const char *)&keystore_del_help),
	CMDENTRY(keystore-show, cmd_keystore_show, "", (const char *)&keystore_show_help),
#if NIMBLE_BLE_SM
#ifdef CONFIG_NIMBLE_PTS
	CMDENTRY(set-remote-oob-sc, cmd_set_remote_oob_sc, "", (const char *)&set_remote_oob_sc_help),
#endif
	CMDENTRY(show-oob-sc, cmd_show_oob_sc, "", NULL),
	CMDENTRY(auth-passkey, cmd_auth_passkey, "", (const char *)&auth_passkey_help),
	CMDENTRY(security-pair, cmd_security_pair, "", (const char *)&security_pair_help),
	CMDENTRY(security-unpair, cmd_security_unpair, "", (const char *)&security_unpair_help),
	CMDENTRY(security-start, cmd_security_start, "", (const char *)&security_start_help),
	CMDENTRY(security-encryption, cmd_security_encryption, "", (const char *)&security_encryption_help),
	CMDENTRY(security-set-data, cmd_security_set_data, "", (const char *)&security_set_data_help),
#endif
	CMDENTRY(test-tx, cmd_test_tx, "", (const char *)&test_tx_help),
	CMDENTRY(phy-set, cmd_phy_set, "", (const char *)&phy_set_help),
	CMDENTRY(phy-set-default, cmd_phy_set_default, "", (const char *)&phy_set_default_help),
	CMDENTRY(phy-read, cmd_phy_read, "", (const char *)&phy_read_help),
	CMDENTRY(dtm-rx, cmd_dtm_rx, "", (const char *)&dtm_rx_help),
	CMDENTRY(dtm-tx, cmd_dtm_tx, "", (const char *)&dtm_tx_help),
	CMDENTRY(dtm-end, cmd_dtm_end, "", (const char *)&dtm_end_help),
};

static void print_help(struct shell_cmd_help *help)
{
	const struct shell_param *params;

	params = help->params;

	printf("Summary:\n");
	printf("%s\n", help->summary);
	printf("Parameters:\n");
	while (params->param) {
		printf("%-30s%s\n", params->param, params->help);
		params++;
	}
}

static int do_bt(int argc, char *argv[])
{
	const struct cli_cmd *cmd;
	int ret;

	argc--;
	argv++;

	cmd = cli_find_cmd(argv[0], ble_cmd, ARRAY_SIZE(ble_cmd));
	if (cmd == NULL) {
		return CMD_RET_USAGE;
	} else {
		if (argc > 1 && !memcmp(argv[1], "help", 4)) {
			print_help((struct shell_cmd_help *)cmd->usage);
			return 0;
		}
	}

	ret = cmd->handler(argc, argv);
	if (ret != 0) {
		printf("error code : 0x%x\n", ret);
		ret = CMD_RET_FAILURE;
	}

	return ret;
}

CMD(bt, do_bt,
		"CLI command bt",
		"bt  init" OR
#if MYNEWT_VAL(BLE_EXT_ADV)
		"bt  advertise-configure" OR
		"bt  advertise-set-addr" OR
		"bt  advertise-set-adv-data" OR
		"bt  advertise-set-scan-rsp" OR
		"bt  advertise-start" OR
		"bt  advertise-stop" OR
		"bt  advertise-remove" OR
#else
		"bt  advertise" OR
#endif
		"bt  connect" OR
		"bt  disconnect" OR
		"bt  show-addr" OR
		"bt  show-conn" OR
		"bt  set-scan-opts" OR
		"bt  scan" OR
		"bt  set" OR
#if !MYNEWT_VAL(BLE_EXT_ADV)
		"bt  set-adv-data" OR
		"bt  set-scan-rsp" OR
#endif
		"bt  set-priv-mode" OR
		"bt  white-list" OR
		"bt  conn-rssi" OR
		"bt  conn-update-params" OR
		"bt  conn-datalen" OR
#ifdef CONFIG_NIMBLE_PTS
		"bt  gatt-svr" OR
#endif
		"bt  gatt-discover-characteristic" OR
		"bt  gatt-discover-descriptor" OR
		"bt  gatt-discover-service" OR
		"bt  gatt-discover-full" OR
		"bt  gatt-find-included-services" OR
		"bt  gatt-exchange-mtu" OR
		"bt  gatt-read" OR
		"bt  gatt-notify" OR
		"bt  gatt-service-changed" OR
		"bt  gatt-service-visibility" OR
		"bt  gatt-show" OR
		"bt  gatt-show-local" OR
		"bt  gatt-write" OR
#if MYNEWT_VAL(BLE_L2CAP_COC_MAX_NUM)
		"bt  local-update" OR
		"bt  l2cap-create-server" OR
		"bt  l2cap-connect" OR
		"bt  l2cap-reconfig" OR
		"bt  l2cap-disconnect" OR
		"bt  l2cap-send" OR
		"bt  l2cap-show-coc" OR
#endif
		"bt  keystore-add" OR
		"bt  keystore-del" OR
		"bt  keystore-show" OR
#if NIMBLE_BLE_SM
#ifdef CONFIG_NIMBLE_PTS
		"bt  set-remote-oob-sc" OR
#endif
		"bt  show-oob-sc" OR
		"bt  auth-passkey" OR
		"bt  security-pair" OR
		"bt  security-unpair" OR
		"bt  security-start" OR
		"bt  security-encryption" OR
		"bt  security-set-data" OR
#endif
		"bt  test-tx" OR
		"bt  phy-set" OR
		"bt  phy-set-default" OR
		"bt  phy-read" OR

		"bt  dtm-rx" OR
		"bt  dtm-tx" OR
		"bt  dtm-end"
);
