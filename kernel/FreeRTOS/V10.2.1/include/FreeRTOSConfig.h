/*
 * Copyright 2021-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __FREERTOSCONFIG_H__
#define __FREERTOSCONFIG_H__

#include <generated/kconfig2freertosopt.h>

/* config fixup */
#define configMINIMAL_STACK_SIZE (CONFIG_DEFAULT_STACK_SIZE / sizeof(StackType_t))
#if ( configNUM_THREAD_LOCAL_STORAGE_POINTERS > 0 )
/*
 * configTHREAD_LOCAL_STORAGE_DELETE_CALLBACKS config option must be enabled to
 * ensure the thread-specific data is cleaned up before the task is deleted.
 * If deletion callbacks are enabled, the number of TLSP's are doubled (i.e.,
 * the length of the TCB's pvThreadLocalStoragePointers This array). This allows
 * the latter half of the array to store the deletion callback pointers (whereas
 * the first half stores the TLSPs themselves).
 */
#define configTHREAD_LOCAL_STORAGE_DELETE_CALLBACKS 1
#endif

/****** Hardware specific settings. *******************************************/

/* This file is included from assembler files - make sure C code is not included
in assembler files. */
#ifndef __ASSEMBLER__
/*
 * The application must provide a function that configures a peripheral to
 * create the FreeRTOS tick interrupt, then define configSETUP_TICK_INTERRUPT()
 * in FreeRTOSConfig.h to call the function.  This file contains a function
 * that is suitable for use on the Zynq MPU.  FreeRTOS_Tick_Handler() must
 * be installed as the peripheral's interrupt handler.
 */

void vConfigureTickInterrupt( void );
#define configSETUP_TICK_INTERRUPT() vConfigureTickInterrupt()

void vClearTickInterrupt( void );
#define configCLEAR_TICK_INTERRUPT() vClearTickInterrupt()

/* worked on items */
#ifdef CONFIG_HEAP_AUTO_SIZE
extern size_t heap_total_size;
#define configTOTAL_HEAP_SIZE heap_total_size
#else
#ifdef CONFIG_HEAP_SIZE
#define configTOTAL_HEAP_SIZE CONFIG_HEAP_SIZE
#else
#define configTOTAL_HEAP_SIZE (CONFIG_HEAP1_SIZE + CONFIG_HEAP2_SIZE \
			       + CONFIG_HEAP3_SIZE)
#endif /* CONFIG_HEAP_SIZE */
#endif /* CONFIG_HEAP_AUTO_SIZE */

#define configTIMER_TASK_STACK_DEPTH (CONFIG_TIMER_TASK_STACK_SIZE / sizeof(StackType_t))

#if 0
extern void hal_config_timer(void);
extern unsigned hal_timer_value(void);
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS	hal_config_timer
#define portGET_RUN_TIME_COUNTER_VALUE 		hal_timer_value
#else
extern unsigned (*ktime)(void);
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS()
#define portGET_RUN_TIME_COUNTER_VALUE 		ktime

#endif

#if( CONFIG_USE_TICKLESS_IDLE == 1 )
#include "portmacro.h"

#ifdef CONFIG_PM_SCM2010
extern void pm_pre_sleep_processing(TickType_t tick);
extern void pm_post_sleep_processing(TickType_t tick);

#define configPRE_SLEEP_PROCESSING( tick )  \
    pm_pre_sleep_processing(tick); \
    /* handle wfi in our own implementation, and let freertos know about this by setting tick to 0 */ \
    tick = 0
#define configPOST_SLEEP_PROCESSING( tick ) \
    pm_post_sleep_processing(tick)
#endif
#else
#define configPRE_SLEEP_PROCESSING(tick)
#define configPOST_SLEEP_PROCESSING(tick)
#endif /* CONFIG_PM_SCM2010 */

#include "assert.h"
#define configASSERT assert
#endif

#endif /* __FREERTOSCONFIG_H__ */
