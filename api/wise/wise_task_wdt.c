/*
 * Copyright 2023-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>

#include <hal/kernel.h>
#include <hal/timer.h>
#include <FreeRTOS/FreeRTOS.h>
#include <sys/queue.h>

#include "cmsis_os.h"

#include "wise_task_wdt.h"
#include "wise_system.h"
#include "scm_wdt.h"

#include <hal/console.h>

#define TASK_WDT_STACK_SIZE		1024

struct task_wdt_entry {
    SLIST_ENTRY(task_wdt_entry) slist_entry;
	osThreadId_t task_id;
	bool has_reset;
};

struct task_wdt_ctx {
	bool started;
	osSemaphoreId_t sem;
	uint32_t expire_tick;
	uint32_t expire_dur_tick;
	SLIST_HEAD(entry_list_head, task_wdt_entry) entries_slist;
};

static struct task_wdt_ctx g_twdt_ctx;

static StaticTask_t task_wdt_cb;
static uint8_t task_wdt_stack[TASK_WDT_STACK_SIZE];

static void task_wdt_timer_feed(void)
{
	struct task_wdt_entry *entry;

	SLIST_FOREACH(entry, &g_twdt_ctx.entries_slist, slist_entry) {
        entry->has_reset = false;
    }
}

static bool task_wdt_check_all_reset(void)
{
	bool found_non_reset = false;
	struct task_wdt_entry *entry;

	SLIST_FOREACH(entry, &g_twdt_ctx.entries_slist, slist_entry) {
		if (!entry->has_reset) {
			found_non_reset = true;
		}
	}

	return !found_non_reset;
}

static struct task_wdt_entry * task_wdt_find_entry(osThreadId_t task_id)
{
	struct task_wdt_entry *target = NULL;
	struct task_wdt_entry *entry;


	SLIST_FOREACH(entry, &g_twdt_ctx.entries_slist, slist_entry) {
		if (entry->task_id == task_id) {
			target = entry;
		}
	}

	return target;
}

static void task_wdt_expire_update(struct task_wdt_ctx *twdt_ctx)
{
	twdt_ctx->expire_tick = ktime() + twdt_ctx->expire_dur_tick;
}

static void task_wdt_check_feed(void)
{
	bool all_reset;

	all_reset = task_wdt_check_all_reset();
	if (all_reset) {
		task_wdt_timer_feed();
		task_wdt_expire_update(&g_twdt_ctx);
		osSemaphoreRelease(g_twdt_ctx.sem);
	}
}

static void task_wdt_err_report(struct task_wdt_ctx *twdt_ctx)
{
	struct task_wdt_entry *entry;
	int i = 0;

	printk("task watchdog triggered\n");
	printk("the following task did not reset watchdog\n");

	SLIST_FOREACH(entry, &g_twdt_ctx.entries_slist, slist_entry) {
		if (!entry->has_reset) {
			printk("#%d - %s\n", i++, osThreadGetName(entry->task_id));
		}
	}

	console_flush();
}

static void task_wdt(void *arg)
{
	struct task_wdt_ctx *twdt_ctx = arg;
	bool all_reset;
	uint32_t flags;

#ifdef CONFIG_TASK_WATCHDOG_RESET
	scm_wdt_start(1000);
#endif

	while (1) {
#ifdef CONFIG_TASK_WATCHDOG_RESET
		scm_wdt_feed();
#endif

		local_irq_save(flags);

		if ((int32_t)(twdt_ctx->expire_tick - ktime()) <= 10 * 1000) {
			/* expire task watchdog */
			all_reset = task_wdt_check_all_reset();
			task_wdt_expire_update(twdt_ctx);
			if (!all_reset) {
				local_irq_restore(flags);
				task_wdt_err_report(twdt_ctx);
				while (1) {
					osDelay(UINT32_MAX);
				}
			}
		} else {
			all_reset = false;
		}

		if (all_reset) {
			task_wdt_timer_feed();
		}

		local_irq_restore(flags);

		osSemaphoreAcquire(twdt_ctx->sem, 1000);
	}
}

wise_err_t wise_task_wdt_add(osThreadId_t task_id)
{
	struct task_wdt_entry *entry;
	uint32_t flags;

	if (task_id == NULL) {
		task_id = osThreadGetId();
	}

	local_irq_save(flags);

	entry = task_wdt_find_entry(task_id);
	if (entry) {
		local_irq_restore(flags);
		return WISE_FAIL;
	}

	entry = calloc(1, sizeof(struct task_wdt_entry));
	if (!entry) {
		local_irq_restore(flags);
		return WISE_ERR_NO_MEM;
	}

	entry->task_id = task_id;
	entry->has_reset = true;

	SLIST_INSERT_HEAD(&g_twdt_ctx.entries_slist, entry, slist_entry);

	task_wdt_check_feed();

	local_irq_restore(flags);

	return WISE_OK;
}

wise_err_t wise_task_wdt_delete(osThreadId_t task_id)
{
	struct task_wdt_entry *entry;
	uint32_t flags;

	local_irq_save(flags);

	entry = task_wdt_find_entry(task_id);
	if (!entry) {
		local_irq_restore(flags);
		return WISE_FAIL;
	}

    SLIST_REMOVE(&g_twdt_ctx.entries_slist, entry, task_wdt_entry, slist_entry);

	task_wdt_check_feed();

	local_irq_restore(flags);

	free(entry);

	return WISE_OK;
}

wise_err_t wise_task_wdt_reset(osThreadId_t task_id)
{
	struct task_wdt_entry *entry;
	uint32_t flags;

	if (task_id == NULL) {
		task_id = osThreadGetId();
	}

	local_irq_save(flags);

	entry = task_wdt_find_entry(task_id);
	if (!entry) {
		local_irq_restore(flags);
		return WISE_FAIL;
	}

	entry->has_reset = true;

	task_wdt_check_feed();

	local_irq_restore(flags);

	return WISE_OK;
}


