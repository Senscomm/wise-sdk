/*
 * Copyright 2011-2020 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */

/*
 * Ayla device agent demo.
 */

#include <stddef.h>
#include <string.h>
#include <sys/time.h>

#include "wise_err.h"
#include <ada/libada.h>
#include <ada/client.h>
#include <ada/sched.h>
#include <ayla/nameval.h>
#include <ayla/log.h>
#ifdef AYLA_WIFI_SUPPORT
#include <adw/wifi.h>
#endif
#include "conf.h"
#include "conf_wifi.h"
#include "demo.h"

#include <cmsis_os.h>

#include <lwip/ip4_addr.h>

#include "cli/cli.h"

static u8 cloud_started;	/* set to 1 after first cloud connection */

static void demo_client_start(void)
{
	int rc;

	/*
	 * Read configuration.
	 */
	client_conf_init();

	/*
	 * Init libada.
	 */
	rc = ada_init();
	if (rc) {
		log_put(LOG_ERR "ADA init failed");
		return;
	}

#ifdef AYLA_LOCAL_CONTROL_SUPPORT
	/*
	 * Enable local control access.
	 */
	rc = ada_client_lc_up();
	if (rc) {
		log_put(LOG_ERR "ADA local control up failed");
	}
#endif

	/*
	 * Start schedule activities.
	 */
	ada_sched_enable();
	demo_ota_init();
#ifdef AYLA_WIFI_SUPPORT
	demo_wifi_init();
#endif
	demo_init();
}

u8 demo_cloud_has_started(void)
{
	return cloud_started;
}

void demo_cloud_up(void)
{
	if (cloud_started) {
		return;
	}
	cloud_started = 1;
}

#ifdef CONFIG_CMDLINE
int run_ayla_cmd(int argc, char *argv[])
{
	const struct ayla_cli_cmd *start, *end, *cmd;
	start = ayla_cmd_start();
	end = ayla_cmd_end();

	for (cmd = start; cmd < end; cmd++) {
		if (strcmp(cmd->command, argv[0]) == 0) {
			return cmd->func(argc, argv);
		}
	}

	for (cmd = start; cmd < end; cmd++) {
		printf("%-16s - %s\n", cmd->command, cmd->help);
	}

	return -1;
}
#endif

static void demo_cmd_exec(const char *command)
{
	/* 
	 * assuming that the command is not a const buffer.
	 * if CONFIG_CMDLINE is not supported, cli_parse_line 
	 * function should be implemented.
	 */
#ifdef CONFIG_CMDLINE
	char *argv[32];
	int argc;

	argc = cli_parse_line((char **)&command, argv);
	run_ayla_cmd(argc, argv);
#endif
}

void demo_start()
{
	log_init();
	printf("\r\n\n%s\r\n", mod_sw_version);

	ada_client_command_func_register(demo_cmd_exec);
#ifdef AYLA_WIFI_SUPPORT
	adw_wifi_init();
	log_put(LOG_INFO "wlan init done");
#endif
	demo_client_start();

#ifdef AYLA_BLUETOOTH_SUPPORT
	demo_bt_init();
#endif
}
