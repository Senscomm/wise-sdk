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
#include <ayla/parse.h>
#include <ada/client.h>
#include <ada/ada_conf.h>

#include "color_format.h"
#include "led_widget.h"
#include "lighting_mgr.h"
#include "lighting_ctrl.h"

#include "ftm.h"
#include "ftm_bulb.h"
#include "ftm_wifi.h"

/*
 * ftm configuration items.
 */
u8 ftm;		        /* FTM enabled */
u8 ftm_wifi;	    /* FTM Wi-Fi test pending */
u8 ftm_bulb;	    /* FTM Bulb test pending */
u8 ftm_bulb_type;	/* FTM Bulb type */

/*
 * Error indication by blinks
 */
static struct blink_spec
{
    u8 on;
    u8 off;
    u8 num;
    u32 period;
} blink;

static ftm_exec ftm_body;

static const struct ada_conf_item ftm_conf_items[] = {
	{ "ftm/ftm", ATLV_UINT, &ftm, sizeof(ftm)},
	{ "ftm/ftm_wifi", ATLV_UINT, &ftm_wifi, sizeof(ftm_wifi)},
	{ "ftm/ftm_bulb", ATLV_UINT, &ftm_bulb, sizeof(ftm_bulb)},
	{ "ftm/ftm_bulb_type", ATLV_UINT, &ftm_bulb_type, sizeof(ftm_bulb_type)},
	{ NULL }
};

/*
 * Export FTM configuration items.
 */
static void ftm_export(void)
{
	conf_put(CT_ftm, ATLV_UINT, &ftm, sizeof(ftm));
	conf_put(CT_ftm_wifi, ATLV_UINT, &ftm_wifi, sizeof(ftm_wifi));
	conf_put(CT_ftm_bulb, ATLV_UINT, &ftm_bulb, sizeof(ftm_bulb));
	conf_put(CT_ftm_bulb_type, ATLV_UINT, &ftm_bulb_type, sizeof(ftm_bulb_type));
}

const struct conf_entry conf_ftm_entry = {
	.token = CT_ftm,
	.export = ftm_export,
};

void ftm_conf_load(void)
{
	const struct ada_conf_item *item;

	for (item = ftm_conf_items; item->name; item++) {
		ada_conf_get_item(item);
	}
}

#define ftm_init_and_check(x)   do {\
    ftm_##x##_init();               \
    if (ftm_body) {                 \
        return;                     \
    }                               \
} while (0)

void ftm_init(void)
{
    conf_table_entry_add(&conf_ftm_entry);
    ftm_conf_load();

    ftm_init_and_check(bulb);
    ftm_init_and_check(wifi);
    /* XXX: (Only) ftm_init_and_check(xxx)s will follow.
     */
}

void ftm_register(ftm_exec body)
{
    ftm_body = body;
}

bool ftm_is_enabled(void)
{
    return (ftm == 1 && ftm_body != NULL);
}

void ftm_run(u32 arg)
{
    (void)arg;

    if (ftm_body) {
        ftm_body();
    }
}

static void ftm_blink(bool timeout);

