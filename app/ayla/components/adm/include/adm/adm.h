/*
 * Copyright 2022 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_ADM_H__
#define __AYLA_ADM_H__

/**
 * This file contains definitions used by the ADM (Ayla Device Matter)
 * integration subsystem.
 */
#include <ayla/utypes.h>
#include <ayla/assert.h>
#include <ada/err.h>
#include <al/al_matter.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Attribute change callback entry flags.
 */
/**
 * call for any endpoint
 */
#define	ADM_ACCE_ANY_ENDPOINT		0x01
/**
 * call for any cluster
 */
#define ADM_ACCE_ANY_CLUSTER		0x02
/**
 * call for any attribute
 */
#define ADM_ACCE_ANY_ATTRIBUTE		0x04
/**
 * call for before to any change
 */
#define ADM_ACCE_PRE_CHANGE		0x40
/**
 * call after any change
 */
#define ADM_ACCE_POST_CHANGE		0x80

/**
 * Initialization macro attribute change callback entries.
 *
 * \param _flags is flags to control callback filtering.
 * \param _endpoint is the endpoint to filter to if the ADM_ACCE_ANY_ENDPOINT
 * flag is not set.
 * \param _cluster is the cluster to filter to if the ADM_ACCE_ANY_CLUSTER
 * flag is not set.
 * \param _attribute is the attribute to filter to if the ADM_ACCE_ANY_ATTRIBUTE
 * flag is not set.
 * \param _callback is the callback to call when the change passes the filters.
 */
#define ADM_ACCE_INIT(_flags, _endpoint, _cluster, _attribute, _callback) { \
    .cluster = _cluster, \
    .attribute = _attribute, \
    .endpoint = _endpoint, \
    .flags = _flags, \
    .callback = _callback}

/**
 * Attribute change callback entry structure.
 */
struct adm_attribute_change_callback {
	u32 cluster;	/**< specific cluster to limit callbacks to */
	u32 attribute;	/**< specific attribute to limit callbacks to */
	u16 endpoint;	/**< specific endpoint to limit callbacks to */
	u8 flags;	/**< additional flags to control filtering */
	enum ada_err (*callback)(u8 post_change, u16 endpoint,
	    u32 cluster, u32 attribute, u8 type, u16 size, u8 *value);
};

/**
 * Matter network events enum.
 */
enum adm_event_id {
	ADM_EVENT_INITIALIZED,	/**< ADM_EVENT_INITIALIZED */
	ADM_EVENT_IPV4_UP,	/**< ADM_EVENT_IPV4_UP */
	ADM_EVENT_IPV4_DOWN,	/**< ADM_EVENT_IPV4_DOWN */
	ADM_EVENT_IPV6_UP,	/**< ADM_EVENT_IPV6_UP */
	ADM_EVENT_IPV6_DOWN,	/**< ADM_EVENT_IPV6_DOWN */
	ADM_EVENT_COMMISSIONING_SESSION_STARTED,
	ADM_EVENT_COMMISSIONING_SESSION_STOPPED,
	ADM_EVENT_COMMISSIONING_WINDOW_OPENED,
	ADM_EVENT_COMMISSIONING_WINDOW_CLOSED,
	ADM_EVENT_COMMISSIONING_COMPLETE,
};

/**
 * Generator macro for 32 bit manufacturer specific Ids used by Matter, such as
 * cluster ids and attribute ids.
 *
 * \param mfg_id is a Manufacturer Code assigned by CSA.
 * \param item_id is the 16 bit id assigned to the item.
 */
#define ADM_ID_GEN(mfg_id, item_id)	((mfg_id << 16) | item_id)

enum adm_attr_rw_type {
	/** Attribute Read */
	ADM_ATTR_READ,
	/** Attribute Write */
	ADM_ATTR_WRITE,
};

/** identify callback type */
enum adm_identify_cb_type {
	/** Callback to start identification */
	ADM_IDENTIFY_START,
	/** Callback to stop identification */
	ADM_IDENTIFY_STOP,
	/** Callback to run a specific identification effect */
	ADM_IDENTIFY_EFFECT,
};

