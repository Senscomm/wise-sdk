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

#include <hal/kernel.h>
#include <hal/device.h>
#include <hal/serial.h>
#include <hal/timer.h>
#include <hal/console.h>
#include <hal/unaligned.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <bug.h>

#define LOG_LINE_MAX 128

struct device *__console;

static LIST_HEAD_DEF(console_list);

static inline int console_write(char *s, int n)
{
	struct console *con;

	list_for_each_entry(con, &console_list, list) {
		con->write(con, s, n);
	}
	return 0;
}

#include <FreeRTOS/stream_buffer.h>

#include <freebsd/compat_param.h>
#include <freebsd/systm.h>

#define MAXNBUF	(sizeof(intmax_t) * NBBY + 1)

struct snprintf_arg {
	char *str;
	size_t remain;
};

/* This is actually used with radix [2..36] */
static char const hex2ascii_data[] = "0123456789abcdefghijklmnopqrstuvwxyz";

static char hex2ascii(int hex)
{
	assert(hex >= 0 && hex < 36);
	return hex2ascii_data[hex];
}

/* XXX don't use toupper() from ctype because it will bring
 * in lots of rodata, especially in wise.ram */
static inline char to_upper(int c)
{
	if (c >= 0x61 && c <= 0x7A)
		c -= 0x20;

	return c;
}

/*
 * Put a NUL-terminated ASCII number (base <= 36) in a buffer in reverse
 * order; return an optional length and a pointer to the last character
 * written in the buffer (i.e., the first character of the string).
 * The buffer pointed to by `nbuf' must have length >= MAXNBUF.
 */
static char *
ksprintn(char *nbuf, uintmax_t num, int base, int *lenp, int upper)
{
	char *p, c;

	p = nbuf;
	*p = '\0';
	do {
		c = hex2ascii(num % base);
		*++p = upper ? to_upper((int)c) : c;
	} while (num /= base);
	if (lenp)
		*lenp = p - nbuf;
	return (p);
}

