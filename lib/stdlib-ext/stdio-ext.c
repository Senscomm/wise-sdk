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

/*

Copyright (c) 2004,2012 Kustaa Nyholm / SpareTimeLabs
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of Kustaa Nyholm or SpareTimeLabs nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDER BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

/* This is a smaller implementation of printf-family of functions,
 * based on tinyprintf code by Kustaa Nyholm.
 * The formats supported by this implementation are:
 *     'd' 'u' 'c' 's' 'x' 'X' 'p'.
 * Zero padding and field width are also supported.
 * If the library is compiled with 'PRINTF_SUPPORT_LONG' defined then the
 * long specifier is also supported.
 * Otherwise it is ignored, so on 32 bit platforms there is no point to use
 * PRINTF_SUPPORT_LONG because int == long.
 */

#include <hal/kernel.h>
#include <hal/device.h>
#include <hal/console.h>
#include <hal/timer.h>
#include <hal/serial.h>         /* FIXME: ugly */
#include <vfs.h>

#include <FreeRTOS/FreeRTOS.h>
#if 0
#include <FreeRTOS/serial.h>
#endif

#include <freebsd/systm.h>
#include <freebsd/compat_param.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#ifdef CONFIG_USE_SIMPLE_PRINTF

#include <limits.h>

struct MemFile;

typedef struct MemFile MEMFILE;

struct MemFile_methods
{
    size_t (*mwrite)(MEMFILE *f, const char *bp, size_t n);
    size_t (*mread)(MEMFILE *f, char *bp, size_t n);
};

/* Open a memory buffer for writing.
 Note: Does not write null terminator.*/
struct MemFile
{
    const struct MemFile_methods *vmt;
    char *buffer;
    size_t bytes_written;
    size_t size;
};

static size_t memfile_write(MEMFILE *f, const char *bp, size_t n)
{
    size_t i = 0;

    while (n--)
    {
        f->bytes_written++;
        if (f->bytes_written <= f->size)
        {
            *f->buffer++ = *bp++;
            i++;
        }
    }

    return i;
}

const struct MemFile_methods MemFile_methods = {
    &memfile_write,
    NULL
};

static MEMFILE *fmemopen_w(MEMFILE* storage, char *buffer, size_t size)
{
    storage->vmt = &MemFile_methods;
    storage->buffer = buffer;
    storage->bytes_written = 0;
    storage->size = size;
    return storage;
}

struct param {
    unsigned char width; /**< field width */
    char lz;            /**< Leading zeros */
    unsigned char sign:1;        /**<  The sign to display (if any) */
    unsigned char alt:1;         /**<  alternate form */
    unsigned char uc:1;          /**<  Upper case (for base16 only) */
    unsigned char left:1;        /**<  Force text to left (padding on right) */
    unsigned char hh:2;          /**<  Short value */
    char base;  /**<  number base (e.g.: 8, 10, 16) */
    char *bf;           /**<  Buffer to output */
};

static void ui2a(unsigned long long int num, struct param *p)
{
    int n = 0;
    unsigned long long int d = 1;
    char *bf = p->bf;

    if (p->hh == 1) {
        num = (unsigned short int)num;
    } else if (p->hh == 2) {
        num = (unsigned char)num;
    }

    while (num / d >= p->base)
        d *= p->base;
    while (d != 0) {
        unsigned long long  dgt = num / d;
        num %= d;
        d /= p->base;
        if (n || dgt > 0 || d == 0) {
            *bf++ = dgt + (dgt < 10 ? '0' : (p->uc ? 'A' : 'a') - 10);
            ++n;
        }
    }
    *bf = 0;
}

static void i2a(long long int num, struct param *p)
{
    if (num < 0) {
        num = -num;
        p->sign = 1;
    }
    ui2a(num, p);
}

static int a2d(char ch)
{
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    else if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    else if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    else
        return -1;
}

static char a2i(char ch, const char **src, int base, unsigned char *nump)
{
    const char *p = *src;
    int num = 0;
    int digit;
    while ((digit = a2d(ch)) >= 0) {
        if (digit > base)
            break;
        num = num * base + digit;
        ch = *p++;
    }
    *src = p;
    *nump = num;
    return ch;
}

static size_t mfwrite(const void *buf, size_t size, size_t nmemb, MEMFILE *stream)
{
    if (stream->vmt->mwrite == NULL) return 0;
    return stream->vmt->mwrite(stream, (char*)buf, size*nmemb) / size;
}

