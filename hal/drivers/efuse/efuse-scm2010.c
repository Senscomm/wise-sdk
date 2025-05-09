/*
 * Copyright 2021-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include <string.h>
#include <stdio.h>

#include <hal/kernel.h>
#include <hal/device.h>
#include <hal/console.h>
#include <hal/efuse.h>

#include <bitfield.h>

#include "vfs.h"

#ifdef CONFIG_EFUSE_BUFFER_MODE
#include "hal/spi-flash.h"
#endif

#define efuse_addr(device, oft) (u32 *)((device)->base[0] + oft)

#define uswap_32(x) \
	((((x) & 0xff000000) >> 24) | \
	 (((x) & 0x00ff0000) >>  8) | \
	 (((x) & 0x0000ff00) <<  8) | \
	 (((x) & 0x000000ff) << 24))

#define OFT_EFUSE_REGCMD				0x00
#define OFT_EFUSE_REGCFG				0x04
#define OFT_EFUSE_POWCFG				0x08
#define OFT_EFUSE_REDCFG				0x0c
#define OFT_EFUSE_REDURD				0x10
#define OFT_EFUSE_DATARD				0x14
#define OFT_EFUSE_STATRD				0x18
#define OFT_EFUSE_CMDRW				    0x80

#define CMD_READ						0x01
#define CMD_WRITE						0x02
#define CMD_IDLE						0x04

#define SCM2010_EFUSE_ROW_MAX			32

/*
#define DEBUG
*/

#if DEBUG
#define debug(arg ...) printf(arg)
#define error(arg ...) printf(arg)
#else
#define debug(arg ...)
#define error(arg ...)
#endif

struct efuse_driver_data {
	struct fops devfs_ops;
	struct device *dev;
#ifdef CONFIG_EFUSE_BUFFER_MODE
	u8 mode;
#endif
} efuse_data;

#ifdef CONFIG_EFUSE_BUFFER_MODE

#define FLASH_BUFFER_ADDR			(CONFIG_EFUSE_FLASH_BUFFER_ADDR)
#define FLASH_BUFFER_SIZE			((CONFIG_EFUSE_NUM_ROW + 2) * 4)
#define FLASH_BUFFER_MAGIC_OFFSET	(CONFIG_EFUSE_NUM_ROW + 0)
#define FLASH_BUFFER_MODE_OFFSET	(CONFIG_EFUSE_NUM_ROW + 1)
#define FLASH_INVALID_VALUE			0xFFFFFFFF
#define FLASH_MAGIC_VALUE			0xCAFEBABE

static u32 ram_buf[CONFIG_EFUSE_NUM_ROW];
static u32 flash_buf[CONFIG_EFUSE_NUM_ROW + 2];

#ifdef CONFIG_DCACHE_ENABLE

#define align_cache(addr)      ((addr) & ~(CONFIG_CACHE_LINE - 1))

static void invalidate(u32 va, size_t size)
{
	u32 addr = align_cache(va);
	u32 end = align_cache(va + size - 1);

	while (addr <= end) {
	  /* Write VA into mcctlbeginaddr CSR. */

	  __nds__write_csr(addr, NDS_MCCTLBEGINADDR);

	  /* Write L1D_VA_INVAL (0b00_0000) to mcctlcommand CSR
	   * to trigger CCTL operation for invalidation.
	   */

	  __nds__write_csr(0x0, NDS_MCCTLCOMMAND);

	  addr += CONFIG_CACHE_LINE;
	}
}

#endif /* CONFIG_DCACHE_ENABLE */

#endif

static u32 efuse_readl(struct device *dev, u32 offset)
{
	u32 *addr, v;

	addr = efuse_addr(dev, offset);
	v = readl(addr);

	return v;
}

static void efuse_writel(u32 val, struct device *dev, u32 offset)
{
	writel(val, efuse_addr(dev, offset));
}

static void efuse_read_raw(struct device *dev, int row, u32 *val)
{
	u32 v;

	/* start read */
	efuse_writel(CMD_READ, dev, OFT_EFUSE_REGCMD);
	/* row : 0 - 31 */
	efuse_writel(row & 0x1f, dev, OFT_EFUSE_CMDRW);

	/* How to know Q[31-0] is valid at this moment? */
	v = efuse_readl(dev, OFT_EFUSE_DATARD);

	/* idle */
	efuse_writel(CMD_IDLE, dev, OFT_EFUSE_REGCMD);

	*val = v;
}

