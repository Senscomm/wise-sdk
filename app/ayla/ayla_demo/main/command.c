/*
 * Copyright 2011-2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "wise_wifi.h"

#include <ayla/utypes.h>
#include <ayla/assert.h>
#include <ayla/tlv.h>
#include <ayla/conf.h>
#include <ayla/log.h>
#include <ayla/clock.h>
#include <ada/ada_conf.h>
#include <ada/client.h>
#include <ada/metrics.h>
#include <adw/wifi.h>
#include <adb/adb.h>
#include <adb/al_bt.h>
#include <sys/time.h>
#include <cmsis_os.h>

#include "conf.h"
#include "conf_wifi.h"
#include "demo.h"
#include "scm_efuse.h"

#define KEY_LENGTH      50
#define VALUE_LENGTH    500

/* heap tracing defines */
#define MAX_TASK_NUM	20
#define MAX_BLOCK_NUM	20

#define MS_TO_TICKS(ms) ((uint32_t)(((uint32_t)(ms) * osKernelGetTickFreq()) / 1000))

/*
 * setup_mode command.
 * setup_mode enable|disable|show [<key>]
 */
int demo_setup_mode_cmd(int argc, char **argv)
{
	if (argc == 2 && !strcmp(argv[1], "show")) {
		printf("setup_mode %sabled\n", conf_setup_mode ? "en" : "dis");
		return 0;
	}
#ifdef DEMO_SETUP_ENABLE_KEY
	if (argc == 3 && !strcmp(argv[1], "enable")) {
		if (strcmp(argv[2], DEMO_SETUP_ENABLE_KEY)) {
			printf("wrong key\n");
			return 0;
		}
		ada_conf_setup_mode(1); /* saves if setting setup mode */
		return 0;
	}
#endif /* SETUP_ENABLE_KEY */
	if (argc == 2 && !strcmp(argv[1], "disable")) {
		ada_conf_setup_mode(0); /* saves if clearing setup mode */
		return 0;
	}
	printf("usage error\n");
	return 0;
}

int demo_save_cmd(int argc, char **argv)
{
	char *args[] = { "conf", "save", NULL};
	if (argc != 1) {
		printf("save: unused args - nothing saved\n");
		return 0;
	}
	conf_cli(2, args);
	return 0;
}

int demo_show_cmd(int argc, char **argv)
{
	if (argc != 2) {
		goto usage;
	}

	if (!strcmp(argv[1], "version")) {
		printf("%s\n", adap_conf_sw_version());
		return 0;
	}
#ifdef AYLA_WIFI_SUPPORT
	if (!strcmp(argv[1], "wifi")) {
		adw_wifi_show();
		return 0;
	}
#endif

usage:
	printf("usage: show [version|wifi]\n");
	return 0;
}

static const char conf_oem_cli_help[] = "oem key <secret> [oem-model]";

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

static int diag_cmd(int argc, char **argv)
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

static wise_err_t demo_time_cmd(int argc, char **argv)
{
	ayla_time_cli(argc, argv);
	return WISE_OK;
}

	ayla_cmd_def(time) = {
		.command = "time",
		.help = ayla_time_cli_help,
		.func = demo_time_cmd,
	};

#ifdef AYLA_WIFI_SUPPORT

static const char wifi_cli_help[] = "wifi <name> <value>";

	ayla_cmd_def(wifi) = {
		.command = "wifi",
		.help = wifi_cli_help,
		.func = &demo_wifi_cmd
	};
#endif

static int wise_oem_cli(int argc, char **argv)
{
	ada_conf_oem_cli(argc, argv);
	return 0;
}

	ayla_cmd_def(oem) = {
		.command = "oem",
		.help = conf_oem_cli_help,
		.func = &wise_oem_cli
	};

static int wise_conf_reset_cli(int argc, char **argv)
{
	if (argc == 3
			&& !strcmp(argv[1], "factory")
			&& !strcmp(argv[2], "all")) {
		al_persist_data_erase(AL_PERSIST_FACTORY);
		argc = 2;
	}

	ada_conf_reset_cli(argc, argv);
	return WISE_OK;
}

	ayla_cmd_def(reset) = {
		.command = "reset",
		.help = "reset [factory] [factory all]",
		.func = &wise_conf_reset_cli
	};

	ayla_cmd_def(setup_mode) = {
		.command = "setup_mode",
		.help = "setup_mode [enable|disable|show] - "
		    "configure/display setup mode",
		.func = &demo_setup_mode_cmd
	};

	ayla_cmd_def(save) = {
		.command = "save",
		.help = "save - save configuration",
		.func = &demo_save_cmd
	};

#ifdef AYLA_LOG_SNAPSHOTS
static wise_err_t wise_log_snap_cli(int argc, char **argv)
{
	ada_log_snap_cli(argc, argv);
	return WISE_OK;
}

	ayla_cmd_def(log_snap) = {
		.command = "log-snap",
		.help = ada_log_snap_cli_help,
		.func = wise_log_snap_cli
	};
#endif

static wise_err_t wise_log_cli(int argc, char **argv)
{
	ada_log_cli(argc, argv);
	return WISE_OK;
}

	ayla_cmd_def(log) = {
		.command = "log",
		.help = ada_log_cli_help,
		.func = wise_log_cli
	};

static wise_err_t wise_log_client_cli(int argc, char **argv)
{
	ada_log_client_cli(argc, argv);
	return WISE_OK;
}

	ayla_cmd_def(log_client) = {
		.command = "log-client",
		.help = "log-client [enable|disable]",
		.func = wise_log_client_cli
	};

	ayla_cmd_def(show) = {
		.command = "show",
		.help = "show [wifi|version] - show wifi and version status",
		.func = &demo_show_cmd
	};

