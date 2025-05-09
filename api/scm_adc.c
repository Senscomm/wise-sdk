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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <cmsis_os.h>

#include "errno.h"
#include "hal/adc.h"
#include "sys/ioctl.h"
#include "scm_adc.h"

#include "hal/console.h"

struct scm_adc_data {
	uint8_t sync;
	osSemaphoreId_t sem;
	scm_adc_cb cb;
	void *ctx;
};

static struct scm_adc_data adc_data;

static void scm_adc_callback(void *ctx)
{
	struct scm_adc_data *adcd = ctx;

	if (adcd->sync) {
		osSemaphoreRelease(adcd->sem);
	} else {
		if (adcd->cb) {
			adcd->cb(adcd->ctx);
		}
	}
}

int scm_adc_read(enum scm_adc_channel ch, uint16_t *buf, uint32_t len)
{
	struct adc_set_cb_arg cb_arg;
	struct adc_read_arg read_arg;
	int fd;
	int ret = 0;

	if (!adc_data.sem) {
		adc_data.sem = osSemaphoreNew(1, 0, NULL);
	}

	fd = open("/dev/adc", 0 ,0);
	if (fd < 0) {
		return WISE_ERR_NOT_FOUND;
	}

	cb_arg.adc_cb = scm_adc_callback;
	cb_arg.ctx = &adc_data;
	ret = ioctl(fd, IOCTL_ADC_SET_CB, &cb_arg);
	if (ret) {
		ret = WISE_ERR_IOCTL;
		goto out;
	}

	adc_data.sync = 1;

	read_arg.ch = ch;
	read_arg.data = buf;
	read_arg.len = len;
	ret = ioctl(fd, IOCTL_ADC_READ, &read_arg);
	if (ret) {
		ret = WISE_ERR_IOCTL;
		goto out;
	}

	ret = osSemaphoreAcquire(adc_data.sem, osWaitForever);
	if (ret != osOK) {
		ret = WISE_FAIL;
		goto out;
	}


out:
	close(fd);

	return ret;
}

int scm_adc_read_async(enum scm_adc_channel ch, uint16_t *buf, uint32_t len, scm_adc_cb cb, void *ctx)
{
	struct adc_set_cb_arg cb_arg;
	struct adc_read_arg read_arg;
	int fd;
	int ret;

	fd = open("/dev/adc", 0 ,0);
	if (fd < 0) {
		return WISE_ERR_NOT_FOUND;
	}

	adc_data.cb = cb;
	adc_data.ctx = ctx;
	adc_data.sync = 0;

	cb_arg.adc_cb = scm_adc_callback;
	cb_arg.ctx = &adc_data;
	ret = ioctl(fd, IOCTL_ADC_SET_CB, &cb_arg);
	if (ret) {
		ret = WISE_ERR_IOCTL;
		goto out;
	}

	read_arg.ch = ch;
	read_arg.data = buf;
	read_arg.len = len;
	ret = ioctl(fd, IOCTL_ADC_READ, &read_arg);
	if (ret) {
		ret = WISE_ERR_IOCTL;
		goto out;
	}

out:
	close(fd);

	return ret;
}

int scm_adc_reset(void)
{
	int fd;
	int ret;

	fd = open("/dev/adc", 0 ,0);
	if (fd < 0) {
		return WISE_ERR_NOT_FOUND;
	}

	ret = ioctl(fd, IOCTL_ADC_RESET, NULL);
	if (ret) {
		ret = WISE_ERR_IOCTL;
	}

	close(fd);

	return WISE_OK;
}
