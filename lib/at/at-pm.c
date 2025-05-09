/*
 * Copyright 2024-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
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

#include "at-sys.h"

#include "cli.h"
#include "hal/pm.h"
#include "hal/pinctrl.h"
#include "sdio/sdioif.h"

#include "wise_wifi.h"

#if defined(CONFIG_ATCMD_AT_PME) && defined(CONFIG_WISE_API_WIFI)

/** Maximum duration for the low power duration */
#define AT_PM_DURATION_MAX	(60 * 60 * 24 * 1000 - 1)

static osTimerId_t pm_timer;

static void at_pm_duration_reached(void *arg)
{
	/* handled from the timer task context */
	printf("PM disabled, user duration\n");
	pm_stay(PM_DEVICE_APP);
}

static int at_pm_enable(int argc, char *argv[])
{
	uint32_t duration = 0;

	if (argc == 4) {
		duration = atoi(argv[AT_CMD_PARAM + 1]);

		printf("%s, dur:%u\n", __func__, duration);
	}

	if (!pm_timer) {
		pm_timer = osTimerNew(at_pm_duration_reached, osTimerOnce, NULL, NULL);
	}

	if (duration > 0) {
		/* duration handled by CLI layer */
		osTimerStart(pm_timer, duration);
	}

	pm_relax(PM_DEVICE_APP);

	return 0;
}

static int at_pm_disable(int argc, char *argv[])
{
	uint32_t duration = 0;

	if (argc == 4) {
		duration = atoi(argv[AT_CMD_PARAM + 1]);
	}

	if (duration > 0) {
		if (duration > AT_PM_DURATION_MAX) {
			return -1;
		}
		pm_staytimeout(duration);
	} else {
		pm_stay(PM_DEVICE_APP);
	}

	return 0;
}

static int at_pm_set(int argc, char *argv[])
{
	int enable = atoi(argv[AT_CMD_PARAM]) ? 1 : 0;

	printf("%s, enable:%d\n", __func__, enable);

	if (enable) {
		at_pm_enable(argc, argv);
	} else {
		at_pm_disable(argc, argv);
	}

	return 0;
}

ATPLUS(PME, NULL, NULL, at_pm_set, NULL);

#endif /* CONFIG_ATCMD_AT_PME */

#if defined(CONFIG_ATCMD_AT_PMEW) && defined(CONFIG_WISE_API_WIFI)

static int at_pm_wifi_set(int argc, char *argv[])
{
	int enable = atoi(argv[AT_CMD_PARAM]) ? WIFI_PS_MAX_MODEM : WIFI_PS_NONE;
	uint32_t sleeptime = 100;
	int ret = -1;

	printf("%s, enable %d\n", __func__, enable);

	ret = wise_wifi_set_ps(enable);

	if (enable && ret == AT_RESULT_CODE_OK) {
		if (argc == 4)
			sleeptime = atoi(argv[AT_CMD_PARAM + 1]);

		if ((sleeptime < IEEE80211_LISTENINT_MINSLEEP) || (sleeptime > IEEE80211_LISTENINT_MAXSLEEP)) {
			printf("station powersave sleep tune must be %d ~ %d.\n",
				IEEE80211_LISTENINT_MINSLEEP, IEEE80211_LISTENINT_MAXSLEEP);
			return AT_RESULT_CODE_ERROR;
		}
		printf("%s, sleeptime %u\n", __func__, sleeptime);

		ret = wise_wifi_set_powersavesleep(sleeptime);
	}

	return ret;
}

ATPLUS(PMEW, NULL, NULL, at_pm_wifi_set, NULL);

#endif /* CONFIG_ATCMD_AT_PMEW */

#if defined(CONFIG_ATCMD_AT_PMR) && defined(CONFIG_WISE_API_WIFI)

static int at_pm_regist_cb(int argc, char *argv[])
{
	int enable = atoi(argv[AT_CMD_PARAM]) ? 1 : 0;

	printf("%s, %s\n", __func__, enable ? "reg" : "unreg");

	if (enable) {
		system("pm reg_cb");
	} else {
		system("pm unreg_cb");
	}

	return 0;
}

ATPLUS(PMR, NULL, NULL, at_pm_regist_cb, NULL);

#endif /* CONFIG_ATCMD_AT_PMEW */

#if defined(CONFIG_ATCMD_AT_PMES) && defined(CONFIG_WISE_API_WIFI)

enum scm_pm_state {
	AT_PM_STATE_ACTIVE      = 0,    /**< ACTIVE - all system components are active */
	AT_PM_STATE_LIGHT_SLEEP = 1,    /**< LIGHT SLEEP - cores are clock gated */
	AT_PM_STATE_DEEP_SLEEP  = 2,    /**< DEEP SLEEP - most components are powered down, XTAL/PLL */
	AT_PM_STATE_HIBERNATION = 3,    /**< HIBERNATION - most power efficient */
};

static const int at_pm_state_to_mode[] = {
	[AT_PM_STATE_ACTIVE]        = PM_MODE_ACTIVE,
	[AT_PM_STATE_LIGHT_SLEEP]   = PM_MODE_LIGHT_SLEEP,
	[AT_PM_STATE_DEEP_SLEEP]    = PM_MODE_DEEP_SLEEP,
	[AT_PM_STATE_HIBERNATION]   = PM_MODE_HIBERNATION,
};

