/*
 * Copyright 2018-2024 Senscomm Semiconductor Co., Ltd.
 */
// Copyright 2018 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/*
 * Inspired by ESP8266_RTOS_SDK
 * (https://github.com/espressif/ESP8266_RTOS_SDK)
 * and will provide wise System API as being ESP8266 style
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include <cmsis_os.h>

#include <hal/kernel.h>
#include <hal/device.h>
#include <hal/wdt.h>
#include "sys/ioctl.h"
#include "vfs.h"

#include "wise_err.h"
#include "wise_log.h"
#include "wise_system.h"

/* FIXME: use appropriate error code instead of WISE_FAIL */

/**
  * @brief  Restart CPU.
  *
  * This function does not return.
  */
void wise_restart(void)
{
	int fd;
	int ret;

	fd = open("/dev/watchdog", 0, 0);
	if (fd < 0) {
		printf("Can't open /dev/watchdog: %s\n", strerror(errno));
		return;
	}
	ret = ioctl(fd, IOCTL_WDT_EXPIRE_NOW, NULL);
	if (ret < 0) {
		printf("ioctl error: %s\n", strerror(errno));
		return;
	}

	/* XXX: maybe non-reachable */
	close(fd);
}

/**
  * @brief  Get the size of available heap.
  *
  * Note that the returned value may be larger than the maximum contiguous block
  * which can be allocated.
  *
  * @return Available heap size, in bytes.
  */
uint32_t wise_get_free_heap_size(void)
{
	return (uint32_t)osKernelGetFreeHeapSize();
}

/**
  * @brief Get the minimum heap that has ever been available
  *
  * @return Minimum free heap ever available
  */
uint32_t wise_get_minimum_free_heap_size( void )
{
	return (uint32_t)osKernelGetMinEverFreeHeapSize();
}

#include <cli.h>

#ifdef CONFIG_CMD_REBOOT

static int do_reboot(int argc, char *argv[])
{
	uint32_t delay;

	if (argc < 2) {
		delay = 1;
	} else {
		delay = strtoul(argv[1], NULL, 10);
	}

	do {
		printf("\x1b[1GRebooting in %d seconds...", delay--);
		osDelay(PERIOD_SEC(1));
	} while (delay > 0);

	wise_restart();

	return CMD_RET_SUCCESS;
}

CMD(reboot, do_reboot,
	"reboot <n>",
	"reboot system after n seconds"
);

#endif
