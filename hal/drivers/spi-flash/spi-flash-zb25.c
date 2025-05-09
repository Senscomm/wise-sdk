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

    status = read_status(flash);
    data[0] = status;
    data[1] = config;
	spi_flash_write_enable(flash);
	ret = spi_flash_cmd_write(flash, &cmd, 1, data, 2);
	if (ret < 0)
		return ret;
	return status;
}

static int zb25_quadio(struct spi_flash *flash, bool enable)
{
    u8 config = read_config(flash);
    bool current = (config & BIT(1)) ? true : false;

    if (enable == current)
        return 0;

    if (enable)
        config |= BIT(1);
    else
        config &= ~BIT(1);

    write_config(flash, config);

    return 0;
}

static int zb25_probe(struct spi_flash *flash)
{
	flash->name = "zb25xxxx";

    flash->quadio = zb25_quadio;

	return 0;
}

SPI_FLASH_DRIVER(zb25) = {
	.vid = { 0x5e, },
	.probe = zb25_probe,
};
