/*
 * Copyright 2012-2015 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <semphr.h>
#include <timers.h>

#include <ayla/assert.h>
#include <ayla/utypes.h>
#include <ada/err.h>
#include <ayla/tlv.h>
#include <ayla/conf_token.h>
#include <ayla/conf.h>
#include <ayla/clock.h>
#include <ayla/log.h>

#include "color_format.h"
#include "led_widget.h"
#include "lighting_mgr.h"
#include "lighting_ctrl.h"

#include "ftm.h"
#include "ftm_bulb.h"

static void ftm_bulb_test_done(void)
{
    ftm_bulb = 0;
    al_persist_data_write(AL_PERSIST_FACTORY, "ftm/ftm_bulb", &ftm_bulb, sizeof(ftm_bulb));
}

static void ftm_color_bulb_fade(bool timeout)
{
    int i;
    struct app_event evt[] = {
        /* 0% Green */
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Color,
                .value = 1280,
            }
        },
        {
            .type = kEventType_Timer,
            .timer_event = {
                .value = 200,
            }
        },
        /* 10% Green */
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Color,
                .value = 7680,
            }
        },
        {
            .type = kEventType_Timer,
            .timer_event = {
                .value = 200,
            }
        },
        /* 20% Green */
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Color,
                .value = 14080,
            }
        },
        {
            .type = kEventType_Timer,
            .timer_event = {
                .value = 200,
            }
        },
        /* 30% Green */
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Color,
                .value = 20480,
            }
        },
        {
            .type = kEventType_Timer,
            .timer_event = {
                .value = 200,
            }
        },
        /* 40% Green */
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Color,
                .value = 26880,
            }
        },
        {
            .type = kEventType_Timer,
            .timer_event = {
                .value = 200,
            }
        },
        /* 50% Green */
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Color,
                .value = 33280,
            }
        },
        {
            .type = kEventType_Timer,
            .timer_event = {
                .value = 200,
            }
        },
        /* 60% Green */
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Color,
                .value = 39680,
            }
        },
        {
            .type = kEventType_Timer,
            .timer_event = {
                .value = 200,
            }
        },
        /* 70% Green */
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Color,
                .value = 46080,
            }
        },
        {
            .type = kEventType_Timer,
            .timer_event = {
                .value = 200,
            }
        },
        /* 80% Green */
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Color,
                .value = 52480,
            }
        },
        {
            .type = kEventType_Timer,
            .timer_event = {
                .value = 200,
            }
        },
        /* 90% Green */
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Color,
                .value = 58880,
            }
        },
        {
            .type = kEventType_Timer,
            .timer_event = {
                .value = 200,
            }
        },
        /* 100% Green */
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Color,
                .value = 65280,
            }
        },
        {
            .type = kEventType_Timer,
            .timer_event = {
                .value = 200,
            }
        },
    };

    lighting_ctrl_terminate(false, false);
    for (i = 0; i < sizeof(evt) / sizeof(evt[0]); i++) {
        lighting_ctrl_add_event(&evt[i]);
    }
    lighting_ctrl_run(-1, 0, NULL);

    ftm_bulb_test_done();

    log_put(LOG_ERR "[%s]", __func__);
}

static void ftm_color_bulb_aging(bool timeout)
{
    int i;
    struct app_event evt[] = {
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_On,
                .value = 1,
            }
        },
        /* 100% CW 8m */
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Mode,
                .value = 0,
            }
        },
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Level,
                .value = 254,
            }
        },
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Temp,
                .value = 100,
            }
        },
        {
            .type = kEventType_Timer,
            .timer_event = {
                .value = 480 * 1000,
            }
        },
        /* 100% WW 8m */
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Temp,
                .value = 1,
            }
        },
        {
            .type = kEventType_Timer,
            .timer_event = {
                .value = 480 * 1000,
            }
        },
        /* 100% Red 8m */
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Mode,
                .value = 1,
            }
        },
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Color,
                .value = 16711680,
            }
        },
        {
            .type = kEventType_Timer,
            .timer_event = {
                .value = 480 * 1000,
            }
        },
        /* 100% Green 8m */
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Color,
                .value = 65280,
            }
        },
        {
            .type = kEventType_Timer,
            .timer_event = {
                .value = 480 * 1000,
            }
        },
        /* 100% Blue 8m */
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Color,
                .value = 255,
            }
        },
        {
            .type = kEventType_Timer,
            .timer_event = {
                .value = 480 * 1000,
            }
        },
    };

    lighting_ctrl_terminate(false, false);
    for (i = 0; i < sizeof(evt) / sizeof(evt[0]); i++) {
        lighting_ctrl_add_event(&evt[i]);
    }
    lighting_ctrl_run(1, 0, ftm_color_bulb_fade);

    log_put(LOG_ERR "[%s]", __func__);
}

