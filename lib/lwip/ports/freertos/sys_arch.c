/*
 * Copyright (c) 2017 Simon Goldschmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Simon Goldschmidt <goldsimon@gmx.de>
 *
 */

/* lwIP includes. */
#include "lwip/debug.h"
#include "lwip/def.h"
#include "lwip/sys.h"
#include "lwip/mem.h"
#include "lwip/stats.h"
#include "lwip/tcpip.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"

/** Set this to 1 to use a mutex for SYS_ARCH_PROTECT() critical regions.
 * Default is 0 and locks interrupts/scheduler for SYS_ARCH_PROTECT().
 */
#ifndef LWIP_RTOS_SYS_ARCH_PROTECT_USES_MUTEX
#define LWIP_RTOS_SYS_ARCH_PROTECT_USES_MUTEX     0
#endif

/** Set this to 1 to include a sanity check that SYS_ARCH_PROTECT() and
 * SYS_ARCH_UNPROTECT() are called matching.
 */
#ifndef LWIP_RTOS_SYS_ARCH_PROTECT_SANITY_CHECK
#define LWIP_RTOS_SYS_ARCH_PROTECT_SANITY_CHECK   0
#endif

/** Set this to 1 to let sys_mbox_free check that queues are empty when freed */
#ifndef LWIP_RTOS_CHECK_QUEUE_EMPTY_ON_FREE
#define LWIP_RTOS_CHECK_QUEUE_EMPTY_ON_FREE       0
#endif

/** Set this to 1 to enable core locking check functions in this port.
 * For this to work, you'll have to define LWIP_ASSERT_CORE_LOCKED()
 * and LWIP_MARK_TCPIP_THREAD() correctly in your lwipopts.h! */
#ifndef LWIP_RTOS_CHECK_CORE_LOCKING
#define LWIP_RTOS_CHECK_CORE_LOCKING              0
#endif

/** Set this to 0 to implement sys_now() yourself, e.g. using a hw timer.
 * Default is 1, where RTOS ticks are used to calculate back to ms.
 */
#ifndef LWIP_RTOS_SYS_NOW_FROM_RTOS
#define LWIP_RTOS_SYS_NOW_FROM_RTOS           1
#endif

#if SYS_LIGHTWEIGHT_PROT && LWIP_RTOS_SYS_ARCH_PROTECT_USES_MUTEX
static osSemaphoreId_t sys_arch_protect_mutex;
#endif
#if SYS_LIGHTWEIGHT_PROT && LWIP_RTOS_SYS_ARCH_PROTECT_SANITY_CHECK
static sys_prot_t sys_arch_protect_nesting;
#endif

#ifndef TICK_PERIOD_MS
#define TICK_PERIOD_MS ((u32_t) 1000 / osKernelGetTickFreq())
#endif

/* Initialize this module (see description in sys.h) */
void
sys_init(void)
{
#if SYS_LIGHTWEIGHT_PROT && LWIP_RTOS_SYS_ARCH_PROTECT_USES_MUTEX
  /* initialize sys_arch_protect global mutex */
  osMutexAttr_t attr = {NULL, osMutexRecursive, NULL, 0};
  sys_arch_protect_mutex = osMutexNew(&attr);
  LWIP_ASSERT("failed to create sys_arch_protect mutex",
    sys_arch_protect_mutex != NULL);
#endif /* SYS_LIGHTWEIGHT_PROT && LWIP_RTOS_SYS_ARCH_PROTECT_USES_MUTEX */
}

#if LWIP_RTOS_SYS_NOW_FROM_RTOS
u32_t
sys_now(void)
{
  return osKernelGetTickCount() * TICK_PERIOD_MS;
}
#endif

u32_t
sys_jiffies(void)
{
  return osKernelGetTickCount();
}

#if SYS_LIGHTWEIGHT_PROT