static void ftm_do_blink(u32 delay)
{
    int i;
    static struct app_event *pEvt = NULL;
    static int numEvt = 0;

    if (pEvt) {
        free(pEvt);
        pEvt = NULL;
        numEvt = 0;
    }

    /* XXX: is there a better way to determine
     *      whether or not any kind of light bulb is supported and enabled?
     */
    if (ftm_bulb_type == FTM_BTYPE_COLOR) {
        struct app_event evt[] = {
            {
                .type = kEventType_Timer,
                .timer_event = {
                    .value = delay,
                }
            },
            {
                .type = kEventType_Light,
                .light_event = {
                    .action = kLightAction_On,
                    .value = 1,
                }
            },
            /* 100% Red */
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
                    .value = blink.on,
                }
            },
            {
                .type = kEventType_Light,
                .light_event = {
                    .action = kLightAction_Off,
                }
            },
            {
                .type = kEventType_Timer,
                .timer_event = {
                    .value = blink.off,
                }
            },
        };
        numEvt = sizeof(evt) / sizeof(evt[0]);
        pEvt = malloc(numEvt * sizeof(struct app_event));
        memcpy(pEvt, evt, numEvt * sizeof(struct app_event));
    } else if (ftm_bulb_type == FTM_BTYPE_TUNABLE_WHITE) {
        struct app_event evt[] = {
            {
                .type = kEventType_Timer,
                .timer_event = {
                    .value = delay,
                }
            },
            {
                .type = kEventType_Light,
                .light_event = {
                    .action = kLightAction_On,
                    .value = 1,
                }
            },
            /* 100% WW */
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
                    .value = blink.on,
                }
            },
            {
                .type = kEventType_Light,
                .light_event = {
                    .action = kLightAction_Off,
                }
            },
            {
                .type = kEventType_Timer,
                .timer_event = {
                    .value = blink.off,
                }
            },
        };
        numEvt = sizeof(evt) / sizeof(evt[0]);
        pEvt = malloc(numEvt * sizeof(struct app_event));
        memcpy(pEvt, evt, numEvt * sizeof(struct app_event));
    } else if (ftm_bulb_type == FTM_BTYPE_FIXED_WHITE) {
        struct app_event evt[] = {
            {
                .type = kEventType_Timer,
                .timer_event = {
                    .value = delay,
                }
            },
            {
                .type = kEventType_Light,
                .light_event = {
                    .action = kLightAction_On,
                    .value = 1,
                }
            },
            /* 100% */
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
                    .value = blink.on,
                }
            },
            {
                .type = kEventType_Light,
                .light_event = {
                    .action = kLightAction_Off,
                }
            },
            {
                .type = kEventType_Timer,
                .timer_event = {
                    .value = blink.off,
                }
            },
        };
        numEvt = sizeof(evt) / sizeof(evt[0]);
        pEvt = malloc(numEvt * sizeof(struct app_event));
        memcpy(pEvt, evt, numEvt * sizeof(struct app_event));
    } else if (ftm_bulb_type == FTM_BTYPE_GROW) {
        /* Not defined yet. */
    }

    if (!pEvt || !numEvt) {
        return;
    }

    lighting_ctrl_terminate(false, true);
    for (i = 0; i < numEvt; i++) {
        lighting_ctrl_add_event(&pEvt[i]);
    }
    lighting_ctrl_run(blink.num, 0, ftm_blink);
}

static void ftm_blink(bool timeout)
{
    struct app_event evt;

    evt.type = kEventType_Install;
    evt.install_event.callback = ftm_do_blink;
    evt.install_event.arg = blink.period;
    app_event_post(&evt);
}

void ftm_error(int on, int off, int blinks, int period)
{
    struct app_event evt;

    blink.on = on;
    blink.off = off;
    blink.num = blinks;
    blink.period = period;

    evt.type = kEventType_Install;
    evt.install_event.callback = ftm_do_blink;
    evt.install_event.arg = 0; /* no initial delay */
    app_event_post(&evt);
}

