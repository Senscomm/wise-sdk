/*
 * Copyright 2011 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include <ayla/utypes.h>
#include <ayla/assert.h>
#include <ayla/snprintf.h>

struct fmt_state {
	unsigned alt_form:1;
	unsigned zero_pad:1;
	unsigned left_adj:1;
	unsigned blank_delim:1;
	unsigned sign_req:1;
	unsigned neg:1;
	char fmt_char;
	unsigned char base;
	unsigned long width;
	unsigned long prec;
	int tot_len;
	int buf_len;
	char *buf;
};

/*
 * Put one character in the vsnprintf output buffer indicated by state.
 * Add to the total length whether or not the character fits.
 */
static void printf_putc(struct fmt_state *state, char c)
{
	int len = state->tot_len++;

	if (len < state->buf_len) {
		state->buf[len] = c;
	}
}

static void printf_putu(struct fmt_state *state, unsigned long long ival)
{
	char nbuf[23];	/* a 64-bit number can be 22 octal digits */
	char *cp = nbuf;
	unsigned long long val = ival;
	int num_width;
	int field_width;
	int zeros;
	int i;

	do {
		i = val % state->base;
		if (i > 9) {
			if (state->fmt_char == 'X') {
				*cp++ = i - 10 + 'A';
			} else {
				*cp++ = i - 10 + 'a';
			}
		} else {
			*cp++ = i + '0';
		}
		val /= state->base;
	} while (val != 0);

	num_width = cp - nbuf;
	field_width = num_width;
	if (field_width < state->prec) {
		zeros = state->prec - field_width;
		field_width = state->prec;
	} else {
		zeros = 0;
	}
	field_width += (state->sign_req | state->neg | state->blank_delim);
	if (state->alt_form && ival != 0) {
		switch (state->fmt_char) {
		case 'x':
		case 'X':
			field_width += 2;
			break;
		case 'o':
			field_width++;
			break;
		}
	}

	if (state->zero_pad && state->width > field_width) {
		zeros += state->width - field_width;
		field_width = state->width;
	} else if (!state->left_adj) {
		while (state->width > field_width) {
			printf_putc(state, ' ');
			field_width++;
		}
	}

	if (state->neg) {
		printf_putc(state, '-');
	} else if (state->sign_req) {
		printf_putc(state, '+');
	} else if (state->blank_delim) {
		printf_putc(state, ' ');
	}

	if (state->alt_form && ival) {
		printf_putc(state, '0');
		if (state->fmt_char == 'x' || state->fmt_char == 'X') {
			printf_putc(state, state->fmt_char);
		}
	}

	while (zeros-- > 0) {
		printf_putc(state, '0');
		field_width++;
	}

	while (cp > nbuf) {
		printf_putc(state, *--cp);
	}

	if (state->left_adj) {
		while (state->width > field_width) {
			printf_putc(state, ' ');
			field_width++;
		}
	}
}

static void printf_putn(struct fmt_state *state, long long val)
{
	if (val < 0) {
		state->neg = 1;
		val = -val;
	}
	printf_putu(state, (unsigned long long)val);
}

static void printf_puts(struct fmt_state *state, char *str)
{
	size_t slen;
	size_t i;

	slen = strlen(str);
	if (!state->left_adj) {
		for (i = slen; state->width > i; i++) {
			printf_putc(state, ' ');
		}
	}
	for (i = 0; i < slen; i++) {
		printf_putc(state, str[i]);
	}
	if (state->left_adj) {
		for (i = slen; state->width > i; i++) {
			printf_putc(state, ' ');
		}
	}
}

#ifndef vsnprintf
int vsnprintf(char *bp, size_t size, const char *fmt, va_list ap)
{
	return libayla_vsnprintf(bp, size, fmt, ap);
}
#endif

