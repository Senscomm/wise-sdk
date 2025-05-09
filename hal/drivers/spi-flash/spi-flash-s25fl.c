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

#include <hal/types.h>
#include <hal/spi-flash.h>
#include <hal/console.h>
#include "spi-flash-internal.h"

#include <string.h>
#include <stdlib.h>

static int write_config(struct spi_flash *flash, u8 config)
{
    u8 cmd = SPI_FLASH_OP_WRSR, status;
    u8 data[2];
	int ret;

	spi_flash_write_enable(flash);
    status = read_status(flash);
    data[0] = status;
    data[1] = config;
	ret = spi_flash_cmd_write(flash, &cmd, 1, data, 2);
	if (ret < 0)
		return ret;
	return status;
}

static int s25fl_unlock(struct spi_flash *flash)
{
	u8 status;

	status = read_status(flash);
	spi_flash_write_enable(flash);
	write_status(flash, status & ~(SR_BP0|SR_BP1|SR_BP2));

	printk("SF: %s (status %02x -> %02x)\n", __func__, status,
	       read_status(flash));

	return 0;
}

static int s25fl_lock(struct spi_flash *flash)
{
	u8 status = read_status(flash);

	spi_flash_write_enable(flash);
	write_status(flash, status | (SR_BP0|SR_BP1|SR_BP2));
	printk("SF: %s (status %02x -> %02x)\n", __func__, status,
	       read_status(flash));

	return 0;
}

static int s25fl_quadio(struct spi_flash *flash, bool enable)
{
    u8 config = read_config(flash);
    bool current = (config & CR_QUAD_EN_SPAN) ? true : false;

    if (enable == current)
        return 0;

    if (enable)
        config |= CR_QUAD_EN_SPAN;
    else
        config &= ~CR_QUAD_EN_SPAN;

    write_config(flash, config);

    return 0;
}

static int s25fl_probe(struct spi_flash *flash)
{
	int i;

    /* Should support SFDP */
	assert(flash->size && flash->num_region);

	flash->name = "s25flxxxx";

	flash->unlock = s25fl_unlock;
	flash->lock = s25fl_lock;
    flash->quadio = s25fl_quadio;

	return 0;
}

SPI_FLASH_DRIVER(s25fl) = {
	.vid = { 0x01, },
	.probe = s25fl_probe,
};
