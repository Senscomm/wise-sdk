/*
 * Copyright 2024-2025 Senscomm Semiconductor Co., Ltd.	All rights reserved.
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
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <soc.h>
#include <hal/bitops.h>
#include <hal/kernel.h>
#include <hal/console.h>
#include <hal/rtc.h>
#include <cmsis_os.h>
#include <hal/i2c.h>
#include "hal/pinctrl.h"
#include "sys/ioctl.h"
#include "hal/timer.h"

#define ERROR
#define DEBUG

#ifdef ERROR
#define LOG_ERR(...)					printk(__VA_ARGS__)
#else
#define LOG_ERR(...)					(void)0
#endif

static uint64_t isr_us;
static uint64_t alarm_us;
static uint32_t alarm_time;
static bool alarm_set;
static bool sd3068b_init = false;


/* SD3068 I2C address (7-bit) */
#define SD3068_I2C_ADDRESS       0x32  /* 7-bit address (0110010) */

/* Control/Status register (Address 0x0F) */
#define SD3068_REG_CTR1           0x0F  /* Control and Status Register 1 */
#define SD3068_CTR1_WRTC3       (1 << 7)
#define SD3068_CTR1_INTAF       (1 << 5) /* Alarm Flag */
#define SD3068_CTR1_INTDF       (1 << 4) /* Countdown Flag */
#define SD3068_CTR1_WRTC2       (1 << 2)

/* Control/Status register (Address 0x10) */
#define SD3068_REG_CTR2           0x10  /* Control and Status Register 2 */
#define SD3068_CTR2_WRTC3       (1 << 7)
#define SD3068_CTR2_INTS1       (1 << 5)
#define SD3068_CTR2_INTS0       (1 << 4)
#define SD3068_CTR2_INTDE       (1 << 2)
#define SD3068_CTR2_INTAE       (1 << 1)
#define SD3068_CTR2_INTFE       (1 << 0)

/* Control/Status register (Address 0x11) */
#define SD3068_REG_CTR3           0x11  /* Control and Status Register 2 */
#define SD3068_CTR3_TDS1       (1 << 5)
#define SD3068_CTR3_TDS0       (1 << 4)

#define SD3068_REG_TD0           0x13
#define SD3068_REG_TD1           0x14
#define SD3068_REG_TD2           0x15

#define SD3068_INT_GPIO 2

struct rtc_driver_data {
	struct device *dev;
	struct device *bus; /* I2C device */
	u16 addr; /* I2C address */
	osSemaphoreId_t sem; /* I2C completion */
};

static int sd3068_rtc_countdown(struct device *dev, uint32_t duration_us);

static int sd3068_i2c_write(struct rtc_driver_data *dev_data,
				   u8 *buf, uint32_t length)
{
	int ret;

	ret = i2c_master_tx(dev_data->bus, dev_data->addr, buf, length);
	if (ret < 0) {
		LOG_ERR("Error %d from i2c_master_tx.\n", ret);
		return -EIO;
	}

	ret = osSemaphoreAcquire(dev_data->sem, 100);
	if (ret != osOK) {
		i2c_reset(dev_data->bus);
		LOG_ERR("No I2C response.\n");
		return -EIO;
	}

	return 0;
}

static int sd3068_write_reg(struct device *dev, uint8_t length, uint8_t *buf)
{
	struct rtc_driver_data *const dev_data = dev->driver_data;
	int ret = 0;

	ret = sd3068_i2c_write(dev_data, buf, length);
	if (ret < 0) {
		LOG_ERR("sd3068_i2c_write fail\n");
		return -EIO;
	}

	return ret;
}

static int sd3068_i2c_read(struct rtc_driver_data *dev_data,
				   u8 *buf, uint8_t length)
{
	int ret;

	ret = i2c_master_rx(dev_data->bus, dev_data->addr, buf, length);
	if (ret < 0) {
		LOG_ERR("Error %d from i2c_master_tx.\n", ret);
		return -EIO;
	}

	ret = osSemaphoreAcquire(dev_data->sem, 100);
	if (ret != osOK) {
		i2c_reset(dev_data->bus);
		LOG_ERR("No I2C response.\n");
		return -EIO;
	}

	return 0;
}

