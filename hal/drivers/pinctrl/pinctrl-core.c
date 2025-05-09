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
#include <soc.h>
#include <hal/kernel.h>
#include <hal/device.h>
#include <hal/pinctrl.h>
#include <hal/console.h>

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#if 0
#define dbg(x...) printk(x)
#define warn(x...) printk(x)
#else
#define dbg(x...)
#define warn(x...)
#endif
#define err(x...) printk(x)

#define pinctrl_get_ops(dev) ((struct pinctrl_ops *)(dev)->driver->ops)

static struct device *pinctrl = NULL; /* only one instance in the system */
static struct pinctrl_platform_pinmap *platform_pinmap = NULL;

int pinctrl_register_controller(struct device *device)
{
	struct pinctrl_ops *ops;

	if (pinctrl) {
		warn("PINCTRL: controller already registered\n");
		return -EBUSY;
	}

	/* Sanity check */
	ops = pinctrl_get_ops(device);
	if (!ops->pins || ops->npins == 0) {
		err("PINCTRL: no pins in driver\n");
		return -EINVAL;
	}
	printk("PINCTRL: pin controller %s registered\n", dev_name(device));

	pinctrl = device;

	return 0;
}

static int request_pin(struct device *device, const char *id, unsigned pin,
					   bool gpio)
{
	struct pinctrl_ops *ops;
	struct pinctrl_pin_desc *desc;
	int ret = -EINVAL;

	if (!pinctrl)
		return -ENODEV;

	ops = pinctrl_get_ops(pinctrl);

	desc = ops->pins + pin;
	if (!desc) {
		err("PINCTRL: pin %d is not registered and so cannot be requested\n",
			pin);
		goto out;
	}

	dbg("PINCTRL: %s/%s request pin %d\n", dev_name(device), id, pin);

	if (desc->refcnt &&
		(desc->owner.client != device  || strcmp(desc->owner.id, id))) {
		err("PINCTRL: pin %d is already claimed by %s(%s)\n",
			pin,
			desc->owner.client ? dev_name(desc->owner.client): "",
			desc->owner.id);

		goto out;
	}

	if (gpio)
		ret = ops->pmxops->gpio_request_enable(pinctrl, pin);
	else if (ops->pmxops->request(pinctrl, pin) == 0)
		ret = ops->pmxops->set_mux(pinctrl, device, id, pin);

	if (ret) {
		err("PINCTRL: %s(%s) failed to request pin %d\n",
			device ? dev_name(device) : "", id, pin);
		goto out;
	}

	/* Claim the pin */
	desc->owner.client = device;
	desc->owner.id = id;
	desc->refcnt++;

	ret = 0;

 out:
	return ret;
}

static int free_pin(struct device *device, const char *id, unsigned pin,
					bool gpio)
{
	struct pinctrl_ops *ops;
	struct pinctrl_pin_desc *desc;
	int ret = -EINVAL;

	if (!pinctrl)
		return -ENODEV;

	ops = pinctrl_get_ops(pinctrl);

	desc = &ops->pins[pin];
	if (!desc || !desc->refcnt) {
		err("PINCTRL: pin %d is not registered and/or claimed, so cannot be freed\n",
			pin);
		goto out;
	}

	dbg("PINCTRL: free pin %d for %s(%s)\n", pin, dev_name(device), id);

	if (desc->refcnt &&
		(desc->owner.client != device  || strcmp(desc->owner.id, id))) {
		err("PINCTRL: pin %d is not owned by %s(%s)\n",
			pin,
			desc->owner.client ? dev_name(desc->owner.client): "",
			desc->owner.id);

		goto out;
	}

	if (gpio)
		ret = ops->pmxops->gpio_disable_free(pinctrl, pin);
	else
		ret = ops->pmxops->free_pmx(pinctrl, pin);

	if (ret) {
		err("PINCTRL: %s(%s) failed to request pin %d\n",
			device ? dev_name(device) : "", id, pin);
		goto out;
	}

	desc->refcnt--;
	if (desc->refcnt == 0) {
		desc->owner.client = NULL;
		desc->owner.id = NULL;
	}

	ret = 0;

 out:
	return ret;
}

int pinctrl_request_pin(struct device *device, const char *id, unsigned pin)
{
	return request_pin(device, id, pin, false);
}

int pinctrl_free_pin(struct device *device, const char *id, unsigned pin)
{
	return free_pin(device, id, pin, false);
}

int gpio_request(struct device *device, const char *id, unsigned pin)
{
	return request_pin(device, id, pin, true);
}

int gpio_free(struct device *device, const char *id, unsigned pin)
{
	return free_pin(device, id, pin, true);
}