int kvprintf(char const *fmt, void (*func)(int, void *), void *arg, int radix,
			 va_list ap)
{
#define PCHAR(c) {			\
	int cc = (c);			\
	if (func)			\
		(*func)(cc,arg);	\
	else *d++ = cc;			\
	retval++;			\
}
	char nbuf[MAXNBUF];
	char *d;
	const char *p, *percent, *q = NULL;
	u_char *up;
	int ch, n;
	uintmax_t num;
	int base, lflag, qflag, tmp, width, ladjust, sharpflag, neg, sign, dot;
	int cflag, hflag, jflag, tflag, zflag;
	int bconv, dwidth, upper;
	char padc;
	int stop = 0, retval = 0;

	num = 0;
	if (!func)
		d = (char *) arg;
	else
		d = NULL;

	if (fmt == NULL)
		fmt = "(fmt null)\n";

	if (radix < 2 || radix > 36)
		radix = 10;

	for (;;) {
		padc = ' ';
		width = 0;
		while ((ch = (u_char)*fmt++) != '%' || stop) {
			if (ch == '\0')
				return (retval);
			PCHAR(ch);
		}
		percent = fmt - 1;
		qflag = 0; lflag = 0; ladjust = 0; sharpflag = 0; neg = 0;
		sign = 0; dot = 0; bconv = 0; dwidth = 0; upper = 0;
		cflag = 0; hflag = 0; jflag = 0; tflag = 0; zflag = 0;
reswitch:	switch (ch = (u_char)*fmt++) {
		case '.':
			dot = 1;
			goto reswitch;
		case '#':
			sharpflag = 1;
			goto reswitch;
		case '+':
			sign = 1;
			goto reswitch;
		case '-':
			ladjust = 1;
			goto reswitch;
		case '%':
			PCHAR(ch);
			break;
		case '*':
			if (!dot) {
				width = va_arg(ap, int);
				if (width < 0) {
					ladjust = !ladjust;
					width = -width;
				}
			} else {
				dwidth = va_arg(ap, int);
			}
			goto reswitch;
		case '0':
			if (!dot) {
				padc = '0';
				goto reswitch;
			}
		case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
				for (n = 0;; ++fmt) {
					n = n * 10 + ch - '0';
					ch = *fmt;
					if (ch < '0' || ch > '9')
						break;
				}
			if (dot)
				dwidth = n;
			else
				width = n;
			goto reswitch;
		case 'b':
			ladjust = 1;
			bconv = 1;
			goto handle_nosign;
		case 'c':
			width -= 1;

			if (!ladjust && width > 0)
				while (width--)
					PCHAR(padc);
			PCHAR(va_arg(ap, int));
			if (ladjust && width > 0)
				while (width--)
					PCHAR(padc);
			break;
		case 'D':
			up = va_arg(ap, u_char *);
			p = va_arg(ap, char *);
			if (!width)
				width = 16;
			while(width--) {
				PCHAR(hex2ascii(*up >> 4));
				PCHAR(hex2ascii(*up & 0x0f));
				up++;
				if (width)
					for (q=p;*q;q++)
						PCHAR(*q);
			}
			break;
		case 'd':
		case 'i':
			base = 10;
			sign = 1;
			goto handle_sign;
		case 'h':
			if (hflag) {
				hflag = 0;
				cflag = 1;
			} else
				hflag = 1;
			goto reswitch;
		case 'j':
			jflag = 1;
			goto reswitch;
		case 'l':
			if (lflag) {
				lflag = 0;
				qflag = 1;
			} else
				lflag = 1;
			goto reswitch;
		case 'n':
			if (jflag)
				*(va_arg(ap, intmax_t *)) = retval;
			else if (qflag)
				*(va_arg(ap, int64_t *)) = retval;
			else if (lflag)
				*(va_arg(ap, long *)) = retval;
			else if (zflag)
				*(va_arg(ap, size_t *)) = retval;
			else if (hflag)
				*(va_arg(ap, short *)) = retval;
			else if (cflag)
				*(va_arg(ap, char *)) = retval;
			else
				*(va_arg(ap, int *)) = retval;
			break;
		case 'o':
			base = 8;
			goto handle_nosign;
		case 'p':
			base = 16;
			sharpflag = (width == 0);
			sign = 0;
			num = (uintptr_t)va_arg(ap, void *);
			goto number;
		case 'q':
			qflag = 1;
			goto reswitch;
		case 'r':
			base = radix;
			if (sign)
				goto handle_sign;
			goto handle_nosign;
		case 's':
			p = va_arg(ap, char *);
			if (p == NULL)
				p = "(null)";
			if (!dot)
				n = strlen (p);
			else
				for (n = 0; n < dwidth && p[n]; n++)
					continue;

			width -= n;

			if (!ladjust && width > 0)
				while (width--)
					PCHAR(padc);
			while (n--)
				PCHAR(*p++);
			if (ladjust && width > 0)
				while (width--)
					PCHAR(padc);
			break;
		case 't':
			tflag = 1;
			goto reswitch;
		case 'u':
			base = 10;
			goto handle_nosign;
		case 'X':
			upper = 1;
		case 'x':
			base = 16;
			goto handle_nosign;
		case 'y':
			base = 16;
			sign = 1;
			goto handle_sign;
		case 'z':
			zflag = 1;
			goto reswitch;
handle_nosign:
			sign = 0;
			if (jflag)
				num = va_arg(ap, uintmax_t);
			else if (qflag)
				num = va_arg(ap, uint64_t);
			else if (tflag)
				num = va_arg(ap, ptrdiff_t);
			else if (lflag)
				num = va_arg(ap, u_long);
			else if (zflag)
				num = va_arg(ap, size_t);
			else if (hflag)
				num = (u_short)va_arg(ap, int);
			else if (cflag)
				num = (u_char)va_arg(ap, int);
			else
				num = va_arg(ap, u_int);
			if (bconv) {
				q = va_arg(ap, char *);
				base = *q++;
			}
			goto number;
handle_sign:
			if (jflag)
				num = va_arg(ap, intmax_t);
			else if (qflag)
				num = va_arg(ap, int64_t);
			else if (tflag)
				num = va_arg(ap, ptrdiff_t);
			else if (lflag)
				num = va_arg(ap, long);
			else if (zflag)
				num = va_arg(ap, ssize_t);
			else if (hflag)
				num = (short)va_arg(ap, int);
			else if (cflag)
				num = (char)va_arg(ap, int);
			else
				num = va_arg(ap, int);
number:
			if (sign && (intmax_t)num < 0) {
				neg = 1;
				num = -(intmax_t)num;
			}
			p = ksprintn(nbuf, num, base, &n, upper);
			tmp = 0;
			if (sharpflag && num != 0) {
				if (base == 8)
					tmp++;
				else if (base == 16)
					tmp += 2;
			}
			if (neg)
				tmp++;

			if (!ladjust && padc == '0')
				dwidth = width - tmp;
			width -= tmp + imax(dwidth, n);
			dwidth -= n;
			if (!ladjust)
				while (width-- > 0)
					PCHAR(' ');
			if (neg)
				PCHAR('-');
			if (sharpflag && num != 0) {
				if (base == 8) {
					PCHAR('0');
				} else if (base == 16) {
					PCHAR('0');
					PCHAR('x');
				}
			}
			while (dwidth-- > 0)
				PCHAR('0');

			while (*p)
				PCHAR(*p--);

			if (bconv && num != 0) {
				/* %b conversion flag format. */
				tmp = retval;
				while (*q) {
					n = *q++;
					if (num & (1 << (n - 1))) {
						PCHAR(retval != tmp ?
						    ',' : '<');
						for (; (n = *q) > ' '; ++q)
							PCHAR(n);
					} else
						for (; *q > ' '; ++q)
							continue;
				}
				if (retval != tmp) {
					PCHAR('>');
					width -= retval - tmp;
				}
			}

			if (ladjust)
				while (width-- > 0)
					PCHAR(' ');

			break;
		default:
			while (percent < fmt)
				PCHAR(*percent++);
			/*
			 * Since we ignore a formatting argument it is no
			 * longer safe to obey the remaining formatting
			 * arguments as the arguments will no longer match
			 * the format specs.
			 */
			stop = 1;
			break;
		}
	}