static void ftm_color_bulb(void)
{
    int i;
    struct app_event evt[] = {
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_On,
                .value = 1,
            }
        },
        /* 100% CW 1s */
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Mode,
                .value = 0,
            }
        },
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Level,
                .value = 254,
            }
        },
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Temp,
                .value = 100,
            }
        },
        {
            .type = kEventType_Timer,
            .timer_event = {
                .value = 1000,
            }
        },
        /* 100% WW 1s */
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Temp,
                .value = 1,
            }
        },
        {
            .type = kEventType_Timer,
            .timer_event = {
                .value = 1000,
            }
        },
        /* 100% Red 1s */
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Mode,
                .value = 1,
            }
        },
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Color,
                .value = 16711680,
            }
        },
        {
            .type = kEventType_Timer,
            .timer_event = {
                .value = 1000,
            }
        },
        /* 100% Green 1s */
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Color,
                .value = 65280,
            }
        },
        {
            .type = kEventType_Timer,
            .timer_event = {
                .value = 1000,
            }
        },
        /* 100% Blue 1s */
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Color,
                .value = 255,
            }
        },
        {
            .type = kEventType_Timer,
            .timer_event = {
                .value = 1000,
            }
        },
        /* All off 2s */
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Off,
            }
        },
        {
            .type = kEventType_Timer,
            .timer_event = {
                .value = 2000,
            }
        },
    };

    lighting_ctrl_terminate(false, false);
    for (i = 0; i < sizeof(evt) / sizeof(evt[0]); i++) {
        lighting_ctrl_add_event(&evt[i]);
    }
    lighting_ctrl_run(4, 0, ftm_color_bulb_aging);

    log_put(LOG_ERR "[%s]", __func__);
}

static void ftm_tw_bulb_fade(bool timeout)
{
    int i;
    struct app_event evt[] = {
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_On,
                .value = 1,
            }
        },
        /* 10% CW */
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Mode,
                .value = 0,
            }
        },
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Level,
                .value = 25,
            }
        },
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Temp,
                .value = 100,
            }
        },
    };

    lighting_ctrl_terminate(false, false);
    for (i = 0; i < sizeof(evt) / sizeof(evt[0]); i++) {
        lighting_ctrl_add_event(&evt[i]);
    }
    lighting_ctrl_run(-1, 0, NULL);

    ftm_bulb_test_done();

    log_put(LOG_ERR "[%s]", __func__);
}

static void ftm_tw_bulb_aging(bool timeout)
{
    int i;
    struct app_event evt[] = {
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_On,
                .value = 1,
            }
        },
        /* 100% WW 10m */
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Mode,
                .value = 0,
            }
        },
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Level,
                .value = 254,
            }
        },
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Temp,
                .value = 1,
            }
        },
        {
            .type = kEventType_Timer,
            .timer_event = {
                .value = 600 * 1000,
            }
        },
        /* 75% WW, 25% CW 10m */
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Temp,
                .value = 25,
            }
        },
        {
            .type = kEventType_Timer,
            .timer_event = {
                .value = 600 * 1000,
            }
        },
        /* 100% CW 10m */
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Temp,
                .value = 100,
            }
        },
        {
            .type = kEventType_Timer,
            .timer_event = {
                .value = 600 * 1000,
            }
        },
    };

    lighting_ctrl_terminate(false, false);
    for (i = 0; i < sizeof(evt) / sizeof(evt[0]); i++) {
        lighting_ctrl_add_event(&evt[i]);
    }
    lighting_ctrl_run(1, 0, ftm_tw_bulb_fade);

    log_put(LOG_ERR "[%s]", __func__);
}

static void ftm_tunable_white_bulb(void)
{
    int i;
    struct app_event evt[] = {
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_On,
                .value = 1,
            }
        },
        /* 100% CW 1s */
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Mode,
                .value = 0,
            }
        },
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Level,
                .value = 254,
            }
        },
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Temp,
                .value = 100,
            }
        },
        {
            .type = kEventType_Timer,
            .timer_event = {
                .value = 1000,
            }
        },
        /* 100% WW 1s */
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Temp,
                .value = 1,
            }
        },
        {
            .type = kEventType_Timer,
            .timer_event = {
                .value = 1000,
            }
        },
        /* All off 2s */
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Off,
            }
        },
        {
            .type = kEventType_Timer,
            .timer_event = {
                .value = 2000,
            }
        },
    };

    lighting_ctrl_terminate(false, false);
    for (i = 0; i < sizeof(evt) / sizeof(evt[0]); i++) {
        lighting_ctrl_add_event(&evt[i]);
    }
    lighting_ctrl_run(4, 0, ftm_tw_bulb_aging);

    log_put(LOG_ERR "[%s]", __func__);
}