static int at_pm_enable_state(int argc, char *argv[])
{
	uint32_t state;
	uint32_t hib_max;

	if (argc < 4) {
		return -1;
	}

	if (strcmp(argv[AT_CMD_PARAM + 1], "LS") == 0) {
		state = AT_PM_STATE_LIGHT_SLEEP;
	} else if (strcmp(argv[AT_CMD_PARAM + 1], "DS") == 0) {
		state = AT_PM_STATE_DEEP_SLEEP;
	} else if (strcmp(argv[AT_CMD_PARAM + 1], "HI") == 0) {
		state = AT_PM_STATE_HIBERNATION;
		if (argc == 5) {
			hib_max = atoi(argv[AT_CMD_PARAM + 2]);
			pm_set_hib_max_count(hib_max);
		}
	} else {
		return AT_RESULT_CODE_ERROR;
	}

	if ((state < AT_PM_STATE_LIGHT_SLEEP) ||
		(state > AT_PM_STATE_HIBERNATION)) {
		return AT_RESULT_CODE_ERROR;
	}

	printf("%s, state:%d\n", __func__, state);

	pm_enable_mode(at_pm_state_to_mode[state]);

	return AT_RESULT_CODE_OK;
}

static int at_pm_disable_state(int argc, char *argv[])
{
	uint32_t state;

	if (argc != 4) {
		return AT_RESULT_CODE_ERROR;
	}

	if (strcmp(argv[AT_CMD_PARAM + 1], "LS") == 0) {
		state = AT_PM_STATE_LIGHT_SLEEP;
	} else if (strcmp(argv[AT_CMD_PARAM + 1], "DS") == 0) {
		state = AT_PM_STATE_DEEP_SLEEP;
	} else if (strcmp(argv[AT_CMD_PARAM + 1], "HI") == 0) {
		state = AT_PM_STATE_HIBERNATION;
	} else {
		return AT_RESULT_CODE_ERROR;
	}

	/* call API */
	if ((state < AT_PM_STATE_LIGHT_SLEEP) ||
		(state > AT_PM_STATE_HIBERNATION)) {
		return AT_RESULT_CODE_ERROR;
	}

	printf("%s, state:%d\n", __func__, state);

	pm_disable_mode(at_pm_state_to_mode[state]);

	return AT_RESULT_CODE_OK;
}

static int at_pm_state_set(int argc, char *argv[])
{
	int enable = atoi(argv[AT_CMD_PARAM]) ? 1 : 0;

	if (enable) {
		at_pm_enable_state(argc, argv);
	} else {
		at_pm_disable_state(argc, argv);
	}

	return AT_RESULT_CODE_OK;
}

ATPLUS(PMES, NULL, NULL, at_pm_state_set, NULL);

#endif

#if defined(CONFIG_ATCMD_AT_PMEWU) && defined(CONFIG_WISE_API_WIFI)


static void at_pm_gpio_detected(void *arg)
{
	/* handled from the timer task context */
	printf("PM disabled, GPIO\n");
	pm_stay(PM_DEVICE_APP);
}

static void at_pm_gpio_sdio_wakeup_detected(void *arg)
{
	sdio_notify_host_reenum();
}

/**
 * - Any lowpower state maintains WIFI connection.
 * - The lowpower state is automatically selected by the OS based on next idle duration.
 */
enum at_gpio_callback {
	AT_GPIO_DEFAULT			= 0,	/**< default callback */
	AT_GPIO_SDIO_WAKEUP		= 1,	/**< callback for sdio wakeup */
};

void *callback_array[] = {
	[AT_GPIO_DEFAULT] = at_pm_gpio_detected,
	[AT_GPIO_SDIO_WAKEUP] = at_pm_gpio_sdio_wakeup_detected,
};

static int at_pm_wakeup_set(int argc, char *argv[])
{
	int enable = atoi(argv[AT_CMD_PARAM]) ? 1 : 0;

	if (enable) {
		uint8_t gpio = atoi(argv[AT_CMD_PARAM + 1]);
		uint8_t cb = 0;
		uint8_t pull = GPIO_INPUT_PULL_UP;
		uint8_t level = 0;

		if (argc >= 5)
			cb = atoi(argv[AT_CMD_PARAM + 2]);

		if (argc >= 6)
			pull = atoi(argv[AT_CMD_PARAM + 3]);

		if (argc == 7)
			level = atoi(argv[AT_CMD_PARAM + 4]);

		printf("%s,enable gpio:%d cb:%d pull:%d level:%d\n", __func__, gpio, cb, pull, level);

		return at_pm_enable_wakeup_gpio(gpio, pull, level, callback_array[cb]);
	} else {
		uint8_t gpio;

		gpio = atoi(argv[AT_CMD_PARAM + 1]);

		printf("%s,disable gpio:%d\n", __func__, gpio);

		return at_pm_disable_wakeup_gpio(gpio);
	}

	return AT_RESULT_CODE_OK;
}

ATPLUS(PMEWU, NULL, NULL, at_pm_wakeup_set, NULL);

#endif