enum adm_identify_type {
	ADM_IDENTIFY_TYPE_NONE          = 0,
	ADM_IDENTIFY_TYPE_VISIBLE_LIGHT = 1,
	ADM_IDENTIFY_TYPE_VISIBLE_LED   = 2,
	ADM_IDENTIFY_TYPE_AUDIBLE_BEEP  = 3,
	ADM_IDENTIFY_TYPE_DISPLAY       = 4,
	ADM_IDENTIFY_TYPE_ACTUATOR      = 5,
};

enum adm_effect_identifier {
	ADM_EFFECT_IDENTIFIER_BLINK          = 0,
	ADM_EFFECT_IDENTIFIER_BREATHE        = 1,
	ADM_EFFECT_IDENTIFIER_OKAY           = 2,
	ADM_EFFECT_IDENTIFIER_CHANNEL_CHANGE = 11,
	ADM_EFFECT_IDENTIFIER_FINISH_EFFECT  = 254,
	ADM_EFFECT_IDENTIFIER_STOP_EFFECT    = 255,
};

enum adm_effect_variant {
	ADM_EFFECT_VARIANT_DEFAULT = 0,
};

struct adm_identify_time_data {
	u8 identify_type;
	u16 identify_time;
};

struct adm_identify_effect_data {
	u8 effect_identify;
	u8 effect_variant;
};

union adm_identify_cb_data {
	struct adm_identify_time_data time;
	struct adm_identify_effect_data effect;
};

/**
 * Initialize the Matter stack.
 */
void adm_init(void);

/**
 * Start the Matter stack.
 *
 * Note: Initialization of the Matter stack is asynchronous and should not
 * be expected to be complete when this function returns.
 *
 * \param cert_declaration is a pointer to the binary certification declaration
 * issued by the CSA for this hardware and software combination. The referenced
 * data must be available for the execution lifetime of the app. May be NULL
 * for devices under development, in which case the device will use test
 * credentials built in to ADM.
 * \parma cd_len is the size in bytes of the certification declaration data.
 */
void adm_start(const u8 *cert_declaration, size_t cd_len);

/**
 * Check if ADM initialization has completed.
 *
 * \returns 0 if not initialized, 1 if initialized
 */
int adm_initialized(void);

/**
 * Generate and configure Matter onboarding credentials.
 *
 * \param secret is a NUL-terminated string containing the secret to use when
 * generating credentials. If NULL, the compiled in secret defined by
 * AYLA_MATTER_QRGEN_SECRET will be used.
 * \returns AE_OK if successful, otherwise, an error.
 */
enum ada_err adm_onboard_config_generate(const char *secret);

/**
 * Get onboarding credentials configuration.
 *
 * \param secret is a NUL-terminated string containing the secret that was
 * used when generating credentials, if adm_onboard_config_generate was used.
 * If NULL, or incorrect, the value returned for passcode will be zero and,
 * qr_str and pairing_str values will be left unmodified.
 * \param passcode is a pointer to the passcode return value. A passcode of
 * zero will be returned if the secret is incorrect.
 * \param vendor a pointer for the return value for the vendor id.
 * \param product is a pointer to the return value for the product id.
 * \param discovery_mask is a pointer to the return value for the discovery
 * mask.
 * \param discriminator is a pointer to the return value for the discriminator
 * value.
 * \param pairing_str is a pointer to the return value buffer for the manual
 * pairing code string.
 * \param qr_string is a pointer to the return value buffer for the QR code
 * payload string.
 * \returns AE_OK if successful, otherwise, an error. A bad or NULL secret
 * will return AE_OK with passcode equal to zero, indicating the passcode,
 * pairing_str and qr_str could not be calculated.
 */