static int mfputc(int c, MEMFILE *f)
{
	unsigned char ch = c;
	return mfwrite(&ch, 1, 1, f) == 1 ? ch : EOF;
}

static int putf(MEMFILE *putp, char c)
{
    if (mfputc(c, putp) == EOF)
        return 0;
    else
        return 1;
}

static unsigned putchw(MEMFILE *putp, struct param *p)
{
    unsigned written = 0;
    char ch;
    int n = p->width;
    char *bf = p->bf;

    /* Number of filling characters */
    while (*bf++ && n > 0)
        n--;
    if (p->sign)
        n--;
    if (p->alt && p->base == 16)
        n -= 2;
    else if (p->alt && p->base == 8)
        n--;

    /* Unless left-aligned, fill with space, before alternate or sign */
    if (!p->lz && !p->left) {
        while (n-- > 0)
            written += putf(putp, ' ');
    }

    /* print sign */
    if (p->sign)
        written += putf(putp, '-');

    /* Alternate */
    if (p->alt && p->base == 16) {
        written += putf(putp, '0');
        written += putf(putp, (p->uc ? 'X' : 'x'));
    } else if (p->alt && p->base == 8) {
        written += putf(putp, '0');
    }

    /* Fill with zeros, after alternate or sign */
    if (p->lz) {
        while (n-- > 0)
            written += putf(putp, '0');
    }

    /* Put actual buffer */
    bf = p->bf;
    while ((ch = *bf++))
        written += putf(putp, ch);

    /* If left-aligned, pad the end with spaces. */
    if (p->left) {
        while (n-- > 0)
            written += putf(putp, ' ');
    }

    return written;
}

static unsigned long long
intarg(int lng, int sign, va_list *va)
{
    unsigned long long val;

    switch (lng) {
    case 0:
        if (sign) {
            val = va_arg(*va, int);
        } else {
            val = va_arg(*va, unsigned int);
        }
        break;

    case 1:
        if (sign) {
            val = va_arg(*va, long);
        } else {
            val = va_arg(*va, unsigned long);
        }
        break;

    case 2:
    default:
        if (sign) {
            val = va_arg(*va, long long);
        } else {
            val = va_arg(*va, unsigned long long);
        }
        break;
    }

    return val;
}

