/*
 * Copyright 2023 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_APP_H__
#define __AYLA_APP_H__

#define APP_VER         "1.0.1"
#define APP_NAME        "ayla_matter_demo"

#define BUILD_STRING            APP_VER " "  __DATE__ " " __TIME__

#define DEMO_OEM_ID	        "0dfc7900" /* may be set to your Ayla OEM ID */
#define DEMO_OEM_MODEL	        "" /* may be set to your OEM model name */
#define DEMO_TEMPLATE_VERSION   "demo_matter 1.1" /* demo template version */

/*
 * Define the number of active schedules supported.
 * These can have any valid name, and should be in the template.
 */
#define DEMO_SCHED_COUNT	5

#define GPIO_BLUE_LED		19
#define GPIO_GREEN_LED		18
#define GPIO_LINK_LED		16
#undef  GPIO_RGB_LED
#define GPIO_BOOT_BUTTON	0
#define GPIO_LED            GPIO_BLUE_LED
/**
 * \brief Demo app initialization.
 */
void demo_init(void);

/**
 * \brief Demo app main loop.
 */
void demo_idle(void);

/**
 * \brief Initialize the OTA.
 */
void demo_ota_init(void);

void sched_conf_load(void);

#endif /* __AYLA_APP_H__ */
