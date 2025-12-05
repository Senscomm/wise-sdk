/*
 * Copyright 2011-2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_DEMO_CONF_H__
#define __AYLA_DEMO_CONF_H__

#define APP_VER         "1.3.2"
#define APP_NAME        "ayla_demo"

#define BUILD_STRING    APP_VER " "  __DATE__ " " __TIME__

/*
 * OEM info
 *
 * The OEM ID and OEM model would normally be configured by the CLI, but can
 * be defined here to be set by the demo program.  Define these if
 * you want them compiled in.
 *
 * The OEM and oem_model strings and the template version determine the
 * template on the first connect and the host name for the service.
 *
 * If these are changed, the encrypted OEM secret must be re-encrypted
 * unless the oem_model "*" (wild-card) when the oem_key was encrypted.
 *
 * The OEM model should be different for different module types.
 * The OEM model must contain only letters, numbers and hyphens ('-').
 */

#define DEMO_OEM_ID	            "d9754164" /* may be set to your Ayla OEM ID */
#define DEMO_OEM_MODEL	        "iworkbulb-dev" /* may be set to your OEM model name */
#define DEMO_TEMPLATE_VERSION   "m color bulb 1.0" /* demo template version */
#define DEMO_FACTORY_NAME       "MFG"
#define DEMO_PURCHASE_ORDER     "PO"
#define DEMO_DEVICE_ID          "Ayla-0506"
#define DEMO_LED_DRIVER         "BP5768D"

/*
 * Define the number of active schedules supported.
 * These can have any valid name, and should be in the template.
 */
#define DEMO_SCHED_COUNT	5

extern u8 conf_connected;

void client_conf_init(void);

void sched_conf_load(void);

void demo_ota_init(void);

#endif /* __AYLA_DEMO_CONF_H__ */