sys_prot_t
sys_arch_protect(void)
{
#if LWIP_RTOS_SYS_ARCH_PROTECT_USES_MUTEX
  osStatus_t ret;
  LWIP_ASSERT("sys_arch_protect_mutex != NULL", sys_arch_protect_mutex != NULL);

  ret = osMutexAcquire(sys_arch_protect_mutex, osWaitForever);
  LWIP_ASSERT("sys_arch_protect failed to take the mutex", ret == osOK);
#else /* LWIP_RTOS_SYS_ARCH_PROTECT_USES_MUTEX */
  osCriticalSectionEnter();
#endif /* LWIP_RTOS_SYS_ARCH_PROTECT_USES_MUTEX */
#if LWIP_RTOS_SYS_ARCH_PROTECT_SANITY_CHECK
  {
    /* every nested call to sys_arch_protect() returns an increased number */
    sys_prot_t ret = sys_arch_protect_nesting;
    sys_arch_protect_nesting++;
    LWIP_ASSERT("sys_arch_protect overflow", sys_arch_protect_nesting > ret);
    return ret;
  }
#else
  return 1;
#endif
}

void
sys_arch_unprotect(sys_prot_t pval)
{
#if LWIP_RTOS_SYS_ARCH_PROTECT_USES_MUTEX
  osStatus_t ret;
#endif
#if LWIP_RTOS_SYS_ARCH_PROTECT_SANITY_CHECK
  LWIP_ASSERT("unexpected sys_arch_protect_nesting", sys_arch_protect_nesting > 0);
  sys_arch_protect_nesting--;
  LWIP_ASSERT("unexpected sys_arch_protect_nesting", sys_arch_protect_nesting == pval);
#endif

#if LWIP_RTOS_SYS_ARCH_PROTECT_USES_MUTEX
  LWIP_ASSERT("sys_arch_protect_mutex != NULL", sys_arch_protect_mutex != NULL);

  ret = osMutexRelease(sys_arch_protect_mutex);
  LWIP_ASSERT("sys_arch_unprotect failed to give the mutex", ret == osOK);
#else /* LWIP_RTOS_SYS_ARCH_PROTECT_USES_MUTEX */
  osCriticalSectionExit(pval);
#endif /* LWIP_RTOS_SYS_ARCH_PROTECT_USES_MUTEX */
  LWIP_UNUSED_ARG(pval);
}

#endif /* SYS_LIGHTWEIGHT_PROT */

void
sys_arch_msleep(u32_t delay_ms)
{
  u32_t delay_ticks = delay_ms / TICK_PERIOD_MS;
  osDelay(delay_ticks);
}

#if !LWIP_COMPAT_MUTEX

/* Create a new mutex*/
err_t
sys_mutex_new(sys_mutex_t *mutex)
{
  osMutexAttr_t attr = {NULL, osMutexRecursive, NULL, 0};

  LWIP_ASSERT("mutex != NULL", mutex != NULL);

  mutex->mut = osMutexNew(&attr);
  if(mutex->mut == NULL) {
    SYS_STATS_INC(mutex.err);
    return ERR_MEM;
  }
  SYS_STATS_INC_USED(mutex);
  return ERR_OK;
}

void
sys_mutex_lock(sys_mutex_t *mutex)
{
  osStatus_t ret;
  LWIP_ASSERT("mutex != NULL", mutex != NULL);
  LWIP_ASSERT("mutex->mut != NULL", mutex->mut != NULL);

  ret = osMutexAcquire(mutex->mut, osWaitForever);
  LWIP_ASSERT("failed to take the mutex", ret == osOK);
#ifdef LWIP_NOASSERT
  UNUSED(ret);
#endif
}

void
sys_mutex_unlock(sys_mutex_t *mutex)
{
  osStatus_t ret;
  LWIP_ASSERT("mutex != NULL", mutex != NULL);
  LWIP_ASSERT("mutex->mut != NULL", mutex->mut != NULL);

  ret = osMutexRelease(mutex->mut);
  LWIP_ASSERT("failed to give the mutex", ret == osOK);
#ifdef LWIP_NOASSERT
  UNUSED(ret);
#endif
}

