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

#include <FreeRTOS/FreeRTOS.h>
#include <FreeRTOS/task.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <cmsis_os.h>

#include <cli.h>
#include <proc.h>
#include <hal/irq.h>
#include <hal/sw-irq.h>

#ifdef CONFIG_CMD_SUSPEND
static int do_suspend(int argc, char *argv[])
{
	TaskHandle_t task;
	int i, pid;

	if (argc < 2)
		return CMD_RET_USAGE;

	if (argc == 2 && strcmp(argv[1], "all") == 0) {
		task_suspend(-1);
	} else {
		for (i = 1; i < argc; i++) {
			pid = strtoul(argv[i], NULL, 0);

			printf("Suspending process %d: ", pid);
			if (task_suspend(pid) < 0) {
				printf("%s\n", strerror(errno));
				return CMD_RET_FAILURE;
			} else {
				task = find_task_by_pid(pid);
				printf("[%s] suspended\n", pcTaskGetName(task));
			}
		}
	}
	return 0;
}

CMD(suspend, do_suspend,
    "suspend process(es)",
    "suspend <PIDs | all>"
    );
#endif /* CONFIG_CMD_SUSPEND */

#ifdef CONFIG_CMD_RESUME
static int do_resume(int argc, char *argv[])
{
	TaskHandle_t task;
	int i, pid;

	if (argc < 2)
		return CMD_RET_USAGE;

	if (argc == 2 && strcmp(argv[1], "all") == 0) {
		task_resume(-1);
	} else {
		for (i = 1; i < argc; i++) {
			pid = strtoul(argv[i], NULL, 0);

			printf("Resuming process %d: ", pid);
			if (task_resume(pid) < 0) {
				printf("%s\n", strerror(errno));
				return CMD_RET_FAILURE;
			} else {
				task = find_task_by_pid(pid);
				printf("[%s] resumed\n", pcTaskGetName(task));
			}
		}
	}
	return 0;
}

CMD(resume, do_resume,
    "resume process(es)",
    "resume <PIDs | all>"
    );
#endif /* CONFIG_CMD_RESUME */

#ifdef CONFIG_CMD_KILL
static int do_kill(int argc, char *argv[])
{
	TaskHandle_t task;
	int pid;

	if (argc != 2)
		return CMD_RET_USAGE;

	pid = strtoul(argv[1], NULL, 0);
	task = find_task_by_pid(pid);
	if (!task) {
		printf("%s (%d) - No such process\n", argv[0], pid);
		return CMD_RET_FAILURE;
	}
	printf("Killing process %d (%s)\n", pid, pcTaskGetName(task));
	osThreadTerminate(task);
	return 0;
}

CMD(kill, do_kill,
    "kill a process",
    "kill PID");
#endif /* CONFIG_CMD_KILL */

#ifdef CONFIG_CMD_IRQ
int show_irq(int argc, char *argv[])
{
	char buf[1024];

	get_irq_stat(buf, sizeof(buf));
	fputs(buf, stdout);
#if (CONFIG_NR_SW_IRQ > 0)
	get_sw_irq_stat(buf, sizeof(buf));
	fputs(buf, stdout);
#endif
	return CMD_RET_SUCCESS;
}

CMD(irq, show_irq,
	"display irq information",
	"irq"
);
#endif /* CONFIG_CMD_IRQ */

#if defined(CONFIG_CMD_DMESG)

#include <FreeRTOS/FreeRTOS.h>
#include <FreeRTOS/task.h>

#include "cmsis_os.h"
#include <cli.h>
#include <proc.h>
#include <hal/console.h>

static int dmesg(int argc, char *argv[])
{
	TaskHandle_t thread;
	const osThreadAttr_t attr = {
		.name = "klogd",
		.stack_size = 512,
		.priority = osPriorityNormal,
	};

	if (argc > 2)
		return CMD_RET_USAGE;
	else if (argc == 1) {
		console_flush();
		return CMD_RET_SUCCESS;
	}

	if (!strcmp(argv[1], "start") || !strcmp(argv[1], "-w")) {
		thread = find_task_by_name("klogd");
		if (thread) {
			printf("klogd is already running (pid = %d)\n",
			       task_pid(thread));
		} else {
			thread = osThreadNew(klogd, NULL, &attr);
			if (!thread)
				printf("Could not start klogd\n");
		}
	} else if (!strcmp(argv[1], "stop") || !strcmp(argv[1], "-k")) {
		thread = find_task_by_name("klogd");
		if (thread) {
			printf("Stopping klogd (pid=%d)\n",
			       task_pid(thread));
			vTaskSuspend(thread);
			osThreadTerminate(thread);
		}
	}
	return CMD_RET_SUCCESS;
}

CMD(dmesg, dmesg,
	"display kernel messages",
	"dmesg [start|stop] or dmesg [-w|-k]"
);

#endif /* CONFIG_CMD_DMESG */
