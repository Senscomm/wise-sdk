/*
 * Copyright 2023 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>

#include "FreeRTOS.h"
#include "task.h"

#include "wise_err.h"
#include "wise_system.h"

#include <ayla/log.h>
#include <ada/libada.h>
#include <adm/adm_cli.h>

#include "app_common.h"
#include "app_int.h"

#include <sys/time.h>
#include <cmsis_os.h>

#include "cli/cli.h"

#define APP_SETUP_ENABLE_KEY	"secretkey"

/*
 * client_conf_sw_build() returns the string to be reported to the cloud as the
 * module image version.
 */
const char *adap_conf_sw_build(void)
{
	return ada_version_build;
}

/*
 * client_conf_sw_version() returns the string reported to LAN clients with the
 * name, version, and more.
 */
const char *adap_conf_sw_version(void)
{
	return ada_version;
}

/*
 * The user-registered flag changed.
 */
void adap_conf_reg_changed(void)
{
	struct ada_conf *cf = &ada_conf;

	log_put(LOG_INFO "%s: user %sregistered",
	    __func__, cf->reg_user ? "" : "un");
}

/*
 * Reset the module, optionally to the factory configuration.
 * If factory is non-zero, the app should remove any startup configuration
 * and other customer-specific information.  The agent configuration will
 * already have been put back to the factory settings in this case.
 */
void adap_conf_reset(int factory)
{
	conf_log(LOG_INFO "Ayla Agent requested %sreset",
	    factory ? "factory " : "");
	wise_restart();
}

static int demo_client_cmd(int argc, char **argv)
{
	ada_client_cli(argc, argv);
	return 0;
}

ayla_cmd_def(client) = {
    .cmd  = "client",
    .help = "client",		/* TODO */
    .func = demo_client_cmd,
};

static int demo_conf_cmd(int argc, char **argv)
{
	/* TODO: cli (init) thread stack must be increase for this to work */
	conf_cli(argc, argv);
	return 0;
}

ayla_cmd_def(conf) = {
    .cmd = "conf",
    .help = "conf",
    .hit = "conf [show] - show configuration" ,
    .func = demo_conf_cmd,
};

static int demo_log_cli(int argc, char **argv)
{
	ada_log_cli(argc, argv);
	return 0;
}

ayla_cmd_def(log) = {
    .cmd = "log",
    .help = ada_log_cli_help,
    .func = demo_log_cli
};

static int demo_matter_cli(int argc, char **argv)
{
    adm_cli(argc, argv);
    return 0;
}

ayla_cmd_def(matter) = {
    .cmd = "matter",
    .help = adm_cli_help,
    .func = demo_matter_cli
};

static int demo_oem_cli(int argc, char **argv)
{
	ada_conf_oem_cli(argc, argv);
	return 0;
}

ayla_cmd_def(oem) = {
    .cmd = "oem",
	.help = "oem key <secret> [oem-model]",
    .func = &demo_oem_cli
};

static int demo_conf_reset_cli(int argc, char **argv)
{
#if 0
	if (argc == 3
			&& !strcmp(argv[1], "factory")
			&& !strcmp(argv[2], "all")) {
		al_persist_data_erase(AL_PERSIST_FACTORY);
		argc = 2;
	}
#endif

	ada_conf_reset_cli(argc, argv);
	return 0;
}

ayla_cmd_def(reset) = {
    .cmd = "reset",
    .help = "reset [factory]",
    .func = &demo_conf_reset_cli
};

static int demo_setup_mode_cli(int argc, char **argv)
{
    app_setup_mode_cmd(argc, argv);
    return 0;
}

ayla_cmd_def(setup_mode) = {
    .cmd = "setup_mode",
    .help = "setup_mode [enable|disable|show] - "
        "configure/display setup mode",
    .func = &demo_setup_mode_cli
};

static int demo_save_cli(int argc, char **argv)
{
    app_save_cmd(argc, argv);
    return 0;
}

ayla_cmd_def(save) = {
    .cmd = "save",
    .help = "save - save configuration",
    .func = &demo_save_cli
};

#if 0 /* not supporting core dump */
static int demo_core_cli(int argc, char **argv)
{
	ada_log_core_cli(argc, argv);
	return WISE_OK;
}

ayla_cmd_def(core) = {
    .cmd = "core",
    .help = ada_log_core_help,
    .func = demo_core_cli,
};
#endif

static int demo_id_cli(int argc, char **argv)
{
	ada_conf_id_cli(argc, argv);
	return 0;
}

ayla_cmd_def(id) = {
    .cmd = "id",
    .help = ada_conf_id_help,
    .func = demo_id_cli,
};

