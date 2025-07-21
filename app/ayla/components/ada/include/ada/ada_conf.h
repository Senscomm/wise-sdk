/*
 * Copyright 2011-2015 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __ADA_ADA_CONF_H__
#define __ADA_ADA_CONF_H__

#if defined(AYLA_LAN_SUPPORT) || defined(AYLA_LOCAL_CONTROL_SUPPORT)
#include <ada/ada_lan_conf.h>
#else
#ifndef DISABLE_LAN_OTA
#define DISABLE_LAN_OTA
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define CLIENT_CONF_PUB_KEY_LEN	300	/* max size of binary RSA public key */
#define CLIENT_CONF_SYMNAME_LEN 40	/* max symbolic name len */
#define CLIENT_CONF_REG_TOK_LEN	8	/* registration token length */
#define CLIENT_CONF_REGION_CODE_LEN 2	/* two character country code */
#define CLIENT_CONF_RSA_MOD_SIZE 256	/**< RSA modulus size */

#define ADA_CONF_VER_LEN	80	/* max length of OTA version string */
#define ADA_CONF_OEM_MAX	20	/**< Max length for OEM strings */
#define ADA_CONF_HWID_MAX	32	/**< Max length for hardware ID */

/*
 * Definitions for in-field provisioning.
 */
#define ADA_CONF_OEM_SIGNATURE_MAX 256	/**< Max OEM signature length */
#define ADA_CONF_PRIVATE_MAX    2048	/**< Max OEM private key length */
#define ADA_CONF_OEM_CIPHER_MAX	300	/**< Max length of OEM cipher */
#define ADA_CONF_SIGNATURE_MAX	256	/**< Max length for signature */
#define ADA_CONF_KEYID_MAX      16	/**< Max length for private key ID */

/*
 * Define the minimum time interval between syncing the device clock with
 * the cloud time.
 */
#define CLIENT_CONF_MIN_SYNC	(15 * 60 * 1000) /* min ms between syncs */

/*
 * Config variable names
 */
#define ADA_CONF_OEM_KEY	"oem/key"	/* name of oem key conf var */
#define ADA_PSM_ID_PUB_KEY	"id/key"

extern const char ada_version[];	/* ADA version */
extern const char ada_version_build[];	/* ADA version and build */

/*
 * Variables provided by platform or app for use by the Ayla client.
 */
extern u8 conf_was_reset;	/* indicates factory reset was done */
extern char conf_sys_dev_id[];		/* DSN: device serial number */
extern char oem[CONF_OEM_MAX + 1];	/* OEM ID, usually 8 hex chars */
extern char oem_model[CONF_OEM_MAX + 1]; /* OEM-model name for template */
extern char template_version[];

/*
 * Flags for client events sent to MCU
 */
enum client_event {
	CLIENT_EVENT_UNREG = BIT(0),
	CLIENT_EVENT_REG = BIT(1)
};

struct ada_conf {
	/*
	 * Items set by platform.
	 */
	u8 enable:1;		/* client enabled */
	u8 get_all:1;		/* get all to-device properties at connect */
	u8 lan_disable:1;	/* set to disable LAN mode */
	u8 test_connect:1;	/* don't count as first connect */
	u8 conf_serv_override;	/* override conf_server, use ads-dev */
	const char *region;	/* region for device service */
	char conf_server[CONF_ADS_HOST_MAX];
				/* server host name, if override desired */
	u16 conf_port;		/* server port, if not default */
	u16 poll_interval;	/* interval for polling if needed, seconds */
	const u8 *mac_addr;	/* system MAC address for identification */
	const char *hw_id;	/* pointer to unique hardware-ID string */

	/*
	 * Items set by client only, but of interest to the platform.
	 */
	char host_symname[CLIENT_CONF_SYMNAME_LEN];
	char reg_token[CLIENT_CONF_REG_TOK_LEN]; /* user registration token */
	u8 reg_user;		/* a user is registered */
	enum client_event event_mask; /* mask of reportable events */

	/*
	 * Optional items that may be set by host app for in-field provisioning.
	 * These are normally read from configuration.
	 * Note: do not use #ifdef to keep the binary compatible.
	 */
	const char *provision_key_id;	/* provision key ID */
	const char *oem_signature;	/* binary signature of hardware ID */
	size_t oem_sig_len;		/* length of signature */
};

extern struct ada_conf ada_conf;

struct ada_conf_item {
	const char *name;	/* config item name */
	enum ayla_tlv_type type;
	void *val;
	ssize_t len;		/* max length of string, or -sizeof(number) */
};

/*
 * Functions provided by platform or app for use by the client or cm.
 */

/*
 * Notify platform that the registration flag changed.
 */
void adap_conf_reg_changed(void);

/*
 * Interim functions, to be eliminated in final version.
 */
