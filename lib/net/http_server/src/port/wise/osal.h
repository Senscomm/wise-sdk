/*
 * SPDX-FileCopyrightText: 2018-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _OSAL_H_
#define _OSAL_H_

#include <FreeRTOS.h>
#include <task.h>
#include <unistd.h>
#include <stdint.h>
#include <cmsis_os.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OS_SUCCESS ESP_OK
#define OS_FAIL    ESP_FAIL

typedef osThreadId_t othread_t;

static inline int httpd_os_thread_create(othread_t *thread,
                                 const char *name, uint16_t stacksize, int prio,
                                 void (*thread_routine)(void *arg), void *arg,
                                 BaseType_t core_id)
{
	osThreadAttr_t attr = {
		.name		= name,
		.stack_size = stacksize,
		.priority	= prio,
	};

	(void) core_id;

	*thread = osThreadNew(thread_routine, arg, &attr);
    if (*thread == NULL) {
        return OS_FAIL;
    }

    return OS_SUCCESS;
}

/* Only self delete is supported */
static inline void httpd_os_thread_delete(void)
{
    osThreadExit();
}

static inline void httpd_os_thread_sleep(int msecs)
{
	osDelay(pdMS_TO_TICKS(msecs));
}

static inline othread_t httpd_os_thread_handle(void)
{
    return osThreadGetId();
}

#ifdef __cplusplus
}
#endif

#endif /* ! _OSAL_H_ */