#if 0 /* not supporting core dump */
static wise_err_t demo_core_cmd(int argc, char **argv)
{
	ada_log_core_cli(argc, argv);
	return WISE_OK;
}

	ayla_cmd_def(core) = {
		.command = "core",
		.help = ada_log_core_help,
		.func = demo_core_cmd,
	};
#endif

static wise_err_t demo_id_cmd(int argc, char **argv)
{
	ada_conf_id_cli(argc, argv);
	return WISE_OK;
}

	ayla_cmd_def(id) = {
		.command = "id",
		.help = ada_conf_id_help,
		.func = demo_id_cmd,
	};

static wise_err_t demo_client_cmd(int argc, char **argv)
{
	ada_client_cli(argc, argv);
	return WISE_OK;
}

	ayla_cmd_def(client) = {
		.command = "client",
		.help = "client",		/* TODO */
		.func = demo_client_cmd,
	};

	ayla_cmd_def(diag) = {
		.command = "diag",
		.help = "diag [mem|tasks] - show diagnostic info",
		.func = &diag_cmd
	};

#ifdef AYLA_BLUETOOTH_SUPPORT
	ayla_cmd_def(bt) = {
		.command = "bt",
		.help = al_bt_cli_help,
		.func = &al_bt_cli
	};
#endif

#ifdef AYLA_METRICS_SUPPORT
static int wise_metrics_cli(int argc, char **argv)
{
	metrics_cli(argc, argv);
	return WISE_OK;
}

	ayla_cmd_def(metrics) = {
		.command = "metrics",
		.help = metrics_cli_help,
		.func = &wise_metrics_cli
	};
#endif

static void cli_crash_overflow(char *arg)
{
	char buf[2000];

	if (arg) {
		cli_crash_overflow(buf);
		(void)arg;
		(void)buf[0];
		printf("crash_overflow arg %p, buf %u", arg, buf[0]);
	}
}

static void cli_crash_hang(void)
{
	/* not supporting crash hang */
}

static void cli_crash_hang_intr(void)
{
	u32 flags;
	u32 time;

	flags = osCriticalSectionEnter();
	time = clock_ms() + 5 * 1000;
	while (clock_gt(time, clock_ms())) {
		;
	}
	osCriticalSectionExit(flags);
}

/*
 * crash command.
 */
static int cli_crash(int argc, char **argv)
{
	volatile int val[3];
	volatile int *ptr;
	enum crash_method {
		CM_ASSERT,	/* assertion */
		CM_ALIGN,	/* unaligned access */
		CM_DIV0,	/* divide by 0 */
		CM_HANG,	/* CPU hard hang.  Should watchdog */
		CM_HANG_INTR,	/* CPU hard hang in blocking interrupts. */
		CM_NULL,	/* NULL pointer dereference */
		CM_STACK,	/* stack overflow */
		CM_WILD,	/* wild pointer, invalid addresss access */
	} method;
	static const char *methods[] = {
		[CM_ASSERT] = "assert",
		[CM_ALIGN] = "align",
		[CM_DIV0] = "div0",
		[CM_HANG] = "hang",
		[CM_HANG_INTR] = "hang_intr",
		[CM_NULL] = "null",
		[CM_STACK] = "stack",
		[CM_WILD] = "wild",
	};

	if (argc < 2 || !strcmp(argv[1], "help")) {
		printcli("usage: crash <method>");
options:
		printcli_s("  valid methods are: ");
		for (method = 0; method < ARRAY_LEN(methods); method++) {
			printcli_s(" %s", methods[method]);
		}
		printcli(".");
		return 0;
	}

	for (method = 0; method < ARRAY_LEN(methods); method++) {
		if (argv[1] && !strcmp(argv[1], methods[method])) {
			break;
		}
	}
	if (method >= ARRAY_LEN(methods)) {
		printcli("invalid option");
		goto options;
	}

	log_put(LOG_WARN "CLI crash %s causing crash", argv[1]);
	osDelay(MS_TO_TICKS(100));

	switch (method) {
	case CM_NULL:
		(*(int *)0)++;
		((void(*)(void))0)();
		break;
	case CM_WILD:
		(*(int *)0x07fffffc)++;	/* arch-dependent address */
		break;
	case CM_DIV0:
		val[0] = 1;
		val[1] = 0;
		val[2] = val[0] / val[1];
		(void)val[2];
		break;
	case CM_ASSERT:
		ASSERT(0);
		break;
	case CM_ALIGN:
		ptr = (int *)cli_crash;
		ptr = (int *)((char *)ptr + 1);
		(void)*ptr;
		printcli("test %p %x %x", ptr, *(int *)ptr, *(int *)cli_crash);
		break;
	case CM_STACK:
		cli_crash_overflow("-");
		break;
	case CM_HANG:
		cli_crash_hang();
		break;
	case CM_HANG_INTR:
		cli_crash_hang_intr();
		break;
	default:
		break;
	}
	log_put(LOG_ERR "CLI crash survived");

	return 0;
}

	ayla_cmd_def(crash) = {
		.command = "crash",
		.help = "crash", "crash <type> - crash test",
		.func = &cli_crash,
	};

static int demo_conf_cmd(int argc, char **argv)
{
	/* TODO: cli (init) thread stack must be increase for this to work */
	conf_cli(argc, argv);
	return 0;
}

	ayla_cmd_def(conf) = {
		.command = "conf",
		.help = "conf",
		.hint = "conf [show] - show configuration" ,
		.func = demo_conf_cmd,
	};

