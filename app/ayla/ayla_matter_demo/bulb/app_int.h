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

#define APP_VER         "1.0.8"
#define APP_NAME        "hdy_matter_floor_lamp"

#define BUILD_STRING            APP_VER " "  __DATE__ " " __TIME__

#define DEMO_OEM_ID	            "ec85a8df" /* may be set to your Ayla OEM ID */
#define DEMO_OEM_MODEL	        "floor-lamp-dev" /* may be set to your OEM model name */
#define DEMO_TEMPLATE_VERSION   "floor_lamp 1.0.3" /* demo template version */


#define DEMO_FACTORY_NAME       "FLO"
#define DEMO_PURCHASE_ORDER     "PO"
#define DEMO_DEVICE_ID          "Ayla-0506"
#define DEMO_LED_DRIVER         "BP5768D"

/*
 * Define the number of active schedules supported.
 * These can have any valid name, and should be in the template.
 */
#define DEMO_SCHED_COUNT	5

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
