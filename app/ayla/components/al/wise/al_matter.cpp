/*
 * Copyright 2022 Ayla Networks, Inc.  All rights reserved.
 */
#ifdef AYLA_MATTER_SUPPORT

#include <string.h>
#include <stdio.h>
#include <ayla/utypes.h>
#include <ayla/log.h>
#include <ayla/mod_log.h>
#include <al/al_matter.h>
#include "wise_log.h"

#include <platform/internal/CHIPDeviceLayerInternal.h>

#include <platform/PlatformManager.h>
#include <platform/KeyValueStoreManager.h>
#include <platform/senscomm/scm1612s/NetworkCommissioningWiFiDriver.h>
#include <platform/senscomm/scm1612s/ConfigurationManagerImpl.h>
#include <app/clusters/network-commissioning/network-commissioning.h>

#include "scm_wifi.h"

#if CHIP_DEVICE_CONFIG_ENABLE_CHIPOBLE
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#endif

using namespace chip::DeviceLayer;
using namespace chip::DeviceLayer::Internal;

namespace {

#if CHIP_DEVICE_CONFIG_ENABLE_WIFI
static chip::app::Clusters::NetworkCommissioning::Instance
    wifi_commissioner(0, &(NetworkCommissioning::WiseWiFiDriver::GetInstance()));
#endif

} // namespace

