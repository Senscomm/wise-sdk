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

#ifndef __WISE_BUG_H__
#define __WISE_BUG_H__

extern void (*panic)(const char *msg);

int kernel_enter_panic(void);
int kernel_leave_panic(void);
int  kernel_in_panic(void);

extern char __logbuf[1024];
extern unsigned __loghead;

void log_printf(const char *format, ...);

#define log_putc(c) {					  \
	__logbuf[__loghead & (sizeof(__logbuf) - 1)] = c; \
	__loghead++;					  \
}

#define log_puts(s) {				\
	while (*s)				\
		log_putc(*s++);			\
}

void *get_pc(void);

#endif