#undef PCHAR
}

static void snprintk_func(int ch, void *arg)
{
	struct snprintf_arg *const info = arg;

	if (info->remain >= 2) {
		*info->str++ = ch;
		info->remain--;
	}
}

int snprintk(char *str, size_t size, const char *format, ...)
{
	int ret;
	va_list ap;
	struct snprintf_arg info = {
		.str = str,
		.remain = size,
	};

	va_start(ap, format);
	ret = kvprintf(format, snprintk_func, &info, 10, ap);
	va_end(ap);
	if (info.remain >= 1)
		*info.str++ = '\0';
	return ret;
}

#ifdef CONFIG_CMD_DMESG

#define PRINTK_BUFSIZ CONFIG_PRINTK_BUF_LEN
#define PRINTK_BUFIDX(i) ((i) & (PRINTK_BUFSIZ - 1))

__kernel__ static char printk_buf[PRINTK_BUFSIZ];
unsigned printk_head = 0;
unsigned printk_tail = 0;

#define printk_ptr printk_buf[PRINTK_BUFIDX(printk_head)]

static void printk_putc(int c, void *arg)
{
	printk_buf[PRINTK_BUFIDX(printk_head)] = c;
	printk_head++;
}



static int kprintf(const char *const fmt, ...)
{
	va_list ap;
	int len;

	va_start(ap, fmt);
	len = kvprintf(fmt, printk_putc, NULL, 10, ap);
	va_end(ap);

	return len;
}

#include <cmsis_os.h>

static osSemaphoreId_t printk_sem;

