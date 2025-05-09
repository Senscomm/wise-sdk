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
static uint32_t fix_hz;


/* PCF8563 I2C address (7-bit) */
#define PCF8563_I2C_ADDRESS       0x51  /* 7-bit address (1010001) */

/* Control/Status register (Address 0x00) */
#define PCF8563_REG_ST1           0x00  /* Control and Status Register 1 */
#define PCF8563_CTRL1_TEST1       (1 << 7) /* Test Mode 1 */
#define PCF8563_CTRL1_STOP        (1 << 5) /* Stops the clock when set */
#define PCF8563_CTRL1_TESTC       (1 << 3) /* Test Mode for Power-On Reset */

/* Control/Status register (Address 0x01) */
#define PCF8563_REG_ST2           0x01  /* Control and Status Register 2 */
#define PCF8563_CTRL2_TI_TP       (1 << 4) /* Timer Interrupt Pulse Mode */
#define PCF8563_CTRL2_AF          (1 << 3) /* Alarm Flag */
#define PCF8563_CTRL2_TF          (1 << 2) /* Timer Flag */
#define PCF8563_CTRL2_AIE         (1 << 1) /* Alarm Interrupt Enable */
#define PCF8563_CTRL2_TIE         (1 << 0) /* Timer Interrupt Enable */

/* Seconds Register (Address 0x02) */
#define PCF8563_REG_SC           0x02  /* Seconds Register (BCD format) */
#define PCF8563_SECONDS_VL        (1 << 7)  /* Voltage Low Indicator */
#define PCF8563_SECONDS_MASK      0x7F  /* Mask for seconds value (00-59 in BCD) */

/* Minutes Register (Address 0x03) */
#define PCF8563_REG_MN           0x03  /* Minutes Register (BCD format) */
#define PCF8563_MINUTES_MASK      0x7F  /* Mask for minutes value (00-59 in BCD) */

/* Hours Register (Address 0x04) */
#define PCF8563_REG_HR             0x04  /* Hours Register (BCD format) */
#define PCF8563_HOURS_MASK        0x3F  /* Mask for hours value (00-23 in BCD) */

/* Days Register (Address 0x05) */
#define PCF8563_REG_DM              0x05  /* Days Register (BCD format) */
#define PCF8563_DAYS_MASK         0x3F  /* Mask for day value (01-31 in BCD) */

/* Weekdays Register (Address 0x06) */
#define PCF8563_REG_DW          0x06  /* Weekdays Register */
#define PCF8563_WEEKDAYS_MASK     0x07  /* Mask for weekday value (0-6) */

/* Century/Months Register (Address 0x07) */
#define PCF8563_REG_MO    0x07  /* Months and Century Register (BCD format) */
#define PCF8563_CENTURY           (1 << 7)  /* Century bit (C) 0:20xx, 1:19xx*/
#define PCF8563_MONTHS_MASK       0x1F  /* Mask for month value (01-12 in BCD) */

/* Years Register (Address 0x08) */
#define PCF8563_REG_YR             0x08  /* Years Register (BCD format) */
#define PCF8563_YEARS_MASK        0xFF  /* Mask for year value (00-99 in BCD) */

/* Minute Alarm Register (Address 0x09) */
#define PCF8563_REG_MINUTE_ALARM      0x09  /* Minute Alarm Register (BCD format) */
#define PCF8563_ALARM_AE          (1 << 7)  /* Alarm Enable bit (AE) */
#define PCF8563_MINUTE_ALARM_MASK 0x7F  /* Mask for minute alarm value (00-59 in BCD) */

/* Hour Alarm Register (Address 0x0A) */
#define PCF8563_REG_HOUR_ALARM        0x0A  /* Hour Alarm Register (BCD format) */
#define PCF8563_HOUR_ALARM_AE     (1 << 7)  /* Alarm Enable bit (AE) */
#define PCF8563_HOUR_ALARM_MASK   0x3F  /* Mask for hour alarm value (00-23 in BCD) */