wise_err_t wise_task_wdt_init(uint32_t timeout_s)
{
	osThreadAttr_t attr = {
		.name 		= "wdt-task",
		.cb_mem     = &task_wdt_cb,
		.cb_size    = sizeof(StaticTask_t),
		.stack_mem  = task_wdt_stack,
		.stack_size = TASK_WDT_STACK_SIZE,
		.priority 	= osPriorityISR,
	};

	if (g_twdt_ctx.started) {
		return WISE_ERR_INVALID_STATE;
	}

	g_twdt_ctx.started = true;
	g_twdt_ctx.expire_dur_tick = ms_to_tick(timeout_s * 1000);
	g_twdt_ctx.sem = osSemaphoreNew(1, 0, NULL);
	task_wdt_expire_update(&g_twdt_ctx);

	if (!osThreadNew(task_wdt, &g_twdt_ctx, &attr)) {
		printf("%s: failed to create task wdt task\n", __func__);
		return WISE_FAIL;
	}

	return WISE_OK;
}

#ifdef CONFIG_CMD_TASK_WATCHDOG_TEST

#include "cli.h"

#define MAX_TEST_TASK_ID 5

osThreadId_t test_task_id[MAX_TEST_TASK_ID];

const char *test_task_name[MAX_TEST_TASK_ID] = {
	"WDTReset-2s",
	"WDTReset-4s",
	"WDTReset-6s",
	"WDTReset-8s",
	"WDTReset-10s",
};

int test_task_period[MAX_TEST_TASK_ID] = {
	2,
	4,
	6,
	8,
	10,
};

static void test_task(void *arg)
{
	int *period = arg;
	int period_ms =  *period * 1000;

	printf("period : %d\n", *period);

	while (1) {
		osDelay(period_ms);

		if (wise_task_wdt_reset(NULL) == WISE_OK) {
			printf("%s:wdt reset\n", osThreadGetName(osThreadGetId()));
		}
	}
}

static int scm_cli_task_wdt_init(int argc, char *argv[])
{
	uint32_t timeout_s;
	int i;
	osThreadAttr_t attr = {
		.stack_size = 1024 * 4,
		.priority 	= osPriorityNormal,
	};

	if (argc < 2) {
		return CMD_RET_USAGE;
	}

	timeout_s = atoi(argv[1]);

	printf("create watchdog task expire %ds\n", timeout_s);

	if (wise_task_wdt_init(timeout_s) != WISE_OK) {
		printf("task watchdog initialize failed\n");
		return CMD_RET_FAILURE;
	}

	for (i = 0; i < MAX_TEST_TASK_ID; i++) {
		attr.name = test_task_name[i];
		test_task_id[i] = osThreadNew(test_task, &test_task_period[i], &attr);
		if (test_task_id[i] == NULL) {
			printf("test task create failed\n");
			return CMD_RET_FAILURE;
		}
	}

	return CMD_RET_SUCCESS;
}

static int scm_cli_task_wdt_add_task(int argc, char *argv[])
{
	int idx;
	int ret;

	if (argc < 2) {
		return CMD_RET_USAGE;
	}

	idx = atoi(argv[1]);
	if (idx > MAX_TEST_TASK_ID - 1) {
		return CMD_RET_FAILURE;
	}

	ret = wise_task_wdt_add(test_task_id[idx]);
	if (ret != WISE_OK) {
		printf("task watchdog add failure 0x%x\n", ret);
		return CMD_RET_FAILURE;
	}

	printf("Add %s\n", osThreadGetName(test_task_id[idx]));

	return CMD_RET_SUCCESS;

}

static int scm_cli_task_wdt_del_task(int argc, char *argv[])
{
	int idx;
	int ret;

	idx = atoi(argv[1]);
	if (idx > MAX_TEST_TASK_ID - 1) {
		return CMD_RET_FAILURE;
	}

	ret = wise_task_wdt_delete(test_task_id[idx]);
	if (ret != WISE_OK) {
		printf("task watchdog delete failure 0x%x\n", ret);
		return CMD_RET_FAILURE;
	}

	printf("Delete %s\n", osThreadGetName(test_task_id[idx]));

	return CMD_RET_SUCCESS;
}

static int scm_cli_task_wdt_assert(int argc, char *argv[])
{
	assert(0);

	return CMD_RET_SUCCESS;
}

static const struct cli_cmd scm_cli_task_wdt_cmd[] = {
	CMDENTRY(init, scm_cli_task_wdt_init, "", ""),
	CMDENTRY(add, scm_cli_task_wdt_add_task, "", ""),
	CMDENTRY(del, scm_cli_task_wdt_del_task, "", ""),
	CMDENTRY(assert, scm_cli_task_wdt_assert, "", ""),
};

static int do_scm_cli_task_wdt(int argc, char *argv[])
{
	const struct cli_cmd *cmd;

	argc--;
	argv++;

	cmd = cli_find_cmd(argv[0], scm_cli_task_wdt_cmd, ARRAY_SIZE(scm_cli_task_wdt_cmd));
	if  (cmd == NULL) {
		return CMD_RET_USAGE;
	}

	return cmd->handler(argc, argv);
}

CMD(twdt, do_scm_cli_task_wdt,
		"CLI for task watchdog API test",
		"twdt init <seconds>" OR
		"twdt add <task id>\n"
		"\t0: 2s\n"
		"\t1: 4s\n"
		"\t2: 6s\n"
		"\t3: 8s\n"
		"\t4: 10s" OR
		"twdt del <task id>" OR
		"twdt assert"
);
#endif