enum ada_err adm_onboarding_config_get(const char *secret, u32 *passcode,
    u16 *vendor, u16 *product, u8 *discovery_mask, u16 *discriminator,
    char *pairing_str, size_t ps_len, char *qr_str, size_t qr_len);


#ifndef AYLA_WIFI_SUPPORT
/**
 * Indicates whether the Wi-Fi network has been provisioned.
 *
 * \returns 0 if not provisioned, 1 if provisioned
 */
int adm_network_configured(void);
#endif

/**
 * Register a callback function for the application to receive notification of
 * network events originating from the Matter stack.
 *
 * \param cb is the callback function to register.
 * \returns AE_OK if successful, otherwise, an error.
 *
 * Callback function paramters.
 *
 * \param id is an identifier provided when the callback function is called
 * indicating the type of event that has occurred.
 */
enum ada_err adm_event_cb_register(void (*cb)(enum adm_event_id id));

/**
 * Register a callback function entry to receive notification of particular
 * attribute value change(s) reported by the Matter stack.
 *
 * The callback function receives a call for any attribute change that
 * passes the filtering criteria. The filtering criteria which determines when
 * the function gets called. All registered callbacks that pass the filter for a
 * particular Matter attribute change get called.
 *
 * \param entry is a pointer to the permanently available callback entry
 * to register.
 * \returns AE_OK if successful, otherwise, an error.
 */
enum ada_err adm_attribute_change_cb_register(
    const struct adm_attribute_change_callback *entry);

/**
 * Write an update to an attribute value to the Matter stack.
 *
 * The application uses this function to modify the value of an attribute
 * in the Matter stack. For example, set an OnOff attribute to on or off state.
 *
 * \param endpoint is the endpoint the write applies to.
 * \param cluster is the cluster containing the attribute.
 * \param attribute is the attribute the write applies to.
 * \param value is a pointer to the value data.
 * \param type indicates the type of the value data.
 * \returns AE_OK if successful, otherwise, an error.
 */
enum ada_err adm_write_attribute(u16 endpoint, u32 cluster, u32 attribute,
    u8 *value, u8 type);

/**
 * Write a Boolean Matter attribute.
 *
 * \param endpoint is the endpoint the write applies to.
 * \param cluster is the cluster containing the attribute.
 * \param attribute is the attribute the write applies to.
 * \param value is the value to write.
 * \returns AE_OK if successful, otherwise, an error.
 */
enum ada_err adm_write_boolean(u16 endpoint, u32 cluster, u32 attribute,
    u8 value);

/**
 * Write a u8 Matter attribute.
 *
 * \param endpoint is the endpoint the write applies to.
 * \param cluster is the cluster containing the attribute.
 * \param attribute is the attribute the write applies to.
 * \param value is the value to write.
 * \returns AE_OK if successful, otherwise, an error.
 */
enum ada_err adm_write_u8(u16 endpoint, u32 cluster, u32 attribute, u8 value);

/**
 * Write a u16 Matter attribute.
 *
 * \param endpoint is the endpoint the write applies to.
 * \param cluster is the cluster containing the attribute.
 * \param attribute is the attribute the write applies to.
 * \param value is the value to write.
 * \returns AE_OK if successful, otherwise, an error.
 */
enum ada_err adm_write_u16(u16 endpoint, u32 cluster, u32 attribute, u16 value);

/**
 * Write a u32 Matter attribute.
 *
 * \param endpoint is the endpoint the write applies to.
 * \param cluster is the cluster containing the attribute.
 * \param attribute is the attribute the write applies to.
 * \param value is the value to write.
 * \returns AE_OK if successful, otherwise, an error.
 */
enum ada_err adm_write_u32(u16 endpoint, u32 cluster, u32 attribute, u32 value);

/**
 * Write a s8 Matter attribute.
 *
 * \param endpoint is the endpoint the write applies to.
 * \param cluster is the cluster containing the attribute.
 * \param attribute is the attribute the write applies to.
 * \param value is the value to write.
 * \returns AE_OK if successful, otherwise, an error.
 */
