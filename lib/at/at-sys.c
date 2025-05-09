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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>

#include <at.h>
#include <wise_system.h>
#include <sys/termios.h>
#include <sys/ioctl.h>

#include "cli.h"
#include "hal/pm.h"
#include "hal/kernel.h"
#include "hal/pinctrl.h"
#include "cmsis_os.h"
#include "wise_wifi.h"
#include "at-sys.h"


#ifdef CONFIG_ATCMD_AT
static int at_exec(int argc, char *argv[])
{
	return AT_RESULT_CODE_OK;
}
AT(AT, NULL, NULL, NULL, at_exec);
#endif /* CONFIG_ATCMD_AT */

#ifdef CONFIG_ATCMD_AT_RST
static int at_rst_exec(int argc, char *argv[])
{
	wise_restart(); /* it actually will not return */
	return AT_RESULT_CODE_OK;
}
ATPLUS(RST, NULL, NULL, NULL, at_rst_exec);
#endif /* CONFIG_ATCMD_AT_RST */

#ifdef CONFIG_ATCMD_AT_CMD
static int at_cmd_query(int argc, char *argv[])
{
	const struct at_cmd *start, *end, *t;
	int nr, i;

	start = at_cmd_start();
	end = at_cmd_end();

	nr = end - start;

	for (t = start, i = 0; t < start + nr; t++, i++) {
		at_printf("%s:%d,%s,%d,%d,%d,%d\r\n",
					argv[AT_CMD_NAME],
					i,
					t->name,
					t->handler[CAT_TST]?1:0,
					t->handler[CAT_QRY]?1:0,
					t->handler[CAT_SET]?1:0,
					t->handler[CAT_EXE]?1:0);
	}

	return AT_RESULT_CODE_OK;
}
ATPLUS(CMD, NULL, at_cmd_query, NULL, NULL);
#endif /* CONFIG_ATCMD_AT_RST */

#ifdef CONFIG_ATCMD_ATE
static int ate_echo_on(int argc, char *argv[])
{
	at_echo(1);
	return AT_RESULT_CODE_OK;
}

static int ate_echo_off(int argc, char *argv[])
{
	at_echo(0);
	return AT_RESULT_CODE_OK;
}
AT(ATE0, NULL, NULL, NULL, ate_echo_off);
AT(ATE1, NULL, NULL, NULL, ate_echo_on);
#endif /* CONFIG_ATCMD_ATE */

#ifdef CONFIG_ATCMD_AT_UART_CUR
static int at_uart_cur_query(int argc, char *argv[])
{
	struct termios termios;
	int speed, dbits, sbits, par, flow;

	ioctl(at_term_fd(), TCGETS, &termios);
	speed = termios.c_ispeed;
	switch (termios.c_cflag & CSIZE) {
	case CS5:
		dbits = 5;
		break;
	case CS6:
		dbits = 6;
		break;
	case CS7:
		dbits = 7;
		break;
	default:
		dbits = 8;
		break;
	}
	if (termios.c_cflag & CSTOPB) {
		if (dbits == 5)
			sbits = 2;
		else
			sbits = 3;
	} else
		sbits = 1;
	if (termios.c_cflag & PARENB)
		par = 1;
	else
		par = 0;
	switch (termios.c_cflag & CRTSCTS) {
	case CRTS_IFLOW:
		flow = 1;
		break;
	case CCTS_OFLOW:
		flow = 2;
		break;
	case CRTSCTS:
		flow = 3;
		break;
	default:
		flow = 0;
		break;
	}
	at_printf("%s:%d,%d,%d,%d,%d\r\n", argv[AT_CMD_NAME], speed, dbits, sbits, par, flow);
	return AT_RESULT_CODE_OK;
}

