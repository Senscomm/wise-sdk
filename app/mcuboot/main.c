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

#include <stdio.h>

#include "hal/types.h"
#include "hal/device.h"
#include "hal/spi-flash.h"
#include "cmsis_os.h"

#define IMAGE_MAGIC_ADDR	CONFIG_SCM2010_OTA_SECONDARY_SLOT_OFFSET
#define IMAGE_MAGIC_SIZE	4
#define IMAGE_MAGIC_VALUE	0x6d666774

#define TIMEOUT_IN_SEC	(CONFIG_BOOT_TIMEOUT)

__weak void mcuboot_main(void)
{
    while(1);
}

void boot_mfg(void)
{
	unsigned long flags;
	void (*entry)(void);

	uint8_t magic[IMAGE_MAGIC_SIZE];

	/* verify the mfg magic */
	flash_read(IMAGE_MAGIC_ADDR, (void *)magic, IMAGE_MAGIC_SIZE);

	if (*(uint32_t *)magic != IMAGE_MAGIC_VALUE) {
		return;
	}

	local_irq_save(flags);

	driver_deinit();

	entry = (void (*)(void))(CONFIG_SCM2010_OTA_SECONDARY_SLOT_OFFSET + 0x20);
	(*entry)();

	/* will never be reached */
	local_irq_restore(flags);
}

int main(void)
{
	int timeout = TIMEOUT_IN_SEC;

	/* try booting mfg if it is found */
	boot_mfg();

	while (timeout){
		printf("\n\x1b[2J\x1b[1;1HPress any key...%d\x1b[0m\n", timeout);
		if(getchar_timeout(1000) > 0)
			return 0;
		timeout--;
	}

	mcuboot_main();

	return 0;
}
