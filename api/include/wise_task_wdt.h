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

#ifndef _WISE_TASK_WDT_H_
#define _WISE_TASK_WDT_H_

#include <stdint.h>

#include "cmsis_os.h"
#include "wise_err.h"

/**
 * @brief Initialize task watchdog
 *
 * @param[in] timeout_s expire time of task watchdog
 */
wise_err_t wise_task_wdt_init(uint32_t timeout_s);

/**
 * @brief Susbscribe a task to the task watchdog
 *
 * @param task_id ID of the task. Input NULL to subscribe the current
 *                running task to the task watchdog
 */
wise_err_t wise_task_wdt_add(osThreadId_t task_id);

/**
 * @brief Unsusbscribe a task to the task watchdog
 *
 * @param task_id ID of the task. Input NULL to unsubscribe the current
 *                running task to the task watchdog
 */
wise_err_t wise_task_wdt_delete(osThreadId_t task_id);

/**
 * @brief Reset the task watchdog.
 *
 * @param task_id ID of the task. Input NULL to reset the current
 *                running task to the task watchdog
 */
wise_err_t wise_task_wdt_reset(osThreadId_t task_id);

#endif //_WISE_TASK_WDT_H_
