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

/*
 * XTX-specific instructions
 */
/* TBD */

static struct erase_method xtx_erase_4_32_64[4] = {
	[0] = { .size =  4*KiB, .opcode = 0x20, },
	[1] = { .size = 32*KiB, .opcode = 0x52, },
	[2] = { .size = 64*KiB, .opcode = 0xd8, },
};

static struct spi_flash_info xtx2xxxx[] = {
	{
		.name = "xt25f08b",
		.id = ID(0x0b4014),
		.method = xtx_erase_4_32_64,
		.region[0] = { .size = 1024*KiB, .method = 0x7 },
		.page_size = 256,
	},
};

static inline int xtx_num_region(struct spi_flash_info *info)
{
	int i;

	for (i = 0; info->region[i].size; i++);

	return i;
}

static int xtx_unlock(struct spi_flash *flash)
{
	u8 status;

	status = read_status(flash);
	spi_flash_write_enable(flash);
	write_status(flash, status & ~(SR_BP0|SR_BP1|SR_BP2|SR_TB));

	printk("SF: %s (status %02x -> %02x)\n", __func__, status,
	       read_status(flash));

	return 0;
}

static int xtx_lock(struct spi_flash *flash)
{
	u8 status = read_status(flash);

	spi_flash_write_enable(flash);
	write_status(flash, status | (SR_BP0|SR_BP1|SR_BP2|SR_TB));
	printk("SF: %s (status %02x -> %02x)\n", __func__, status,
	       read_status(flash));

	return 0;
}

static int xtx_probe(struct spi_flash *flash)
{
	struct spi_flash_info *info;
	off_t start = 0;
	int i;

	if (flash->size && flash->num_region)
		goto fixup; /* handled by SFDP */

	for (i = 0; i < ARRAY_SIZE(xtx2xxxx); i++) {
		info = &xtx2xxxx[i];
		if (!memcmp(info->id, flash->id, 3))
			goto found;
	}

	return -EINVAL;
 found:
	/* not handled by SFDP, so need a legacy way */
	flash->size = 0;
	flash->page_size = info->page_size;
	flash->pp_timeout = 100 * 1000; /* 100 ms */
	flash->name = info->name;

	for (i = 0; info->method[i].size != 0; i++) {
		flash->erase_info[i] = info->method[i];
		flash->erase_info[i].timeout = 1000 * 1000; /* 1000 ms */
	}

	flash->num_region = xtx_num_region(info);
	flash->region = info->region;
	for (i = 0; i < flash->num_region; i++) {
		struct erase_region *r = &flash->region[i];
		r->start = start;
		start += r->size;
		flash->size += r->size;
	}

 fixup:
	flash->unlock = xtx_unlock;
	flash->lock = xtx_lock;

	return 0;
}

SPI_FLASH_DRIVER(xtx) = {
	.vid = { 0x0b, },
	.probe = xtx_probe,
};
