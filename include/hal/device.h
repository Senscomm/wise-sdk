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

#ifndef __DEVICE_H__
#define __DEVICE_H__

#include <hal/types.h>

#include <stdlib.h>
#include <string.h>

#define DEVICE_MAX_BASE		4
#define DEVICE_MAX_IRQS		4
#define DEVICE_MAX_PIOS		4

struct driver;

struct device {
	const char *name;

	unsigned flags;

	struct driver *driver;
	void *driver_data;

	void *priv;

	union {
		/* register */
		u8 *base[DEVICE_MAX_BASE];	/* NULL terminated array */
		struct {
			const char *bus; /* IO bus, e.g., I2S, SPI, etc. */
			u8 pins[DEVICE_MAX_PIOS]; /* GPIOs for interrupt, reset, etc. */
		} io;
	};

	/* interrupt */
	int irq[DEVICE_MAX_IRQS]; 	/* -1 if not in use */
	int pri[DEVICE_MAX_IRQS];
};

#define declare_device_single(name, x)		\
	struct device name##_device 		\
		__attribute__((used, aligned(4), section(".device_tab_"#x)))

#define declare_device_array(name, x)		\
	struct device name##_device[] 		\
		__attribute__((used, aligned(4), section(".device_tab_"#x)))

#define __device__(x) \
	__attribute__((used, aligned(4), section(".device_tab_"#x)))

static inline const char *dev_name(struct device *dev)
{
	if (!dev)
		return NULL;

	return dev->name;
}

#ifndef CONFIG_BUILD_ROM
__ilm__
#endif
static inline int dev_id(struct device *dev)
{
	char *p = strchr(dev->name, '.');
	if (p)
		return atoi(p + 1);
	else
		return 0;

}

const char *dev_version(struct device *device);

/**
 * device_get_by_name() - finds a device object by name
 */
struct device *device_get_by_name(const char *name);

struct driver {
	const char *name;
	int (*probe)(struct device *dev);
	int (*suspend)(struct device *dev, u32 *idle);
	int (*resume)(struct device *dev);
	int (*version)(struct device *dev, char *buf, int size);
	int (*shutdown)(struct device *dev);
	void *ops;
};

#define declare_driver(name) 			\
	const struct driver name##_driver 		\
		__attribute__((used, aligned(4), section(".driver_tab")))

int device_bind_driver(struct device *dev, struct driver *drv);
int driver_init();
int driver_deinit(void);
int pm_suspend_device(u32 *);
void pm_resume_device(void);

#endif