int _printk(const char *const fmt, ...)
{
	va_list ap;
	unsigned timestamp = 0;
	int len = 0;
	unsigned long flags;

	local_irq_save(flags);
	timestamp = tick_to_us(ktime());
	if (printk_buf[PRINTK_BUFIDX(printk_head-1)] == '\n')
		len += kprintf("[%06u.%06u] ", timestamp/1000000,
			       timestamp%1000000);
	va_start(ap, fmt);
	len += kvprintf(fmt, printk_putc, NULL, 10, ap);
	va_end(ap);
	local_irq_restore(flags);

	if (kernel_in_panic())
		console_flush();
	else
		osSemaphoreRelease(printk_sem);

	return len;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(printk, &printk, &_printk);
#else
__func_tab__ int (*printk)(const char *const fmt, ...) = _printk;
#endif

static bool is_power_of_2(unsigned long n)
{
        return (n != 0 && ((n & (n - 1)) == 0));
}

static const char hex_asc[] = "0123456789abcdef";
#define hex_asc_lo(x)   hex_asc[((x) & 0x0f)]
#define hex_asc_hi(x)   hex_asc[((x) & 0xf0) >> 4]

int hex_dump_to_buffer(const void *buf, size_t len, int rowsize, int groupsize,
		       char *linebuf, size_t linebuflen, bool ascii)
{
	const u8 *ptr = buf;
	int ngroups;
	u8 ch;
	int j, lx = 0;
	int ascii_column;
	int ret;

	if (rowsize != 16 && rowsize != 32)
		rowsize = 16;

	if (len > rowsize)		/* limit to one line at a time */
		len = rowsize;
	if (!is_power_of_2(groupsize) || groupsize > 8)
		groupsize = 1;
	if ((len % groupsize) != 0)	/* no mixed size output */
		groupsize = 1;
	if (groupsize > 4)
		groupsize = 4;

	ngroups = len / groupsize;
	ascii_column = rowsize * 2 + rowsize / groupsize + 1;

	if (!linebuflen)
		goto overflow1;

	if (!len)
		goto nil;

	if (groupsize == 4) {
		const u32 *ptr4 = buf;

		for (j = 0; j < ngroups; j++) {
			ret = snprintf(linebuf + lx, linebuflen - lx,
				       "%s%8.8x", j ? " " : "",
				       get_unaligned_le32(ptr4 + j));
			if (ret >= linebuflen - lx)
				goto overflow1;
			lx += ret;
		}
	} else if (groupsize == 2) {
		const u16 *ptr2 = buf;

		for (j = 0; j < ngroups; j++) {
			ret = snprintf(linebuf + lx, linebuflen - lx,
				       "%s%4.4x", j ? " " : "",
				       get_unaligned_le16(ptr2 + j));
			if (ret >= linebuflen - lx)
				goto overflow1;
			lx += ret;
		}
	} else {
		for (j = 0; j < len; j++) {
			if (linebuflen < lx + 2)
				goto overflow2;
			ch = ptr[j];
			linebuf[lx++] = hex_asc_hi(ch);
			if (linebuflen < lx + 2)
				goto overflow2;
			linebuf[lx++] = hex_asc_lo(ch);
			if (linebuflen < lx + 2)
				goto overflow2;
			linebuf[lx++] = ' ';
		}
		if (j)
			lx--;
	}

nil:
	linebuf[lx] = '\0';
	return lx;
overflow2:
	linebuf[lx++] = '\0';
overflow1:
	return ascii ? ascii_column + len : (groupsize * 2 + 1) * ngroups - 1;
}

void print_hex_dump(const char *prefix_str, int prefix_type,
		    int rowsize, int groupsize,
		    const void *buf, size_t len, bool ascii)
{
	const u8 *ptr = buf;
	int i, linelen, remaining = len;
	unsigned char linebuf[32 * 3 + 2 + 32 + 1];
	const char *level = "";

	if (rowsize != 16 && rowsize != 32)
		rowsize = 16;

	for (i = 0; i < len; i += rowsize) {
		linelen = min(remaining, rowsize);
		remaining -= rowsize;

		hex_dump_to_buffer(ptr + i, linelen, rowsize, groupsize,
				   (char *)linebuf, sizeof(linebuf), ascii);

		switch (prefix_type) {
		case DUMP_PREFIX_ADDRESS:
			printk("%s%s%p: %s\n",
			       level, prefix_str, ptr + i, linebuf);
			break;
		case DUMP_PREFIX_OFFSET:
			printk("%s%s%.8x: %s\n", level, prefix_str, i, linebuf);
			break;
		default:
			printk("%s%s%s\n", level, prefix_str, linebuf);
			break;
		}
	}
}


void console_flush(void)
{
	char c;

	/* Overflow */
	if (printk_head - printk_tail > PRINTK_BUFSIZ)
		printk_tail  = printk_head - PRINTK_BUFSIZ;

	for (; printk_tail < printk_head; printk_tail++) {
		c = printk_buf[PRINTK_BUFIDX(printk_tail)];
		console_write(&c, 1);
	}

	printk_tail = printk_head;
}

void klogd(void *ptr)
{
	TickType_t timeout;

	if (printk_sem == NULL) {
		printk_sem = osSemaphoreNew(1, 0, NULL);
		assert(printk_sem);
	}

	timeout = ptr ? *(TickType_t *) ptr : portMAX_DELAY;
	while (!kernel_in_panic()) {
		osSemaphoreAcquire(printk_sem, timeout);
		if (kernel_in_panic())
			break;
		console_flush();
		if (timeout == 0)
			break;
	}
}
#else
int _printk(const char *const fmt, ...)
{
	return 0;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(printk, &printk, &_printk);
#else
__func_tab__ int (*printk)(const char *const fmt, ...) = _printk;
#endif

#endif

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(register_console, &register_console, &_register_console);
#else
__func_tab__ int (*register_console)(struct console *con) = _register_console;
#endif

int _register_console(struct console *con)
{
	printk("CONSOLE: add %s\n", con->name);

	INIT_LIST_HEAD(&con->list);
	list_add_tail(&con->list, &console_list);
	return 0;
}

static void putc_func(int c, void *arg)
{
	putchar(c);
}

/**
 * db_printf() - private printf() capable of handling %b option
 */
int db_printf(const char *format, ...)
{
	int ret;
	va_list ap;

	va_start(ap, format);
	ret = kvprintf(format, putc_func, NULL, 10, ap);
	va_end(ap);
	return ret;
}
