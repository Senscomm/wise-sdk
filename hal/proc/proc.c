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

#include <cli.h>

#include <hal/kernel.h>
#include <hal/kmem.h>

typedef TaskHandle_t thread_t;
typedef TaskStatus_t threadinfo_t;

#define ti_pid(ti) 	((int) (ti)->xTaskNumber)
#define ti_name(ti)	(ti)->pcTaskName

void iterate_task(void (*callback)(TaskStatus_t *, uint32_t *, void *), uint32_t *jiffies, void *arg)
{
	TaskStatus_t *info;
	int i, nr_task;

	nr_task = uxTaskGetNumberOfTasks();
	info = kmalloc(nr_task * sizeof(*info));
	if (info == NULL)
		return;

	nr_task = uxTaskGetSystemState(info, nr_task, jiffies);
	for (i = 0; i < nr_task; i++) {
		callback(&info[i], jiffies, arg);
	}

	kfree(info);
	return;
}

struct task_iterator_data {
	int pid;
	void (*action)(thread_t task);
	thread_t task;
	int ret;
};

static void task_match_pid(TaskStatus_t *ti, uint32_t *jiffies, void *data)
{
	struct task_iterator_data *ctx = data;

	if (ti->xTaskNumber == ctx->pid) {
		assert(ctx->task == NULL);
		ctx->task = ti->xHandle;
	}
}

thread_t find_task_by_pid(int pid)
{
	struct task_iterator_data context = {
		.pid = pid,
	};

	iterate_task(task_match_pid, NULL, &context);

	return context.task;
}

static const char *unfreezables[] = {
	CONFIG_IDLE_TASK_NAME,
	"init",
	"shell",
	"init",
};

int task_is_freezable(thread_t task)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(unfreezables); i++) {
		if (!strcmp(pcTaskGetName(task), unfreezables[i]))
			return 0;
	}
	return 1;
}

static void try_suspend_one_task(threadinfo_t *ti, uint32_t *jiffies, void *data)
{
	thread_t task = ti->xHandle;

	if (task_is_freezable(task)) {
		printf("Suspending %s\n", ti_name(ti));
		vTaskSuspend(task);
	} else {
		printf("%s is not freezable\n", ti_name(ti));
	}
}

static void task_suspend_all(void)
{
	iterate_task(try_suspend_one_task, NULL, NULL);

	return;
}

int task_suspend(int pid)
{
	thread_t task;

	if (pid == -1) {
		task_suspend_all();
		return 0;
	}

	if ((task = find_task_by_pid(pid)) == NULL) {
		errno = ESRCH;
		return -1;
	}
	if (!task_is_freezable(task)) {
		errno = EACCES;
		return -1;
	}
	vTaskSuspend(task);
	return 0;
}

static void try_resume_one_task(threadinfo_t *ti, uint32_t *jiffies, void *data)
{
	thread_t task = ti->xHandle;

	if (eTaskGetState(task) == eSuspended) {
		printf("Resuming %s\n", ti_name(ti));
		vTaskResume(task);
	}
}

static void task_resume_all(void)
{
	iterate_task(try_resume_one_task, NULL, NULL);

	return;
}

int task_resume(int pid)
{
	thread_t task;

	if (pid == -1) {
		task_resume_all();
		return 0;
	}

	if ((task = find_task_by_pid(pid)) == NULL) {
		errno = ESRCH;
		return -1;
	}
	if (eTaskGetState(task) != eSuspended) {
		errno = EACCES;
		return -1;
	}
	vTaskResume(task);
	return 0;
}