const char *client_conf_pub_key_base64(void);
void client_conf_pub_key_set(struct ayla_tlv *);
void ada_conf_file_cli(int argc, char **argv);

/*
 * Check if changing to a test server is enabled.
 */
int client_conf_server_change_en(void);

/*
 * Get the state of test server mode.
 */
u8 conf_sys_test_mode_get(void);

/*
 * Update state of test mode in conf.
 */
void conf_sys_test_mode_update(u8 enable);

/*
 * Save the conf_setup_mode and conf_mfg_mode flags.
 */
void ada_conf_persist_setup();

/*
 * Save the conf_was_reset flag.
 */
void ada_conf_persist_reset(void);

/*
 * Save timezone and DST info.
 */
void ada_conf_persist_timezone(void);

/*
 * Save the client/server/default flag.
 */
void ada_conf_persist_default(void);

/*
 * Return string to be reported to the cloud as the module image version.
 * This may return ada_version_build.
 */
const char *adap_conf_sw_build(void);

/*
 * Return string reported to LAN clients with the name, version and build info.
 * This may return ada_version.
 */
const char *adap_conf_sw_version(void);

/*
 * Called by the connection manager to start services, once interface is up.
 * Returns non-zero if client is disabled, but starts server and dnss.
 */
struct netif;
int client_conf_services_up(struct netif *);

/*
 * Called by the connection manager to stop services when interfaces are down.
 */
void client_conf_services_down(struct netif *);

extern u8 gpio_mode;

/*
 * Reset the system, optionally to the factory configuration.
 */
void ada_conf_reset(int factory_reset);

/*
 * Initialize config subsystem.
 */
void ada_conf_init(void);

/*
 * Load known config items from factory or startup config.
 */
void ada_conf_load(void);

/*
 * Load config items for other modules
 */
void oem_conf_load(void);
void client_conf_load(void);
void adw_conf_load(void);
void log_conf_load(void);

int ada_conf_get_item(const struct ada_conf_item *item);
int ada_conf_export_item(const struct ada_conf_item *item);

/*
 * Allocate config context with dryrun set for an error-checking run.
 */
struct ada_conf_ctx *ada_conf_dryrun_new(void);

/*
 * Turn off dryrun.
 */
void ada_conf_dryrun_off(struct ada_conf_ctx *);

int ada_conf_set(struct ada_conf_ctx *, const char *name, const char *val);

/*
 * Commit changes (save if necessary).
 */
int ada_conf_commit(struct ada_conf_ctx *);

/*
 * Finish config changes and perform reset or any other action requested.
 */
void ada_conf_close(struct ada_conf_ctx *);

/*
 * Abort changes and free context (on error).
 */
void ada_conf_abort(struct ada_conf_ctx *);

/*
 * CLI "id" command.
 */
void ada_conf_id_cli(int argc, char **argv);
extern const char ada_conf_id_help[];

/*
 * CLI "oem" command.
 */
void ada_conf_oem_cli(int argc, char **argv);

/*
 * CLI "reset" command.
 */
void ada_conf_reset_cli(int argc, char **argv);

/*
 * Enable or disable setup mode.
 *
 * When in setup mode, all saves go to the factory configuration.
 *
 * It is often best to do a factory reset, to get rid of any custom
 * configuration, before re-enabling setup mode.
 */
void ada_conf_setup_mode(int enable);

/*
 * Config interfaces to platform.
 */

/*
 * Get config variable from the startup configuration.
 * The buffer is filled with a string representation of the value.
 * The buffer may be written even if an error is returned.
 * The buffer will be NUL-terminated on success.
 * Returns the string length of the value or negative on failure.
 */
int adap_conf_get(const char *name, void *buf, size_t len);

/*
 * Set a config item.  Returns 0 on success, -1 on error.
 * The value may or may not be a string.
 */
int adap_conf_set(const char *name, const void *val, size_t len);

/*
 * Reset config item to factory setting
 */
int adap_conf_reset_factory(const char *name);

/*
 * Get encrypted OEM secret into supplied buffer.
 * The OEM secret is a string encrypted with the public key that validates
 * the device as manufactured by the OEM.
 * Returns length, or a negative value on error.
 */
int adap_conf_oem_key_get(void *buf, size_t len);

/*
 * Get public key into supplied buffer.
 * Returns length, or a negative value on error.
 */
int adap_conf_pub_key_get(void *buf, size_t len);

/*
 * Reset the platform to the saved startup or factory configuration.
 */
void adap_conf_reset(int factory);

/*
 * Set config commit reset flag.
 */
void client_conf_set_commit_reset(u8 reset);

/*
 * Generate and save OEM key
 */
enum conf_error oem_set_key(const char *oem_secret, size_t sec_len,
    const char *model);

#ifdef __cplusplus
}
#endif

#endif /* __ADA_ADA_CONF_H__ */
