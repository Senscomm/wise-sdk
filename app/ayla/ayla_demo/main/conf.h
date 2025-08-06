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

/*
 * ODM company name for factory log.
 */
#define DEMO_CONF_ODM  "ADA demo customer" /* replace with your company name */

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
#define DEMO_OEM_ID	"0dfc7900"	/* may be set to your Ayla OEM ID */
#define DEMO_OEM_MODEL	""		/* may be set to your OEM model name */

#ifdef DEMO_MSG_PROP
#ifndef AYLA_FILE_PROP_SUPPORT
#error DEMO_MSG_PROP requires AYLA_FILE_PROP_SUPPORT
#endif
#define DEMO_TEMPLATE_VERSION "demo_esp_msg 1.10" /* demo template version */
#else
#define DEMO_TEMPLATE_VERSION "demo_esp 1.10" /* demo template version */
#endif

/*
 * Define the number of active schedules supported.
 * These can have any valid name, and should be in the template.
 */
#define DEMO_SCHED_COUNT	5

/*
 * Module names used for PSM subsystem.
 */
#define ADA_CONF_MOD	"ada"		/* most items */

/*
 * string length limits
 */
#define CONF_PATH_STR_MAX	64	/* max len of conf variable name */
#define CONF_ADS_HOST_MAX (CONF_OEM_MAX + 1 + CONF_MODEL_MAX + 1 + 24)
					/* max ADS hostname length incl NUL */
#define ADA_PUB_KEY_LEN	400
struct cli_cmds {
    char *org_cmd;
    char *dup_cmd;
};

extern char conf_sys_model[];
extern char conf_sys_serial[];
extern char conf_sys_mfg_model[];
extern char conf_sys_mfg_serial[];
extern const char conf_sys_template_version[];

extern const char mod_sw_build[];
extern const char mod_sw_version[];
extern u8 conf_connected;

void client_conf_init(void);

void sched_conf_load(void);

void demo_ota_init(void);

#endif /* __AYLA_DEMO_CONF_H__ */
