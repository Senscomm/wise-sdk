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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "hal/kernel.h"
#include "cli.h"
#include "scm_pm.h"
#include "scm_gpio.h"
#include "cmsis_os.h"

static osTimerId_t pm_timer;
static osTimerId_t gpio_timer;

#ifdef CONFIG_SERIAL_CONSOLE
/*
 * This is only to demonstrate the pm state changes with console.
 * PM state changes are notified with the following conditions
 * - invoked from the IDLE task context
 * - global interrupt disabled
 * Here, we make use of the UART register directly to print the state changes.
 * Real application should replace them with a meaningful work.
 */
static void pm_log_state_change(enum scm_pm_state state)
{
	#if (CONFIG_SERIAL_CONSOLE_PORT == 0)
	#define UART_BASE	UART0_BASE_ADDR
	#elif (CONFIG_SERIAL_CONSOLE_PORT == 1)
	#define UART_BASE	UART1_BASE_ADDR
	#endif

	static const char *name[] = {
		[SCM_PM_STATE_ACTIVE]		= "ACTIVE     ",
		[SCM_PM_STATE_LIGHT_SLEEP]	= "LIGHT_SLEEP",
		[SCM_PM_STATE_DEEP_SLEEP]	= "DEEP_SLEEP ",
		[SCM_PM_STATE_HIBERNATION]	= "HIBERNATION",
	};
	int len = strlen(name[0]);
	int i;

	for (i = 0; i < len; i++) {
		writel(name[state][i], UART_BASE + 0x20);
	}
	writel('\n', UART_BASE + 0x20);

	while(1) {
		if((readl(UART_BASE + 0x34) & 0x60) == 0x60) {
			break;
		}
	}
}
#endif

static void pm_duration_reached(void *arg)
{
	/* handled from the timer task context */
	printf("PM disabled, user duration\n");
	scm_pm_disable_lowpower();
}

static void pm_gpio_detected(void *arg)
{
	/* handled from the timer task context */
	printf("PM disabled, GPIO\n");
	scm_pm_disable_lowpower();
}

static int scm_cli_pm_gpio_notify(uint32_t pin, void *ctx)
{
	/* start the gpio timer to expire right away */
	osTimerStart(gpio_timer, 1);

	return 0;
}

static void scm_cli_pm_notify(enum scm_pm_state state)
{
	switch (state) {
		case SCM_PM_STATE_ACTIVE:
		case SCM_PM_STATE_LIGHT_SLEEP:
		case SCM_PM_STATE_DEEP_SLEEP:
		case SCM_PM_STATE_HIBERNATION:
#ifdef CONFIG_SERIAL_CONSOLE
			pm_log_state_change(state);
#endif
			break;
		default:
			break;
	}
}

static int scm_cli_pm_power_down(int argc, char *argv[])
{
	uint32_t duration;

	if (argc != 2) {
		return CMD_RET_USAGE;
	}

	duration = atoi(argv[1]);

	/* call API */
	scm_pm_power_down(duration);

	return 0;
}

static int scm_cli_pm_enable_wakeup(int argc, char *argv[])
{
	uint8_t gpio;
	uint8_t pull = SCM_GPIO_PROP_INPUT_PULL_UP;

	if (argc < 2) {
		return CMD_RET_USAGE;
	}

	gpio = atoi(argv[1]);

	if (gpio > 7) {
		return CMD_RET_USAGE;
	}

	if (argc == 3) {
		pull = atoi(argv[2]);
	}

	if (pull < SCM_GPIO_PROP_INPUT || pull > SCM_GPIO_PROP_INPUT_PULL_DOWN) {
		return CMD_RET_USAGE;
	}

	if (!gpio_timer) {
		gpio_timer = osTimerNew(pm_gpio_detected, osTimerOnce, NULL, NULL);
	}

	/* configure gpio */
	scm_gpio_configure(gpio, pull);
	scm_gpio_enable_interrupt(gpio, SCM_GPIO_INT_BOTH_EDGE, scm_cli_pm_gpio_notify, NULL);

	/* call API */
	scm_pm_enable_wakeup_io(gpio);

	return 0;
}

static int scm_cli_pm_disable_wakeup(int argc, char *argv[])
{
	uint8_t gpio;

	if (argc != 2) {
		return CMD_RET_USAGE;
	}

	gpio = atoi(argv[1]);

	scm_gpio_disable_interrupt(gpio);

	/* call API */
	scm_pm_disable_wakeup_io(gpio);

	return 0;
}