static int sd3068_read_reg(struct device *dev,
				   uint8_t reg, uint8_t length, uint8_t *val)
{
	int ret;
	struct rtc_driver_data *const dev_data = dev->driver_data;
	uint8_t data[1];

	data[0] = reg;
	/* First, send the register address to be read */
	ret = sd3068_i2c_write(dev_data, data, 1);  // Send the register address
	if (ret < 0) {
		return -EIO;
	}

	/* Now read the data from that register */
	ret = sd3068_i2c_read(dev_data, val, length);
	if (ret < 0) {
		LOG_ERR("sd3068_i2c_read failed to read register data\n");
		return -EIO;
	}
	return 0;

}

static void i2c_notify(struct i2c_event *event, void *ctx)
{
	struct rtc_driver_data *data = ctx;

	switch (event->type) {
	case I2C_EVENT_MASTER_TRANS_CMPL:
		osSemaphoreRelease(data->sem);
		break;
	default:
		LOG_ERR("Unknown I2C response: %d\n", event->type);
		break;
	}
}

static osSemaphoreId_t g_sync_signal;

static int sd3068_isr(uint32_t pin, void *ctx)
{
	osSemaphoreRelease(g_sync_signal);
	return 0;
}

#define abs(x) ((x) < 0 ? -(x) : (x))

static int sd3068_isr_exec(void)
{
	uint32_t diff_time = (uint32_t)abs(isr_us - alarm_us);

	printk("Countdown completed! %d (%d)\n", diff_time, (int)(diff_time - alarm_time));

	return 0;
}

static int sd3068_isr_register(struct device *dev)
{
	int ret;
	int fd;
	struct gpio_interrupt_enable rtc_int;
	struct gpio_configure rtc_int_cfg;

	fd = open("/dev/gpio", 0, 0);
	if (fd < 0) {
		return -1;
	}

	rtc_int_cfg.pin = SD3068_INT_GPIO;
	rtc_int_cfg.property = GPIO_INPUT_PULL_UP;

	ret = ioctl(fd, IOCTL_GPIO_CONFIGURE, &rtc_int_cfg);
	if (ret)
		goto exit;

	rtc_int.pin = SD3068_INT_GPIO;
	rtc_int.type = GPIO_INTR_FULLING_EDGE;
	rtc_int.intr_cb = (void (*)(u32, void *))sd3068_isr;
	rtc_int.ctx = dev;

	ret = ioctl(fd, IOCTL_GPIO_INTERRUPT_ENABLE, &rtc_int);

exit:

	close(fd);

	return ret;

}

static void sd3068_rtc_thread(void *param)
{
	while (1) {
		osSemaphoreAcquire(g_sync_signal, osWaitForever);
		isr_us = tick_to_us(ktime());
		sd3068_isr_exec();

	}
}

static int sd3068_rtc_init(struct device *dev)
{
	uint8_t buf[4];
	int ret;

	/* write unlock */
	buf[0] = SD3068_REG_CTR2;
	buf[1] = 0x80;
	ret = sd3068_write_reg(dev, 2, buf);
	if (ret) {
		return -EIO;
	}

	buf[0] = SD3068_REG_CTR1;
	buf[1] = 0x84;
	ret = sd3068_write_reg(dev, 2, buf);
	if (ret) {
		return -EIO;
	}

	/* init count=0x0 */
	buf[0] = SD3068_REG_CTR3;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 0;
	ret = sd3068_write_reg(dev, 4, buf);
	if (ret) {
		return -EIO;
	}
	/* 4096Hz, TDS1=0 TDS0=0, 0 */
	buf[0] = SD3068_REG_CTR3;
	buf[1] = 0;
	ret = sd3068_write_reg(dev, 2, buf);
	if (ret) {
		return -EIO;
	}

	return 0;
}