/* Day Alarm Register (Address 0x0B) */
#define PCF8563_REG_DAY_ALARM         0x0B  /* Day Alarm Register (BCD format) */
#define PCF8563_DAY_ALARM_AE      (1 << 7)  /* Alarm Enable bit (AE) */
#define PCF8563_DAY_ALARM_MASK    0x3F  /* Mask for day alarm value (01-31 in BCD) */

/* Weekday Alarm Register (Address 0x0C) */
#define PCF8563_REG_WEEKDAY_ALARM     0x0C  /* Weekday Alarm Register */
#define PCF8563_WEEKDAY_ALARM_AE  (1 << 7)  /* Alarm Enable bit (AE) */
#define PCF8563_WEEKDAY_ALARM_MASK 0x07  /* Mask for weekday alarm value (0-6) */

/* CLKOUT Control Register (Address 0x0D) */
#define PCF8563_REG_CLKOUT_CONTROL    0x0D  /* CLKOUT Control Register */
#define PCF8563_CLKOUT_FE         (1 << 7)  /* FE: CLKOUT output enable */
#define PCF8563_REG_CLKO_F_MASK		0x03 /* frequenc mask */
#define PCF8563_REG_CLKO_F_32768HZ	0x00
#define PCF8563_REG_CLKO_F_1024HZ	0x01
#define PCF8563_REG_CLKO_F_32HZ		0x02
#define PCF8563_REG_CLKO_F_1HZ		0x03


/* Timer Control Register (Address 0x0E) */
#define PCF8563_REG_TIMER_CONTROL     0x0E  /* Timer Control Register */
#define PCF8563_TMRC_ENABLE          (1 << 7)  /* Timer Enable */
#define PCF8563_TMRC_4096	0
#define PCF8563_TMRC_64		1
#define PCF8563_TMRC_1		2
#define PCF8563_TMRC_1_60	3
#define PCF8563_TMRC_MASK	3

/* Count down timer (Address 0x0F) */
#define PCF8563_REG_TMR		0x0F /* timer */


#define PCF8563_INT_GPIO 2

#define bin2bcd(x) (((x / 10) << 4) | (x % 10))
#define bcd2bin(x) (((x) & 0x0F) + ((x) >> 4) * 10)
struct rtc_driver_data {
	struct device *dev;
	struct device *bus; /* I2C device */
	u16 addr; /* I2C address */
	osSemaphoreId_t sem; /* I2C completion */
};

static int pcf85631_rtc_countdown(struct device *dev, uint32_t duration_us);

