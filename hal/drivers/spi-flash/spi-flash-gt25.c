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

#define SPI_FLASH_OP_RDSR_L (SPI_FLASH_OP_RDSR)
#define SPI_FLASH_OP_RDSR_H (SPI_FLASH_OP_RDSR | 0x30)
#define SPI_FLASH_OP_WRSR_L (SPI_FLASH_OP_WRSR)
#define SPI_FLASH_OP_WRSR_H (SPI_FLASH_OP_WRSR | 0x30)

static int read_status_h(struct spi_flash *flash)
{
	u8 cmd = SPI_FLASH_OP_RDSR_H, status;
	int ret;

	ret = spi_flash_cmd_read(flash, &cmd, 1, 0, &status, 1);
	if (ret < 0)
		return ret;

	return status;
}

static int write_status_h(struct spi_flash *flash, u8 status)
{
	u8 cmd = SPI_FLASH_OP_WRSR_H;

	spi_flash_write_enable(flash);

	return spi_flash_cmd_write(flash, &cmd, 1, &status, 1);
}

static int write_config(struct spi_flash *flash, u8 config)
{
	u8 cmd = SPI_FLASH_OP_WRSR, status;
	u8 data[2];
	int ret;

	status = read_status(flash);
	data[0] = status;
	data[1] = config;
	spi_flash_write_enable(flash);
	ret = spi_flash_cmd_write(flash, &cmd, 1, data, 2);
	if (ret < 0)
		return ret;
	return status;
}

static int gt25_quadio(struct spi_flash *flash, bool enable)
{
	u8 config = read_config(flash);
	bool current = (config & BIT(1)) ? true : false;
	u8 status;

	status = read_status_h(flash);
	if ((status & BIT(1)) == 0)
	{
		status |= BIT(1);
		write_status_h(flash, status);
	}

	if (enable == current)
		return 0;

	if (enable)
		config |= BIT(1);
	else
		config &= ~BIT(1);

	write_config(flash, config);

	return 0;
}

static int gt25_probe(struct spi_flash *flash)
{
	flash->name = "gt25xxxx";

	flash->quadio = gt25_quadio;

	return 0;
}

SPI_FLASH_DRIVER(gt25) = {
	.vid = {
		0xc4,
	},
	.probe = gt25_probe,
};