void
sys_mutex_free(sys_mutex_t *mutex)
{
  LWIP_ASSERT("mutex != NULL", mutex != NULL);
  LWIP_ASSERT("mutex->mut != NULL", mutex->mut != NULL);

  SYS_STATS_DEC(mutex.used);
  osMutexDelete(mutex->mut);
  mutex->mut = NULL;
}

#endif /* !LWIP_COMPAT_MUTEX */

err_t
sys_sem_new(sys_sem_t *sem, u8_t initial_count)
{
  osSemaphoreAttr_t attr = {NULL, 0, NULL, 0};
  LWIP_ASSERT("sem != NULL", sem != NULL);
  LWIP_ASSERT("initial_count invalid (not 0 or 1)",
    (initial_count == 0) || (initial_count == 1));

  sem->sem = osSemaphoreNew(1, 0, &attr);
  if(sem->sem == NULL) {
    SYS_STATS_INC(sem.err);
    return ERR_MEM;
  }
  SYS_STATS_INC_USED(sem);

  if(initial_count == 1) {
    osStatus_t ret = osSemaphoreRelease(sem->sem);
    LWIP_ASSERT("sys_sem_new: initial give failed", ret == osOK);
#ifdef LWIP_NOASSERT
    UNUSED(ret);
#endif
  }
  return ERR_OK;
}

void
sys_sem_signal(sys_sem_t *sem)
{
  osStatus_t ret;
  LWIP_ASSERT("sem != NULL", sem != NULL);
  LWIP_ASSERT("sem->sem != NULL", sem->sem != NULL);

  ret = osSemaphoreRelease(sem->sem);
  /* queue full is OK, this is a signal only... */
  LWIP_ASSERT("sys_sem_signal: sane return value",
    (ret == osOK) || (ret == osErrorResource));
#ifdef LWIP_NOASSERT
  UNUSED(ret);
#endif

}

u32_t
sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout_ms)
{
  osStatus_t ret;
  LWIP_ASSERT("sem != NULL", sem != NULL);
  LWIP_ASSERT("sem->sem != NULL", sem->sem != NULL);

  if(!timeout_ms) {
    /* wait infinite */
    ret = osSemaphoreAcquire(sem->sem, osWaitForever);
    LWIP_ASSERT("taking semaphore failed", ret == osOK);
  } else {
    u32_t timeout_ticks = timeout_ms / TICK_PERIOD_MS;
    ret = osSemaphoreAcquire(sem->sem, timeout_ticks);
    if (ret == osErrorTimeout) {
      /* timed out */
      return SYS_ARCH_TIMEOUT;
    }
    LWIP_ASSERT("taking semaphore failed", ret == osOK);
  }

  /* Old versions of lwIP required us to return the time waited.
     This is not the case any more. Just returning != SYS_ARCH_TIMEOUT
     here is enough. */
  return 1;
}

void
sys_sem_free(sys_sem_t *sem)
{
  LWIP_ASSERT("sem != NULL", sem != NULL);
  LWIP_ASSERT("sem->sem != NULL", sem->sem != NULL);

  SYS_STATS_DEC(sem.used);
  osSemaphoreDelete(sem->sem);
  sem->sem = NULL;
}

err_t
sys_mbox_new(sys_mbox_t *mbox, int size)
{
  osMessageQueueAttr_t attr = {NULL, 0, NULL, 0, NULL, 0};

  LWIP_ASSERT("mbox != NULL", mbox != NULL);
  LWIP_ASSERT("size > 0", size > 0);

  mbox->mbx = osMessageQueueNew(size, sizeof(void *), &attr);
  if(mbox->mbx == NULL) {
    SYS_STATS_INC(mbox.err);
    return ERR_MEM;
  }
  SYS_STATS_INC_USED(mbox);
  return ERR_OK;
}

void
sys_mbox_post(sys_mbox_t *mbox, void *msg)
{
  osStatus_t ret;
  LWIP_ASSERT("mbox != NULL", mbox != NULL);
  LWIP_ASSERT("mbox->mbx != NULL", mbox->mbx != NULL);

  ret = osMessageQueuePut(mbox->mbx, &msg, 0, osWaitForever);
  LWIP_ASSERT("mbox post failed", ret == osOK);
#ifdef LWIP_NOASSERT
  UNUSED(ret);
#endif
}

