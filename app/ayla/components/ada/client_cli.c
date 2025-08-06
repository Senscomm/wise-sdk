/*
 * Copyright 2011-2015 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <ayla/utypes.h>
#include <ayla/assert.h>
#include <ada/err.h>
#include <ayla/log.h>
#include <ayla/mod_log.h>
#include <ayla/http.h>
#include <ayla/conf.h>
#include <ayla/clock.h>
#include <ayla/nameval.h>
#include <ayla/timer.h>

#include <ada/prop.h>
#include <ada/server_req.h>
#include <ada/ada_conf.h>
#include <ada/client.h>
#include "client_req.h"
#include <ada/client_ota.h>
#include "client_int.h"
#include "client_req.h"
#include "http2mqtt_client.h"
#ifdef AYLA_BATCH_ADS_UPDATES
#include "client_batch.h"
#endif /* AYLA_BATCH_ADS_UPDATES */

static const char *client_conn_states[] = CLIENT_CONN_STATES;

const char *client_conn_state_string(enum client_conn_state conn_state)
{
	const char *state = NULL;

	if ((unsigned)conn_state < ARRAY_LEN(client_conn_states)) {
		state = client_conn_states[conn_state];
	}
	if (!state) {
		state = "unknown";
	}
	return state;
}

#ifdef AYLA_LAN_SUPPORT
/*
 * Show LAN client information.
 */
static void client_lan_info(void)
{
	const struct http_client *hc;
	struct client_lan_reg *lan;
	struct ada_lan_conf *lcf = &ada_lan_conf;
	u8 lan_enable = lcf->enable_mask & LAN_CONF_ENABLE_LAN_BIT;
	struct client_state *state = &client_state;

	printcli("client: lan mode %s", lan_enable ? "enabled" : "disabled");
	if (!lan_enable) {
		return;
	}
	printcli("client: lan auto_echo %s",
	    (lcf->auto_echo) ? "enabled" : "disabled");
	for (lan = client_lan_reg;
	    lan < &client_lan_reg[CLIENT_LAN_REGS]; lan++) {
		if (lan->uri[0] == '\0') {
			continue;
		}
		hc = &lan->http_client;
		printcli("client: lan #%u %s:%u%s",
		    lan->id, hc->host, hc->host_port,
		    lan->uri);
		if (!lan->valid_key) {
			printcli("client: lan #%u no valid_key", lan->id);
		}
		if (lan->cmd_pending) {
			printcli("client: lan #%u cmd_pending SET", lan->id);
		}
		if (state->get_echo_inprog) {
			printcli("client: get_echo_inprog SET");
		}
		if (state->cmd_pending) {
			printcli("client: cmd_pending SET");
		}
		if (state->cmd_rsp_pending) {
			printcli("client: rsp_pending SET");
		}
	}
}
#endif /* AYLA_LAN_SUPPORT */

/*
 * client command.
 */
void ada_client_cli(int argc, char **argv)
{
	struct client_state *state = &client_state;
	struct http_client *hc = &state->http_client;
	struct ada_conf *cf = &ada_conf;
	enum conf_token tk;
	char port_str[18];
	char *prefix = "client";

	if (argc == 1) {
		snprintf(port_str, sizeof(port_str), ":%d", hc->host_port);
		printcli("%s: %s \"mqtts://%s%s\"", prefix,
		    cf->enable ? "enabled" : "disabled",
		    hc->host,
		    hc->host_port != MQTT_CLIENT_SERVER_PORT_SSL ? port_str :
		    "");
#ifdef AYLA_LAN_SUPPORT
		client_lan_info();
#endif
		printcli("%s: state %s http_state=%u conn_time=%lu", prefix,
		    client_conn_state_string(state->conn_state),
		    hc->state, state->connect_time);
#ifdef AYLA_BATCH_ADS_UPDATES
		client_batch_info();
#endif
		return;
	}
	if (argc < 2) {
usage:
		printcli("usage:");
		printcli("%s <enable|disable>", prefix);
		printcli("%s server region <region code>", prefix);
		return;
	}
	if (argc == 2 && !strcmp(argv[1], "test")) {
		cf->test_connect = 1;
		return;
	}
#ifdef AYLA_BATCH_ADS_UPDATES
	if (client_batch_cli(argc, argv) == AE_OK) {
		/*
		 * Client batch CLI command processed
		 */
		return;
	}
#endif /* AYLA_BATCH_ADS_UPDATES */
	tk = conf_token_parse(argv[1]);
	if (tk == CT_enable) {
		if (argc != 2) {
			goto usage;
		}
		cf->enable = 1;
		return;
	}
	if (!strcmp(argv[1], "disable")) {
		if (argc != 2) {
			goto usage;
		}
		cf->enable = 0;
		return;
	}

	if (tk == CT_server && argc == 3) {
		if (!client_conf_server_change_en()) {
			printcli("server changes not allowed");
			return;
		}
#ifdef AYLA_LAN_SUPPORT
		CLIENT_LANIP_WIPE_KEY(&ada_lan_conf);
#endif
		if (client_set_server(argv[2])) {
			printcli("invalid server name");
		}
		return;
	}

	if (tk == CT_server && argc == 4 &&
	    conf_token_parse(argv[2]) == CT_region) {
		if (!mfg_or_setup_mode_ok()) {
			return;
		}
		if (client_set_region(argv[3])) {
			printcli("unknown region code %s", argv[3]);
			goto usage;
		}
		return;
	}
	goto usage;
}

/*
 * Client command - deprecated API.
 */
void client_cli(int argc, char **argv)
{
	ada_client_cli(argc, argv);
}