static void efuse_write_raw(struct device *dev, int row, u32 val)
{
    int i;
    u32 v;
    u32 old;

	efuse_read_raw(dev, row, &old);

	/* enable power supply */
	efuse_writel(0x2, dev, OFT_EFUSE_POWCFG);
	/* start write */
	efuse_writel(CMD_WRITE, dev, OFT_EFUSE_REGCMD);

	for (i = 0, v = val; i < 32; i++, v = v >> 1) {
		if ((v & 0x1) && !bf_get(old, i, i)) {
			/* bit[i] : 0 -> 1 */
			/* addr[11: 0] = addr[11:10] | addr[9:5] | addr[4:0] where
			 * addr[11:10] is zero,
			 * addr[ 9: 5] is equal to the bit number, 0...31, and
			 * addr[ 4: 0] is equal to the row address, 0...31.
			 */
			efuse_writel((i << 5) | row , dev, OFT_EFUSE_CMDRW);

			/* How to check if it is ready for the next address? */
		}
	}

	/* idle */
	efuse_writel(CMD_IDLE, dev, OFT_EFUSE_REGCMD);
	/* disable power supply */
	efuse_writel(0x0, dev, OFT_EFUSE_POWCFG);
}

#ifdef CONFIG_EFUSE_BUFFER_MODE

static void efuse_flash_clear(void)
{
	memset(flash_buf, 0, FLASH_BUFFER_SIZE);
}

static void efuse_flash_read(void)
{
	flash_read(FLASH_BUFFER_ADDR, flash_buf, FLASH_BUFFER_SIZE);
}

static void efuse_flash_write(void)
{
	flash_erase(FLASH_BUFFER_ADDR, FLASH_BUFFER_SIZE,
			(EB_MIN_BLOCK | EB_SUPERSET | EB_UNALIGNED));
	flash_write(FLASH_BUFFER_ADDR, flash_buf, FLASH_BUFFER_SIZE);
}

static u8 efuse_flash_validate(struct device *dev)
{
	/* prepare the flash to be used as efuse buffer, if not */
	int i;

	efuse_flash_read();

	for (i = 0; i < CONFIG_EFUSE_NUM_ROW; i++) {
		if (flash_buf[i] != FLASH_INVALID_VALUE) {
			if (flash_buf[FLASH_BUFFER_MAGIC_OFFSET] == FLASH_MAGIC_VALUE) {
				/* flash buffer is valid */
				return flash_buf[FLASH_BUFFER_MODE_OFFSET];
			} else {
				break;
			}
		}
	}

	efuse_flash_clear();
	flash_buf[FLASH_BUFFER_MAGIC_OFFSET] = FLASH_MAGIC_VALUE;
	flash_buf[FLASH_BUFFER_MODE_OFFSET] = EFUSE_MODE_RAW;
	efuse_flash_write();

	return EFUSE_MODE_RAW;
}

static int scm2010_efuse_set_mode(struct device *dev, u8 mode)
{
	struct efuse_driver_data *priv = dev->driver_data;

	if (mode > EFUSE_MODE_FLASH_BUFFER) {
		return -EINVAL;
	}

	priv->mode = mode;

	efuse_flash_read();
	flash_buf[FLASH_BUFFER_MODE_OFFSET] = mode;
	efuse_flash_write();

	return 0;
}

static int scm2010_efuse_get_mode(struct device *dev, u8 *mode)
{
	struct efuse_driver_data *priv = dev->driver_data;

	*mode = priv->mode;

	return 0;
}

static int scm2010_efuse_sync(struct device *dev)
{
	struct efuse_driver_data *priv = dev->driver_data;
	int i;
	u32 val = 0;
	u32 *pv;

	if (priv->mode == EFUSE_MODE_RAW ||
		priv->mode > EFUSE_MODE_FLASH_BUFFER) {
		return -EIO;
	}

	for (i = 0; i < CONFIG_EFUSE_NUM_ROW; i++) {
		if (priv->mode == EFUSE_MODE_RAM_BUFFER) {
			val = ram_buf[i];
		} else if (priv->mode == EFUSE_MODE_FLASH_BUFFER) {
			pv = (u32 *)(FLASH_BUFFER_ADDR + (i * 4));
#ifdef CONFIG_DCACHE_ENABLE
			invalidate((u32)pv, sizeof(*pv));
#endif
			val = *pv;
		}

		efuse_write_raw(dev, i, val);
	}

	return 0;
}

