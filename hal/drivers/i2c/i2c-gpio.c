#include <stdio.h>
#include <hal/init.h>
#include <hal/kernel.h>
#include <hal/pinctrl.h>
#include <hal/device.h>
#include <hal/timer.h>
#include <hal/kmem.h>
#include <hal/console.h>
#include <hal/i2c.h>

#include "vfs.h"

#define I2C_GPIO_DELAY		4
#define I2C_GPIO_SCL		CONFIG_I2C_GPIO_PIN_SCL
#define I2C_GPIO_SDA		CONFIG_I2C_GPIO_PIN_SDA

struct i2c_gpio_driver_data {
	struct fops devfs_ops;
	struct device *dev;
	uint8_t pin_scl;
	uint8_t pin_sda;

	struct i2c_event event;
	i2c_cb cb;
	void *ctx;
};

static int i2c_gpio_tx_byte(struct i2c_gpio_driver_data *priv, uint8_t value);

static void i2c_gpio_set_low(uint8_t pin)
{
	gpio_direction_output(pin, 0);
}

static void i2c_gpio_set_high(uint8_t pin)
{
	gpio_direction_output(pin, 1);
}

static void i2c_gpio_read_setup(uint8_t pin)
{
	gpio_direction_input(pin);
}

static uint8_t i2c_gpio_read_input(uint8_t pin)
{
	return gpio_get_value(pin);
}

static int i2c_gpio_start(struct device *dev, uint8_t addr)
{
	struct i2c_gpio_driver_data *priv = dev->driver_data;

	i2c_gpio_set_low(priv->pin_sda);
	udelay(I2C_GPIO_DELAY/2);
	i2c_gpio_set_low(priv->pin_scl);
	return i2c_gpio_tx_byte(priv, addr);
}

static int i2c_gpio_stop(struct device *dev)
{
	struct i2c_gpio_driver_data *priv = dev->driver_data;

	i2c_gpio_set_low(priv->pin_sda);
	udelay(I2C_GPIO_DELAY);
	i2c_gpio_set_high(priv->pin_scl);
	udelay(I2C_GPIO_DELAY);
	i2c_gpio_set_high(priv->pin_sda);
	udelay(I2C_GPIO_DELAY);

	return 0;
}

static uint8_t i2c_gpio_read_byte(struct i2c_gpio_driver_data *priv, bool nack)
{
	uint8_t val = 0;
	int i;

	i2c_gpio_set_high(priv->pin_sda);
	for (i = 0; i < 8; i++) {
		i2c_gpio_read_setup(priv->pin_sda);
		i2c_gpio_set_high(priv->pin_scl);
		udelay(I2C_GPIO_DELAY/2);
		val <<= 1;
		if (i2c_gpio_read_input(priv->pin_sda)) {
			val |= 1;
		}
		i2c_gpio_set_low(priv->pin_scl);
	}

	if (nack) {
		i2c_gpio_set_high(priv->pin_sda);
	} else {
		i2c_gpio_set_low(priv->pin_sda);
	}
	i2c_gpio_set_high(priv->pin_scl);
	udelay(I2C_GPIO_DELAY);
	i2c_gpio_set_low(priv->pin_scl);
	udelay(I2C_GPIO_DELAY);
	i2c_gpio_set_low(priv->pin_sda);

	return val;
}

static int i2c_gpio_recv(struct device *dev, uint8_t *rx_buf, uint32_t len)
{
	struct i2c_gpio_driver_data *priv = dev->driver_data;
	int i;

	for (i = 0; i < len - 1; i++) {
		rx_buf[i] = i2c_gpio_read_byte(priv, false);
	}

	rx_buf[len - 1] = i2c_gpio_read_byte(priv, true); //Give NACK on last byte read

	return 0;
}

static int i2c_gpio_tx_byte(struct i2c_gpio_driver_data *priv, uint8_t value)
{
	uint8_t curr;
	uint8_t ack;

	for (curr = 0x80; curr != 0; curr >>= 1) {
		if (curr & value) {
			i2c_gpio_set_high(priv->pin_sda);
		} else {
			i2c_gpio_set_low(priv->pin_sda);
		}

		udelay(I2C_GPIO_DELAY/2);

		i2c_gpio_set_high(priv->pin_scl);

		udelay(I2C_GPIO_DELAY/2);

		i2c_gpio_set_low(priv->pin_scl);

		udelay(2);
	}

	// get Ack or Nak
	i2c_gpio_read_setup(priv->pin_sda);
	i2c_gpio_set_high(priv->pin_scl);
	udelay(I2C_GPIO_DELAY/2);
	ack = i2c_gpio_read_input(priv->pin_sda);
	i2c_gpio_set_low(priv->pin_scl);
	udelay(I2C_GPIO_DELAY/2);
	i2c_gpio_set_low(priv->pin_sda);

	if (0 != ack) {
		return -EIO;
	}

	return 0;
}

static int i2c_gpio_send(struct device *dev, uint8_t *tx_buf , uint32_t len)
{
	struct i2c_gpio_driver_data *priv = dev->driver_data;
	int ret;
	int i;

	for (i = 0; i < len; i++) {
		ret = i2c_gpio_tx_byte(priv, tx_buf[i]);
		if (ret) {
			return ret;
		}
	}

	return 0;
}