extern "C" {

int al_matter_server_init(void)
{
#if CHIP_DEVICE_CONFIG_ENABLE_WIFI
	wifi_commissioner.Init();
#endif
	return 0;
}

static int al_matter_config_u32_update(SCM1612SConfig::Key key, uint32_t value,
    u8 overwrite)
{
	CHIP_ERROR chip_err;
	uint32_t curr_value;

	if (!overwrite && SCM1612SConfig::ConfigValueExists(key)) {
		return 0;
	}

	chip_err = SCM1612SConfig::ReadConfigValue(key, curr_value);
	if (chip_err == CHIP_NO_ERROR && value == curr_value) {
		return 0;
	}

	if (SCM1612SConfig::WriteConfigValue(key, value) != CHIP_NO_ERROR) {
		return 1;
	}

	return 0;
}

static int al_matter_config_str_update(SCM1612SConfig::Key key, const char *str,
    u8 overwrite)
{
	CHIP_ERROR chip_err;
	char buf[100];
	size_t len = sizeof(buf) - 1;

	if (!overwrite && SCM1612SConfig::ConfigValueExists(key)) {
		return 0;
	}

	buf[len] = '\0';

	/*
	 * Don't write if value hasn't changed.
	 */
	chip_err = SCM1612SConfig::ReadConfigValueStr(key, buf, len, len);
	if (chip_err == CHIP_NO_ERROR && !strcmp(str, buf)) {
		return 0;
	}

	if (SCM1612SConfig::WriteConfigValueStr(key, str) != CHIP_NO_ERROR) {
		return -1;
	}

	return 0;
}

int al_matter_serial_number_config_set(const char *serial_number, u8 overwrite)
{
	return al_matter_config_str_update(SCM1612SConfig::kConfigKey_SerialNum,
	    serial_number, overwrite);
}

int al_matter_mfg_date_config_set(const char *mfg_date, u8 overwrite)
{
	return al_matter_config_str_update(
	    SCM1612SConfig::kConfigKey_ManufacturingDate, mfg_date, overwrite);
}

int al_matter_discriminator_config_set(uint16_t discriminator, u8 overwrite)
{
	return al_matter_config_u32_update(
	    SCM1612SConfig::kConfigKey_SetupDiscriminator,
	    static_cast<uint32_t>(discriminator), overwrite);
}

int al_matter_spake2p_config_set(uint32_t iter_count, const char *b64_salt,
    const char *b64_verifier)
{
	if (al_matter_config_u32_update(
	    SCM1612SConfig::kConfigKey_Spake2pIterationCount,
	    iter_count, 1)) {
		return 1;
	}

	if (al_matter_config_str_update(
	    SCM1612SConfig::kConfigKey_Spake2pSalt,
	    b64_salt, 1)) {
		return 1;
	}

	if (al_matter_config_str_update(
	    SCM1612SConfig::kConfigKey_Spake2pVerifier,
	    b64_verifier, 1)) {
		return 1;
	}

	return 0;
}

int al_matter_vendor_id_config_set(uint16_t vendor_id, u8 overwrite)
{
	return al_matter_config_u32_update(
	    SCM1612SConfig::kConfigKey_VendorId,
	    static_cast<uint32_t>(vendor_id), overwrite);
}

int al_matter_product_id_config_set(uint16_t product_id, u8 overwrite)
{
	return al_matter_config_u32_update(
	    SCM1612SConfig::kConfigKey_ProductId,
	    static_cast<uint32_t>(product_id), overwrite);
}

int al_matter_vendor_name_config_set(const char *vendor_name, u8 overwrite)
{
	return al_matter_config_str_update(SCM1612SConfig::kConfigKey_VendorName,
	    vendor_name, overwrite);
}

int al_matter_product_name_config_set(const char *product_name, u8 overwrite)
{
	return al_matter_config_str_update(SCM1612SConfig::kConfigKey_ProductName,
	    product_name, overwrite);
}

int al_matter_hardware_version_config_set(const char *hardware_version,
    u8 overwrite)
{
	return al_matter_config_str_update(
	    SCM1612SConfig::kConfigKey_HardwareVersionString, hardware_version,
	    overwrite);
}

/*
 * Set the unique_id. The Basic cluster spec says the unique_id does not
 * need to be human readable. However, code in the Matter stack reads the
 * unique id using string ops instead of bin ops. Therefore, this API
 * requires the unique_id to be a null terminated c-string.
 *
 * Note: The SCM1612SConfig::WriteConfigValueStr(Key key, const char * str,
 * size_t strLen) uses strcpy so it cannot take binary data.
 */
int al_matter_unique_id_config_set(const char *unique_id, u8 overwrite)
{
	return al_matter_config_str_update(SCM1612SConfig::kConfigKey_UniqueId,
	    unique_id, overwrite);
}

void al_matter_config_reset(void)
{
	/* Erase all values in the chip-config NVS namespace */
	SCM1612SConfig::ClearNamespace(SCM1612SConfig::kConfigNamespace_ChipConfig);

	/* Erase all values in the chip-counters NVS namespace */
	SCM1612SConfig::ClearNamespace(SCM1612SConfig::kConfigNamespace_ChipCounters);

#if CHIP_DEVICE_CONFIG_ENABLE_WIFI
	/* Erase Wi-Fi config */
    scm_wifi_clear_config(WIFI_IF_STA);
#endif

	/* Erase all key-values including fabric info */
	PersistedStorage::KeyValueStoreMgrImpl().ErasePartition();
}

void al_matter_config_erase(void)
{
	SCM1612SConfig::ClearNamespace(SCM1612SConfig::kConfigNamespace_ChipConfig);
	SCM1612SConfig::ClearNamespace(SCM1612SConfig::kConfigNamespace_ChipFactory);
	SCM1612SConfig::ClearNamespace(SCM1612SConfig::kConfigNamespace_ChipCounters);

#if CHIP_DEVICE_CONFIG_ENABLE_WIFI
	/* Erase Wi-Fi config */
    scm_wifi_clear_config(WIFI_IF_STA);
#endif

	/* Erase all key-values including fabric info */
	PersistedStorage::KeyValueStoreMgrImpl().ErasePartition();
}

void al_matter_fully_provisioned(void)
{
#if CHIP_DEVICE_CONFIG_ENABLE_CHIPOBLE
	if (ble_hs_is_enabled()) {
		adm_log(LOG_DEBUG "%s: Stop nimble", __func__);
        nimble_port_stop();
        do {
		    vTaskDelay(100);
        } while (!nimble_port_is_stopped());
	}
#endif
}

void al_matter_ipv6_assigned(void)
{
/* XXX: what should we do?? */
#if 0
	esp_route_hook_init(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"));
#endif
}

int al_matter_ssid_get(char *ssid, size_t len)
{
    wifi_config_t stationConfig;

    int err = scm_wifi_get_config(WIFI_IF_STA, &stationConfig);
    if (err != WISE_OK)
    {
        return -1;
    }

	len--;
	ssid[len] = '\0';
	strncpy(ssid, (char *) stationConfig.sta.ssid, len);

	return 0;
}

} /* extern "C" */
#endif