/*
 * Print diagnostic info for debug or for customers reporting problems.
 */
#if configUSE_TRACE_FACILITY == 1
static const char *taskStateString(osThreadState_t state)
{
	switch (state) {
	case osThreadRunning:
		return "run";
	case osThreadReady:
		return "rdy";
	case osThreadBlocked:
		return "blk";
	case osThreadTerminated:
		return "del";
	default:
		return "unk";
	}
}
#endif

static void cli_diag_tasks(void)
{
#if configUSE_TRACE_FACILITY == 1
	uint32_t count;
	osThreadId_t *table;
	uint32_t i;

	count = osThreadGetCount() + 1;	/* allow for one extra */
	table = malloc(count * sizeof(*table));
	if (!table) {
		printcli("alloc for %lu tasks failed", count);
		return;
	}

	count = osThreadEnumerate(table, count);
	printcli("\ntasks:");
	printcli("   %30s %6s %7s %6s %8s",
	    "",
	    "cprio", "state", "unused", "handle");
	if (count) {
		for (i = 0; i < count; i++) {
			printcli("   %30s %6u %7s %6lu %8lx",
				osThreadGetName(table[i]),
				osThreadGetPriority(table[i]),
			    taskStateString(osThreadGetState(table[i])),
				osThreadGetStackSpace(table[i]),
			    (uint32_t)table[i]);
		}
	} else {
		printcli("Task list changing. Please try again.");
	}
	free(table);
#endif
}

static void cli_diag_mem_task(void)
{
	/* not supporting diag mem task */
}

static void cli_diag_mem(void)
{
	printf("Current:\t%12ld B\n", (long)(configTOTAL_HEAP_SIZE - osKernelGetFreeHeapSize()));
#ifndef CONFIG_PORT_NEWLIB
	printf("Maximum:\t%12ld B\n", (long)(configTOTAL_HEAP_SIZE - osKernelGetMinEverFreeHeapSize()));
#endif
	printf("Free:\t\t%12ld B\n", (long)osKernelGetFreeHeapSize());
	printf("Total:  \t%12ld B\n", (long)configTOTAL_HEAP_SIZE);
}

static int demo_diag_cmd(int argc, char **argv)
{
	u8 display_mem = 0;
	u8 display_tasks = 0;

	if (argc == 2 && !strcmp(argv[1], "mem")) {
		display_mem = 1;
	}
	if (argc == 2 && !strcmp(argv[1], "tasks")) {
		display_tasks = 1;
	}

	if (!display_mem && !display_tasks) {
		/* display everything */
		display_mem = 1;
		display_tasks = 1;
	}
	if (display_mem) {
		cli_diag_mem();
		cli_diag_mem_task();
	}
	if (display_tasks) {
		cli_diag_tasks();
	}
	return WISE_OK;
}

ayla_cmd_def(diag) = {
    .cmd = "diag",
    .help = "diag [mem|tasks] - show diagnostic info",
    .func = &demo_diag_cmd
};

#ifdef CONFIG_CMDLINE

int run_ayla_cmd(int argc, char *argv[])
{
	const struct app_cli_info *start, *end, *cmd;
	start = ayla_cmd_start();
	end = ayla_cmd_end();

	for (cmd = start; cmd < end; cmd++) {
		if (strcmp(cmd->cmd, argv[0]) == 0) {
			return cmd->func(argc, argv);
		}
	}

	for (cmd = start; cmd < end; cmd++) {
		printf("%-16s - %s\n", cmd->cmd, cmd->help);
	}

	return -1;
}

#endif

void app_cmd_exec(const char *command)
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

void app_setup_mode_cmd(int argc, char **argv)
{
	if (argc == 2 && !strcmp(argv[1], "show")) {
		printf("setup_mode %sabled\n", conf_setup_mode ? "en" : "dis");
		return;
	} else if (argc == 3 && !strcmp(argv[1], "enable")) {
		if (strcmp(argv[2], APP_SETUP_ENABLE_KEY)) {
			printf("wrong key\n");
			return;
		}
		ada_conf_setup_mode(1); /* saves if setting setup mode */
		return;
	} else if (argc == 2 && !strcmp(argv[1], "disable")) {
		ada_conf_setup_mode(0); /* saves if clearing setup mode */
		return;
	}
	printf("usage error\n");
}

void app_save_cmd(int argc, char **argv)
{
	char *args[] = { "conf", "save", NULL};
	if (argc != 1) {
		printf("save: unused args - nothing saved\n");
		return;
	}
	conf_cli(2, args);
}