static int at_uart_cur_set(int argc, char *argv[])
{
	struct termios termios;
	int speed, dbits, sbits, par, flow;

	speed = atoi(argv[2]);
	dbits = atoi(argv[3]);
	sbits = atoi(argv[4]);
	par = atoi(argv[5]);
	flow = atoi(argv[6]);

	if (ioctl(at_term_fd(), TCGETS, &termios))
		goto fail;

	termios.c_ispeed = speed;
	termios.c_cflag &= ~CSIZE;
	switch (dbits) {
	case 5:
		termios.c_cflag |= CS5;
		break;
	case 6:
		termios.c_cflag |= CS6;
		break;
	case 7:
		termios.c_cflag |= CS7;
		break;
	default:
		termios.c_cflag |= CS8;
		break;
	}

	if (sbits > 1)
		termios.c_cflag |= CSTOPB;
	else
		termios.c_cflag &= ~CSTOPB;

	termios.c_cflag &= ~(PARENB | PARODD);
	if (par) {
		termios.c_cflag |= PARENB;
		if (par == 1)
			termios.c_cflag |= PARODD;
	}

	termios.c_cflag &= ~CRTSCTS;
	if (flow & 1)
		termios.c_cflag |= CRTS_IFLOW;
	if (flow & 2)
		termios.c_cflag |= CCTS_OFLOW;

	if (ioctl(at_term_fd(), TCSETS, &termios))
		goto fail;

	return AT_RESULT_CODE_OK;

fail:
	return AT_RESULT_CODE_ERROR;
}
ATPLUS(UART_CUR, NULL, at_uart_cur_query, at_uart_cur_set, NULL);
#endif /* CONFIG_ATCMD_AT_UART_CUR */

#ifdef CONFIG_ATCMD_AT_SYSRAM
static int at_sysram_query(int argc, char *argv[])
{
	at_printf("%s:%d\r\n", argv[AT_CMD_NAME], wise_get_free_heap_size());
	return AT_RESULT_CODE_OK;
}
ATPLUS(SYSRAM, NULL, at_sysram_query, NULL, NULL);
#endif /* CONFIG_ATCMD_AT_SYSRAM */

#ifdef CONFIG_PM_SCM2010

static osTimerId_t at_pm_timer;
static osTimerId_t gpio_timer;

enum at_pm_state {
	AT_PM_STATE_ACTIVE			= 0,	/**< ACTIVE - all system components are active */
	AT_PM_STATE_LIGHT_SLEEP		= 1,	/**< LIGHT SLEEP - cores are clock gated */
	AT_PM_STATE_DEEP_SLEEP		= 2,	/**< DEEP SLEEP - most components are powered down, XTAL/PLL */
	AT_PM_STATE_HIBERNATION		= 3,	/**< HIBERNATION - most power efficient */
	AT_PM_STATE_MAX = AT_PM_STATE_HIBERNATION,
};

static const int at_pm_state_to_mode[] = {
	[AT_PM_STATE_ACTIVE]		= PM_MODE_ACTIVE,
	[AT_PM_STATE_LIGHT_SLEEP]	= PM_MODE_LIGHT_SLEEP,
	[AT_PM_STATE_DEEP_SLEEP]	= PM_MODE_DEEP_SLEEP,
	[AT_PM_STATE_HIBERNATION]	= PM_MODE_HIBERNATION,
};

enum at_pm_wk_source {
	AT_PM_WK_RSV1		= 0,	/**< reserved */
	AT_PM_WK_RSV2		= 1,	/**< reserved */
	AT_PM_WK_GPIO		= 2,	/**< wakeup by GPIO */
	AT_PM_WK_MAX = AT_PM_WK_GPIO,
};

static void at_pm_clear_state(void)
{
	pm_disable_mode(at_pm_state_to_mode[AT_PM_STATE_LIGHT_SLEEP]);
	pm_disable_mode(at_pm_state_to_mode[AT_PM_STATE_DEEP_SLEEP]);
	pm_disable_mode(at_pm_state_to_mode[AT_PM_STATE_HIBERNATION]);
}

#if defined(CONFIG_ATCMD_AT_SLEEP) && defined(CONFIG_CMDLINE)
static int at_sleep_query(int argc, char *argv[])
{
	int state = AT_PM_STATE_ACTIVE;

	/* Shows the highest PM mode */
	if (pm_query_mode() >> PM_MODE_HIBERNATION)
		state = AT_PM_STATE_HIBERNATION;
	else if (pm_query_mode() >> PM_MODE_DEEP_SLEEP)
		state = AT_PM_STATE_DEEP_SLEEP;
	else if (pm_query_mode() >> PM_MODE_LIGHT_SLEEP)
		state = AT_PM_STATE_LIGHT_SLEEP;

	at_printf("%s:%d\r\n", argv[AT_CMD_NAME], state);
	return AT_RESULT_CODE_OK;
}

static int at_sleep_set(int argc, char *argv[])
{
	int state = atoi(argv[2]);

	if (state > AT_PM_STATE_MAX)
		return AT_RESULT_CODE_ERROR;

	/* First disable all mode for PM */
	at_pm_clear_state();

	/* Enable PM mode by input */
	pm_enable_mode(at_pm_state_to_mode[state]);

	return AT_RESULT_CODE_OK;
}
ATPLUS(SLEEP, NULL, at_sleep_query, at_sleep_set, NULL);
#endif