enum ada_err adm_write_s8(u16 endpoint, u32 cluster, u32 attribute, s8 value);

/**
 * Write a s16 Matter attribute.
 *
 * \param endpoint is the endpoint the write applies to.
 * \param cluster is the cluster containing the attribute.
 * \param attribute is the attribute the write applies to.
 * \param value is the value to write.
 * \returns AE_OK if successful, otherwise, an error.
 */
enum ada_err adm_write_s16(u16 endpoint, u32 cluster, u32 attribute, s16 value);

/**
 * Write a s32 Matter attribute.
 *
 * \param endpoint is the endpoint the write applies to.
 * \param cluster is the cluster containing the attribute.
 * \param attribute is the attribute the write applies to.
 * \param value is the value to write.
 * \returns AE_OK if successful, otherwise, an error.
 */
enum ada_err adm_write_s32(u16 endpoint, u32 cluster, u32 attribute, s32 value);

/**
 * Write a string Matter attribute.
 *
 * \param endpoint is the endpoint the write applies to.
 * \param cluster is the cluster containing the attribute.
 * \param attribute is the attribute the write applies to.
 * \param value is the string to write.
 * \returns AE_OK if successful, otherwise, an error.
 */
enum ada_err adm_write_string(u16 endpoint, u32 cluster,
    u32 attribute, char *value);

/**
 * Get a Boolean value from Matter data buffer.
 *
 * \param type is Matter data type.
 * \param size is the number of bytes in the Matter data buffer.
 * \param data is a pointer to the Matter data buffer.
 * \param value is a pointer to the decoded value buffer.
 * \returns AE_OK if successful, otherwise, an error.
 */
enum ada_err adm_get_boolean(u8 type, u16 size, u8 *data, u8 *value);

/**
 * Get a u8 value from Matter data buffer.
 *
 * \param type is Matter data type.
 * \param size is the number of bytes in the Matter data buffer.
 * \param data is a pointer to the Matter data buffer.
 * \param value is a pointer to the decoded value buffer.
 * \returns AE_OK if successful, otherwise, an error.
 */
enum ada_err adm_get_u8(u8 type, u16 size, u8 *data, u8 *value);

/**
 * Get a u16 value from Matter data buffer.
 *
 * \param type is Matter data type.
 * \param size is the number of bytes in the Matter data buffer.
 * \param data is a pointer to the Matter data buffer.
 * \param value is a pointer to the decoded value buffer.
 * \returns AE_OK if successful, otherwise, an error.
 */
enum ada_err adm_get_u16(u8 type, u16 size, u8 *data, u16 *value);

/**
 * Get a u32 value from Matter data buffer.
 *
 * \param type is Matter data type.
 * \param size is the number of bytes in the Matter data buffer.
 * \param data is a pointer to the Matter data buffer.
 * \param value is a pointer to the decoded value buffer.
 * \returns AE_OK if successful, otherwise, an error.
 */
enum ada_err adm_get_u32(u8 type, u16 size, u8 *data, u32 *value);

/**
 * Get a s8 value from Matter data buffer.
 *
 * \param type is Matter data type.
 * \param size is the number of bytes in the Matter data buffer.
 * \param data is a pointer to the Matter data buffer.
 * \param value is a pointer to the decoded value buffer.
 * \returns AE_OK if successful, otherwise, an error.
 */
enum ada_err adm_get_s8(u8 type, u16 size, u8 *data, s8 *value);

/**
 * Get a s16 value from Matter data buffer.
 *
 * \param type is Matter data type.
 * \param size is the number of bytes in the Matter data buffer.
 * \param data is a pointer to the Matter data buffer.
 * \param value is a pointer to the decoded value buffer.
 * \returns AE_OK if successful, otherwise, an error.
 */
enum ada_err adm_get_s16(u8 type, u16 size, u8 *data, s16 *value);

