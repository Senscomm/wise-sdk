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
#include <hal/types.h>
#include <hal/spi-flash.h>
#include <hal/console.h>
#include "spi-flash-internal.h"

#include <string.h>
#include <stdlib.h>

/*#define DEBUG*/

#ifdef DEBUG
#define dbg(arg ...) printk(arg);
#else
static void dbg(const char *fmt, ...)
{
	return;
}
#endif
#define err(arg ...) printk(arg);

/*
 * SST-specific instructions
 */
#define SPI_FLASH_OP_BP		0x02	/* Byte program */
#define SPI_FLASH_OP_AAI_WP	0xad	/* Auto address increment word program */
#define SPI_FLASH_OP_ULBPR	0x98	/* Global block protection unlock (SST26xxx only) */

static struct erase_method sst_erase_4_32_64[4] = {
	[0] = { .size =  4*KiB, .opcode = 0x20, },
	[1] = { .size = 32*KiB, .opcode = 0x52, },
	[2] = { .size = 64*KiB, .opcode = 0xd8, },
};

static struct erase_method sst_erase_4_64[4] = {
	[0] = { .size =  4*KiB, .opcode = 0x20, },
	[1] = { .size = 64*KiB, .opcode = 0xd8, },
};

static struct spi_flash_info sst2xxxx[] = {
	{
		.name = "sst25vf040b",
		.id = ID(0xbf258d),
		.method = sst_erase_4_32_64,
		.region[0] = { .size = 512*KiB, .method = 0x7 },
	},
	{
		.name = "sst25vf080b",
		.id = ID(0xbf258e),
		.method = sst_erase_4_32_64,
		.region[0] = { .size = 1024*KiB, .method = 0x7 },
	},
	{
		.name = "sst25vf016b",
		.id = ID(0xbf2541),
		.method = sst_erase_4_32_64,
		.region[0] = { .size = 2048*KiB, .method = 0x7 },
	},
	{
		.name = "sst25wf040b",
		.id = ID(0x621613),
		.method = sst_erase_4_64,
		.region[0] = { .size = 512*KiB, .method = 0x3 }, /* 4K, 64K */
		.page_size = 256,
	},
};

static inline int sst_num_region(struct spi_flash_info *info)
{
	int i;

	for (i = 0; info->region[i].size; i++);

	return i;
}

static int sst_wb(struct spi_flash *flash, u32 offset, u8 byte)
{
	u8 cmd[4];
	int ret;

	cmd[0] = SPI_FLASH_OP_BP;
	spi_flash_addr(cmd, offset);

	ret = spi_flash_write_sequence(flash, cmd, 4, &byte, 1,
				       flash->pp_timeout);

	return ret;
}

static int sst_aai_wp(struct spi_flash *flash, u32 offset, size_t size,
		      void *buf)
{
	u8 cmd[4], *tx = buf;
	int cmdlen, actual = 0, ret = 0;
	u8 status __maybe_unused;

	status = read_status(flash);

	dbg("SF: ST AAI WP 0x%08x-0x%08x, (%d K)\n",
	    offset, offset + size, size/1024);
	dbg("SF: status=0x%02x\n", read_status(flash));

	spi_flash_write_enable(flash);
	write_status(flash, 0x0);
	dbg("SF: status %02x -> %02x\n", status, read_status(flash));

	if (offset & 1) {
		sst_wb(flash, offset, tx[actual]);
		offset++;
		actual++;
	}

	spi_flash_write_enable(flash);

	cmd[0] = SPI_FLASH_OP_AAI_WP;
	spi_flash_addr(cmd, offset);
	cmdlen = 4;

	while (actual + 2 <= size) {
		if (spi_flash_cmd_write(flash, cmd, cmdlen, buf + actual, 2) ||
		    spi_flash_wait_till_ready(flash, flash->pp_timeout)) {
			err("%s failed at 0x%08x\n", __func__,
			       offset + actual);
			ret = -EIO;
			break;
		}
		cmdlen = 1;
		actual += 2;
	}

	spi_flash_write_disable(flash); /* exit AAI mode */

	if (ret == 0 && actual < size) {
		ret = sst_wb(flash, offset + actual, tx[actual]);
	}

	return (ret == 0) ? actual: ret;
}


static int sst_unlock(struct spi_flash *flash)
{
	u8 status;
	u32 id = JDEC_ID(flash->id);

	if (id == 0xbf2658) {
		u8 cmd = SPI_FLASH_OP_ULBPR;
		return spi_flash_cmd_write(flash, &cmd, 1, NULL, 0);
	}

	status = read_status(flash);
	spi_flash_write_enable(flash);
	write_status(flash, status & ~(SR_BP0|SR_BP1|SR_BP2));

	dbg("SF: %s (status %02x -> %02x)\n", __func__, status,
	       read_status(flash));

	return 0;
}

static int sst_lock(struct spi_flash *flash)
{
	u8 status = read_status(flash);

	spi_flash_write_enable(flash);
	write_status(flash, status | 0x1c);
	dbg("SF: %s (status %02x -> %02x)\n", __func__, status,
	       read_status(flash));

	return 0;
}

static int sst_probe(struct spi_flash *flash)
{
	struct spi_flash_info *info;
	off_t start = 0;
	int i;

	if (flash->size && flash->num_region)
		goto fixup; /* handled by SFDP */

	for (i = 0; i < ARRAY_SIZE(sst2xxxx); i++) {
		info = &sst2xxxx[i];
		if (!memcmp(info->id, flash->id, 3))
			goto found;
	}

	return -EINVAL;
 found:
	/* not handled by SFDP, so need a legacy way */
	flash->size = 0;
	flash->name = info->name;

	for (i = 0; info->method[i].size != 0; i++) {
		flash->erase_info[i] = info->method[i];
		flash->erase_info[i].timeout = 100 * 1000; /* 100 ms */
	}

	flash->num_region = sst_num_region(info);
	flash->region = info->region;
	for (i = 0; i < flash->num_region; i++) {
		struct erase_region *r = &flash->region[i];
		r->start = start;
		start += r->size;
		flash->size += r->size;
	}

	if (flash->pp_timeout == 0)
		flash->pp_timeout = 100 * 1000; /* 100 ms */

	if (flash->page_size == 0)
		flash->write = sst_aai_wp;

 fixup:
	flash->unlock = sst_unlock;
	flash->lock = sst_lock;

	return 0;
}

SPI_FLASH_DRIVER(sst) = {
	.vid = { 0xbf, 0x62, },
	.probe = sst_probe,
};