static int __gpio_set_direction(unsigned pin, bool input)
{
	struct pinctrl_ops *ops;

	if (!pinctrl)
		return -ENODEV;

	ops = pinctrl_get_ops(pinctrl);

	return ops->pmxops->gpio_set_direction(pinctrl, pin, input);
}

int gpio_direction_input(unsigned pin)
{
	return __gpio_set_direction(pin, true);
}

int gpio_direction_output(unsigned pin, int value)
{
	struct pinctrl_ops *ops;

	if (!pinctrl)
		return -ENODEV;

	ops = pinctrl_get_ops(pinctrl);

	ops->gpops->set(pinctrl, pin, value);
	return ops->pmxops->gpio_set_direction(pinctrl, pin, false);
}

int gpio_get_value(unsigned pin)
{
	struct pinctrl_ops *ops;

	if (!pinctrl)
		return -ENODEV;

	ops = pinctrl_get_ops(pinctrl);

	return ops->gpops->get(pinctrl, pin);
}

int gpio_set_value(unsigned pin, int value)
{
	struct pinctrl_ops *ops;

	if (!pinctrl)
		return -ENODEV;

	ops = pinctrl_get_ops(pinctrl);

	return ops->gpops->set(pinctrl, pin, value);
}

int gpio_set_config(unsigned pin, unsigned config)
{
	struct pinctrl_ops *ops;

	if (!pinctrl)
		return -ENODEV;

	ops = pinctrl_get_ops(pinctrl);

	return ops->confops->pin_config_set(pinctrl, pin, config);
}

int gpio_interrupt_enable(unsigned pin, int edge,
		void (*intr_cb)(u32 pin, void *ctx), void *ctx)
{
	struct pinctrl_ops *ops;
	int ret = -EINVAL;

	printf("irq enable request\n");

	if (!pinctrl)
		return -ENODEV;

	ops = pinctrl_get_ops(pinctrl);
	if (ops->gpops->irq_en) {
		printf("irq enable\n");
		ret = ops->gpops->irq_en(pinctrl, pin, edge, intr_cb, ctx);
	}
	return ret;
}

int gpio_interrupt_disable(unsigned pin)
{
	struct pinctrl_ops *ops;
	int ret = -EINVAL;

	if (!pinctrl)
		return -ENODEV;

	ops = pinctrl_get_ops(pinctrl);
	if (ops->gpops->irq_dis) {
		ret = ops->gpops->irq_dis(pinctrl, pin);
	}

	return ret;
}

/*
 * Should be done after device/driver registration/binding is finished.
 */
static int __attribute__((used))
pinctrl_apply_platform_pinmap(struct pinctrl_platform_pinmap *bpm)
{
	struct device *dev;
	struct pinctrl_pin_map *pmap = bpm->map;
	int i, ret = 0, failure = 0;

	if (!pinctrl)
		return -ENODEV;

	for (i = 0; i < bpm->nr_map; i++) {
		dev = device_get_by_name(pmap[i].func);
		if (!dev)
			continue;

		ret = pinctrl_request_pin(dev, pmap[i].id, pmap[i].pin);
		if (ret) {
			failure++;
			pmap[i].status = ret;
		}
	}

	if (failure)
		return -EINVAL;

	return 0;
}


int pinctrl_set_platform_pinmap(struct pinctrl_platform_pinmap *bpm)
{
	int i, j;

	if (platform_pinmap)
		return -EBUSY;

	platform_pinmap = bpm;

	for (i = 0; i < bpm->nr_map; i++) {
		for (j = 0; j < bpm->nr_map; j++) {
			if (i == j) {
				continue;
			}
			if (bpm->map[i].pin == bpm->map[j].pin) {
				warn("PINCTRL: duplicate pinmap for %d\n", bpm->map[i].pin);
			}
		}
	}

	return 0;
}

struct pinctrl_platform_pinmap *pinctrl_get_platform_pinmap(void)
{
	return platform_pinmap;
}


struct pinctrl_pin_map *pinctrl_lookup_platform_pinmap(struct device *device,
													   const char *id)
{
	int i;
	struct pinctrl_pin_map *pmap;

	if (!platform_pinmap)
		return NULL;

	pmap = platform_pinmap->map;

	for (i = 0; i < platform_pinmap->nr_map; i++) {
		if (!strcmp(pmap[i].func, dev_name(device)) &&
			!strcmp(pmap[i].id, id))
				return &pmap[i];
	}

	return NULL;
}

#ifdef CONFIG_PM_IO
__iram__ int pinctrl_powerdown(struct device *device)
{
	struct pinctrl_ops *ops;

	ops = pinctrl_get_ops(pinctrl);

	return ops->pmops->powerdown(pinctrl);
}

__iram__ int pinctrl_powerup(struct device *device)
{
	struct pinctrl_ops *ops;

	ops = pinctrl_get_ops(pinctrl);

	return ops->pmops->powerup(pinctrl);
}
#endif