/**
 * Get a s32 value from Matter data buffer.
 *
 * \param type is Matter data type.
 * \param size is the number of bytes in the Matter data buffer.
 * \param data is a pointer to the Matter data buffer.
 * \param value is a pointer to the decoded value buffer.
 * \returns AE_OK if successful, otherwise, an error.
 */
enum ada_err adm_get_s32(u8 type, u16 size, u8 *data, s32 *value);

/**
 * Set the Matter device type of an endpoint.
 *
 * This API is used to change the device type of an endpoint from the default
 * that was configured in the ZAP generated code. It is useful when the same
 * firmware runs on multiple similar devices. Rather than requiring a trivially
 * different firmware images to specify the device type, the device type can
 * be set at runtime.
 *
 * This API has the following limitations:
 *
 * 1) It must be called after the Matter stack has been initialized, such that
 * it overwrites the ZAP default rather than the other way around.
 *
 * 2) It should only be called once during initialization. It allocates some
 * memory that will be leaked if it is called multiple times for the same
 * endpoint.
 *
 * 3) It only supports simple endpoints with one device type.
 *
 * The application uses this function to modify the value of an attribute
 * in the Matter stack. For example, set an OnOff attribute to on or off state.
 *
 * \param endpoint is the endpoint the change applies to.
 * \param device_id the ID of the device type being set.
 * \param version is the version number of the device type supported.
 * \returns AE_OK if successful, otherwise, an error.
 */
enum ada_err adm_endpoint_type_set(u16 endpoint, u16 device_id, u8 version);

/**
 * Set the Certification Declaration (CD)
 *
 * \param cert_declaration is a pointer to the binary cert declaration
 * issued by the CSA for this hardware and software combination. The referenced
 * data must be saved in static data area for the execution lifetime of app.
 * \parma cd_len is the size in bytes of the certification declaration data.
 */
void adm_cd_set(const u8 *cert_declaration, size_t cd_len);

/**
 * Get the Certification Declaration (CD)
 *
 * \param cert_declaration is a pointer for saving the cert declaration.
 * \parma cd_len is the size in bytes of the cert_declaration buffer length.
 * \returns the certification declaration length if successful, otherwise -1.
 */
int adm_cd_get(u8 *cert_declaration, size_t cd_len);

/** Register External Attribute Read/Write callback
 *
 * Register the common external storage attribute callback.
 * Whenever an external storage attribute read or write request
 * is received by the device, the callback will be called.
 *
 * @param[in] callback external storage attribute read or write callback.
 *
 * @param[in] ep_id   - Endpoint ID to read or write.
 * @param[in] cls_id  - Cluster ID to read or write.
 * @param[in] attr_id - Attribute ID to read or write.
 * @param[in] type    - read or write type.
 * @param[in] buff    - Pointer to the buff.
 * @param[in] len     - the buff length.
 *
 * @return AE_OK on success.
 * @return error in case of failure.
 */
void adm_ext_attr_cb_register(enum ada_err (*callback)(
			u16 ep_id, u16 cls_id, u16 attr_id,
			enum adm_attr_rw_type type, u8 *buff, u16 len));

/** Register identification callback
 *
 * Register the common identification callback. Whenever an identify request
 * is received by the device, the callback will be called
 * with the appropriate identify_cb_type_t.
 *
 * @param[in] callback identification update callback.
 *
 * Callback  identification
 *
 * @param[in] type callback type.
 * @param[in] endpoint_id Endpoint ID to identify.
 * @param[in] data Pointer to the data.
 *
 * @return AE_OK on success.
 * @return error in case of failure.
 */
void adm_identify_cb_register(enum ada_err (*callback)(
				enum adm_identify_cb_type type,
				u16 endpoint_id,
				union adm_identify_cb_data *data));

/** Init identification data
 *
 * @param[in] endpoint_id Endpoint ID to identify.
 * @param[in] identify_type identify type.
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
enum ada_err adm_identify_init(u16 endpoint_id,
			enum adm_identify_type identify_type);

#ifdef __cplusplus
}
#endif

#endif
