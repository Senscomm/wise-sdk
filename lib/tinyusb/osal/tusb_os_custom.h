/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * This file is part of the TinyUSB stack.
 */

#ifndef _TUSB_TUSB_OS_CUSTOM_H_
#define _TUSB_TUSB_OS_CUSTOM_H_

// FreeRTOS Headers
#include TU_INCLUDE_PATH(CFG_TUSB_OS_INC_PATH,FreeRTOS.h)
#include TU_INCLUDE_PATH(CFG_TUSB_OS_INC_PATH,semphr.h)
#include TU_INCLUDE_PATH(CFG_TUSB_OS_INC_PATH,queue.h)
#include TU_INCLUDE_PATH(CFG_TUSB_OS_INC_PATH,task.h)

#include "cmsis_os.h"

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------+
// TASK API
//--------------------------------------------------------------------+
static inline void osal_task_delay(uint32_t msec)
{
  osDelay( pdMS_TO_TICKS(msec) );
}

//--------------------------------------------------------------------+
// Semaphore API
//--------------------------------------------------------------------+
typedef osSemaphoreAttr_t osal_semaphore_def_t;
typedef osSemaphoreId_t   osal_semaphore_t;

static inline osal_semaphore_t osal_semaphore_create(osal_semaphore_def_t* semdef)
{
  (void) semdef;
  return osSemaphoreNew(1, 1, NULL);
}

static inline bool osal_semaphore_post(osal_semaphore_t sem_hdl, bool in_isr)
{
  // in_isr is handled inherently in osSemaphoreRelease().
  (void) in_isr;
  return (osSemaphoreRelease(sem_hdl) != osOK);
}

static inline bool osal_semaphore_wait (osal_semaphore_t sem_hdl, uint32_t msec)
{
  uint32_t const to = (msec == OSAL_TIMEOUT_WAIT_FOREVER) ? osWaitForever : pdMS_TO_TICKS(msec);
  return (osSemaphoreAcquire(sem_hdl, to) != osOK);
}

static inline void osal_semaphore_reset(osal_semaphore_t const sem_hdl)
{
  // TODO implement later
  (void) sem_hdl;
}

//--------------------------------------------------------------------+
// MUTEX API (priority inheritance)
//--------------------------------------------------------------------+
typedef osMutexAttr_t     osal_mutex_def_t;
typedef osMutexId_t       osal_mutex_t;

static inline osal_mutex_t osal_mutex_create(osal_mutex_def_t* mdef)
{
  (void) mdef;
  return osMutexNew(NULL);
}

static inline bool osal_mutex_lock (osal_mutex_t mutex_hdl, uint32_t msec)
{
  uint32_t const to = (msec == OSAL_TIMEOUT_WAIT_FOREVER) ? osWaitForever : pdMS_TO_TICKS(msec);
  return (osMutexAcquire(mutex_hdl, to) != osOK);
}

static inline bool osal_mutex_unlock(osal_mutex_t mutex_hdl)
{
  return (osMutexRelease(mutex_hdl) != osOK);
}

//--------------------------------------------------------------------+
// QUEUE API
//--------------------------------------------------------------------+

// role device/host is used by OS NONE for mutex (disable usb isr) only
#define OSAL_QUEUE_DEF(_role, _name, _depth, _type) \
  static _type _name##_##buf[_depth];\
  osal_queue_def_t _name = { .depth = _depth, .item_sz = sizeof(_type), .buf = _name##_##buf };

typedef struct
{
  uint16_t depth;
  uint16_t item_sz;
  void*    buf;

  StaticQueue_t sq;
}osal_queue_def_t;

typedef osMessageQueueId_t osal_queue_t;

static inline osal_queue_t osal_queue_create(osal_queue_def_t* qdef)
{
  osMessageQueueAttr_t attr = {
	  .cb_mem  = &qdef->sq,
	  .cb_size = sizeof(qdef->sq),
	  .mq_mem  = qdef->buf,
	  .mq_size = qdef->depth * qdef->item_sz
  };
  return osMessageQueueNew(qdef->depth, qdef->item_sz, &attr);
}

static inline bool osal_queue_receive(osal_queue_t qhdl, void* data)
{
  return (osMessageQueueGet(qhdl, data, 0, osWaitForever) == osOK);
}

static inline bool osal_queue_send(osal_queue_t qhdl, void const * data, bool in_isr)
{
  uint32_t timeout;
  osStatus_t status;
  osKernelState_t state = osKernelGetState();

  /* We also need to consider situations where
   * we are in a task context with interrupts masked.
   * In such cases, we can't set non-zero timeout.
   */
  in_isr = in_isr || (state == osKernelError);

  timeout = (in_isr ? 0 : osWaitForever);

  status = osMessageQueuePut(qhdl, data, 0, timeout);

  if (status == osErrorNeedSched)
  {
    portYIELD_FROM_ISR(true);
	/* This is actually not an error. */
	status = osOK;
  }

  return (status != osOK);
}

static inline bool osal_queue_empty(osal_queue_t qhdl)
{
  return osMessageQueueGetCount(qhdl) == 0;
}

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_TUSB_OS_CUSTOM_H_ */