err_t
sys_mbox_trypost(sys_mbox_t *mbox, void *msg)
{
  osStatus_t ret;
  LWIP_ASSERT("mbox != NULL", mbox != NULL);
  LWIP_ASSERT("mbox->mbx != NULL", mbox->mbx != NULL);

  ret = osMessageQueuePut(mbox->mbx, &msg, 0, 0);
  if (ret == osOK || ret == osErrorNeedSched) {
	  return ERR_OK;
  } else if (ret == osErrorResource) {
	  return ERR_WOULDBLOCK;
  } else {
	  //LWIP_ASSERT("mbox trypost failed", ret == osErrorResource);
	  assert(0);
	  SYS_STATS_INC(mbox.err);
	  return ERR_MEM;
  }
}

err_t
sys_mbox_trypost_fromisr(sys_mbox_t *mbox, void *msg)
{
  osStatus_t ret;
  LWIP_ASSERT("mbox != NULL", mbox != NULL);
  LWIP_ASSERT("mbox->mbx != NULL", mbox->mbx != NULL);

  ret = osMessageQueuePut(mbox->mbx, &msg, 0, 0);
  if (ret == osOK) {
    return ERR_OK;
  } else if (ret == osErrorNeedSched) {
    return ERR_NEED_SCHED;
  } else {
    LWIP_ASSERT("mbox trypost failed", ret == osErrorResource);
    SYS_STATS_INC(mbox.err);
    return ERR_MEM;
  }
}

u32_t
sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout_ms)
{
  osStatus_t ret;
  void *msg_dummy;
  LWIP_ASSERT("mbox != NULL", mbox != NULL);
  LWIP_ASSERT("mbox->mbx != NULL", mbox->mbx != NULL);

  if (!msg) {
    msg = &msg_dummy;
  }

  if (!timeout_ms) {
    /* wait infinite */
    ret = osMessageQueueGet(mbox->mbx, &(*msg), NULL, osWaitForever);
    LWIP_ASSERT("mbox fetch failed", ret == osOK);
  } else {
    u32_t timeout_ticks = timeout_ms / TICK_PERIOD_MS;
    ret = osMessageQueueGet(mbox->mbx, &(*msg), NULL, timeout_ticks);
    if (ret == osErrorTimeout) {
      /* timed out */
      *msg = NULL;
      return SYS_ARCH_TIMEOUT;
    }
    LWIP_ASSERT("mbox fetch failed", ret == osOK);
  }

  /* Old versions of lwIP required us to return the time waited.
     This is not the case any more. Just returning != SYS_ARCH_TIMEOUT
     here is enough. */
  return 1;
}

u32_t
sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg)
{
  osStatus_t ret;
  void *msg_dummy;
  LWIP_ASSERT("mbox != NULL", mbox != NULL);
  LWIP_ASSERT("mbox->mbx != NULL", mbox->mbx != NULL);

  if (!msg) {
    msg = &msg_dummy;
  }

  ret = osMessageQueueGet(mbox->mbx, &(*msg), NULL, 0);
  if (ret == osErrorResource) {
    *msg = NULL;
    return SYS_MBOX_EMPTY;
  }
  LWIP_ASSERT("mbox fetch failed", ret == osOK);

  /* Old versions of lwIP required us to return the time waited.
     This is not the case any more. Just returning != SYS_ARCH_TIMEOUT
     here is enough. */
  return 1;
}

void
sys_mbox_free(sys_mbox_t *mbox)
{
  LWIP_ASSERT("mbox != NULL", mbox != NULL);
  LWIP_ASSERT("mbox->mbx != NULL", mbox->mbx != NULL);

#if LWIP_RTOS_CHECK_QUEUE_EMPTY_ON_FREE
  {
    u32_t msgs_waiting = osMessageQueueGetCount(mbox->mbx);
    LWIP_ASSERT("mbox quence not empty", msgs_waiting == 0);

    if (msgs_waiting != 0) {
      SYS_STATS_INC(mbox.err);
    }
  }
#endif

  osMessageQueueDelete(mbox->mbx);

  SYS_STATS_DEC(mbox.used);
}