#if defined(CONFIG_ATCMD_AT_SLEEPWKCFG) && defined(CONFIG_CMDLINE)

static const int at_pm_gpio_mode[] = {
	[AT_GPIO_PROP_INPUT_PULL_UP]	= GPIO_INPUT_PULL_UP,
	[AT_GPIO_PROP_INPUT_PULL_DOWN]	= GPIO_INPUT_PULL_DOWN,
};

#define GPIO_AON_MAX	7

static void at_pm_gpio_detected(void *arg)
{
	/* handled from the timer task context */
#if 1
	printf("PM disabled, GPIO\n");
#endif
	pm_stay(PM_DEVICE_APP);
}

static int at_pm_gpio_notify(uint32_t pin, void *ctx)
{
	/* start the gpio timer to expire right away */
	osTimerStart(gpio_timer, 1);

	return 0;
}

int at_gpio_write(uint32_t pin, uint8_t value)
{
	struct gpio_write_value arg;
	int fd;
	int ret;

	fd = open("/dev/gpio", 0, 0);
	if (fd < 0) {
		return AT_RESULT_CODE_ERROR;
	}

	arg.pin = pin;
	arg.value = value;

	ret = ioctl(fd, IOCTL_GPIO_WRITE, &arg);

	close(fd);

	if (ret) {
		return AT_RESULT_CODE_ERROR;
	}
	return AT_RESULT_CODE_OK;
}

int at_gpio_configure(uint32_t pin, enum gpio_property property)
{
	struct gpio_configure arg;
	int fd;
	int ret;

	fd = open("/dev/gpio", 0, 0);
	if (fd < 0) {
		return AT_RESULT_CODE_ERROR;
	}

	arg.pin = pin;
	arg.property = property;

	ret = ioctl(fd, IOCTL_GPIO_CONFIGURE, &arg);

	close(fd);

	if (ret) {
		return AT_RESULT_CODE_ERROR;
	}
	return AT_RESULT_CODE_OK;
}

int at_gpio_enable_interrupt(uint32_t pin, enum gpio_intr_type type, void *notify, void *ctx)
{
	struct gpio_interrupt_enable arg;
	int fd;
	int ret;

	fd = open("/dev/gpio", 0, 0);
	if (fd < 0) {
		return AT_RESULT_CODE_ERROR;
	}

	arg.pin = pin;
	arg.type = type;
	arg.intr_cb = (void (*)(u32, void *))notify;
	arg.ctx = ctx;

	ret = ioctl(fd, IOCTL_GPIO_INTERRUPT_ENABLE, &arg);

	close(fd);

	if (ret) {
		return AT_RESULT_CODE_ERROR;
	}
	return AT_RESULT_CODE_OK;
}

int at_gpio_disable_interrupt(uint32_t pin)
{
	struct gpio_interrupt_disable arg;
	int fd;
	int ret;

	fd = open("/dev/gpio", 0, 0);
	if (fd < 0) {
		return AT_RESULT_CODE_ERROR;
	}

	arg.pin = pin;

	ret = ioctl(fd, IOCTL_GPIO_INTERRUPT_DISABLE, &arg);

	close(fd);

	if (ret) {
		return AT_RESULT_CODE_ERROR;
	}
	return AT_RESULT_CODE_OK;
}

static int at_gpio_set_level(int gpio, u8 level)
{
	printf("GPIO: set pin=%d, level=%d\n", gpio, level);
	return at_gpio_write((uint32_t)gpio, level);
}

static int at_pm_enable_output_gpio(uint8_t gpio, uint8_t level)
{
	int ret;

	if (gpio > GPIO_AON_MAX)
		return AT_RESULT_CODE_ERROR;

	if (level > 1) {
		return AT_RESULT_CODE_ERROR;
	}

	/* configure gpio */
	ret = at_gpio_configure(gpio, GPIO_OUTPUT);

	if (ret != AT_RESULT_CODE_OK) {
#if 0
		printf("at_gpio_configure fail\n");
#endif
		goto done;
	}

	ret = at_gpio_set_level(gpio, level);

	if (ret != AT_RESULT_CODE_OK) {
#if 0
		printf("at_gpio_configure fail\n");
#endif
		goto done;
	}

	/* call API */
	pm_enable_wakeup_io(gpio);

done:
	return ret;
}