void ftm_success(void)
{
    int i;
    static struct app_event *pEvt = NULL;
    static int numEvt = 0;

    if (pEvt) {
        free(pEvt);
        pEvt = NULL;
        numEvt = 0;
    }

    /* XXX: is there a better way to determine
     *      whether or not any kind of light bulb is supported and enabled?
     */
    if (ftm_bulb_type == FTM_BTYPE_COLOR) {
        struct app_event evt[] = {
            {
                .type = kEventType_Light,
                .light_event = {
                    .action = kLightAction_On,
                    .value = 1,
                }
            },
            {
                .type = kEventType_Light,
                .light_event = {
                    .action = kLightAction_Mode,
                    .value = 1,
                }
            },
            /* 1% Red */
            {
                .type = kEventType_Light,
                .light_event = {
                    .action = kLightAction_Color,
                    .value = 65536,
                }
            },
            {
                .type = kEventType_Timer,
                .timer_event = {
                    .value = 500,
                }
            },
            /* 50% Red */
            {
                .type = kEventType_Light,
                .light_event = {
                    .action = kLightAction_Color,
                    .value = 8388608,
                }
            },
            {
                .type = kEventType_Timer,
                .timer_event = {
                    .value = 500,
                }
            },
            /* 100% Red */
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
                    .value = 500,
                }
            },
            /* 1% Green */
            {
                .type = kEventType_Light,
                .light_event = {
                    .action = kLightAction_Color,
                    .value = 256,
                }
            },
            {
                .type = kEventType_Timer,
                .timer_event = {
                    .value = 500,
                }
            },
            /* 50% Green */
            {
                .type = kEventType_Light,
                .light_event = {
                    .action = kLightAction_Color,
                    .value = 32768,
                }
            },
            {
                .type = kEventType_Timer,
                .timer_event = {
                    .value = 500,
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
                    .value = 500,
                }
            },
            /* 1% Blue */
            {
                .type = kEventType_Light,
                .light_event = {
                    .action = kLightAction_Color,
                    .value = 1,
                }
            },
            {
                .type = kEventType_Timer,
                .timer_event = {
                    .value = 500,
                }
            },
            /* 50% Blue */
            {
                .type = kEventType_Light,
                .light_event = {
                    .action = kLightAction_Color,
                    .value = 128,
                }
            },
            {
                .type = kEventType_Timer,
                .timer_event = {
                    .value = 500,
                }
            },
            /* 100% Blue */
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
                    .value = 500,
                }
            },
            /* 1% CW */
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
                    .value = 2,
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
                    .value = 500,
                }
            },
            /* 50% CW */
            {
                .type = kEventType_Light,
                .light_event = {
                    .action = kLightAction_Level,
                    .value = 127,
                }
            },
            {
                .type = kEventType_Timer,
                .timer_event = {
                    .value = 500,
                }
            },
            /* 100% CW */
            {
                .type = kEventType_Light,
                .light_event = {
                    .action = kLightAction_Level,
                    .value = 254,
                }
            },
            {
                .type = kEventType_Timer,
                .timer_event = {
                    .value = 500,
                }
            },
            /* 1% WW */
            {
                .type = kEventType_Light,
                .light_event = {
                    .action = kLightAction_Level,
                    .value = 2,
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
                    .value = 500,
                }
            },
            /* 50% WW */
            {
                .type = kEventType_Light,
                .light_event = {
                    .action = kLightAction_Level,
                    .value = 127,
                }
            },
            {
                .type = kEventType_Timer,
                .timer_event = {
                    .value = 500,
                }
            },
            /* 100% WW */
            {
                .type = kEventType_Light,
                .light_event = {
                    .action = kLightAction_Level,
                    .value = 254,
                }
            },
            {
                .type = kEventType_Timer,
                .timer_event = {
                    .value = 500,
                }
            },
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
        numEvt = sizeof(evt) / sizeof(evt[0]);
        pEvt = malloc(numEvt * sizeof(struct app_event));
        memcpy(pEvt, evt, numEvt * sizeof(struct app_event));
    } else if (ftm_bulb_type == FTM_BTYPE_TUNABLE_WHITE) {
        struct app_event evt[] = {
            {
                .type = kEventType_Light,
                .light_event = {
                    .action = kLightAction_On,
                    .value = 1,
                }
            },
            /* 0% CW */
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
                    .value = 1,
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
                    .value = 500,
                }
            },
            /* 50% CW */
            {
                .type = kEventType_Light,
                .light_event = {
                    .action = kLightAction_Level,
                    .value = 127,
                }
            },
            {
                .type = kEventType_Timer,
                .timer_event = {
                    .value = 500,
                }
            },
            /* 100% CW */
            {
                .type = kEventType_Light,
                .light_event = {
                    .action = kLightAction_Level,
                    .value = 254,
                }
            },
            {
                .type = kEventType_Timer,
                .timer_event = {
                    .value = 500,
                }
            },
            /* 0% WW */
            {
                .type = kEventType_Light,
                .light_event = {
                    .action = kLightAction_Level,
                    .value = 1,
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
                    .value = 500,
                }
            },
            /* 50% WW */
            {
                .type = kEventType_Light,
                .light_event = {
                    .action = kLightAction_Level,
                    .value = 127,
                }
            },
            {
                .type = kEventType_Timer,
                .timer_event = {
                    .value = 500,
                }
            },
            /* 100% WW */
            {
                .type = kEventType_Light,
                .light_event = {
                    .action = kLightAction_Level,
                    .value = 254,
                }
            },
            {
                .type = kEventType_Timer,
                .timer_event = {
                    .value = 500,
                }
            },
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
        numEvt = sizeof(evt) / sizeof(evt[0]);
        pEvt = malloc(numEvt * sizeof(struct app_event));
        memcpy(pEvt, evt, numEvt * sizeof(struct app_event));
    } else if (ftm_bulb_type == FTM_BTYPE_FIXED_WHITE) {
        struct app_event evt[] = {
            {
                .type = kEventType_Light,
                .light_event = {
                    .action = kLightAction_On,
                    .value = 1,
                }
            },
            /* 0% */
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
                    .value = 1,
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
                    .value = 500,
                }
            },
            /* 50% */
            {
                .type = kEventType_Light,
                .light_event = {
                    .action = kLightAction_Level,
                    .value = 127,
                }
            },
            {
                .type = kEventType_Timer,
                .timer_event = {
                    .value = 500,
                }
            },
            /* 100% */
            {
                .type = kEventType_Light,
                .light_event = {
                    .action = kLightAction_Level,
                    .value = 254,
                }
            },
            {
                .type = kEventType_Timer,
                .timer_event = {
                    .value = 500,
                }
            },
        };
        numEvt = sizeof(evt) / sizeof(evt[0]);
        pEvt = malloc(numEvt * sizeof(struct app_event));
        memcpy(pEvt, evt, numEvt * sizeof(struct app_event));
    } else if (ftm_bulb_type == FTM_BTYPE_GROW) {
        /* Not defined yet. */
    }

    if (!pEvt || !numEvt) {
        return;
    }

    lighting_ctrl_terminate(false, true);
    for (i = 0; i < numEvt; i++) {
        lighting_ctrl_add_event(&pEvt[i]);
    }
    lighting_ctrl_run(-1, 0, NULL);
}