static int scm2010_efuse_load(struct device *dev)
{
	struct efuse_driver_data *priv = dev->driver_data;
	u32 val;
	int i;

	if (priv->mode == EFUSE_MODE_RAW ||
		priv->mode > EFUSE_MODE_FLASH_BUFFER) {
		return -EIO;
	}

	for (i = 0; i < CONFIG_EFUSE_NUM_ROW; i++) {
		efuse_read_raw(dev, i, &val);
		if (priv->mode == EFUSE_MODE_RAM_BUFFER) {
			ram_buf[i] = val;
		} else if (priv->mode == EFUSE_MODE_FLASH_BUFFER) {
			flash_buf[i] = val;
		}
	}

	if (priv->mode == EFUSE_MODE_FLASH_BUFFER) {
		flash_buf[FLASH_BUFFER_MAGIC_OFFSET] = FLASH_MAGIC_VALUE;
		flash_buf[FLASH_BUFFER_MODE_OFFSET] = priv->mode;
		efuse_flash_write();
	}

	return 0;
}
#endif

static int scm2010_efuse_read(struct device *dev, int row, u32 *val)
{
#ifdef CONFIG_EFUSE_BUFFER_MODE
	struct efuse_driver_data *priv = dev->driver_data;
	u32 *pv;
#endif

	if (row >= SCM2010_EFUSE_ROW_MAX) {
		return -EINVAL;
	}

#ifdef CONFIG_EFUSE_BUFFER_MODE
	if (priv->mode == EFUSE_MODE_RAW) {
		efuse_read_raw(dev, row, val);
	} else if (priv->mode == EFUSE_MODE_RAM_BUFFER) {
		*val = ram_buf[row];
	} else if (priv->mode == EFUSE_MODE_FLASH_BUFFER) {
		pv = (u32 *)(FLASH_BUFFER_ADDR + (row * 4));
#ifdef CONFIG_DCACHE_ENABLE
		invalidate((u32)pv, sizeof(*pv));
#endif
		*val = *pv;
	} else {
		return -EIO;
	}
#else
	efuse_read_raw(dev, row, val);
#endif

	return 0;
}

static int scm2010_efuse_write(struct device *dev, int row, u32 val)
{
#ifdef CONFIG_EFUSE_BUFFER_MODE
	struct efuse_driver_data *priv = dev->driver_data;
#endif

	if (row >= SCM2010_EFUSE_ROW_MAX) {
		return -EINVAL;
	}

#ifdef CONFIG_EFUSE_BUFFER_MODE
	if (priv->mode == EFUSE_MODE_RAW) {
		efuse_write_raw(dev, row, val);
	} else if (priv->mode == EFUSE_MODE_RAM_BUFFER) {
		ram_buf[row] = val;
	} else if (priv->mode == EFUSE_MODE_FLASH_BUFFER) {
		efuse_flash_read();
		flash_buf[row] = val;
		efuse_flash_write();
	} else {
		return -EIO;
	}
#else
	efuse_write_raw(dev, row, val);
#endif

    return 0;
}

/* devfs callbacks */