int at_pm_disable_output_gpio(uint8_t gpio)
{
	if (gpio > GPIO_AON_MAX) {
		return AT_RESULT_CODE_ERROR;
	}

	pm_disable_wakeup_io(gpio);

	return AT_RESULT_CODE_OK;
}

int at_pm_enable_wakeup_gpio(uint8_t gpio, uint8_t pull, uint8_t level, void *cb)
{
	int ret;

	if (gpio > GPIO_AON_MAX)
		return AT_RESULT_CODE_ERROR;

	if (pull > GPIO_INPUT_PULL_DOWN) {
		return AT_RESULT_CODE_ERROR;
	}

	if (pull > GPIO_OUTPUT) {
		/* pull only set timer and interrupt for input */
		if (!gpio_timer) {
			gpio_timer = osTimerNew(cb, osTimerOnce, NULL, NULL);
		}

		/* configure gpio */
		ret = at_gpio_configure(gpio, pull);
		if (ret != AT_RESULT_CODE_OK) {
#if 0
			printf("at_gpio_configure fail\n");
#endif
			goto done;
		}

		ret = at_gpio_enable_interrupt(gpio, GPIO_INTR_FULLING_EDGE, at_pm_gpio_notify, NULL);
		if (ret != AT_RESULT_CODE_OK) {
#if 0
			printf("at_gpio_configure fail\n");
#endif
			goto done;
		}
	} else{
		/* set to output case */
		printf("output: pin %d lv %d\n", gpio, level);
		at_pm_enable_output_gpio(gpio, level);
	}

	/* call API */
	pm_enable_wakeup_io(gpio);

done:
	return ret;
}

int at_pm_disable_wakeup_gpio(uint8_t gpio)
{
	if (gpio > GPIO_AON_MAX) {
		return AT_RESULT_CODE_ERROR;
	}

	at_gpio_disable_interrupt(gpio);

	pm_disable_wakeup_io(gpio);

	return AT_RESULT_CODE_OK;
}

static int at_sleepwkcfg_set(int argc, char *argv[])
{
	int source = atoi(argv[2]);
	int number = atoi(argv[3]);

	if (source > AT_PM_WK_MAX)
		return AT_RESULT_CODE_ERROR;

	switch (source) {
		case AT_PM_WK_GPIO:
		{
			uint8_t gpio = number;
			uint8_t pull = AT_GPIO_PROP_INPUT_PULL_UP;

			if (argc == 5)
				pull = atoi(argv[4]);

			pull = at_pm_gpio_mode[pull];

#if 0
			printf("gpio: %d, pull: %d\n", gpio, pull);
#endif

			return at_pm_enable_wakeup_gpio(gpio, pull, 0, at_pm_gpio_detected);
		}

		case AT_PM_WK_RSV1:
		case AT_PM_WK_RSV2:
		default:
			break;
	}

	return AT_RESULT_CODE_ERROR;
}
ATPLUS(SLEEPWKCFG, NULL, NULL, at_sleepwkcfg_set, NULL);
#endif

#if defined(CONFIG_ATCMD_AT_GSLP) && defined(CONFIG_CMDLINE)

static void at_pm_duration_reached(void *arg)
{
	/* handled from the timer task context */
#if 0
	printf("PM disabled, user duration\n");
#endif
	pm_stay(PM_DEVICE_APP);
}

static int at_gslp_set(int argc, char *argv[])
{
	int enable = WIFI_PS_MAX_MODEM;
	int duration = atoi(argv[AT_CMD_PARAM]);

#if 0 /* comment it to let GSLP goes to PM by enabled mode */
	/* First disable all mode for PM */
	at_pm_clear_state();

	/* Enable PM mode by input */
	pm_enable_mode(at_pm_state_to_mode[AT_PM_STATE_DEEP_SLEEP]);
#endif

	wise_wifi_set_ps(enable);

	if (!at_pm_timer) {
		at_pm_timer = osTimerNew(at_pm_duration_reached, osTimerOnce, NULL, NULL);
	}

	if (duration > 0) {
		/* duration handled by CLI layer */
		osTimerStart(at_pm_timer, duration);
	}

	/* call API */
	pm_relax(PM_DEVICE_APP);

	return AT_RESULT_CODE_OK;
}
ATPLUS(GSLP, NULL, NULL, at_gslp_set, NULL);
#endif /* CONFIG_ATCMD_AT_GSLP */
#endif /* CONFIG_PM_SCM2010 */
