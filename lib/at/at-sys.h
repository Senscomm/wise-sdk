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

#ifndef __AT_SYS_H__
#define __AT_SYS_H__

#include <hal/types.h>
#include "hal/pinctrl.h"

/**
 * GPIO pin property
 */
enum at_gpio_property {
	AT_GPIO_PROP_INPUT_PULL_UP,
	AT_GPIO_PROP_INPUT_PULL_DOWN,
	AT_GPIO_PROP_MAX = AT_GPIO_PROP_INPUT_PULL_DOWN,
};

int at_gpio_configure(uint32_t pin, enum gpio_property property);
int at_gpio_enable_interrupt(uint32_t pin, enum gpio_intr_type type,
									void *notify, void *ctx);
int at_gpio_disable_interrupt(uint32_t pin);
int at_pm_enable_wakeup_gpio(uint8_t gpio, uint8_t pull, uint8_t level, void *cb);
int at_pm_disable_wakeup_gpio(uint8_t gpio);


#endif // __AT_SYS_H__
