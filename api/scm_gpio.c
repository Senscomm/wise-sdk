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
#include "hal/types.h"
#include "hal/pinctrl.h"

#include "sys/ioctl.h"
#include "scm_gpio.h"

struct gpio_arg {
	uint32_t pin;
	uint8_t value;
};

int scm_gpio_configure(uint32_t pin, enum scm_gpio_property property)
{
	struct gpio_configure arg;
	int fd;
	int ret;

	fd = open("/dev/gpio", 0, 0);
	if (fd < 0) {
		return WISE_ERR_NOT_FOUND;
	}

	arg.pin = pin;
	arg.property = property;

	ret = ioctl(fd, IOCTL_GPIO_CONFIGURE, &arg);

	close(fd);

	if (ret) {
		return WISE_ERR_IOCTL;
	}
	return WISE_OK;
}

int scm_gpio_write(uint32_t pin, uint8_t value)
{
	struct gpio_write_value arg;
	int fd;
	int ret;

	fd = open("/dev/gpio", 0, 0);
	if (fd < 0) {
		return WISE_ERR_NOT_FOUND;
	}

	arg.pin = pin;
	arg.value = value;

	ret = ioctl(fd, IOCTL_GPIO_WRITE, &arg);

	close(fd);

	if (ret) {
		return WISE_ERR_IOCTL;
	}
	return WISE_OK;
}

int scm_gpio_read(uint32_t pin, uint8_t *value)
{
	struct gpio_read_value arg;
	int fd;
	int ret;

	fd = open("/dev/gpio", 0, 0);
	if (fd < 0) {
		return WISE_ERR_NOT_FOUND;
	}

	arg.pin = pin;

	ret = ioctl(fd, IOCTL_GPIO_READ, &arg);
	if (ret) {
		close(fd);
		return WISE_ERR_IOCTL;
	}

	*value = arg.value;
	close(fd);

	return WISE_OK;
}

int scm_gpio_enable_interrupt(uint32_t pin, enum scm_gpio_int_type type, scm_gpio_notify notify, void *ctx)
{
	struct gpio_interrupt_enable arg;
	int fd;
	int ret;

	fd = open("/dev/gpio", 0, 0);
	if (fd < 0) {
		return WISE_ERR_NOT_FOUND;
	}

	arg.pin = pin;
	arg.type = type;
	arg.intr_cb = (void (*)(u32, void *))notify;
	arg.ctx = ctx;

	ret = ioctl(fd, IOCTL_GPIO_INTERRUPT_ENABLE, &arg);

	close(fd);

	if (ret) {
		return WISE_ERR_IOCTL;
	}
	return WISE_OK;
}

int scm_gpio_disable_interrupt(uint32_t pin)
{
	struct gpio_interrupt_disable arg;
	int fd;
	int ret;

	fd = open("/dev/gpio", 0, 0);
	if (fd < 0) {
		return WISE_ERR_NOT_FOUND;
	}

	arg.pin = pin;

	ret = ioctl(fd, IOCTL_GPIO_INTERRUPT_DISABLE, &arg);

	close(fd);

	if (ret) {
		return WISE_ERR_IOCTL;
	}
	return WISE_OK;
}