/*
 * Handle FGM CLI commands.
 */
void ftm_cli(int argc, char **argv)
{
	if (argc <= 1 || !strcmp(argv[1], "help")) {
		printcli("ftm: %sabled", ftm ? "en" : "dis");
		printcli("ftm_wifi: %sabled", ftm_wifi ? "en" : "dis");
		printcli("ftm_bulb: %sabled", ftm_bulb ? "en" : "dis");
		printcli("ftm_bulb_type: %d", ftm_bulb_type);
		return;
	}
	if (!strcmp(argv[1], "help")) {
		goto usage;
	}

	if (!mfg_or_setup_mode_ok()) {
		return;
	}

	if (argc == 2) {
		if (!strcmp(argv[1], "wifi")
                || !strcmp(argv[1], "bulb")
                || !strcmp(argv[1], "bulb_type")) {
			printcli("error: invalid ftm value");
			goto usage;	/* missed intended third argument */
		}
        if (!strcmp(argv[1], "enable")) {
            ftm = 1;
        } else if (!strcmp(argv[1], "disable")) {
            ftm = 0;
        }
		return;
	}
	if (argc == 3) {
        if (!strcmp(argv[1], "wifi")) {
            if (!strcmp(argv[2], "enable")) {
                ftm_wifi = 1;
            } else if (!strcmp(argv[2], "disable")) {
                ftm_wifi = 0;
            } else {
                goto usage;
            }
            return;
        } else if (!strcmp(argv[1], "bulb_type")) {
            ftm_bulb_type = (u8)atoi(argv[2]);
            return;
        } else if (!strcmp(argv[1], "bulb")) {
            if (!strcmp(argv[2], "enable")) {
                ftm_bulb = 1;
            } else if (!strcmp(argv[2], "disable")) {
                ftm_bulb = 0;
            } else {
                goto usage;
            }
            return;
        } else {
            goto usage;
        }
	}

usage:
	printcli("usage: ftm [help|show]");
	printcli("   or: ftm [enable|disable]");
	printcli("   or: ftm wifi [enable|disable]");
	printcli("   or: ftm bulb [enable|disable]");
	printcli("   or: ftm bulb_type [<bulb-type>]");
}

