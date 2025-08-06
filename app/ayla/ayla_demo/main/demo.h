/*
 * Copyright 2011-2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_DEMO_H__
#define __AYLA_DEMO_H__

/*
 * Demo platform setup_enable key.
 * If this is set, it allows setup mode to be
 * enabled when this key is entered on
 * the CLI "setup_mode enable <key>".
 */

#define DEMO_SETUP_ENABLE_KEY "secretkey"
/*
 * Nvs NameSpace For application
 */
#define AYLA_STORAGE "ayla_namespace"

#define GPIO_BLUE_LED		19
#define GPIO_GREEN_LED		18
#define GPIO_LINK_LED		16
#undef  GPIO_RGB_LED
#define GPIO_BOOT_BUTTON	0

/*
 * Start demo main entry.
 */
void demo_start(void);

/*
 * Initialize demo.
 */
void demo_init(void);

/*
 * Application thread idle-loop function.
 */
void demo_idle(void);

/*
 * CLI save command - to save config
 */
int demo_save_cmd(int argc, char **argv);

/*
 * CLI show command
 */
int demo_show_cmd(int argc, char **argv);

/*
 * CLI setup_mode command
 */
int demo_setup_mode_cmd(int argc, char **argv);

/*
 * Demo start.
 */
void demo_cloud_up(void);

/*
 * Check if cloud connection has come up at least once.
 */
u8 demo_cloud_has_started(void);

#ifdef AYLA_BLUETOOTH_SUPPORT
/*
 * Bluetooth demo initialization
 */
int demo_bt_is_provisioning(void);
void demo_bt_init(void);
#endif

/*
 * Callback when the device should identify itself to the end user, for
 * example, briefly blinking an LED.
 */
void demo_identify_cb(void);

#include <u-boot/linker-lists.h>
#include "wise_err.h"

struct ayla_cli_cmd {
	const char *command;
	int (*func)(int argc, char *argv[]);
	const char *help;
	const char *hint;
};

#define ayla_cmd_def(cmd)   ll_entry_declare(struct ayla_cli_cmd, cmd, _ayla_)
#define ayla_cmd_start()    ll_entry_start(struct ayla_cli_cmd, _ayla_)
#define ayla_cmd_end()      ll_entry_end(struct ayla_cli_cmd, _ayla_)

int run_ayla_cmd(int argc, char *argv[]);

extern const char mod_sw_build[];
extern const char mod_sw_version[];

#endif /* __AYLA_DEMO_H__ */
