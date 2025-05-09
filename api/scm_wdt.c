/*
 * Copyright 2023-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
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
#include <unistd.h>
#include <fcntl.h>

#include "errno.h"
#include "scm_wdt.h"
#include "sys/ioctl.h"
#include "hal/wdt.h"

#include "wise_err.h"

int scm_wdt_start(uint32_t ms)
{
	int ret;
	int fd;
	uint64_t wdt_timeout;

	fd = open("/dev/watchdog", 0, 0);
	if (fd < 0) {
		printf("file not found\n");
		return WISE_ERR_NOT_FOUND;
	}

	wdt_timeout = ms;
	ret = ioctl(fd, IOCTL_WDT_START, &wdt_timeout);

	close(fd);

	if (ret) {
		printf("wdt start failed\n");
		return WISE_ERR_IOCTL;
	}

	return WISE_OK;
}

int scm_wdt_stop(void)
{
	int ret;
	int fd;

	fd = open("/dev/watchdog", 0, 0);
	if (fd < 0) {
		return WISE_ERR_NOT_FOUND;
	}

	ret = ioctl(fd, IOCTL_WDT_STOP, NULL);

	close(fd);

	if (ret) {
		return WISE_ERR_IOCTL;
	}

	return WISE_OK;
}

int scm_wdt_feed(void)
{
	int ret;
	int fd;

	fd = open("/dev/watchdog", 0, 0);
	if (fd < 0) {
		return WISE_ERR_NOT_FOUND;
	}

	ret = ioctl(fd, IOCTL_WDT_FEED, NULL);

	close(fd);

	if (ret) {
		return WISE_ERR_IOCTL;
	}

	return WISE_OK;
}