static int scm_cli_pm_enable(int argc, char *argv[])
{
	uint32_t duration = 0;

	if (argc == 2) {
		duration = atoi(argv[1]);
	}

	if (!pm_timer) {
		pm_timer = osTimerNew(pm_duration_reached, osTimerOnce, NULL, NULL);
	}

	if (duration > 0) {
		/* duration handled by CLI layer */
		osTimerStart(pm_timer, duration);
	}

	/* call API */
	scm_pm_enable_lowpower();

	return 0;
}

static int scm_cli_pm_disable(int argc, char *argv[])
{
	uint32_t duration = 0;

	if (argc == 2) {
		duration = atoi(argv[1]);
	}

	if (duration > 0) {
		/* call API: PM support internal timeout */
		scm_pm_disable_lowpower_timeout(duration);
	} else {
		/* call API */
		scm_pm_disable_lowpower();
	}

	return 0;
}

static int scm_cli_pm_enable_state(int argc, char *argv[])
{
	uint32_t state;
	uint32_t hib_max;

	if (argc < 2) {
		return CMD_RET_USAGE;
	}

	if (strcmp(argv[1], "LS") == 0) {
		state = SCM_PM_STATE_LIGHT_SLEEP;
	} else if (strcmp(argv[1], "DS") == 0) {
		state = SCM_PM_STATE_DEEP_SLEEP;
	} else if (strcmp(argv[1], "HI") == 0) {
		state = SCM_PM_STATE_HIBERNATION;
		if (argc == 3) {
			hib_max = atoi(argv[2]);
			scm_pm_set_hib_max_count(hib_max);
		}
	} else {
		return CMD_RET_USAGE;
	}

	/* call API */
	scm_pm_enable_state(state);

	return 0;
}

static int scm_cli_pm_disable_state(int argc, char *argv[])
{
	uint32_t state;

	if (argc != 2) {
		return CMD_RET_USAGE;
	}

	if (strcmp(argv[1], "LS") == 0) {
		state = SCM_PM_STATE_LIGHT_SLEEP;
	} else if (strcmp(argv[1], "DS") == 0) {
		state = SCM_PM_STATE_DEEP_SLEEP;
	} else if (strcmp(argv[1], "HI") == 0) {
		state = SCM_PM_STATE_HIBERNATION;
	} else {
		return CMD_RET_USAGE;
	}

	/* call API */
	scm_pm_disable_state(state);

	return 0;
}

static int scm_cli_pm_reg_handler(int argc, char *argv[])
{
	/* call API */
	scm_pm_register_handler(scm_cli_pm_notify);

	return 0;
}

static int scm_cli_pm_unreg_handler(int argc, char *argv[])
{
	/* call API */
	scm_pm_unregister_handler(scm_cli_pm_notify);

	return 0;
}

static const struct cli_cmd scm_cli_pm_cmd[] = {
	CMDENTRY(down, scm_cli_pm_power_down, "", ""),
	CMDENTRY(enable_wakeup, scm_cli_pm_enable_wakeup, "", ""),
	CMDENTRY(disable_wakeup, scm_cli_pm_disable_wakeup, "", ""),
	CMDENTRY(enable, scm_cli_pm_enable, "", ""),
	CMDENTRY(disable, scm_cli_pm_disable, "", ""),
	CMDENTRY(enable_state, scm_cli_pm_enable_state, "", ""),
	CMDENTRY(disable_state, scm_cli_pm_disable_state, "", ""),
	CMDENTRY(reg_cb, scm_cli_pm_reg_handler, "", ""),
	CMDENTRY(unreg_cb, scm_cli_pm_unreg_handler, "", ""),
};

static int do_scm_cli_pm(int argc, char *argv[])
{
	const struct cli_cmd *cmd;

	argc--;
	argv++;

	cmd = cli_find_cmd(argv[0], scm_cli_pm_cmd, ARRAY_SIZE(scm_cli_pm_cmd));
	if (cmd == NULL)
		return CMD_RET_USAGE;

	return cmd->handler(argc, argv);
}

CMD(pm, do_scm_cli_pm,
	"CLI for PM API test",
	"pm down [duration msec]" OR
	"pm enable_wakeup [gpio pin]" OR
	"pm disable_wakeup [gpio pin]" OR
	"pm enable [duration msec]" OR
	"pm disable [duration msec]" OR
	"pm enable_state [state, LS:light sleep, DS:deep sleep, HI:hibernation] [HIB max count]" OR
	"pm disable_state [state, LS:light sleep, DS:deep sleep, HI:hibernation]" OR
	"pm reg_cb" OR
	"pm unreg_cb" OR
);
