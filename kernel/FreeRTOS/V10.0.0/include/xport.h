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

#ifndef __WISE_FREERTOS_XPORT_H__
#define __WISE_FREERTOS_XPORT_H__

#include <FreeRTOS/FreeRTOS.h>
#include <FreeRTOS/task.h>

/*
 * In case FreeRTOS port layer functions (*Port*()) should not
 * be called directly from FreeRTOS kernel core for some reason
 * (e.g. kernel core is ROMized), include this ile after defining
 * FREERTOS_PORT macro in port.c file.
 *
 * This will rename port function, and register a function table
 * structure via freertos_register_port() call, and makes the
 * port function call indirect via this table.
 *
 * Different ports define different port function, so this is not
 * a very general solution yet. Currently, only Andes D10 port
 * has been implemented and tested.
 */

struct freertos_port {
	StackType_t *(*init_stack)(StackType_t *, TaskFunction_t, void *);
	BaseType_t  (*start_scheduler)(void);
	void  	    (*end_scheduler)(void);
	void        (*enter_critical)(void);
	void        (*exit_critical)(void);
	void        (*yield)(void);
	void        (*stack_overflow)(TaskHandle_t, signed char *);
	unsigned long (*irq_save)(void);
	void        (*irq_restore)(unsigned long flags);
};

int freertos_register_port(struct freertos_port *);

#ifdef FREERTOS_PORT

#include <hal/init.h>

#define concat(x, y) xconcat(x, y)
#define xconcat(x, y) x ## y

StackType_t *concat(FREERTOS_PORT,_init_stack)(StackType_t *, TaskFunction_t, void *);
BaseType_t  concat(FREERTOS_PORT, _start_scheduler)(void);
void        concat(FREERTOS_PORT, _end_scheduler)(void);
void        concat(FREERTOS_PORT, _enter_critical)(void);
void        concat(FREERTOS_PORT, _exit_critical)(void);
void	    concat(FREERTOS_PORT, _yield)(void);
unsigned long concat(FREERTOS_PORT, _irq_save)(void);
void        concat(FREERTOS_PORT, _irq_restore)(unsigned long);
void        concat(FREERTOS_PORT, _stack_overflow)(TaskHandle_t, signed char *);

static struct freertos_port concat(FREERTOS_PORT,_port) = {
	.init_stack      = concat(FREERTOS_PORT, _init_stack),
 	.start_scheduler = concat(FREERTOS_PORT, _start_scheduler),
 	.end_scheduler   = concat(FREERTOS_PORT, _end_scheduler),
	.enter_critical  = concat(FREERTOS_PORT, _enter_critical),
	.exit_critical   = concat(FREERTOS_PORT, _exit_critical),
	.yield           = concat(FREERTOS_PORT, _yield),
	.stack_overflow  = concat(FREERTOS_PORT, _stack_overflow),
	.irq_save        = concat(FREERTOS_PORT, _irq_save),
	.irq_restore     = concat(FREERTOS_PORT, _irq_restore),
};

static int concat(FREERTOS_PORT,_register_port)(void)
{
	return freertos_register_port(&concat(FREERTOS_PORT, _port));
}
 __initcall__(arch, concat(FREERTOS_PORT, _register_port));

#define pxPortInitialiseStack 		concat(FREERTOS_PORT, _init_stack)
#define xPortStartScheduler 		concat(FREERTOS_PORT, _start_scheduler)
#define vPortEndScheduler 		concat(FREERTOS_PORT, _end_scheduler)
#define vPortEnterCritical 		concat(FREERTOS_PORT, _enter_critical)
#define vPortExitCritical 		concat(FREERTOS_PORT, _exit_critical)
#define vPortYield 			concat(FREERTOS_PORT, _yield)
#define vApplicationStackOverflowHook 	concat(FREERTOS_PORT, _stack_overflow)
#define ulPortSetInterruptMask		concat(FREERTOS_PORT, _irq_save)
#define vPortClearInterruptMask		concat(FREERTOS_PORT, _irq_restore)

#endif /* FREERTOS_PORT */

#endif /*__WISE_FREERTOS_XPORT_H__ */
