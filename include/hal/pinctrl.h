/*
 * Copyright 2018-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef __PINCTRL_H__
#define __PINCTRL_H__

#include <hal/kernel.h>
#include <hal/device.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * High-level device driver
 */

#define IOCTL_GPIO_CONFIGURE			(1)
#define IOCTL_GPIO_READ					(2)
#define IOCTL_GPIO_WRITE				(3)
#define IOCTL_GPIO_INTERRUPT_ENABLE		(4)
#define IOCTL_GPIO_INTERRUPT_DISABLE	(5)

enum gpio_property {
	GPIO_OUTPUT				= 0,
	GPIO_INPUT				= 1,
	GPIO_INPUT_PULL_UP		= 2,
	GPIO_INPUT_PULL_DOWN	= 3,
};

enum gpio_intr_type {
	GPIO_INTR_RISING_EDGE	= 0,
	GPIO_INTR_FULLING_EDGE	= 1,
	GPIO_INTR_BOTH_EDGE		= 2,
};

struct gpio_configure {
	u32 pin;
	enum gpio_property property;
};

struct gpio_read_value {
	u32 pin;
	int value;
};

struct gpio_write_value {
	u32 pin;
	int value;
};

struct gpio_interrupt_enable {
	u32 pin;
	enum gpio_intr_type type;
	void (*intr_cb)(u32 pin, void *ctx);
	void *ctx;
};

struct gpio_interrupt_disable {
	u32 pin;
};

/*
 * Low-level device driver
 */

/*
 * Inspired by Linux pinctrl subsystem
 */

struct pinctrl_pin_desc {
	unsigned pin;
	struct {
		struct device *client;
		const char *id;
	} owner;
	int refcnt;
	void *driver_data;
};

#define PIN_CONFIG_PULL_DISABLE				0
#define PIN_CONFIG_PULL_UP					1
#define PIN_CONFIG_PULL_DOWN				2
#define PIN_CONFIG_DRIVE_STRENGTH			3
#define PIN_CONFIG_INPUT_ENABLE				4
#define PIN_CONFIG_PARAM_MASK				(0x7FFF)

#define PIN_CONFIG_MK_CMD(cmd, value) ((cmd << 16) | ((value) & PIN_CONFIG_PARAM_MASK))

#define PIN_INTR_RISE_EDGE					0
#define PIN_INTR_FALL_EDGE					1
#define PIN_INTR_BOTH_EDGE					2

/**
 *
 */
struct pinconf_ops {
	int (*pin_config_get)(struct device *pctl, u32 pin, u32 *config);
	int (*pin_config_set)(struct device *pctl, u32 pin, u32 config);
};

/**
 * struct pinmux_ops
 *
 * @set_mux: set_mux(pctl, dev, "rts", pin(6))
 *
 * Questions:
 * 1. Do we need pinctrl_gpio_range?
 * - All the pins can be configured as GPIO
 * - There is only one GPIO chip (so to speak).
 */
struct pinmux_ops {
	int (*request)(struct device *pctl, u32 pin);
	int (*free_pmx)(struct device *pctl, u32 pin);
#if 0
	int (*get_function_count)(struct device *pctl);
	const char (*get_function_name)(struct device *pctl, unsigned selector);

	struct pinmux *(*get_mux)(struct device *client, const char *id);
#endif

	int (*set_mux)(struct device *pctl, struct device *client, const char *id, u32 pin);

	int (*gpio_request_enable)(struct device *pctl, unsigned pin);
	int (*gpio_disable_free)(struct device *pctl, unsigned pin);
	int (*gpio_set_direction)(struct device *pctl, unsigned pin, bool input);
};

struct gpio_ops {
	int (*get)(struct device *pctl, u32 pin);
	int (*set)(struct device *pctl, u32 pin, int value);
	int (*to_irq)(struct device *pctl, u32 pin);
	int (*irq_en)(struct device *pctl, u32 pin, int edge,
			void (*intr_cb)(u32 pin, void *ctx), void *ctx);
	int (*irq_dis)(struct device *pctl, u32 pin);
};

struct pin_pm_ops {
	int (*powerdown)(struct device *pctl);
	int (*powerup)(struct device *pctl);
};

/**
 * struct pinctrl_desc
 */
struct pinctrl_ops {
	const char *name;
	struct pinctrl_pin_desc *pins;
	unsigned npins;

	const struct pinmux_ops *pmxops;
	const struct pinconf_ops *confops;
	const struct gpio_ops *gpops;
	const struct pin_pm_ops *pmops;

	void *priv;
};

struct pm_conf {
	int pm_out;		/* 0 : input, 1 : output */
	int pm_val;		/* 0 : output low, 1 : output high */
	int pm_pen;		/* 0 : pull disabled, 1 : pull enabled */
	int pm_pdn;		/* 0 : pull up, 1 : pull down */
};

struct pinctrl_pin_map {
	unsigned pin;
	const char *func;
	const char *id;
	int status;
	struct pm_conf *pmconf; /* 0 : not alternated, others : set to GPIO */
};

#define pinmap(p, f, i, pm)  { \
	.pin = p,				\
	.func = f,				\
	.id = i, 				\
	.pmconf = pm, 				\
}

struct pinctrl_platform_pinmap {
	struct pinctrl_pin_map *map;
	int nr_map;
};

/*
struct pinmux *pinctrl_get_pinmux(struct device *device, const char *id);
*/

/* pinctrl driver API */
int pinctrl_register_controller(struct device *pctl);

/* pinctrl client API */

/* Legacy style GPIO API */
int gpio_request(struct device *device, const char *id, unsigned pin);
int gpio_free(struct device *device, const char *id, unsigned pin);
int gpio_direction_input(unsigned pin);
int gpio_direction_output(unsigned pin, int value);
int gpio_get_value(unsigned pin);
int gpio_set_value(unsigned pin, int value);

/* Pin configuration */
int gpio_set_config(unsigned pin, unsigned config);

/**
 * pinctrl_request_pin() - request a pin and
 */
int pinctrl_request_pin(struct device *device, const char *id, unsigned pin);
int pinctrl_free_pin(struct device *device, const char *id, unsigned pin);

int pinctrl_set_platform_pinmap(struct pinctrl_platform_pinmap *bpm);
struct pinctrl_platform_pinmap *pinctrl_get_platform_pinmap(void);

struct pinctrl_pin_map *pinctrl_lookup_platform_pinmap(struct device *device, const char *id);

int pinctrl_powerdown(struct device *device);
int pinctrl_powerup(struct device *device);

int gpio_interrupt_enable(unsigned pin, int edge, void (*intr_cb)(u32 pin, void *ctx), void *ctx);
int gpio_interrupt_disable(unsigned pin);

#ifdef __cplusplus
}
#endif

#endif
