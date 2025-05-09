/*
 * Copyright 2024-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include "FreeRTOS.h"
#include "task.h"

extern int debug_exception(void);
extern int debug_assertion(void);
extern int debug_stack_ovf(void);
extern int debug_heap_leak(void);
extern int debug_dead_lock(void);

int main(void)
{
#ifdef CONFIG_DEBUG_TYPE_EXCEPTION
	(void)debug_exception();
#endif
#ifdef CONFIG_DEBUG_TYPE_ASSERTION
	(void)debug_assertion();
#endif
#ifdef CONFIG_DEBUG_TYPE_STACK_OVF
	(void)debug_stack_ovf();
#endif
#ifdef CONFIG_DEBUG_TYPE_HEAP_LEAK
	(void)debug_heap_leak();
#endif
	return 0;
}
