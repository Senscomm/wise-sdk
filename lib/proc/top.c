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
#include <unistd.h>
#include <string.h>

#include <cli.h>
#include <proc.h>

/*
 * FreeRTOSv10.0.0 vTaskList() has a bug where the current task (running)
 * is not properly formatted. Instead of correcting it, we implement our own.
 *
 * FIXME: remove task formatting functions from FreeRTOSConfig.h
 */

#define args(...) __VA_ARGS__
#define I(x) x
#define S(x) I(args x)

#define xstr(s) str(s)
#define str(s) #s

#define FMT "%-" xstr(configMAX_TASK_NAME_LEN) "s"
extern int time_hz;
#define tick_to_second(x) ((x) / (time_hz))

struct xtime {
	int dd;
	int hh;
	int mm;
	int ss;
};

static void ddhhmmss(long seconds, struct xtime *time) {

	if (!time)
		return;

	time->dd = seconds / (24 * 60 * 60);
	seconds -= time->dd * (24 * 60 * 60);
	time->hh = seconds / (60 * 60);
	seconds -= time->hh * (60 * 60);
	time->mm = seconds / 60;
	seconds -= time->mm * 60;
	time->ss = seconds;
};

static int do_top(int argc, char *argv[])
{
	TaskStatus_t *table, *task;
	int c, opt, interval = 3 * 1000; /* 3 seconds */
	int i, n_task, 	stat[6] = {0,};
	char *state[] = {"X", "R", "B", "S", "D", "I"};
	unsigned long ratio;
	uint32_t jiffies;
	struct xtime time;

	optind = 1;
	while ((opt = getopt(argc, argv, "d:")) != -1) {
		switch (opt) {
		case 'd':
			interval = atoi(optarg) * 1000;
			break;
		default:
			break;
		}
	}

	while (1) {
		memset(stat, 0, sizeof(stat));

		n_task = uxTaskGetNumberOfTasks();
		table = malloc(n_task * sizeof(TaskStatus_t));
		if (!table)
			return CMD_RET_FAILURE;

		n_task = uxTaskGetSystemState(table, n_task, &jiffies);

		ddhhmmss(tick_to_second(jiffies), &time);

		/* Print heading */
		printf("\n\x1b[2J\x1b[1;1Htop - %2d:%02d:%02d %3d days up\n\n",
			   time.hh, time.mm, time.ss, time.dd);

		printf("Mem(KiB): %5lu total, %5lu free\n",
			   (unsigned long) configTOTAL_HEAP_SIZE/1024,
			   (unsigned long) xPortGetFreeHeapSize()/1024);

		printf("\n\x1b[7m%4s%5s%6s%3s%7s%11s   %-44s\x1b[0m\n",
			   "PID", "PR", "STWM", "S", "%CPU+", "TIME+", "TASK");

		for (i = 0; i < n_task; i++) {
			task = table + i;

			ratio = ((uint64_t)task->ulRunTimeCounter) * 1000 / jiffies;
			ddhhmmss(tick_to_second(task->ulRunTimeCounter), &time);

			printf("%4lu%5lu%6d%3s%5lu.%1d%5d:%02d:%02d   "FMT"\n",
				   task->xTaskNumber,
				   task->uxCurrentPriority,
				   task->usStackHighWaterMark,
				   state[task->eCurrentState],
				   ratio / 10, (int) ratio % 10,
				   time.hh, time.mm, time.ss,
				   task->pcTaskName);

			stat[task->eCurrentState]++;
		}

		printf("\x1b[s\x1b[2;1H"
			   "Tasks: %3d total, %3d running, %3d sleeping, %3d blocked"
			   "\x1b[u",
			   n_task, stat[0] + stat[1], stat[3], stat[2]);

		vPortFree(table);

		/* Can we detect the serial input? */
		c = getchar_timeout(interval);
		if (c >= 0) {
			ungetc(c, stdin);
			return CMD_RET_SUCCESS;
		}
	}

	return CMD_RET_SUCCESS;
}

CMD(top, do_top,
	"display FreeRTOS tasks",
	"top -d secs"
);

static void print_task_info(threadinfo_t *ti, uint32_t *jiffies, void *data)
{
	char *state[] = {"X", "R", "B", "S", "D", "I"};
	struct xtime time;
	unsigned long ratio;

	ratio = ((uint64_t)ti->ulRunTimeCounter) * 1000 / *jiffies;
	ddhhmmss(tick_to_second(ti->ulRunTimeCounter), &time);

	printf("%4lu%5lu%6d%3s%5lu.%1d%5d:%02d:%02d "FMT" (0x%x-0x%x, 0x%x)""\n",
	       ti->xTaskNumber,
	       ti->uxCurrentPriority,
	       ti->usStackHighWaterMark,
	       state[ti->eCurrentState],
	       ratio / 10, (int) ratio % 10,
	       time.hh, time.mm, time.ss,
	       ti->pcTaskName,
		   ti->pxStackBase,
#if ( ( portSTACK_GROWTH > 0 ) || ( configRECORD_STACK_HIGH_ADDRESS == 1 ) )
		   ti->pxEndOfStack,
#else
		   0,
#endif
		   *(uint32_t *)(ti->xHandle) /* the last stack pointer */
		   );
}

static int do_ps(int argc, char *argv[])
{
	uint32_t jiffies;
	threadinfo_t info[20];
	int nr_task = sizeof(info)/sizeof(info[0]);

	nr_task = uxTaskGetSystemState(info, nr_task, &jiffies);

	printf("%4s%5s%6s%3s%7s%11s   %-44s\n",
	       "PID", "PR", "STWM", "S", "%CPU+", "TIME+", "TASK");

	iterate_task(print_task_info, &jiffies, NULL);

	return 0;
}

CMD(ps, do_ps, "report the current process snapshot", "ps");