static void i2c_cmpl(struct device *dev, enum i2c_event_type evt)
{
	struct i2c_gpio_driver_data *priv = dev->driver_data;

	if (priv->cb) {
		priv->event.type = evt;
		priv->cb(&priv->event, priv->ctx);
	}
}

static int i2c_gpio_master_tx(struct device *dev, uint16_t addr, uint8_t *tx_buf, uint32_t tx_len)
{
	int ret;

	addr = addr << 1;

	ret = i2c_gpio_start(dev, addr);
	if (ret) {
		goto done;
	}

	ret = i2c_gpio_send(dev, tx_buf, tx_len);

done:

	i2c_gpio_stop(dev);

	i2c_cmpl(dev, I2C_EVENT_MASTER_TRANS_CMPL);

	return ret;
}

static int i2c_gpio_master_rx(struct device *dev, uint16_t addr, uint8_t *rx_buf, uint32_t rx_len)
{
	int ret;

	addr = (addr << 1) | 0x1;

	ret = i2c_gpio_start(dev, addr);
	if (ret) {
		goto done;
	}

	ret = i2c_gpio_recv(dev, rx_buf, rx_len);

done:

	i2c_gpio_stop(dev);

	i2c_cmpl(dev, I2C_EVENT_MASTER_TRANS_CMPL);

	return ret;
}

static int i2c_gpio_configure(struct device *dev, struct i2c_cfg *cfg, i2c_cb cb, void *ctx)
{
	struct i2c_gpio_driver_data *priv = dev->driver_data;
	uint32_t v;

	v = readl(GPIO_BASE_ADDR + 4);
	if (cfg->pull_up_en) {
		v |= (1 << priv->pin_scl) | (1 << priv->pin_sda);
	} else {
		v &= ~((1 << priv->pin_scl) | (1 << priv->pin_sda));
	}
	writel(v, GPIO_BASE_ADDR + 4);

	priv->cb = cb;
	priv->ctx = ctx;

	return 0;
}

static int i2c_gpio_shutdown(struct device *dev)
{
	free(dev->driver_data);

	return 0;
}

static int i2c_gpio_ioctl(struct file *file, unsigned int cmd, void *arg)
{
	struct i2c_gpio_driver_data *priv = file->f_priv;
	struct device *dev = priv->dev;
	int ret = 0;

	switch (cmd) {
	case IOCTL_I2C_CONFIGURE: {
		struct i2c_cfg_arg *cfg_arg = arg;
		ret = i2c_ops(dev)->configure(dev, cfg_arg->cfg, cfg_arg->cb, cfg_arg->cb_ctx);
		break;
	}
	case IOCTL_I2C_MASTER_TX: {
		struct i2c_master_tx_arg *tx_arg = arg;
		ret = i2c_ops(dev)->master_tx(dev, tx_arg->addr, tx_arg->tx_buf, tx_arg->tx_len);
		break;
	}
	case IOCTL_I2C_MASTER_RX: {
		struct i2c_master_rx_arg *rx_arg = arg;
		ret = i2c_ops(dev)->master_rx(dev, rx_arg->addr, rx_arg->rx_buf, rx_arg->rx_len);
		break;
	}
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int i2c_gpio_probe(struct device *dev)
{
	struct i2c_gpio_driver_data *priv;
	struct file *file;

	if (gpio_request(dev, "scl", I2C_GPIO_SCL) < 0) {
		return -EBUSY;
	}

	if (gpio_request(dev, "sda", I2C_GPIO_SDA) < 0) {
		return -EBUSY;
	}

	priv = kmalloc(sizeof(struct i2c_gpio_driver_data));
	if (!priv) {
		return -ENOMEM;
	}

	memset(priv, 0, sizeof(struct i2c_gpio_driver_data));

	priv->pin_scl = I2C_GPIO_SCL;
	priv->pin_sda = I2C_GPIO_SDA;

	i2c_gpio_set_high(priv->pin_scl);
	i2c_gpio_set_high(priv->pin_sda);

	priv->dev = dev;
	priv->devfs_ops.ioctl = i2c_gpio_ioctl;

	file = vfs_register_device_file("/dev/i2c-gpio", &priv->devfs_ops, priv);
	if (!file) {
		printk("%s: failed to register as %s\n", dev_name(dev), "/dev/i2c-gpio");
	}

	dev->driver_data = priv;

	printk("I2C-GPIO: SCL=%d, SDA=%d\n", priv->pin_scl, priv->pin_sda);

	return 0;
}

struct i2c_ops i2c_gpio_ops = {
	.configure = i2c_gpio_configure,
	.master_tx = i2c_gpio_master_tx,
	.master_rx = i2c_gpio_master_rx,
};

static declare_driver(i2c_gpio) = {
	.name 		= "i2c-gpio",
	.probe		= i2c_gpio_probe,
	.shutdown	= i2c_gpio_shutdown,
	.ops		= &i2c_gpio_ops,
};
