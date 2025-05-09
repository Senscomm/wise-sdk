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

/*
 * This sample demonstrate IR signal generate using timer PWM and one-shot timer.
 *
 * [Hardware Setup]
 *
 * GPIO 15 is PWM output.
 */

#include <stdio.h>
#include <stdlib.h>

#include "hal/kernel.h"
#include "hal/console.h"
#include "cli.h"
#include "scm_timer.h"

#define TIMER_ID				SCM_TIMER_IDX_0

#define TIMER_PWM_CH			SCM_TIMER_CH_0
#define TIMER_PERIOD_CH			SCM_TIMER_CH_1

#define TIMER_PWM_OFF_OFFSET	63 //usec
#define TIMER_PWM_ON_OFFSET		37 //usec

/*
#define ENABLE_GPIO_DEBUGGING
*/

#ifdef ENABLE_GPIO_DEBUGGING
#include "scm_gpio.h"
#define DEBUG_GPIO              (16)
#endif

struct timer_pwm_ir_info {
    int pwm_turn_on;
    struct scm_timer_cfg period_cfg;
    int on_dur;
    int off_dur;
};

struct timer_pwm_ir_info g_info;

static int scm_cli_oneshot_expire(enum scm_timer_event_type type, void *ctx)
{
    struct timer_pwm_ir_info *info = ctx;

    if (info->pwm_turn_on) {
        info->pwm_turn_on = 0;
        info->period_cfg.data.oneshot.duration = (info->off_dur);
        scm_timer_configure(TIMER_ID, TIMER_PERIOD_CH, &info->period_cfg, scm_cli_oneshot_expire, info);
        scm_timer_start(TIMER_ID, TIMER_PERIOD_CH);
        scm_timer_stop(TIMER_ID, TIMER_PWM_CH);
#ifdef ENABLE_GPIO_DEBUGGING
        scm_gpio_write(DEBUG_GPIO, 1);
#endif
    } else {
        info->pwm_turn_on = 1;
        info->period_cfg.data.oneshot.duration = (info->on_dur);
        scm_timer_configure(TIMER_ID, TIMER_PERIOD_CH, &info->period_cfg, scm_cli_oneshot_expire, info);
        scm_timer_start(TIMER_ID, TIMER_PERIOD_CH);
        scm_timer_start(TIMER_ID, TIMER_PWM_CH);
#ifdef ENABLE_GPIO_DEBUGGING
        scm_gpio_write(DEBUG_GPIO, 0);
#endif
    }

    return 0;
}

static int scm_cli_timer_pwm_ir_start(int argc, char *argv[])
{
    struct scm_timer_cfg pwm_cfg;

    if (argc < 3) {
        return CMD_RET_USAGE;
    }

    pwm_cfg.mode = SCM_TIMER_MODE_PWM;
    pwm_cfg.intr_en = 0;

    /* Setting 38Khz */
    pwm_cfg.data.pwm.high = 13;
    pwm_cfg.data.pwm.low = 13;
    pwm_cfg.data.pwm.park = 0;

    scm_timer_configure(TIMER_ID, TIMER_PWM_CH, &pwm_cfg, NULL, NULL);

    g_info.on_dur = atoi(argv[1]) - TIMER_PWM_ON_OFFSET;
    g_info.off_dur = atoi(argv[2]) - TIMER_PWM_OFF_OFFSET;

    g_info.period_cfg.mode = SCM_TIMER_MODE_ONESHOT;
    g_info.period_cfg.intr_en = 1;
    g_info.period_cfg.data.oneshot.duration = g_info.on_dur;

    g_info.pwm_turn_on = 1;

    scm_timer_configure(TIMER_ID, TIMER_PERIOD_CH, &g_info.period_cfg, scm_cli_oneshot_expire, &g_info);

#ifdef ENABLE_GPIO_DEBUGGING
    scm_gpio_configure(DEBUG_GPIO, SCM_GPIO_PROP_OUTPUT);
    scm_gpio_write(DEBUG_GPIO, 1);
#endif

    scm_timer_start(TIMER_ID, TIMER_PERIOD_CH);
    scm_timer_start(TIMER_ID, TIMER_PWM_CH);

    return CMD_RET_SUCCESS;
}

#ifdef CONFIG_CMDLINE

#include "cli.h"

static int scm_cli_timer_pwm_ir_stop(int argc, char *argv[])
{
    scm_timer_stop(TIMER_ID, TIMER_PERIOD_CH);
    scm_timer_stop(TIMER_ID, TIMER_PWM_CH);

    return CMD_RET_SUCCESS;
}

static const struct cli_cmd scm_timer_pwm_ir_cmd[] = {
    CMDENTRY(start, scm_cli_timer_pwm_ir_start, "", ""),
    CMDENTRY(stop, scm_cli_timer_pwm_ir_stop, "", ""),
};

static int do_scm_timer_pwm_ir(int argc, char *argv[])
{
    const struct cli_cmd *cmd;

    argc--;
    argv++;

    cmd = cli_find_cmd(argv[0], scm_timer_pwm_ir_cmd,
            ARRAY_SIZE(scm_timer_pwm_ir_cmd));
    if (cmd == NULL) {
        return CMD_RET_USAGE;
    }

    return cmd->handler(argc, argv);
}

CMD(ir, do_scm_timer_pwm_ir,
        "CLI for TIMER PWM IR test",
        "ir start <on(us)> <off(us)>" OR
        "ir stop"
   );

#endif
