/*
 * Copyright 2022 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_AL_MATTER_H__
#define __AYLA_AL_MATTER_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * Platform Matter Interfaces
 */

/**
 * Initialize platform configuration subsystem.
 *
 * \returns 0 on success, -1 on failure
 */
int al_matter_config_init(void);

/**
 * Set the device serial number in the Matter config. Does not rewrite if the
 * value hasn't changed. Only writes if the value doesn't already exist in the
 * config when the overwrite flag is not set.
 *
 * Note: This API does not enforce Matter specification requirements.
 *
 * \param serial_number is a serial number string complying with Matter
 * specifications for the SerialNumber attribute.
 * \param overwrite - 0 do not overwrite an existing value, non-zero overwrite
 * if value is different
 * \returns 0 on success, -1 on failure
 */
int al_matter_serial_number_config_set(const char *serial_number, u8 overwrite);

/**
 * Set the manufacturing date in the Matter config. Does not rewrite if the
 * value hasn't changed. Only writes if the value doesn't already exist in the
 * config when the overwrite flag is not set.
 *
 * Note: This API does not enforce Matter specification requirements.
 *
 * \param mfg_date is the manufacturing date string complying with Matter
 * specifications for the ManufacturingDate attribute. Matter requires the
 * first 8 characters to be in the format YYYYMMDD.
 * \param overwrite - 0 do not overwrite an existing value, non-zero overwrite
 * if value is different
 * \returns 0 on success, -1 on failure
 */
int al_matter_mfg_date_config_set(const char *mfg_date, u8 overwrite);

/**
 * Set the vendor ID in the Matter config. Does not rewrite if the value
 * hasn't changed. Only writes if the value doesn't already exist in the config
 * when the overwrite flag is not set.
 *
 * Note: This API does not enforce Matter specification requirements.
 *
 * \param vendor_id is the ID assigned to the vendor by the CSA
 * \param overwrite - 0 do not overwrite an existing value, non-zero overwrite
 * if value is different
 * \returns 0 on success, -1 on failure
 */
int al_matter_vendor_id_config_set(uint16_t vendor_id, u8 overwrite);

/**
 * Set the product ID in the Matter config. Does not rewrite if the value
 * hasn't changed. Only writes if the value doesn't already exist in the config
 * when the overwrite flag is not set.
 *
 * Note: This API does not enforce Matter specification requirements.
 *
 * \param product_id is the vendor assigned Matter product ID for the product
 * \param overwrite - 0 do not overwrite an existing value, non-zero overwrite
 * if value is different
 * \returns 0 on success, -1 on failure
 */
int al_matter_product_id_config_set(uint16_t product_id, u8 overwrite);

/**
 * Set the vendor name in the Matter config. Does not rewrite if the value
 * hasn't changed. Only writes if the value doesn't already exist in the config
 * when the overwrite flag is not set.
 *
 * Note: This API does not enforce Matter specification requirements.
 *
 * \param vendor_name is the vendor name string complying with Matter
 * specifications for the VendorName attribute
 * \param overwrite - 0 do not overwrite an existing value, non-zero overwrite
 * if value is different
 * \returns 0 on success, -1 on failure
 */
int al_matter_vendor_name_config_set(const char *vendor_name, u8 overwrite);

/**
 * Set the product name in the Matter config. Does not rewrite if the value
 * hasn't changed. Only writes if the value doesn't already exist in the config
 * when the overwrite flag is not set.
 *
 * Note: This API does not enforce Matter specification requirements.
 *
 * \param product_name is the product name string complying with Matter
 * specifications for the ProductName attribute
 * \param overwrite - 0 do not overwrite an existing value, non-zero overwrite
 * if value is different
 * \returns 0 on success, -1 on failure
 */
int al_matter_product_name_config_set(const char *product_name, u8 overwrite);

/**
 * Set the hardware version in the Matter config. Does not rewrite if the value
 * hasn't changed. Only writes if the value doesn't already exist in the config
 * when the overwrite flag is not set.
 *
 * Note: This API does not enforce Matter specification requirements.
 *
 * \param hardware_version is the vendor name string complying with Matter
 * specifications for the HardwareVersion attribute
 * \param overwrite - 0 do not overwrite an existing value, non-zero overwrite
 * if value is different
 * \returns 0 on success, -1 on failure
 */
int al_matter_hardware_version_config_set(const char *hardware_version,
    u8 overwrite);

/**
 * Set the discriminator in the Matter config. Does not rewrite if the value
 * hasn't changed. Only writes if the value doesn't already exist in the config
 * when the overwrite flag is not set.
 *
 * Note: This API does not enforce Matter specification requirements.
 *
 * \param discriminator is a 12-bit value complying with Matter specifications.
 * \param overwrite - 0 do not overwrite an existing value, non-zero overwrite
 * if value is different
 * \returns 0 on success, -1 on failure
 */
int al_matter_discriminator_config_set(uint16_t discriminator, u8 overwrite);

/**
 * Set SPAKE2+ values in config.
 *
 * Note: This API does not enforce Matter specification requirements.
 *
 * \param iter_count is the SPAKE2+ iteration count complying with the Matter
 * specifications for PBKDF iterations
 * \param b64_salt is the base64 encoded SPAKE2+ salt complying with the Matter
 * specifications for PBKDF salt
 * \param b64_verifier is the base64 encoded SPAKE2+ verifier generated
 * according to the Matter specifications
 * \returns 0 on success, -1 on failure
 */
int al_matter_spake2p_config_set(uint32_t iter_count, const char *b64_salt,
    const char *b64_verifier);

/**
 * Initialize platform resources needed by Matter server.
 *
 * \returns 0 on success, -1 on failure
 */
int al_matter_server_init(void);

/**
 * Factory reset Matter config.
 */
void al_matter_config_reset(void);

/**
 * Erase all of the Matter config including the Matter factory config.
 *
 * This API is for testing of credential provisioning features. It is not
 * intended to be used in normal operation.
 */
void al_matter_config_erase(void);

/**
 * Indicate to AL layer that Matter has been fully provisioned.
 *
 * This may be used to shutdown the BLE interface, etc.
 */
void al_matter_fully_provisioned(void);

/**
 * Indicate to AL layer that an IPv6 address has been assigned.
 */
void al_matter_ipv6_assigned(void);

/**
 * Get the SSID of the Wi-Fi network connection, if connected.
 *
 * \returns length of SSID or 0 if its not available
 */
int al_matter_ssid_get(char *ssid, size_t len);

void adm_log(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* __AYLA_AL_MATTER_H__ */
