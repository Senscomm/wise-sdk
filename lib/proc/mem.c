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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>

#include <hal/types.h>
#include <FreeRTOS/FreeRTOS.h>

#include <cli.h>

#if CONFIG_CMD_MEM

bool is_valid_addr(void *ptr, bool writeable) __attribute__((weak));
bool is_valid_addr(void *ptr, bool writeable)
{
	return true;
}

/* FIXME: ctype later if necessary */
static int _isprint(unsigned char c)
{
	if (c < 32)
		return 0;

	if (c >= 127)
		return 0;

	return 1;

}


int hexdump(void *buffer, size_t sz)
{
	unsigned char *start, *end, *buf = (unsigned char *)buffer;
	char *ptr, line[128];
	int cursor;

	start = (unsigned char *)(((portPOINTER_SIZE_TYPE) buf) & ~15);
	end = (unsigned char *)((((portPOINTER_SIZE_TYPE) buf) + sz + 15) & ~15);

	while (start < end) {

		if (!is_valid_addr(start, false)) {
			printf("invalid address:0x%x\n", start);
			return -1;
		}

		ptr = line;
		ptr += sprintf(ptr, "0x%08lx: ", (unsigned long) start);

		for (cursor = 0; cursor < 16; cursor++) {
			if ((start + cursor < buf) || (start + cursor >= buf + sz))
				ptr += sprintf(ptr, "..");
			else
				ptr += sprintf(ptr, "%02x", *(start + cursor));

			if ((cursor & 1))
				*ptr++ = ' ';
			if ((cursor & 3) == 3)
				*ptr++ = ' ';
		}
		ptr += sprintf(ptr, "  ");

		/* ascii */
		for (cursor = 0; cursor < 16; cursor++) {
			if ((start + cursor < buf) || (start + cursor >= buf + sz))
				ptr += sprintf(ptr, ".");
			else
				ptr += sprintf(ptr, "%c", _isprint(start[cursor]) ? start[cursor] : ' ');
		}
		ptr += sprintf(ptr, "\n");
		printf("%s", line);
		start += 16;
	}

	return 0;
}

static void __hexdump(unsigned long start, unsigned long end,
		      unsigned long p, size_t sz, const unsigned char *c)
{
	while (start < end) {
		unsigned int pos = 0;
		char buf[64];
		int nl = 0;

		pos += sprintf(buf + pos, "%08lx: ", start);

		do {
			if ((start < p) || (start >= (p + sz)))
				pos += sprintf(buf + pos, "..");
			else
				pos += sprintf(buf + pos, "%02x", *(c++));

			if (!(++start & 15)) {
				buf[pos++] = '\n';
				nl = 1;
			} else {
				nl = 0;
				if (!(start & 1))
					buf[pos++] = ' ';
				if (!(start & 3))
					buf[pos++] = ' ';
			}

		} while (start & 15);

		if (!nl)
			buf[pos++] = '\n';

		buf[pos] = '\0';
		printf("%s", buf);
	}
}

void hexdump1(const void *ptr, size_t sz)
{
	unsigned long p = (unsigned long)ptr;
	unsigned long start = p & ~(unsigned long)15;
	unsigned long end = (p + sz + 15) & ~(unsigned long)15;
	const unsigned char *c = ptr;

	__hexdump(start, end, p, sz, c);
}

int do_hexdump(int argc, char *argv[])
{
	unsigned long start, size;

	if (argc < 2)
		return CMD_RET_USAGE;

	start = strtoul(argv[1], NULL, 16);
	if (argc < 3)
		size = 32;
	else
		size = strtoul(argv[2], NULL, 0);


	if (hexdump((void *) start, size) < 0)
		return CMD_RET_FAILURE;

	return CMD_RET_SUCCESS;
}

CMD(hexdump, do_hexdump,
	"hexdump address size",
	"hexadecimal dump"
);

#define WRITEM(t, a, v)						\
				do {				\
					*(t *)a = (t)v;		\
				} while (0);

static void writem(const void *ptr, unsigned long val, size_t sz)
{
	switch (sz) {
	case 4:
		WRITEM(uint32_t, ptr, val);
		break;
	case 2:
		WRITEM(uint16_t, ptr, val);
		break;
	case 1:
		WRITEM(uint8_t, ptr, val);
		break;
	default:
		printf("[%s, %d] invalid op size!(%d)\n", __func__, __LINE__, (int) sz);
		break;
	}
}

#ifndef U32_MAX
#define U32_MAX ((uint32_t)~0U)
#endif

int do_write(int argc, char *argv[])
{
	unsigned long start, value, size = 4;
	int c = 0;

	opterr = 0;

	while ((c = getopt(argc, argv, "bsl")) != -1) {
		switch (c) {
		case 'b':
			size = 1;
			break;
		case 's':
			size = 2;
			break;
		case 'l':
			size = 4;
			break;
		case '?':
			if (_isprint(optopt))
				printf("Unknown option '-%c'.\n", optopt);
			else
				printf("Unknown option character.\n");
			return CMD_RET_USAGE;
		}
	}
	if ((argc - optind) < 2)
		return CMD_RET_USAGE;

	start = strtoul(argv[optind], NULL, 16);
	value = strtoul(argv[optind + 1], NULL, 16);

	if (!is_valid_addr((void *)start, true) || (start > U32_MAX - size)) {
		printf("invalid address:0x%x\n", start);
		return CMD_RET_FAILURE;
	}

	writem((void *) start, value, size);

	optind = 1;

	return CMD_RET_SUCCESS;
}