static int sd3068_rtc_probe(struct device *dev)
{
	struct rtc_driver_data *priv;
	struct i2c_cfg i2c;

	int ret = 0;

	priv = zalloc(sizeof(*priv));
	if (!priv) {
		return -ENOMEM;
	}

	priv->dev = dev;
	priv->bus = device_get_by_name(dev->io.bus);
	if (!priv->bus) {
		LOG_ERR("%s: failed to get a bus\n", dev_name(dev));
		goto exit;
	}

	i2c.role = I2C_ROLE_MASTER;
	i2c.addr_len = I2C_ADDR_LEN_7BIT;
	i2c.bitrate = 400 * 1000;
	i2c.dma_en = 0;
	i2c.pull_up_en = 1;

	priv->sem = osSemaphoreNew(1, 0, NULL);
	priv->addr = SD3068_I2C_ADDRESS;

	ret = i2c_configure(priv->bus, &i2c, i2c_notify, priv);

	dev->driver_data = priv;

	ret = sd3068_isr_register(dev);

	osThreadAttr_t attr = {
		.name 		= "rtc",
		.stack_size = 1024,
		.priority 	= osPriorityNormal,
	};

	g_sync_signal = osSemaphoreNew(1, 0, NULL);
	if (g_sync_signal == NULL) {
		printk("failed to create sync_signal\n");
		ret = -1;
		goto exit;
	}

	if (osThreadNew(sd3068_rtc_thread, NULL, &attr) == NULL) {
			printk("%s: failed to create sync task\n", __func__);
			ret = -1;
			goto exit;
	}

	printk("%s: registered\n", dev_name(dev));

exit:
	if (ret) {
		if (priv)
			free(priv);
	}
	return 0;
}

static int sd3068_rtc_countdown(struct device *dev, uint32_t duration_us)
{
	uint8_t buf[4];
	int ret = 0;
	uint64_t tmp = (uint64_t)duration_us * 4096;
	uint32_t countdown_value;
	countdown_value = tmp/1000000;

	if (!sd3068b_init) {
		sd3068_rtc_init(dev);
		sd3068b_init = true;
	}

	/* 0fH 10H, clear INTDF and INTDE */
	buf[0] = SD3068_REG_CTR1;
	buf[1] = 0x84;
	buf[2] = 0xb0;
	ret = sd3068_write_reg(dev, 3, buf);
	if (ret) {
		return -EIO;
	}

	buf[0] = SD3068_REG_TD0;
	buf[1] = countdown_value & 0xff;
	buf[2] = (countdown_value & 0xff00) >> 8;
	/* watcher use only dtim 30, it will not over 0xffff.
	 * buf[3] = (countdown_value & 0xff0000) >> 16;
	 * ret = sd3068_write_reg(dev, 4, buf);
	 */
	ret = sd3068_write_reg(dev, 3, buf);

	if (ret) {
		return -EIO;
	}

	if (alarm_set == 1) {
		alarm_set = 0;
		alarm_us = tick_to_us(ktime());
	}
	/* Enable INT
	 * INTS1=1, INTS0=1, IM=0 and INTDE=1, 0011 0100
	 */
	buf[0] = SD3068_REG_CTR2;
	buf[1] = 0xb4;
	ret = sd3068_write_reg(dev, 2, buf);
	if (ret) {
		return -EIO;
	}

	return ret;
}

/* SD3068 not implement get,set, and reset ops */
struct rtc_ops sd3068_rtc_ops = {
	.get = NULL,
	.set = NULL,
	.reset = NULL,
	.get_32khz_count = NULL,
	.set_32khz_alarm = sd3068_rtc_countdown,
	.clear_32khz_alarm = NULL
};

static declare_driver(rtc) = {
	.name = "sd3068",
	.probe = sd3068_rtc_probe,
	.ops = &sd3068_rtc_ops,
};

#ifdef CONFIG_CMD_RTC
/**
 * RTC CLI commands
 */
#include <cli.h>

#include <stdio.h>
#include <stdlib.h>

static int do_rtc_get_time(int argc, char *argv[])
{
	struct device *dev = device_get_by_name("sd3068");
	struct rtc_time time = {0};

	if (!dev) {
		return CMD_RET_FAILURE;
	}

	rtc_get(dev, &time);

	printf("secs=%d, mins=%d, hours=%d, "
			"mday=%d, mon=%d, year=%d, wday=%d\n",
			time.tm_sec, time.tm_min, time.tm_hour,
			time.tm_mday, time.tm_mon, time.tm_year, time.tm_wday);

	return CMD_RET_SUCCESS;
}