#if 0
static void ftm_grow_bulb_fade(bool timeout)
{
    return;
}

static void ftm_grow_bulb_aging(bool timeout)
{
    return;
}
#endif

static void ftm_grow_bulb(void)
{
    log_put(LOG_ERR "[%s]", __func__);
}

static void ftm_w_bulb_fade(bool timeout)
{
    int i;
    struct app_event evt[] = {
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_On,
                .value = 1,
            }
        },
        /* 10% */
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Mode,
                .value = 0,
            }
        },
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Level,
                .value = 25,
            }
        },
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Temp,
                .value = 50,
            }
        },
    };

    lighting_ctrl_terminate(false, false);
    for (i = 0; i < sizeof(evt) / sizeof(evt[0]); i++) {
        lighting_ctrl_add_event(&evt[i]);
    }
    lighting_ctrl_run(-1, 0, NULL);

    ftm_bulb_test_done();

    log_put(LOG_ERR "[%s]", __func__);
}

static void ftm_w_bulb_aging(bool timeout)
{
    int i;
    struct app_event evt[] = {
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_On,
                .value = 1,
            }
        },
        /* 100% 30m */
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Mode,
                .value = 0,
            }
        },
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Level,
                .value = 254,
            }
        },
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Temp,
                .value = 50,
            }
        },
        {
            .type = kEventType_Timer,
            .timer_event = {
                .value = 1800 * 1000,
            }
        },
    };

    lighting_ctrl_terminate(false, false);
    for (i = 0; i < sizeof(evt) / sizeof(evt[0]); i++) {
        lighting_ctrl_add_event(&evt[i]);
    }
    lighting_ctrl_run(1, 0, ftm_w_bulb_fade);

    log_put(LOG_ERR "[%s]", __func__);
}

static void ftm_fixed_white_bulb(void)
{
    int i;
    struct app_event evt[] = {
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_On,
                .value = 1,
            }
        },
        /* 100% 1s */
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Mode,
                .value = 0,
            }
        },
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Level,
                .value = 254,
            }
        },
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Temp,
                .value = 50,
            }
        },
        {
            .type = kEventType_Timer,
            .timer_event = {
                .value = 1000,
            }
        },
        /* 80% 1s */
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Level,
                .value = 203,
            }
        },
        {
            .type = kEventType_Timer,
            .timer_event = {
                .value = 1000,
            }
        },
        /* 60% 1s */
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Level,
                .value = 152,
            }
        },
        {
            .type = kEventType_Timer,
            .timer_event = {
                .value = 1000,
            }
        },
        /* 40% 1s */
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Level,
                .value = 102,
            }
        },
        {
            .type = kEventType_Timer,
            .timer_event = {
                .value = 1000,
            }
        },
        /* 20% 1s */
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Level,
                .value = 50,
            }
        },
        {
            .type = kEventType_Timer,
            .timer_event = {
                .value = 1000,
            }
        },
        /* All off 2s */
        {
            .type = kEventType_Light,
            .light_event = {
                .action = kLightAction_Off,
            }
        },
        {
            .type = kEventType_Timer,
            .timer_event = {
                .value = 2000,
            }
        },
    };

    lighting_ctrl_terminate(false, false);
    for (i = 0; i < sizeof(evt) / sizeof(evt[0]); i++) {
        lighting_ctrl_add_event(&evt[i]);
    }
    lighting_ctrl_run(4, 0, ftm_w_bulb_aging);

    log_put(LOG_ERR "[%s]", __func__);
}

void ftm_bulb_init(void)
{
    if (ftm_bulb == 0) {
        return;
    }
   
    switch (ftm_bulb_type) {
    case FTM_BTYPE_COLOR:
        ftm_register(&ftm_color_bulb);
        break;
    case FTM_BTYPE_TUNABLE_WHITE:
        ftm_register(&ftm_tunable_white_bulb);
        break;
    case FTM_BTYPE_FIXED_WHITE:
        ftm_register(&ftm_fixed_white_bulb);
        break;
    case FTM_BTYPE_GROW:
        ftm_register(&ftm_grow_bulb);
        break;
    case FTM_BTYPE_NONE:
    default:
        return;
    }
}
