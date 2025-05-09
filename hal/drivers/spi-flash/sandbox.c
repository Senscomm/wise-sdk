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
#include <hal/console.h>
#include <hal/spi-flash.h>

#include "spi-flash-internal.h"

#include <string.h>

extern const char sandbox_sfdp[];

static off_t address(const u8 *cmd)
{
	return (cmd[1] << 16 | cmd[2] << 8 | cmd[3]);
}

static int sandbox_cmd_read(struct device *dev, const u8 *cmd, size_t cmdlen,
			    size_t dummy_len, void *rx, size_t rxlen)
{
	unsigned char *buf = rx;
	off_t offset = address(cmd);

	switch (cmd[0]) {
	case SPI_FLASH_OP_RDSR:
		buf[0] = 0;
		return 0;
	case SPI_FLASH_OP_READ:
		memcpy(buf, dev->base[1] + offset, rxlen);
		return 0;
	case SPI_FLASH_OP_RDID:
		memcpy(buf, "\x12\x34\x56", 3);
		return 0;
	case SPI_FLASH_OP_RDSFDP:
		memcpy(buf, sandbox_sfdp + offset, rxlen);
		return 0;
	default:
		printk("unknown read op %02x\n", cmd[0]);
	}

	return 0;
}

static int sandbox_cmd_write(struct device *dev, const u8 *cmd, size_t cmdlen,
			     void *tx, size_t txlen)
{
	off_t addr = address(cmd);
	unsigned erase_size;

	switch (cmd[0]) {
	case SPI_FLASH_OP_SE:
	case SPI_FLASH_OP_BE:
		if (cmd[0] == SPI_FLASH_OP_SE)
			erase_size = 4 * 1024;
		else if (addr < 0x8000) {
			erase_size = 8 * 1024;
		} else if (addr < 0x10000) {
			erase_size = 32 * 1024;
		} else if (addr < 0xf0000) {
			erase_size = 64 * 1024;
		} else if (addr < 0xf8000) {
			erase_size = 32 * 1024;
		} else if (addr < 0x100000) {
			erase_size = 8 * 1024;
		}
		printk("Erasing %p-%p (%d KiB)\n", dev->base[1] + addr,
		       dev->base[1] + addr + erase_size, erase_size/1024);
		memset(dev->base[1] + addr, 0xff, erase_size);
		break;
	case SPI_FLASH_OP_PP:
		memcpy(dev->base[1] + addr, tx, txlen);
		break;
	case SPI_FLASH_OP_WREN:
	case SPI_FLASH_OP_WRDI:
		break;
	default:
		printk("write cmd %02x unhandled\n", cmd[0]);
		break;
	}
	return 0;
}
#if 0
static const struct spi_flash_info sandbox_flash_info = {
	.name = "sandbox-spi-flash",
	.id = { 0x12, 0x34, 0x56 },
	.page_size = 256,
	.flags = 0,
};
#endif
static struct spi_flash_master_ops sandbox_flash_ops = {
	.cmd_read = sandbox_cmd_read,
	.cmd_write = sandbox_cmd_write,
};

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

static int sandbox_flash_probe(struct device *dev)
{
	struct spi_flash *flash;
	int fd;
	struct stat stat;

	fd = open(SANDBOX_SPI_IMAGE_FILE, O_RDWR);
	if (fd < 0) {
		printk("SF: failed to open serial flash %s\n",
		       SANDBOX_SPI_IMAGE_FILE);
		return -ENODEV;
	}
	fstat(fd, &stat);
	dev->base[1] = mmap((void *)0x40000000, stat.st_size,
			    PROT_READ|PROT_WRITE,
			    MAP_SHARED|MAP_32BIT, fd, 0);

	if (dev->base[1] == NULL)
		return -EINVAL;

	flash = spi_flash_probe(dev);
	if (!flash)
		return -EINVAL;
	spi_flash_add_device(flash);

	return 0;
}

static declare_driver(sandbox_sfc) = {
	.name = "sandbox-flash",
	.probe = sandbox_flash_probe,
	.ops = &sandbox_flash_ops,
};

static declare_device_single(sandbox_sfc, 2) = {
	.name = "sandbox-flash",
};
