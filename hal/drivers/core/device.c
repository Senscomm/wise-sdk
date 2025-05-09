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
#include <soc.h>
#include <hal/kernel.h>
#include <hal/device.h>
#include <hal/console.h>

#include <stdio.h>

#if 1
#define dbg(args...)
#else
#define dbg(args...) printk(args)
#endif

#define warn(args...) printk(args)

extern struct device __device_tab_start, __device_tab_end;

__iram__ struct device *device_get_by_name(const char *name)
{
	struct device *dev;

	for (dev = &__device_tab_start; dev < &__device_tab_end; dev++) {
		if (!strcmp(name, dev->name))
			return dev;
	}

	return NULL;
}

extern struct driver  __driver_tab_start, __driver_tab_end;

struct driver *driver_get_by_name(const char *name)
{
	struct driver *drv;
	char *p, buf[32];

	if (!name)
		return NULL;

	strcpy(buf, name);
	p = strchr(buf, '.');
	if (p)
		*p = '\0';

	for (drv = &__driver_tab_start; drv < &__driver_tab_end; drv++) {
		if (!strcmp(drv->name, buf))
			return drv;
	}
	return NULL;
}

int device_bind_driver(struct device *dev, struct driver *drv)
{
	int ret = 0;

	if (dev->driver) /* already bound */
		return 0;

	if (!drv)
		drv = driver_get_by_name(dev->name);

	dbg("Binding device %s to driver %s\n", dev_name(dev), drv->name);

	dev->driver = drv;

	if (drv->probe)
		ret = drv->probe(dev);

	return ret;
}

#define __init__(x)

/*
 * Scan all devices declared using declare_device() and
 * bind their respective driver.
 */
int  __init__(1) driver_init(void)
{
	struct device *dev;
	struct driver *drv;
	int ret = 0, result = 0;

	dbg("%s\n", __func__);

	for (dev = &__device_tab_start; dev < &__device_tab_end; dev++)	{

		if (dev->driver) /* already done in preinit() */
			continue;

		dbg("%s:\n", dev_name(dev));

		drv = driver_get_by_name(dev->name);
		if (!drv) {
			dbg("no driver found\n");
			continue;
		}

		ret = device_bind_driver(dev, drv);
		if (ret) {
			warn("No match for driver %s\n", dev_name(dev));
			if (!result)
				result = ret;
		}
	}
	return result;
}

/*
 * Scan all devices declared using declare_device() and
 * shutdown their respective driver.
 */
int driver_deinit(void)
{
	struct device *dev;
	struct driver *drv;
	int ret = 0;

	dbg("%s\n", __func__);

	for (dev = &__device_tab_start; dev < &__device_tab_end; dev++)	{

		dbg("%s:\n", dev_name(dev));

		drv = driver_get_by_name(dev->name);
		if (!drv) {
			dbg("no driver found\n");
			continue;
		}

        if (drv->shutdown) {
		    ret = drv->shutdown(dev);
        }
	}

	/* NB: we have to loop twice because shutting a driver down may involves
	 * other devices. Shutting down UART driver, for example, will require
	 * pintctrl device to be functioning.
	 */
	for (dev = &__device_tab_start; dev < &__device_tab_end; dev++)	{
		/* Unlink the driver */
		dev->driver = NULL;
	}

	return ret;
}

#ifdef CONFIG_PM_DM

__iram__ int pm_suspend_device(unsigned *duration)
{
	struct device *dev;
	struct driver *driver;
	int ret = 0;

	for (dev = &__device_tab_start; dev < &__device_tab_end; dev++) {
		driver = dev->driver;
		if (!driver || !driver->suspend)
			continue;
		ret = driver->suspend(dev, duration);
		if (ret < 0)
			goto fail;
	}
	return 0;
 fail:
#if 0
	printk("[PM] suspend request rejected by %s (ret=%d)\n",
	       dev_name(dev), ret);
#endif
	for (; dev >= &__device_tab_start; dev--) {
		driver = dev->driver;
		if (!driver || !driver->resume)
			continue;
		driver->resume(dev);
	}
	return ret;
}

__iram__ void pm_resume_device(void)
{
	struct device *dev;
	struct driver *driver;

	for (dev = &__device_tab_start; dev < &__device_tab_end; dev++) {
		driver = dev->driver;
		if (!driver || !driver->resume)
			continue;
		driver->resume(dev);
	}
}

#endif