sys_thread_t
sys_thread_new(const char *name, lwip_thread_fn thread, void *arg, int stacksize, int prio)
{
  osThreadId_t rtos_task;
  osThreadAttr_t attr = {name, 0, NULL, 0, NULL, stacksize, prio+osPriorityNormal};
  sys_thread_t lwip_thread;

  LWIP_ASSERT("invalid stacksize", stacksize > 0);

  rtos_task = osThreadNew(thread, arg, &attr);
  LWIP_ASSERT("task creation failed", rtos_task != NULL);

  lwip_thread.thread_handle = rtos_task;
  return lwip_thread;
}

#if LWIP_NETCONN_SEM_PER_THREAD
#if configNUM_THREAD_LOCAL_STORAGE_POINTERS > 0

sys_sem_t *
sys_arch_netconn_sem_get(void)
{
  void *ret;
  TaskHandle_t task = xTaskGetCurrentTaskHandle();
  LWIP_ASSERT("task != NULL", task != NULL);

  /* XXX: allocate per-thread semaphore based on
   *      actual demand.
   */
try:
  ret = pvTaskGetThreadLocalStoragePointer(task, 0);
  if (ret == NULL) {
    sys_arch_netconn_sem_alloc();
	goto try;
  }
  return ret;
}

/* Clean up callback for deleted tasks.
   If the thread was created and that thread ends,
   then the FreeRTOS thread-local-storage is removed before the FreeRTOS task is deleted.
 */
static void sys_arch_netconn_sem_deleted_callback(int index, void *v_sem)
{
  sys_sem_t *sem = (sys_sem_t *)v_sem;
  assert(sem != NULL);

  sys_sem_free(sem);
  mem_free(sem);
}

void
sys_arch_netconn_sem_alloc(void)
{
  void *ret;
  TaskHandle_t task = xTaskGetCurrentTaskHandle();
  LWIP_ASSERT("task != NULL", task != NULL);

  ret = pvTaskGetThreadLocalStoragePointer(task, 0);
  if(ret == NULL) {
    sys_sem_t *sem;
    err_t err;
    /* need to allocate the memory for this semaphore */
    sem = mem_malloc(sizeof(sys_sem_t));
    LWIP_ASSERT("sem != NULL", sem != NULL);
    err = sys_sem_new(sem, 0);
    LWIP_ASSERT("err == ERR_OK", err == ERR_OK);
    LWIP_ASSERT("sem invalid", sys_sem_valid(sem));
#ifdef LWIP_NOASSERT
    UNUSED(err);
#endif
    vTaskSetThreadLocalStoragePointerAndDelCallback(task, 0, sem,
      sys_arch_netconn_sem_deleted_callback);
  }
}

void sys_arch_netconn_sem_free(void)
{
  void* ret;
  TaskHandle_t task = xTaskGetCurrentTaskHandle();
  LWIP_ASSERT("task != NULL", task != NULL);

  ret = pvTaskGetThreadLocalStoragePointer(task, 0);
  if(ret != NULL) {
    sys_sem_t *sem = ret;
    sys_sem_free(sem);
    mem_free(sem);
    vTaskSetThreadLocalStoragePointer(task, 0, NULL);
  }
}

#else /* configNUM_THREAD_LOCAL_STORAGE_POINTERS > 0 */
#error LWIP_NETCONN_SEM_PER_THREAD needs configNUM_THREAD_LOCAL_STORAGE_POINTERS
#endif /* configNUM_THREAD_LOCAL_STORAGE_POINTERS > 0 */

#endif /* LWIP_NETCONN_SEM_PER_THREAD */

#if LWIP_RTOS_CHECK_CORE_LOCKING
#if LWIP_TCPIP_CORE_LOCKING

/** Flag the core lock held. A counter for recursive locks. */
static u8_t lwip_core_lock_count;
static osThreadId_t lwip_core_lock_holder_thread = NULL;