CMD(write, do_write,
	"write -(b|s|l) address value",
	"write (1/2/4) bytes"
);

#define READM(t, a, v)						\
				do {				\
					*v = *(t *)a;		\
				} while (0);

static unsigned long readm(const void *ptr, size_t sz)
{
	unsigned long val = 0ul;
	switch (sz) {
	case 4:
		READM(uint32_t, ptr, (&val));
		break;
	case 2:
		READM(uint16_t, ptr, (&val));
		break;
	case 1:
		READM(uint8_t, ptr, (&val));
		break;
	default:
		printf("[%s, %d] invalid op size!(%d)\n", __func__, __LINE__, (int) sz);
		break;
	}

	return val;
}

int do_read(int argc, char *argv[])
{
	unsigned long start, value, len, size = 4;
	int c = 0, i;
	int dump_flag = 0;
	opterr = 0;
	optind = 0;

	while ((c = getopt(argc, argv, "dbsl")) != -1) {
		switch (c) {
		case 'd':
			dump_flag = 1;
			break;
		case 'b':
			size = 1;
			break;
		case 's':
			size = 2;
			break;
		case 'l':
			size = 4;
			break;
		case '?':
			if (_isprint(optopt))
				printf("Unknown option '-%c'.\n", optopt);
			else
				printf("Unknown option character.\n");
			return CMD_RET_USAGE;
		}
	}
	if ((argc - optind) < 1)
		return CMD_RET_USAGE;

	start = strtoul(argv[optind], NULL, 16);
	if ((argc - optind) < 2)
		len = 1;
	else
		len = strtoul(argv[optind+1], NULL, 0);

	for (i = 0; i < len; i++) {
		if (!is_valid_addr((void *)start, false)) {
			printf("invalid address:0x%x\n", start);
			return CMD_RET_FAILURE;
		}

		value = readm((void *)start + size * 0, size);
		if(dump_flag == 1)
			printf("%x, ", (unsigned int)value);
		else
			printf("[0x%x] 0x%08x\n",(void *)start, (unsigned int)value);
		start += size;
	}

	if(dump_flag == 1) printf("\n");
	optind = 1;

	return CMD_RET_SUCCESS;
}

CMD(read, do_read,
	"read -(d|b|s|l) address length",
	"read (d:dump/b:1/s:2/l:4) bytes"
);

int do_memcmp(int argc, char *argv[])
{
	unsigned long addr1, addr2;
	unsigned int i, size;
	char *s1, *s2, *end;

	addr1 = strtoul(argv[1], &end, 16);
	if (*end != '\0')
		return CMD_RET_USAGE;

	addr2 = strtoul(argv[2], &end, 16);
	if (*end != '\0')
		return CMD_RET_USAGE;

	size = strtoul(argv[3], NULL, 0);

	s1 = (char *) addr1;
	s2 = (char *) addr2;
	for (i = 0; i < size; i++) {
		if (!is_valid_addr(&s1[i], false) || !is_valid_addr(&s2[i], false)) {
			printf("invalid address:0x%x, 0x%x\n", &s1[i], &s2[i]);
			return CMD_RET_FAILURE;
		}

		if (s1[i] != s2[i]) {
			printf("mismatch: 0x%x(%02x), 0x%x(%02x)\n",
			       &s1[i], s1[i],
			       &s2[i], s2[i]);
			return CMD_RET_FAILURE;
		}
	}

	return 0;
}

CMD(memcmp, do_memcmp,
    "compare memory",
    "memcmp addr1 addr2 len");

#endif

#ifdef CONFIG_CMD_HEAP
#include <string.h>
#include "cmsis_os.h"

int do_heap(int argc, char *argv[])
{
	argc--, argv++;

	if (!argc) {
		printf("Current:\t%12ld B\n", (long)(configTOTAL_HEAP_SIZE - osKernelGetFreeHeapSize()));
#ifndef CONFIG_PORT_NEWLIB
		printf("Maximum:\t%12ld B\n", (long)(configTOTAL_HEAP_SIZE - osKernelGetMinEverFreeHeapSize()));
#endif
		printf("Free:\t\t%12ld B\n", (long)osKernelGetFreeHeapSize());
		printf("Total:  \t%12ld B\n", (long)configTOTAL_HEAP_SIZE);
	} else if (!strcmp(argv[0], "list")) {
#ifdef CONFIG_USE_MALLOC_DEBUG
extern void vPortMemoryScan( void );
       vPortMemoryScan();
#else
		printf("Enable USE_MALLOC_DEBUG in menuconfig.\n");
#endif
	} else if (!strcmp(argv[0], "check")) {
#ifdef CONFIG_USE_MALLOC_DEBUG
extern void vPortCheckIntegrity( void );
       vPortCheckIntegrity();
#else
		printf("Enable USE_MALLOC_DEBUG in menuconfig.\n");
#endif
	} else
		return CMD_RET_USAGE;

	return CMD_RET_SUCCESS;
}

CMD(heap, do_heap,
	"kernel heap status",
	"heap" OR
	"heap list" OR
    "heap check"
);
#endif
