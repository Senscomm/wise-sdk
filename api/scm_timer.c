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
#include "hal/timer.h"

#include "sys/ioctl.h"
#include "scm_timer.h"

static const char *timer_dev_name[SCM_TIMER_IDX_MAX] = {
	"/dev/timer0",
	"/dev/timer1",
};

int scm_timer_configure(enum scm_timer_idx idx, enum scm_timer_ch ch,
						struct scm_timer_cfg *cfg, scm_timer_notify notify, void *ctx)
{
	struct timer_cfg_arg arg;
	int fd;
	int ret;

	fd = open(timer_dev_name[idx], 0, 0);
	if (fd < 0) {
		return WISE_ERR_NOT_FOUND;
	}

	arg.ch = ch;
	if (cfg->mode == SCM_TIMER_MODE_PERIODIC) {
		arg.config = HAL_TIMER_PERIODIC;
		arg.param = cfg->data.periodic.duration;
	} else if (cfg->mode == SCM_TIMER_MODE_ONESHOT) {
		arg.config = HAL_TIMER_ONESHOT;
		arg.param = cfg->data.oneshot.duration;
	} else if (cfg->mode == SCM_TIMER_MODE_FREERUN) {
		arg.config = HAL_TIMER_FREERUN;
		arg.param = cfg->data.freerun.freq;
	} else if (cfg->mode == SCM_TIMER_MODE_PWM) {
		arg.config = HAL_TIMER_PWM;
		if (cfg->data.pwm.park) {
			arg.config |= HAL_TIMER_PWM_PARK;
		}
		arg.param = (cfg->data.pwm.high << 16) |
					(cfg->data.pwm.low & 0xffff);
	}
	if (cfg->intr_en) {
		arg.config |= HAL_TIMER_IRQ;
	}
	arg.isr = (int (*)(enum timer_event_type, void *))notify;
	arg.ctx = ctx;

	ret = ioctl(fd, IOCTL_TIMER_CONFIGURE, &arg);

	close(fd);

	if (ret) {
		return WISE_ERR_IOCTL;
	}

	return WISE_OK;
}

int scm_timer_start(enum scm_timer_idx idx, enum scm_timer_ch ch)
{
	struct timer_start_arg arg;
	int fd;
	int ret;

	fd = open(timer_dev_name[idx], 0, 0);
	if (fd < 0) {
		return WISE_ERR_NOT_FOUND;
	}

	arg.ch = ch;
	ret = ioctl(fd, IOCTL_TIMER_START, &arg);

	close(fd);

	if (ret) {
		return WISE_ERR_IOCTL;
	}

	return WISE_OK;
}

int scm_timer_stop(enum scm_timer_idx idx, enum scm_timer_ch ch)
{
	struct timer_stop_arg arg;
	int fd;
	int ret;

	fd = open(timer_dev_name[idx], 0, 0);
	if (fd < 0) {
		return WISE_ERR_NOT_FOUND;
	}

	arg.ch = ch;
	ret = ioctl(fd, IOCTL_TIMER_STOP, &arg);

	close(fd);

	if (ret) {
		return WISE_ERR_IOCTL;
	}

	return WISE_OK;
}

int scm_timer_start_multi(enum scm_timer_idx idx, uint8_t chs)
{
	struct timer_start_multi_arg arg;
	int fd;
	int ret;

	fd = open(timer_dev_name[idx], 0, 0);
	if (fd < 0) {
		return WISE_ERR_NOT_FOUND;
	}

	arg.chs = chs;
	ret = ioctl(fd, IOCTL_TIMER_START_MULTI, &arg);

	close(fd);

	if (ret) {
		return WISE_ERR_IOCTL;
	}

	return WISE_OK;
}

int scm_timer_stop_multi(enum scm_timer_idx idx, uint8_t chs)
{
	struct timer_stop_multi_arg arg;
	int fd;
	int ret;

	fd = open(timer_dev_name[idx], 0, 0);
	if (fd < 0) {
		return WISE_ERR_NOT_FOUND;
	}

	arg.chs = chs;
	ret = ioctl(fd, IOCTL_TIMER_STOP_MULTI, &arg);

	close(fd);

	if (ret) {
		return WISE_ERR_IOCTL;
	}

	return WISE_OK;
}

int scm_timer_value(enum scm_timer_idx idx, enum scm_timer_ch ch, uint32_t *value)
{
	struct timer_value_arg arg;
	int fd;
	int ret;

	fd = open(timer_dev_name[idx], 0, 0);
	if (fd < 0) {
		return WISE_ERR_NOT_FOUND;
	}

	arg.ch = ch;
	arg.value = value;
	ret = ioctl(fd, IOCTL_TIMER_VALUE, &arg);

	close(fd);

	if (ret) {
		return WISE_ERR_IOCTL;
	}

	return WISE_OK;
}
