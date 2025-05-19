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

#ifndef __WISE_PROC_H__
#define __WISE_PROC_H__

#include <FreeRTOS/FreeRTOS.h>
#include <FreeRTOS/task.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef TaskHandle_t thread_t;
typedef TaskStatus_t threadinfo_t;

void iterate_task(void (*callback)(threadinfo_t *, uint32_t *, void *), uint32_t *jiffies, void *arg);
thread_t find_task_by_pid(int pid);

static inline thread_t find_task_by_name(const char *name)
{
	return xTaskGetHandle(name);
};

static inline int task_pid(thread_t th)
{
	threadinfo_t tsk;
	vTaskGetTaskInfo(th, &tsk, pdFALSE, eRunning);
	return tsk.xTaskNumber;
}

static inline const char *task_name(thread_t th)
{
	threadinfo_t tsk;
	vTaskGetTaskInfo(th, &tsk, pdFALSE, eRunning);
	return tsk.pcTaskName;
}

int task_is_freezable(thread_t task);
int task_suspend(int pid);
int task_resume(int pid);

#ifdef __cplusplus
}
#endif

#endif /* __WISE_PROC_H__ */
