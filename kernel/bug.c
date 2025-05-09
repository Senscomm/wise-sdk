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

#ifdef CONFIG_BOOTROM

void panic(const char *msg) {;}

int kernel_enter_panic(void) {return 0;}

void hal_assert_fail(const char *assert, const char *file, unsigned line,
		     const char *func) {;}

#else

#include <stdio.h>
#include <string.h>

void *get_pc(void)
{
	return __builtin_return_address(0);
}

#include <FreeRTOS/FreeRTOS.h>
#include <FreeRTOS/task.h>
#include <hal/console.h>
#include <hal/compiler.h>
#include <hal/rom.h>
#include <bug.h>

static void default_panic(const char *msg)
{
	printk("%s", msg);

	while (1);
}

void (*panic)(const char *) = default_panic;

static char panic_buf[128];

void _hal_assert_fail(const char *assert, const char *file, unsigned line,
		     const char *func)
{
	vTaskSuspendAll();
	if (assert) {
	    snprintf(panic_buf, sizeof(panic_buf),
                "BUG: assertion(%s), %s() at %d",
                assert, func, line);
	} else {
		snprintf(panic_buf, sizeof(panic_buf),
                "BUG: assertion at 0x%x", __builtin_return_address(0));
	}
	panic(panic_buf);
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(hal_assert_fail, &hal_assert_fail, &_hal_assert_fail);
#else
__func_tab__ void (*hal_assert_fail)(const char *assert, const char *file,
	unsigned line, const char *func) = _hal_assert_fail;
#endif


#include <freebsd/atomic.h>

static atomic_t kernel_panic_level = ATOMIC_INIT(0);

int kernel_enter_panic(void)
{
	return atomic_inc(&kernel_panic_level);
}

int kernel_leave_panic(void)
{
	return atomic_dec(&kernel_panic_level);
}

int kernel_in_panic(void)
{
	return atomic_read(&kernel_panic_level);
}

#if 0 /* Should go elsewhere */
#include <assert.h>
#include <cli.h>

static int do_assert(int argc, char *argv[])
{
	assert(0);
	return 0;
}
CMD(assert, do_assert, "assert", "assert");
#endif

#ifdef CONFIG_NV_LOG

char __logbuf[1024] __attribute__((section(".retent")));
unsigned __loghead  __attribute__((section(".retent"))) = 0;

static void logputc_func(int c, void *arg)
{
	log_putc(c);
}

void log_printf(const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	kvprintf(format, logputc_func, NULL, 10, ap);
	va_end(ap);
}

#define LOGBUFSIZ sizeof(__logbuf)
#define LOGBUFIDX(i) ((i) & (LOGBUFSIZ - 1))

static int do_log(int argc, char *argv[])
{
	unsigned i = 0;

	if (argc == 2 && strcmp(argv[1], "reset") == 0) {
		memset(__logbuf, 0, sizeof(__logbuf));
		__loghead = 0;
		return 0;
	}

	if (__loghead > LOGBUFSIZ)
		/* overflow */
		i = __loghead - LOGBUFSIZ;

	for (; i < __loghead; i++) {
		putchar(__logbuf[LOGBUFIDX(i)]);
	}

	return 0;
}
CMD(log, do_log, "print log", "log [clear]");
#endif /* CONFIG_NV_LOG */

#endif /* CONFIG_BOOTROM */
