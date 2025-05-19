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

#ifndef __SCM_GPIO_H__
#define __SCM_GPIO_H__

#include <stdint.h>

#include "wise_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * GPIO pin property
 */
enum scm_gpio_property {
	SCM_GPIO_PROP_OUTPUT,
	SCM_GPIO_PROP_INPUT,
	SCM_GPIO_PROP_INPUT_PULL_UP,
	SCM_GPIO_PROP_INPUT_PULL_DOWN,
};

/**
 * SCM GPIO value
 */
enum scm_gpio_value {
	SCM_GPIO_VALUE_LOW,
	SCM_GPIO_VALUE_HIGH,
};

/**
 * GPIO interrupt type
 */
enum scm_gpio_int_type {
	SCM_GPIO_INT_RISING_EDGE,
	SCM_GPIO_INT_FALLING_EDGE,
	SCM_GPIO_INT_BOTH_EDGE,
};

/**
 * GPIO notification callback
 */
typedef int (*scm_gpio_notify)(uint32_t pin, void *ctx);

/**
 * @brief Set GPIO configuration
 *
 * @param[in] pin the pin number
 * @param[in] prop the properties of GPIO
 */
int scm_gpio_configure(uint32_t pin, enum scm_gpio_property property);

/**
 * @brief Write GPIO output value
 *
 * @param[in] pin the pin number
 * @param[in] value high or low value
 */
int scm_gpio_write(uint32_t pin, uint8_t value);

/**
 * @brief Read GPIO input value
 *
 * @param[in] pin the pin number
 * @param[in] value high or low value
 */
int scm_gpio_read(uint32_t pin, uint8_t *value);

/**
 * @brief Enable GPIO interrupt
 *
 * @param[in] pin the pin number
 * @param[in] type the interrupt type
 * @param[in] notify notification function
 * @param[in] ctx user context to be used in the callback
 */
int scm_gpio_enable_interrupt(uint32_t pin, enum scm_gpio_int_type type, scm_gpio_notify notify, void *ctx);

/**
 * @brief Disable GPIO interrupt
 *
 * @param[in] pin the pin number
 */
int scm_gpio_disable_interrupt(uint32_t pin);

#ifdef __cplusplus
}
#endif

#endif //__SCM_GPIO_H__
