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

#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#include <u-boot/list.h>
#include <hal/types.h>

/**
 * struct console: a minimal console structure
 */
struct console {
	const char *name;
	void *private;
	struct list_head list;

	int (*write)(struct console *, char *, int);
};

int _register_console(struct console *);
extern int (*register_console)(struct console *con);

void console_flush(void);
void klogd(void *ptr);

int snprintk(char *str, size_t size, const char *format, ...);
int db_printf(const char *fmt, ...);

extern int (*printk)(const char *const fmt, ...);

enum {
	DUMP_PREFIX_NONE,
	DUMP_PREFIX_ADDRESS,
	DUMP_PREFIX_OFFSET
};

int hex_dump_to_buffer(const void *buf, size_t len, int rowsize, int groupsize,
		       char *linebuf, size_t linebuflen, bool ascii);

void print_hex_dump(const char *prefix_str, int prefix_type,
		    int rowsize, int groupsize,
		    const void *buf, size_t len, bool ascii);

#define print_hex_dump_bytes(prefix_str, prefix_type, buf, len) \
	print_hex_dump(prefix_str, prefix_type, 16, 1, buf, len, false)

#endif