static int scm2010_efuse_ioctl(struct file *file, unsigned int cmd, void *arg)
{
	struct efuse_driver_data *priv = file->f_priv;
	struct device *dev = priv->dev;
	int ret = 0;
	struct efuse_rw_data *rw_data;
#ifdef CONFIG_EFUSE_BUFFER_MODE
	u8 mode;
#endif

	switch (cmd) {
	case IOCTL_EFUSE_READ_ROW:
		rw_data = arg;
		ret = efuse_get_ops(dev)->read_row(dev, rw_data->row, rw_data->val);
		break;
	case IOCTL_EFUSE_WRITE_ROW:
		rw_data = arg;
		ret = efuse_get_ops(dev)->write_row(dev, rw_data->row, *rw_data->val);
		break;
#ifdef CONFIG_EFUSE_BUFFER_MODE
	case IOCTL_EFUSE_SET_MODE:
		mode = *(u8 *)arg;
		ret = efuse_get_ops(dev)->set_mode(dev, mode);
		break;
	case IOCTL_EFUSE_GET_MODE:
		ret = efuse_get_ops(dev)->get_mode(dev, &mode);
		*(u32 *)arg = mode;
		break;
	case IOCTL_EFUSE_SYNC:
		ret = efuse_get_ops(dev)->sync(dev);
		break;
	case IOCTL_EFUSE_LOAD:
		ret = efuse_get_ops(dev)->load(dev);
		break;
#endif
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int scm2010_efuse_probe(struct device *dev)
{
	struct efuse_driver_data *priv;
	char buf[32];
	struct file *file;
    u32 v;

	priv = &efuse_data;

	/* Set default timing parameters. */
	v = efuse_readl(dev, OFT_EFUSE_REGCFG);
	v &= ~0x3fffffff;
	v |= 0x780c86;
	efuse_writel(v, dev, OFT_EFUSE_REGCFG);

	priv->dev = dev;
#ifdef CONFIG_EFUSE_BUFFER_MODE
	priv->mode = efuse_flash_validate(dev);
#endif
	dev->driver_data = priv;

	sprintf(buf, "/dev/efuse");

	priv->devfs_ops.ioctl = scm2010_efuse_ioctl;

	file = vfs_register_device_file(buf, &priv->devfs_ops, priv);
	if (!file) {
		printk("%s: failed to register as %s\n", dev_name(dev), buf);
		return -1;
	}

	printk("EFUSE: %s registered as %s\n", dev_name(dev), buf);

	return 0;
}

struct efuse_ops scm2010_efuse_ops = {
    .read_row  = scm2010_efuse_read,
    .write_row = scm2010_efuse_write,
#ifdef CONFIG_EFUSE_BUFFER_MODE
	.set_mode = scm2010_efuse_set_mode,
	.get_mode = scm2010_efuse_get_mode,
	.sync = scm2010_efuse_sync,
	.load = scm2010_efuse_load,
#endif
};

static declare_driver(efuse) = {
    .name  = "efuse-scm2010",
    .probe = scm2010_efuse_probe,
    .ops   = &scm2010_efuse_ops,
};

#ifdef CONFIG_CMD_EFUSE
/**
 * EFUSE CLI commands
 */
#include <cli.h>

#include <stdio.h>
#include <stdlib.h>

static void print_u32(int row, void *addr)
{
	u8 *byte = (u8 *)addr;

	printf("[%02d] 0x%08x, %02x.%02x.%02x.%02x\n", row, *(u32 *)addr,
			*byte, *(byte + 1), *(byte + 2), *(byte + 3));
}

static int do_efuse_read(int argc, char *argv[])
{
	struct device *dev = device_get_by_name("efuse-scm2010");
	int row;
	u32 val;

	if (argc < 2)
		return CMD_RET_USAGE;

	row = (int)strtoul(argv[1], NULL, 10);

	efuse_read(dev, row, &val);

	print_u32(row, &val);

	return CMD_RET_SUCCESS;
}

static int do_efuse_write(int argc, char *argv[])
{
	struct device *dev = device_get_by_name("efuse-scm2010");
	int row;
	u32 val;

	if (argc < 3)
		return CMD_RET_USAGE;

	row = (int)strtoul(argv[1], NULL, 10);
	val = (int)strtoul(argv[2], NULL, 16);

	efuse_write(dev, row, val);

	return CMD_RET_SUCCESS;
}

static int do_efuse_dump(int argc, char *argv[])
{
	struct device *dev = device_get_by_name("efuse-scm2010");
	int row;
	u32 val;

	for (row = 0; row < CONFIG_EFUSE_NUM_ROW; row++) {
		efuse_read(dev, row, &val);
		print_u32(row, &val);
	}

	return CMD_RET_SUCCESS;
}

#ifdef CONFIG_EFUSE_BUFFER_MODE
static int do_efuse_set_mode(int argc, char *argv[])
{
	struct device *dev = device_get_by_name("efuse-scm2010");
	u8 mode;

	if (argc < 2)
		return CMD_RET_USAGE;

	mode = atoi(argv[1]);

	efuse_set_mode(dev, mode);

	return CMD_RET_SUCCESS;
}

static int do_efuse_sync(int argc, char *argv[])
{
	struct device *dev = device_get_by_name("efuse-scm2010");

	scm2010_efuse_sync(dev);

	return CMD_RET_SUCCESS;
}

static int do_efuse_load(int argc, char *argv[])
{
	struct device *dev = device_get_by_name("efuse-scm2010");

	scm2010_efuse_load(dev);

	return CMD_RET_SUCCESS;
}
#endif

static const struct cli_cmd efuse_cmd[] = {
	CMDENTRY(read,  do_efuse_read, "", ""),
	CMDENTRY(write,	do_efuse_write, "", ""),
	CMDENTRY(dump,  do_efuse_dump, "", ""),
#ifdef CONFIG_EFUSE_BUFFER_MODE
	CMDENTRY(mode,  do_efuse_set_mode, "", ""),
	CMDENTRY(sync,	do_efuse_sync, "", ""),
	CMDENTRY(load,	do_efuse_load, "", ""),
#endif
};

static int do_efuse(int argc, char *argv[])
{
	const struct cli_cmd *cmd;

	argc--;
	argv++;

	cmd = cli_find_cmd(argv[0], efuse_cmd, ARRAY_SIZE(efuse_cmd));
	if (cmd == NULL)
		return CMD_RET_USAGE;

	return cmd->handler(argc, argv);
}

CMD(efused, do_efuse,
	"test routines for eFuse",
	"efused dump" OR
	"efused read <row>" OR
	"efused write <row> <val>"
#ifdef CONFIG_EFUSE_BUFFER_MODE
	OR
	"efused mode" OR
    "efused sync" OR
    "efused load"
#endif
);

#endif