size_t tfp_format(MEMFILE *putp, const char *fmt, va_list va)
{
    size_t written = 0;
    struct param p;
    char bf[23];
    char ch;
    char lng;
    void *v;
    double d;
    int n;
    int i;

    p.bf = bf;

    while ((ch = *(fmt++))) {
        if (ch != '%') {
            written += putf(putp, ch);
        } else {
            /* Init parameter struct */
            p.lz = 0;
            p.alt = 0;
            p.width = 0;
            p.sign = 0;
            p.left = 0;
            p.uc = 0;
            p.hh = 0;
            lng = 0;

            /* Flags */
            while ((ch = *(fmt++))) {
                switch (ch) {
                case '0':
                    if (!p.left) {
                        p.lz = 1;
                    }
                    continue;
                case '#':
                    p.alt = 1;
                    continue;
                case '-':
                    p.left = 1;
                    p.lz = 0;
                    continue;
                default:
                    break;
                }
                break;
            }

            /* Width */
            if (ch == '*') {
                i = intarg(0, 1, &va);
                if (i > UCHAR_MAX) {
                    p.width = UCHAR_MAX;
                } else if (i > 0) {
                    p.width = i;
                }
                ch = *(fmt++);
            } else if (ch >= '0' && ch <= '9') {
                ch = a2i(ch, &fmt, 10, &(p.width));
            }
            if (ch == 'l') {
                ch = *(fmt++);
                lng = 1;

                if (ch == 'l') {
                    ch = *(fmt++);
                    lng = 2;
                }
            } else if (ch == 'h') {
                ch = *(fmt++);
                p.hh = 1;

                if (ch == 'h') {
                    ch = *(fmt++);
                    p.hh = 2;
                }
            }

            if (ch == 'z') {
                ch = *(fmt++);
            }

            /* precision */
            if (ch == '.') {
                int precision;
                ch = *(fmt++);

                if (ch == '*') {
                    ch = *(fmt++);
                    precision = intarg(0, 1, &va);
                    if (precision < 0) {
                        precision = 0;
                    }
                } else {
                    while (ch >= '0' && ch <= '9') {
                        precision = precision * 10 + (ch - '0');
                        ch = *(fmt++);
                    }
                }
            }

            switch (ch) {
            case 0:
                goto abort;
            case 'u':
                p.base = 10;
                ui2a(intarg(lng, 0, &va), &p);
                written += putchw(putp, &p);
                break;
            case 'd':
            case 'i':
                p.base = 10;
                i2a(intarg(lng, 1, &va), &p);
                written += putchw(putp, &p);
                break;
            case 'x':
            case 'X':
                p.base = 16;
                p.uc = (ch == 'X');
                ui2a(intarg(lng, 0, &va), &p);
                written += putchw(putp, &p);
                break;
            case 'o':
                p.base = 8;
                ui2a(intarg(lng, 0, &va), &p);
                written += putchw(putp, &p);
                break;
            case 'p':
                v = va_arg(va, void *);
                p.base = 16;
                ui2a((uintptr_t)v, &p);
                p.width = 2 * sizeof(void*);
                p.lz = 1;
                written += putf(putp, '0');
                written += putf(putp, 'x');
                written += putchw(putp, &p);
                break;
            case 'c':
                written += putf(putp, (char)(va_arg(va, int)));
                break;
            case 's':
                p.bf = va_arg(va, char *);
                written += putchw(putp, &p);
                p.bf = bf;
                break;
            case 'g':
            case 'f':
                p.base = 10;
                d = va_arg(va, double);
                /* Convert to an int to get the integer part of the number. */
                n = d;
                /* Convert to ascii */
                i2a(n, &p);
                /* When the double was converted to an int it was truncated
                 * towards 0.  If the number is in the range (-1, 0), the
                 * negative sign was lost.  Preserve the sign in this case.
                 */
                if (d < 0.0) {
                    p.sign = 1;
                }
                /* Ignore left align for integer part */
                p.left = 0;
                /* Subtract width for decimal part and decimal point */
                if (p.width >= 4) {
                    p.width -= 4;
                } else {
                    p.width = 0;
                }
                /* Write integer part to console */
                written += putchw(putp, &p);
                /* Take the decimal part and multiply by 1000 */
                n = (d-n)*1000;
                /* Convert to ascii */
                i2a(n, &p);
                /* Set the leading zeros for the next integer output to 3 */
                p.lz = 3;
                /* Always use the same decimal width */
                p.width = 3;
                /* Ignore sign for decimal part*/
                p.sign = 0;
                /* Output a decimal point */
                putf(putp, '.');
                /* Output the decimal part. */
                written += putchw(putp, &p);
                break;
            case '%':
                written += putf(putp, ch);
                break;
            default:
                break;
            }
        }
    }
 abort:;

 return written;
}

int _vsnprintf(char *str, size_t size, const char *fmt, va_list va)
{
    struct MemFile state;
    MEMFILE *f = fmemopen_w(&state, str, size);
    tfp_format(f, fmt, va);
    if (size > 0) {
        if (state.bytes_written < size) {
            *(state.buffer) = '\0';
        } else {
            str[size - 1] = '\0';
        }
    }
    return state.bytes_written;
}

int
patch_std_kvprintf(const char *format, void (*fn)(int c, void *arg),
              void *arg, va_list ap)
{
    char buf[256], *ptr = buf;
    char *nbuf = NULL;
    int size, ret = 0;

    size = _vsnprintf(buf, sizeof(buf), format, ap);
    if (size >= sizeof(buf)) {
        /* Output would be truncated */
        nbuf = malloc(size + 10);
        if (nbuf == NULL)
            goto out;
        _vsnprintf(nbuf, size + 10, format, ap);
        ptr = nbuf;
    }

    for (; *ptr; ptr++, ret++)
        fn(*ptr, arg);

    if (nbuf)
        free(nbuf);
  out:
    return ret;
}

extern int (*std_kvprintf)(const char *format, void(*fn)(int c, void *arg), void *arg, va_list ap);
PATCH(std_kvprintf, &std_kvprintf, &patch_std_kvprintf);

#endif


int os_asprintf(char **ptr, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = vasprintf(ptr, fmt, ap);
	va_end(ap);

	return ret;
}

int os_vasprintf(char **ptr, const char *fmt, va_list ap)
{
	size_t size = 0;
	va_list ap2;
	char *buf;

	va_copy(ap2, ap);

	size = vsnprintf(NULL, 0, fmt, ap2);

	va_end(ap2);

	if (size < 0) {
		return -1;
	}

	buf = (char *)malloc(size + 1);
	if (buf == NULL) {
		return -1;
	}

	buf[size] = 0;

	size = vsprintf(buf, fmt, ap);

	*ptr = buf;

	return size;
}

int usleep (unsigned long  __useconds)
{
    udelay(__useconds);
    return 0;
}