#ifdef LWIP_LOCK_LAST_HOLDER_DEBUG
static const char *lwip_core_lock_func;
static int lwip_core_lock_line;
static const char *lwip_core_unlock_func;
static int lwip_core_unlock_line;
#endif
void
#ifdef LWIP_LOCK_LAST_HOLDER_DEBUG
sys_lock_tcpip_core(const char *func, const int line)
#else
sys_lock_tcpip_core(void)
#endif
{
   sys_mutex_lock(&lock_tcpip_core);
   if (lwip_core_lock_count == 0) {
       lwip_core_lock_holder_thread = osThreadGetId();
#ifdef LWIP_LOCK_LAST_HOLDER_DEBUG
       lwip_core_unlock_func = NULL;
       lwip_core_unlock_line = 0;
#endif
   }
   lwip_core_lock_count++;
#ifdef LWIP_LOCK_LAST_HOLDER_DEBUG
   lwip_core_lock_func = func;
   lwip_core_lock_line = line;
#endif
}

void
#ifdef LWIP_LOCK_LAST_HOLDER_DEBUG
sys_unlock_tcpip_core(const char *func, const int line)
#else
sys_unlock_tcpip_core(void)
#endif
{
   lwip_core_lock_count--;
   if (lwip_core_lock_count == 0) {
#ifdef LWIP_LOCK_LAST_HOLDER_DEBUG
       lwip_core_lock_func = NULL;
       lwip_core_lock_line = 0;
#endif
       lwip_core_lock_holder_thread = 0;
   }
   sys_mutex_unlock(&lock_tcpip_core);
#ifdef LWIP_LOCK_LAST_HOLDER_DEBUG
   lwip_core_unlock_func = func;
   lwip_core_unlock_line = line;
#endif
}

#endif /* LWIP_TCPIP_CORE_LOCKING */

static osThreadId_t lwip_tcpip_thread;

void
sys_mark_tcpip_thread(void)
{
  lwip_tcpip_thread = osThreadGetId();
}

void
sys_check_core_locking(void)
{
  /* Embedded systems should check we are NOT in an interrupt context here */

  if (lwip_tcpip_thread != 0) {
    osThreadId_t current_thread = osThreadGetId();
	int core_locked, tcpip_thread_context;

#if LWIP_TCPIP_CORE_LOCKING
	core_locked = (current_thread == lwip_core_lock_holder_thread && lwip_core_lock_count > 0) ? 1 : 0;

#endif /* LWIP_TCPIP_CORE_LOCKING */
	tcpip_thread_context = (current_thread == lwip_tcpip_thread) ? 1 : 0;
    LWIP_ASSERT("Function called without core lock and from a wrong thread",
			!!core_locked || !!tcpip_thread_context);
#ifdef LWIP_NOASSERT
    UNUSED(core_locked);
    UNUSED(tcpip_thread_context);
#endif

  }
}

int
sys_is_tcpip_context(void)
{
  if (lwip_tcpip_thread != 0)
	return (osThreadGetId() == lwip_tcpip_thread) ? 1 : 0;
  else
    return 0;
}

int
sys_is_core_locked(void)
{
#if LWIP_TCPIP_CORE_LOCKING
  if (lwip_tcpip_thread != 0) {
    osThreadId_t current_thread = osThreadGetId();
	return (current_thread == lwip_core_lock_holder_thread && lwip_core_lock_count > 0) ? 1 : 0;
  }
#endif /* LWIP_TCPIP_CORE_LOCKING */
  return 0;
}


#ifdef LWIP_LOCK_LAST_HOLDER_DEBUG
const char *sys_lock_current_holder_fn(void)
{
   return lwip_core_lock_func;
}

int sys_lock_current_holder_ln(void)
{
   return lwip_core_lock_line;
}

const char *sys_lock_last_holder_fn(void)
{
   return lwip_core_unlock_func;
}

int sys_lock_last_holder_ln(void)
{
   return lwip_core_unlock_line;
}
#endif

#endif /* LWIP_RTOS_CHECK_CORE_LOCKING*/