int libayla_vsnprintf(char *bp, size_t size, const char *fmt, va_list ap)
{
	struct fmt_state state;
	char cbuf[2];
	unsigned long long uval = 0;
	char *ptr;
	char len_mod;
	long long val;
	size_t len;

	/*
	 * Guard against underflow of length calculations like
	 *	tlen = snprintf(buf, sizeof(buf), ...);
	 *	tlen += snprintf(buf + tlen, sizeof(buf) - tlen, ...);
	 * the second snprintf might use a negative length cast to unsigned.
	 * Treat it as a zero length.
	 * This is non-standard, but seems safe and handy.
	 */
	if (size > MAX_S32) {
		size = 0;
	}
	state.buf_len = size;
	state.tot_len = 0;
	state.buf = bp;

	while (*fmt != '\0') {
		if (*fmt != '%') {
			printf_putc(&state, *fmt++);
			continue;
		}

		state.alt_form = 0;
		state.zero_pad = 0;
		state.blank_delim = 0;
		state.left_adj = 0;
		state.sign_req = 0;
		state.width = 0;
		state.prec = 1;
		state.neg = 0;
		len_mod = 0;
		state.base = 10;

		switch (*++fmt) {
		case '#':
			state.alt_form = 1;
			fmt++;
			break;
		case '-':
			state.left_adj = 1;
			fmt++;
			break;
		case ' ':
			state.blank_delim = 1;
			fmt++;
			break;
		case '+':
			state.sign_req = 1;
			fmt++;
			break;
		case '0':
			state.zero_pad = 1;
			/* fall through */
		case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			state.width = strtoul(fmt, (char **)&fmt, 10);
			if (*fmt == '.') {
				state.prec = strtoul(fmt + 1,
				    (char **)&fmt, 10);
			}
			break;
		}

		/*
		 * Length modifier
		 */
		switch (*fmt) {
		case 'h':
			len_mod = *fmt++;
			if (*fmt == 'h') {
				len_mod = 'H';
				fmt++;
			}
			break;
		case 'l':
			len_mod = *fmt++;
			if (*fmt == 'l') {
				len_mod = 'q';
				fmt++;
			}
			break;
		case 'q':
			len_mod = *fmt++;
			break;
		case 'j':
			len_mod = *fmt++;
			break;
		case 'z':
			len_mod = *fmt++;
			break;
		case 't':
			len_mod = *fmt++;
			break;
		}

		/*
		 * conversion specifier
		 */
		state.fmt_char = *fmt;
		switch (*fmt) {
		case 'd':
		case 'i':
			switch (len_mod) {
			case 0:
			default:
				val = va_arg(ap, int);
				break;
			case 'l':
				val = va_arg(ap, long);
				break;
			case 'q':
				val = va_arg(ap, long long);
				break;
			case 'z':
				val = va_arg(ap, size_t);
				break;
			case 't':
				val = va_arg(ap, ptrdiff_t);
				break;
			}
			printf_putn(&state, val);
			fmt++;
			break;
		case 'o':
			state.base = 8;
			goto put_uint;
		case 'u':
put_uint:
			switch (len_mod) {
			case 0:
			default:
				uval = va_arg(ap, unsigned int);
				break;
			case 'l':
				uval = va_arg(ap, unsigned long);
				break;
			case 'q':
				uval = va_arg(ap, unsigned long long);
				break;
			case 'z':
				uval = va_arg(ap, size_t);
				break;
			case 't':
				uval = va_arg(ap, ptrdiff_t);
				break;
			}
			printf_putu(&state, uval);
			fmt++;
			break;
		case 'x':
		case 'X':
			state.base = 16;
			goto put_uint;
		case 'p':
			state.base = 16;
			uval = (ptrdiff_t)va_arg(ap, void *);
			printf_putu(&state, uval);
			fmt++;
			break;
		case 'c':
			cbuf[0] = va_arg(ap, int);
			cbuf[1] = '\0';
			printf_puts(&state, cbuf);
			fmt++;
			break;
		case 's':
			ptr = va_arg(ap, char *);
			if (ptr == NULL) {
				ptr = "(null)";
			}
			printf_puts(&state, ptr);
			fmt++;
			break;
		case 'n': case 'm': case 'e': case 'E': case 'f':
		case 'F': case 'g': case 'G': case 'a': case 'A':
		case '%':
		default:
			printf_putc(&state, *fmt++);
			break;
		}
	}

	/*
	 * Add NUL termination to buffer.
	 */
	if (size > 0) {
		len = state.tot_len + 1;
		if (len > size) {
			len = size;
		}
		bp[len - 1] = '\0';
	}

	return state.tot_len;
}