static int do_rtc_set_time(int argc, char *argv[])
{
	struct device *dev = device_get_by_name("sd3068");
	struct rtc_time time;

	if (!dev) {
		return CMD_RET_FAILURE;
	}

	if (argc < 5) {
		return CMD_RET_USAGE;
	}

	time.tm_sec = strtoul(argv[1], NULL, 0);
	time.tm_min = strtoul(argv[2], NULL, 0);
	time.tm_hour = strtoul(argv[3], NULL, 0);
	time.tm_mday = strtoul(argv[4], NULL, 0);
	time.tm_wday = strtoul(argv[5], NULL, 0);
	time.tm_mon = strtoul(argv[6], NULL, 0);
	time.tm_year = strtoul(argv[7], NULL, 0);

	rtc_set(dev, &time);

	return CMD_RET_SUCCESS;
}

static int do_rtc_reset(int argc, char *argv[])
{
	struct device *dev = device_get_by_name("sd3068");

	if (!dev) {
		return CMD_RET_FAILURE;
	}

	rtc_reset(dev);

	return CMD_RET_SUCCESS;
}

static int do_rtc_set_countdown(int argc, char *argv[])
{
	struct device *dev = device_get_by_name("sd3068");
	uint32_t alarm;

	if (argc < 2) {
		return CMD_RET_USAGE;
	}

	alarm = atoi(argv[1]);

	alarm_time = alarm;
	alarm_set = 1;
	rtc_set_32khz_alarm(dev, alarm);

	printf("after %d(us) alarm\n", alarm);

	return CMD_RET_SUCCESS;
}

static int do_rtc_read(int argc, char *argv[])
{
	struct device *dev = device_get_by_name("sd3068");
	uint8_t buf[15];
	uint8_t reg, length, i;

	if (argc != 3) {
		return CMD_RET_USAGE;
	}

	reg = strtoul(argv[1], NULL, 0);
	length = strtoul(argv[2], NULL, 0);

	sd3068_read_reg(dev, reg, length, buf);

	for (i = 0; i < length; i++)
		printf("reg[0x%x] = 0x%01x\n", reg + i, buf[i]);

	return CMD_RET_SUCCESS;
}

static int do_rtc_write(int argc, char *argv[])
{
	struct device *dev = device_get_by_name("sd3068");
	uint8_t buf[15];
	uint8_t reg, length, i;

	if (argc < 3) {
		return CMD_RET_USAGE;
	}

	reg = atoi(argv[1]);
	length = argc - 2;

	buf[0] = reg;
	for (i = 1; i < length + 1; i++)
		buf[i] = strtoul(argv[2 + i], NULL, 0);

	sd3068_write_reg(dev, length + 1, buf);

	return CMD_RET_SUCCESS;
}


static const struct cli_cmd rtc_cmd[] = {
	CMDENTRY(get, do_rtc_get_time, "", ""),
	CMDENTRY(set, do_rtc_set_time, "", ""),
	CMDENTRY(reset, do_rtc_reset, "", ""),
	CMDENTRY(cd, do_rtc_set_countdown, "", ""),
	CMDENTRY(read, do_rtc_read, "", ""),
	CMDENTRY(write, do_rtc_write, "", ""),
};

static int do_rtc(int argc, char *argv[])
{
	const struct cli_cmd *cmd;

	argc--;
	argv++;

	cmd = cli_find_cmd(argv[0], rtc_cmd, ARRAY_SIZE(rtc_cmd));
	if (cmd == NULL)
		return CMD_RET_USAGE;

	return cmd->handler(argc, argv);
}

CMD(extrtc, do_rtc,
		"test routines for RTC (Real Time Clock)",
		"extrtc get" OR
		"extrtc set <day> <hour> <min> <sec>" OR
		"extrtc reset" OR
		"extrtc cd" OR
		"extrtc read <reg> <length>" OR
		"extrtc write <reg> <val 1> <val 2> ..." OR
	);
#endif