static int pcf8563_i2c_write(struct rtc_driver_data *dev_data,
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

static int pcf8563_write_reg(struct device *dev,
				   uint8_t reg, uint8_t length, uint8_t *buf)

{
	struct rtc_driver_data *const dev_data = dev->driver_data;
	int i, ret = 0;

	for (i = 0; i < length; i++) {
		uint8_t data[2] = { reg + i, buf[i] };

		ret = pcf8563_i2c_write(dev_data, data, sizeof(data));
		if (ret < 0) {
			LOG_ERR("pcf8563_i2c_write fail\n");
			return -EIO;
		}
	}

	return ret;
}

static int pcf8563_i2c_read(struct rtc_driver_data *dev_data,
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

static int pcf8563_read_reg(struct device *dev,
				   uint8_t reg, uint8_t length, uint8_t *val)
{
	int ret;
	struct rtc_driver_data *const dev_data = dev->driver_data;
	uint8_t data[1];

	data[0] = reg;
	/* First, send the register address to be read */
	ret = pcf8563_i2c_write(dev_data, data, 1);  // Send the register address
	if (ret < 0) {
		return -EIO;
	}

	/* Now read the data from that register */
	ret = pcf8563_i2c_read(dev_data, val, length);
	if (ret < 0) {
		LOG_ERR("pcf8563_i2c_read failed to read register data\n");
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

static uint32_t remaining_time_us;
static osSemaphoreId_t g_sync_signal;

static int pcf85631_isr(uint32_t pin, void *ctx)
{
	osSemaphoreRelease(g_sync_signal);
	return 0;
}

#define abs(x) ((x) < 0 ? -(x) : (x))

static int pcf85631_isr_exec(void)
{
	struct device *dev = rtc_get_device();
	uint8_t reg_val;
	uint8_t timer_ctrl = 0;
	int ret;

	// read control register 2 (0x01H)
	ret = pcf8563_read_reg(dev, PCF8563_REG_ST2, 1, &reg_val);
	if (ret < 0) {
		printk("Failed to read control/status register 2\n");
		return -EIO;
	}


	if (reg_val & PCF8563_CTRL2_AF) {
		//printk("Alarm interrupt triggered (AF set)\n");

		reg_val &= ~PCF8563_CTRL2_AF;
		ret = pcf8563_write_reg(dev, PCF8563_REG_ST2, 1, &reg_val);
		if (ret < 0) {
			printk("Failed to clear AF flag\n");
			return -EIO;
		}
	}

	/* countdown case alerm*/
	if (reg_val & PCF8563_CTRL2_TF) {
		//printk("Timer interrupt triggered (TF set)\n");

		reg_val &= ~PCF8563_CTRL2_TF;
		ret = pcf8563_write_reg(dev, PCF8563_REG_ST2, 1, &reg_val);
		if (ret < 0) {
			printk("Failed to clear TF flag\n");
			return -EIO;
		}

		timer_ctrl = 0;
		ret = pcf8563_write_reg(dev, PCF8563_REG_TIMER_CONTROL, 1, &timer_ctrl);
		if (ret < 0) {
			LOG_ERR("Failed to write timer control register\n");
			return -EIO;
		}

		if (remaining_time_us > 245) {
			printk("Remaining time: %u us\n", remaining_time_us);
			/* set remaining time*/
			pcf85631_rtc_countdown(dev, remaining_time_us);
		} else {
			uint32_t diff_time = (uint32_t)abs(isr_us - alarm_us);
			printk("Countdown completed! %d (%d)\n", diff_time, (int)(diff_time - alarm_time));
		}
	}

	return 0;
}


static int pcf85631_isr_register(struct device *dev)
{
	int ret;
	int fd;
	struct gpio_interrupt_enable rtc_int;
	struct gpio_configure rtc_int_cfg;

	fd = open("/dev/gpio", 0, 0);
	if (fd < 0) {
		return -1;
	}


	rtc_int_cfg.pin = PCF8563_INT_GPIO;
	rtc_int_cfg.property = GPIO_INPUT_PULL_UP;

	ret = ioctl(fd, IOCTL_GPIO_CONFIGURE, &rtc_int_cfg);

	if (ret)
		goto exit;

	rtc_int.pin = PCF8563_INT_GPIO;
	rtc_int.type = GPIO_INTR_FULLING_EDGE;
	rtc_int.intr_cb = (void (*)(u32, void *))pcf85631_isr;
	rtc_int.ctx = dev;

	ret = ioctl(fd, IOCTL_GPIO_INTERRUPT_ENABLE, &rtc_int);

exit:

	close(fd);

	return ret;

}

__maybe_unused static int pcf85631_rtc_set_countdown_freq(struct device *dev, uint8_t freq)
{
	uint8_t buf[1] = {0};

	if (freq > PCF8563_TMRC_1_60) {
		return -1;
	}
	buf[0] = freq;
	return pcf8563_write_reg(dev, PCF8563_REG_TIMER_CONTROL, 1, buf);
}

static void pcf85631_rtc_thread(void *param)
{
	while (1) {
		osSemaphoreAcquire(g_sync_signal, osWaitForever);
		isr_us = tick_to_us(ktime());
		pcf85631_isr_exec();

	}
}

static int pcf85631_rtc_probe(struct device *dev)
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
	i2c.master_clock = 400 * 1000;
	i2c.dma_en = 0;
	i2c.pull_up_en = 1;

	priv->sem = osSemaphoreNew(1, 0, NULL);
	priv->addr = PCF8563_I2C_ADDRESS;

	ret = i2c_configure(priv->bus, &i2c, i2c_notify, priv);

	dev->driver_data = priv;

	ret = pcf85631_isr_register(dev);

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

	if (osThreadNew(pcf85631_rtc_thread, NULL, &attr) == NULL) {
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

static int pcf85631_rtc_get(struct device *dev, struct rtc_time *time)
{
	uint8_t buf[9];
	int ret;

	/* Read seconds */
	ret = pcf8563_read_reg(dev, PCF8563_REG_ST1, 9, buf);
	if (ret < 0) {
		LOG_ERR("Failed to read seconds from RTC\n");
		return -EIO;
	}

	if (buf[PCF8563_REG_SC] & PCF8563_SECONDS_VL) {
		LOG_ERR("low voltage detected, date/time is not reliable.\n");
		return -EINVAL;
	}



	time->tm_sec = bcd2bin(buf[PCF8563_REG_SC] & 0x7F); /* Mask out VL (Voltage Low) flag */
	time->tm_min = bcd2bin(buf[PCF8563_REG_MN] & 0x7F);
	time->tm_hour = bcd2bin(buf[PCF8563_REG_HR] & 0x3F);	// 24-hour format
	time->tm_mday = bcd2bin(buf[PCF8563_REG_DM] & 0x3F);
	time->tm_wday = (buf[PCF8563_REG_DW] & 0x07);  // Day of the week is not in BCD format
	time->tm_mon = bcd2bin(buf[PCF8563_REG_MO] & 0x1F);  // Mask to get month (1-12)
	if (buf[PCF8563_REG_MO] & PCF8563_CENTURY) {
		time->tm_year = 1900;  // If century bit is set, year is 19xx
	} else {
		time->tm_year = 2000;  // Otherwise, year is 20xx
	}
	time->tm_year = (time->tm_year / 100) * 100 + bcd2bin(buf[PCF8563_REG_YR]);  // Combine century with year

	// tm_yday and tm_isdst are not set by the RTC, leave them unset
	time->tm_yday = 0;
	time->tm_isdst = 0;

	return 0;

}

static int pcf85631_rtc_set(struct device *dev, const struct rtc_time *time)
{
	int centry = 1;
	uint8_t buf[9];

	if(time->tm_year >= 2000)
		centry = 0;

	/* hours, minutes and seconds */
	buf[PCF8563_REG_SC] = bin2bcd(time->tm_sec);
	buf[PCF8563_REG_MN] = bin2bcd(time->tm_min);
	buf[PCF8563_REG_HR] = bin2bcd(time->tm_hour);

	buf[PCF8563_REG_DM] = bin2bcd(time->tm_mday);

	/* month, 1 - 12 */
	buf[PCF8563_REG_DW] = bin2bcd(time->tm_wday);

	/* year and century */
	buf[PCF8563_REG_MO] = centry ? PCF8563_CENTURY | bin2bcd(time->tm_mon) : bin2bcd(time->tm_mon);

	buf[PCF8563_REG_YR] = bin2bcd(time->tm_year % 100);


	pcf8563_write_reg(dev, PCF8563_REG_SC, 9 - PCF8563_REG_SC, buf + PCF8563_REG_SC);

	return 0;
}

static int pcf85631_rtc_reset(struct device *dev)
{
	struct rtc_time time;

	memset(&time, 0, sizeof(time));
	time.tm_mday = 1;
	return pcf85631_rtc_set(dev, &time);
}

static int pcf85631_rtc_countdown(struct device *dev, uint32_t duration_us)
{
	uint8_t timer_control;
	uint32_t countdown_value;
	uint32_t clock_frequency;
	uint32_t timer_period;
	int ret;
	uint8_t reg_val;

	if (fix_hz == 0) {

	remaining_time_us = duration_us;

	if (duration_us >= ((1000000 / 64) * 255)) {
		clock_frequency = 1;  // 1 Hz
		timer_control = PCF8563_TMRC_1;
	} else if (duration_us >= ((1000000 / 4096) *255)) {
		clock_frequency = 64;  // 64 Hz
		timer_control = PCF8563_TMRC_64;
	} else {
		clock_frequency = 4096;  // 4096 Hz
		timer_control = PCF8563_TMRC_4096;
	}

	timer_control |= PCF8563_TMRC_ENABLE;
	timer_period = 1000000 / clock_frequency;
	countdown_value = duration_us / timer_period;

	countdown_value = (remaining_time_us > timer_period * 255) ? 255 : remaining_time_us / timer_period;
	remaining_time_us -= countdown_value * timer_period;

	if (countdown_value == 0 || countdown_value > 255) {
		LOG_ERR("Invalid countdown value\n");
		return -EINVAL;
	}

	} else {
		timer_control = PCF8563_TMRC_64;
		timer_control |= PCF8563_TMRC_ENABLE;
		clock_frequency = 64;  // 64 Hz
		timer_period = 1000000 / clock_frequency;
		countdown_value = duration_us / timer_period;
		remaining_time_us = 0;
	}

	ret = pcf8563_write_reg(dev, PCF8563_REG_TIMER_CONTROL, 1, &timer_control);
	if (ret < 0) {
		LOG_ERR("Failed to write timer control register\n");
		return -EIO;
	}

	ret = pcf8563_write_reg(dev, PCF8563_REG_TMR, 1, (uint8_t *) &countdown_value);
	if (ret < 0) {
		LOG_ERR("Failed to write countdown value register\n");
		return -EIO;
	}

	if (alarm_set == 1) {
		alarm_set = 0;
		alarm_us = tick_to_us(ktime());
	}

	reg_val |= PCF8563_CTRL2_TIE;
	ret = pcf8563_write_reg(dev, PCF8563_REG_ST2, 1, &reg_val);
	if (ret) {
		return -EIO;
	}

	return ret;
}


struct rtc_ops pcf85631_rtc_ops = {
	.get = pcf85631_rtc_get,
	.set = pcf85631_rtc_set,
	.reset = pcf85631_rtc_reset,
	.get_32khz_count = NULL,
	.set_32khz_alarm = pcf85631_rtc_countdown,
	.clear_32khz_alarm = NULL
};

static declare_driver(rtc) = {
	.name = "pcf85631lx",
	.probe = pcf85631_rtc_probe,
	.ops = &pcf85631_rtc_ops,
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
	struct device *dev = rtc_get_device();
	struct rtc_time time;

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
	struct device *dev = rtc_get_device();
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
	struct device *dev = rtc_get_device();

	if (!dev) {
		return CMD_RET_FAILURE;
	}

	rtc_reset(dev);

	return CMD_RET_SUCCESS;
}

static int do_rtc_set_countdown(int argc, char *argv[])
{
	struct device *dev = rtc_get_device();
	uint32_t alarm;

	if (argc < 2) {
		return CMD_RET_USAGE;
	}

	alarm = atoi(argv[1]);

	if (argc == 3)
		fix_hz = atoi(argv[2]);

	alarm_time = alarm;
	alarm_set = 1;
	rtc_set_32khz_alarm(dev, alarm);

	printf("after %d(us) alarm\n", alarm);

	return CMD_RET_SUCCESS;
}

static int do_rtc_read(int argc, char *argv[])
{
	struct device *dev = rtc_get_device();
	uint8_t buf[15];
	uint8_t reg, length, i;

	if (argc != 3) {
		return CMD_RET_USAGE;
	}

	reg = strtoul(argv[1], NULL, 0);
	length = strtoul(argv[2], NULL, 0);

	pcf8563_read_reg(dev, reg, length, buf);

	for (i = 0; i < length; i++)
		printf("reg[0x%x] = 0x%01x\n", reg + i, buf[i]);

	return CMD_RET_SUCCESS;
}

static int do_rtc_write(int argc, char *argv[])
{
	struct device *dev = rtc_get_device();
	uint8_t buf[15];
	uint8_t reg, length, i;

	if (argc < 3) {
		return CMD_RET_USAGE;
	}

	reg = atoi(argv[1]);
	length = argc - 2;
	for (i = 0; i < length; i++)
		buf[i] = strtoul(argv[2 + i], NULL, 0);

	pcf8563_write_reg(dev, reg, length, buf);

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

CMD(rtc, do_rtc,
		"test routines for RTC (Real Time Clock)",
		"rtc get" OR
		"rtc set <day> <hour> <min> <sec>" OR
		"rtc reset" OR
		"rtc cd" OR
		"rtc read <reg> <length>" OR
		"rtc write <reg> <val 1> <val 2> ..." OR
   );

#endif
