/*
 * Copyright 2022 Ayla Networks, Inc.  All rights reserved.
 */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <ayla/log.h>
#include <ayla/conf.h>
#include <ayla/base64.h>
#include <al/al_matter.h>
#include <CHIPVersion.h>
#include <lib/support/Span.h>
#include <platform/CommissionableDataProvider.h>
#include <platform/DeviceInstanceInfoProvider.h>
#include <platform/ConfigurationManager.h>
#include <platform/DiagnosticDataProvider.h>
#include <credentials/DeviceAttestationCredsProvider.h>
#include <credentials/CHIPCert.h>
#include <crypto/CHIPCryptoPAL.h>
#include <platform/pfm_matter.h>
#include <adm/adm.h>
#include "adm_int.h"
#include "adm_data_provider.h"

#define PAIRING_CODE_LEN	11	/* manual pairing code string chars */
#define QR_PAYLOAD_LEN		22	/* QR code payload string chars */

using namespace chip;
using namespace chip::DeviceLayer;
using namespace chip::Credentials;
using namespace chip::DeviceLayer::Ayla;

extern "C" {

struct adm_set_item {
	const char *name;
	const AlMatterConfig::Key key;
	int (*handler)(struct adm_set_item *item, const char *value);
	u32 constraint0;
	u32 constraint1;
};

extern const char adm_cli_help[] = "matter ["
	"certs"
	"|erase"
	"|info"
	"|log [all|<mod> enable|disable]"
	"|qrgen [secret]"
	"|qrshow [secret]"
	"|reset"
	"|show"
	"|set <item> <value>"
	"|spake"
	"]";

static int adm_cli_log(int argc, char **argv)
{
	const char *mod;
	int i;

	if (argc == 2) {
#ifndef AYLA_SCM_SUPPORT
        /* To avoid shadowing warning */
		int i = 0;
#endif
		for (i = 0, mod = adm_log_mod_name(i); mod;
		    i++, mod = adm_log_mod_name(i)) {
			printcli("\t%3s: %s", mod,
			    adm_log_enabled(i) ? "enabled" : "disabled");
		}
		return 0;
	}

	if (argc == 4) {
		if (!strcmp(argv[2], "all")) {
			i = ADM_LOG_INDEX_ALL;
		} else {
			for (i = 0; argv[2][i]; i++) {
				argv[2][i] = toupper(argv[2][i]);
			}
			i = adm_log_index(argv[2]);
			if (i < 0) {
				printcli("Unknown log module: %s", argv[2]);
				return 0;
			}
		}
		if (!strcmp(argv[3], "disable")) {
			adm_log_disable(i);
			return 0;
		} else if (!strcmp(argv[3], "enable")) {
			adm_log_enable(i);
			return 0;
		} else {
			goto error_exit;
		}
	}

error_exit:
	return -1;
}

static int adm_cli_qr_payload_show(int argc, char **argv)
{
	u32 passcode;
	u16 vendor;
	u16 product;
	u16 discriminator;
	u8 discovery_mask;
	const char *secret;
	char pairing_str[PAIRING_CODE_LEN+1];
	char qr_str[QR_PAYLOAD_LEN+1];
	enum ada_err err;

	if (argc == 2) {
		secret = NULL;
	} else if (argc == 3) {
		secret = argv[2];
	} else {
		printcli("too many arguments");
		return -1;
	}

	/*
	 * Reload the device's credentials because typically this command
	 * will be run in the factory immediately after the credentials are
	 * configured. The credentials loaded at boot up in the factory
	 * will be test credentials because credentials didn't exist in
	 * the config. Doing the reload ensures the correct vendor ID and
	 * product ID will be extracted from the DAC if the certs were written
	 * to the config since the last reset.
	 */
	adm_credentials_load();

	err = adm_onboarding_config_get(secret, &passcode, &vendor, &product,
	    &discovery_mask, &discriminator, pairing_str, sizeof(pairing_str),
	    qr_str, sizeof(qr_str));

	if (err) {
		printcli("error: invalid secret or non-qrgen config");
		return -1;
	}
	printcli("vendor:\t\t%u (0x%02x)", vendor, vendor);
	printcli("product:\t%u (0x%02x)", product, product);
	printcli("discovery mask:\t0x%02x", discovery_mask);
	printcli("discriminator:\t%u", discriminator);

	if (!passcode)  {
		printcli("invalid secret or non-qrgen config");
		return 0;
	}

	printcli("passcode:\t%08lu", passcode);
	printcli("pairing code:\t%s", pairing_str);
	printcli("QR payload:\t%s", qr_str);

	/*
	 * Cheat on the URL encoding here. The first 3 characters are defined
	 * by the spec as "MT:". The colon needs to be escaped with "%3A".
	 */
	ASSERT(strlen(qr_str) > 3);
	printcli("QR code URL:\t"
	    "https://project-chip.github.io/connectedhomeip/qrcode.html"
	    "?data=MT%%3A%s", qr_str + 3);

	return 0;
}

static int adm_cli_qr_generate(int argc, char **argv)
{
	enum ada_err err;

	if (argc == 2) {
		err = adm_onboard_config_generate(NULL);
	} else if  (argc == 3) {
		err = adm_onboard_config_generate(argv[2]);
	} else {
		printcli("too many arguments");
		return -1;
	}

	if (err) {
		printcli("failure %d", err);
		return -1;
	}

	printcli("success");
	return 0;
}

static int adm_cli_info_show(void)
{
	DeviceInstanceInfoProvider *diip = GetDeviceInstanceInfoProvider();
	CommissionableDataProvider *cdp = GetCommissionableDataProvider();
	ConfigurationManager &cm =
	    ConfigurationManagerImpl::GetDefaultInstance();
	AdmDataProvider *admp = AdmDataProvider::GetAdmDataProvider();
	uint8_t temp8;
	uint16_t temp16;
	uint32_t temp32;
	size_t length;
	uint16_t year;
	uint8_t month;
	uint8_t day;
	uint8_t buf[64];
	MutableByteSpan mutable_span;
	ChipError chip_err;

	buf[0] = '\0';
	diip->GetVendorName((char *)buf, sizeof(buf));
	printcli("vendor:\t\t\t%s", buf);

	buf[0] = '\0';
	diip->GetProductName((char *)buf, sizeof(buf));
	printcli("product:\t\t%s", buf);

	buf[0] = '\0';
	diip->GetSerialNumber((char *)buf, sizeof(buf));
	printcli("serial #:\t\t%s", buf);

	memset(buf, 0, sizeof(buf));
	cm.GetPrimaryWiFiMACAddress(buf);
	printcli("MAC address:\t\t%02x:%02x:%02x:%02x:%02x:%02x", buf[0],
	    buf[1], buf[2], buf[3], buf[4], buf[5]);

	memset(buf, 0, sizeof(buf));
	cm.GetUniqueId((char *)buf, sizeof(buf));
	printcli("unique id:\t\t%s", buf);

	memset(buf, 0, sizeof(buf));
	mutable_span = MutableByteSpan(buf);
	chip_err = diip->GetRotatingDeviceIdUniqueId(mutable_span);
	if (chip_err == CHIP_NO_ERROR) {
		dumpcli("rotating id:\t\t",
		    mutable_span.data(), mutable_span.size());
	} else {
		printcli("rotating id:");
	}

	diip->GetHardwareVersion(temp16);
	printcli("HW version #:\t\t%u", temp16);

	buf[0] = '\0';
	diip->GetHardwareVersionString((char *)buf, sizeof(buf));
	printcli("HW version:\t\t%s", buf);

	temp32 = 0;
	cm.GetSoftwareVersion(temp32);
	printcli("SW version #:\t\t%lu", temp32);

	buf[0] = '\0';
	cm.GetSoftwareVersionString((char *)buf, sizeof(buf));
	printcli("SW version:\t\t%s", buf);

	diip->GetVendorId(temp16);
	printcli("vendor id:\t\t%u (0x%04X) %s", temp16, temp16,
			temp16 == ADM_TEST_VENDOR_ID ? "TEST" : "");

	diip->GetProductId(temp16);
	printcli("product id:\t\t%u (0x%04X)", temp16, temp16);

	temp32 = 0;
	cm.GetDeviceTypeId(temp32);
	printcli("device type id:\t\t%lu", temp32);

	temp16 = 0;
	cdp->GetSetupDiscriminator(temp16);
	printcli("discriminator:\t\t%u", temp16);

	year = 0;
	month = 0;
	day = 0;
	diip->GetManufacturingDate(year, month, day);
	printcli("manufacturing date:\t%04u-%02u-%02u", year, month, day);

	memset(buf, 0, sizeof(buf));
	admp->GetPartNumber((char *)buf, sizeof(buf));
	printcli("part number:\t\t%s", buf);

	memset(buf, 0, sizeof(buf));
	admp->GetProductURL((char *)buf, sizeof(buf));
	printcli("product URL:\t\t%s", buf);

	memset(buf, 0, sizeof(buf));
	admp->GetProductLabel((char *)buf, sizeof(buf));
	printcli("product label:\t\t%s", buf);

	memset(buf, 0, sizeof(buf));
	cm.GetCommissionableDeviceName((char *)buf, sizeof(buf));
	printcli("commissionable name:\t%s", buf);

	temp8 = 0;
	cm.GetRegulatoryLocation(temp8);
	printcli("regulatory location:\t%u", temp8);

	length = 0;
	cm.GetCountryCode((char *)buf, sizeof(buf), length);
	if (length >= sizeof(buf)) {
		length = sizeof(buf) - 1;
	}
	buf[length] = '\0';
	printcli("country code:\t\t%s", buf);

	temp32 = 0;
	cm.GetBootReason(temp32);
	printcli("boot reason:\t\t%lu", temp32);

	temp32 = 0;
	cm.GetRebootCount(temp32);
	printcli("reboot count:\t\t%lu", temp32);

	temp32 = 0;
	cm.GetTotalOperationalHours(temp32);
	printcli("total oper. hours:\t%lu", temp32);

	return 0;
}

static int adm_cli_certs_show(void)
{
	DeviceAttestationCredentialsProvider *dacp =
	    GetDeviceAttestationCredentialsProvider();
	size_t length = Credentials::kMaxDERCertLength;
	uint8_t *buf;
	MutableByteSpan mutable_span;

	buf =  (uint8_t *)malloc(length);
	if (!buf) {
		printcli("malloc failed");
		return -1;
	}

	memset(buf, 0, length);
	mutable_span = MutableByteSpan(buf, length);
	dacp->GetDeviceAttestationCert(mutable_span);
	dumpcli("DAC:\t\t\t", mutable_span.data(), mutable_span.size());

	memset(buf, 0, length);
	mutable_span = MutableByteSpan(buf, length);
	dacp->GetProductAttestationIntermediateCert(mutable_span);
	dumpcli("PAI cert.:\t\t", mutable_span.data(), mutable_span.size());

	memset(buf, 0, length);
	mutable_span = MutableByteSpan(buf, length);
	dacp->GetCertificationDeclaration(mutable_span);
	dumpcli("cert. declaration:\t", mutable_span.data(),
	    mutable_span.size());

	free(buf);
	return 0;
}

static int adm_cli_spake_show(void)
{
	CommissionableDataProvider *cdp = GetCommissionableDataProvider();
	uint32_t temp32 = 0;
	size_t length = 1024;
	uint8_t *buf;
	MutableByteSpan mutable_span;

	buf = (uint8_t *)malloc(length);
	if (!buf) {
		printcli("malloc failed");
		return -1;
	}

	cdp->GetSpake2pIterationCount(temp32);
	printcli("SPAKE2+ iter. count:\t%lu", temp32);

	memset(buf, 0, length);
	mutable_span = MutableByteSpan(buf, length);
	cdp->GetSpake2pSalt(mutable_span);
	dumpcli("SPAKE2+ salt:\t\t", mutable_span.data(), mutable_span.size());

	memset(buf, 0, length);
	mutable_span = MutableByteSpan(buf, length);
	cdp->GetSpake2pVerifier(mutable_span, length);
	dumpcli("SPAKE2+ verifier:\t", mutable_span.data(), mutable_span.size());

	free(buf);
	return 0;
}

static int adm_cli_wifi_show(void)
{
	char ssid[33] = { 0 };	/* max SSID is 32 + 1 for nul */
	MutableByteSpan bssid;
	int8_t rssi = -128;
	uint16_t channel = 0;
	const u8 *p;

	al_matter_ssid_get(ssid, sizeof(ssid));
	printcli("SSID:\t\t%s", ssid);

	/*
	 * The span data pointer points to a static array when the call returns.
	 * There's no need to create the span with a buffer to back it.
	 */
	GetDiagnosticDataProvider().GetWiFiBssId(bssid);
        if (bssid.size() == 6)
        {
        	p = bssid.data();
        	printcli("BSSID:\t\t%02x:%02x:%02x:%02x:%02x:%02x",
        	    p[0], p[1], p[2], p[3], p[4], p[5]);
        }

	GetDiagnosticDataProvider().GetWiFiChannelNumber(channel);
	printcli("Channel:\t\%u", channel);

	GetDiagnosticDataProvider().GetWiFiRssi(rssi);
	printcli("RSSI:\t\t%d", rssi);

	return 0;
}

static int adm_cli_set_u32(struct adm_set_item *item, const char *value_string)
{
	CHIP_ERROR chip_err;
	uint32_t value;
	char *errptr;
	size_t len = strlen(value_string);

	if (len == 0) {
		chip_err = AlMatterConfig::ClearConfigValue(item->key);
		if (chip_err != CHIP_NO_ERROR) {
			printcli("clear failed: %s", ErrorStr(chip_err));
			return -1;
		}
		return 0;
	}

	value = strtoul(value_string, &errptr, 10);
	if (*errptr != '\0' || value < item->constraint0 ||
	    value > item->constraint1) {
		printcli("bad value: %s", value_string);
		return -1;
	}

	chip_err = AlMatterConfig::WriteConfigValue(item->key, value);
	if (chip_err != CHIP_NO_ERROR) {
		printcli("write failed: %s", ErrorStr(chip_err));
		return -1;
	}

	return 0;
}

static int adm_cli_set_string(struct adm_set_item *item, const char *value_string)
{
	CHIP_ERROR chip_err;
	size_t len = strlen(value_string);

	if (len == 0) {
		chip_err = AlMatterConfig::ClearConfigValue(item->key);
		if (chip_err != CHIP_NO_ERROR) {
			printcli("clear failed: %s", ErrorStr(chip_err));
			return -1;
		}
		return 0;
	}

	if (len < item->constraint0 || len > item->constraint1) {
		printcli("invalid length: %s", value_string);
		return -1;
	}

	chip_err = AlMatterConfig::WriteConfigValueStr(item->key, value_string);
	if (chip_err != CHIP_NO_ERROR) {
		printcli("write failed: %s", ErrorStr(chip_err));
		return -1;
	}

	return 0;
}

static int adm_cli_set_blob(struct adm_set_item *item, const char *value_string)
{
	CHIP_ERROR chip_err;
	size_t len = item->constraint1;
	u8 *buf;

	if (value_string[0] == '\0') {
		chip_err = AlMatterConfig::ClearConfigValue(item->key);
		if (chip_err != CHIP_NO_ERROR) {
			printcli("clear failed: %s", ErrorStr(chip_err));
			return -1;
		}
		return 0;
	}

	/*
	 * Allocate buffer big enough for the largest size blob.
	 */
	buf = (u8 *)malloc(len);
	if (!buf) {
		printcli("alloc of %u bytes failed", len);
		return -1;
	}

	if (ayla_base64_decode(value_string, strlen(value_string), buf, &len)) {
		printcli("base64 decode failed");
		goto failure;
	}

	if (len < item->constraint0 || len > item->constraint1) {
		printcli("invalid length: %s", value_string);
		goto failure;
	}

	chip_err = AlMatterConfig::WriteConfigValueBin(item->key, buf, len);
	if (chip_err != CHIP_NO_ERROR) {
		printcli("write failed: %s", ErrorStr(chip_err));
		goto failure;
	}

	free(buf);
	return 0;

failure:
	free(buf);
	return -1;
}

static int adm_cli_set_mfg_date(struct adm_set_item *item, const char *value_string)
{
	CHIP_ERROR chip_err;
	size_t len = strlen(value_string);
	char buf[5];
	u32 value;
	u8 month;
	char *errptr;
	static const u8 month_days[] =
	    { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

	if (len == 0) {
		chip_err = AlMatterConfig::ClearConfigValue(item->key);
		if (chip_err != CHIP_NO_ERROR) {
			printcli("clear failed: %s", ErrorStr(chip_err));
			return -1;
		}
		return 0;
	}

	/*
	 * Required date format is YYYY-MM-DD<vendor_defined>.
	 *
	 * Do a basic sanity check of the date value for confidence the Matter
	 * stack will be able to parse it.
	 */
	if (len < item->constraint0 || len > item->constraint1) {
		goto invalid_date;
	}

	if (value_string[4] != '-' || value_string[7] != '-') {
		goto invalid_date;
	}

	/* year is 4 digit decimal number */
	memcpy(buf, value_string, 4);
	buf[4] = '\0';
	value = strtoul(buf, &errptr, 10);
	if (*errptr != '\0') {
		goto invalid_date;
	}

	/* month is 2 digits from 01 through 12 */
	memcpy(buf, value_string + 5, 2);
	buf[2] = '\0';
	value = strtoul(buf, &errptr, 10);
	if (*errptr != '\0' || value < 1 || value > 12) {
		goto invalid_date;
	}
	month = value - 1;

	/* day is 2 digits from 01 to 31 */
	memcpy(buf, value_string + 8, 2);
	buf[2] = '\0';
	value = strtoul(buf, &errptr, 10);
	if (*errptr != '\0' || value < 1 || value > month_days[month]) {
		goto invalid_date;
	}

	chip_err = AlMatterConfig::WriteConfigValueStr(item->key, value_string);
	if (chip_err != CHIP_NO_ERROR) {
		printcli("write failed: %s", ErrorStr(chip_err));
		return -1;
	}
	return 0;

invalid_date:
	printcli("invalid value: %s", value_string);
	printcli("date format: YYYY-MM-DD<vendor_defined> (max %lu characters)",
	    item->constraint1);
	return -1;
}

static struct adm_set_item adm_set_items[] = {
	{
	    "country-code",
	    AlMatterConfig::kConfigKey_CountryCode,
	    adm_cli_set_string,
	    ConfigurationManager::kMaxLocationLength, /* always 2 bytes */
	    ConfigurationManager::kMaxLocationLength
	},
	{
	    "dac-cert",
	    AlMatterConfig::kConfigKey_DACCert,
	    adm_cli_set_blob,
	    1,
	    Credentials::kMaxDERCertLength
	},
	{
	    "dac-priv-key",
	    AlMatterConfig::kConfigKey_DACPrivateKey,
	    adm_cli_set_blob,
	    Crypto::kP256_PrivateKey_Length,
	    Crypto::kP256_PrivateKey_Length
	},
	{
	    "discriminator",
	    AlMatterConfig::kConfigKey_SetupDiscriminator,
	    adm_cli_set_u32,
	    0,
	    kMaxDiscriminatorValue
	},
	{
	    "hardware-ver",
	    AlMatterConfig::kConfigKey_HardwareVersion,
	    adm_cli_set_u32,
	    0,
	    MAX_U16
	},
	{
	    "hw-ver-str",
	    AlMatterConfig::kConfigKey_HardwareVersionString,
	    adm_cli_set_string,
	    1,
	    ConfigurationManager::kMaxHardwareVersionStringLength
	},
	{
	    "mfg-date",
	    AlMatterConfig::kConfigKey_ManufacturingDate,
	    adm_cli_set_mfg_date,
	    10,		/* YYYY-MM-DD is minimum */
	    ConfigurationManager::kMaxManufacturingDateLength
	},
#ifdef TODO	/* doesn't seem to be fully supported in Matter v1.0-branch */
	{
	    "part-number",
	    AlMatterConfig::kConfigKey_PartNumber,
	    adm_cli_set_string,
	    1,
	    ConfigurationManager::kMaxPartNumberLength
	},
#endif
	{
	    "pai-cert",
	    AlMatterConfig::kConfigKey_PAICert,
	    adm_cli_set_blob,
	    1,
	    Credentials::kMaxDERCertLength
	},
	{
	    "product-name",
	    AlMatterConfig::kConfigKey_ProductName,
	    adm_cli_set_string,
	    1,
	    ConfigurationManager::kMaxProductNameLength
	},
	{
	    "product-label",
	    AlMatterConfig::kConfigKey_ProductLabel,
	    adm_cli_set_string,
	    1,
	    ConfigurationManager::kMaxProductLabelLength
	},
	{
	    "product-url",
	    AlMatterConfig::kConfigKey_ProductURL,
	    adm_cli_set_string,
	    1,
	    ConfigurationManager::kMaxProductURLLength
	},
	{
	    "reg-location",
	    AlMatterConfig::kConfigKey_RegulatoryLocation,
	    adm_cli_set_u32,
	    0,
	    2	/* 0 = Indoor, 1 = Outdoor, 2 = IndoorOutdoor */
	},
	{
	    "spake-iter-cnt",
	    AlMatterConfig::kConfigKey_Spake2pIterationCount,
	    adm_cli_set_u32,
	    Crypto::kSpake2p_Min_PBKDF_Iterations,
	    Crypto::kSpake2p_Max_PBKDF_Iterations
	},
	{
	    "spake-salt",
	    AlMatterConfig::kConfigKey_Spake2pSalt,
	    adm_cli_set_string,
	    BASE64_LEN_EXPAND(Crypto::kSpake2p_Min_PBKDF_Salt_Length),
	    BASE64_LEN_EXPAND(Crypto::kSpake2p_Max_PBKDF_Salt_Length)
	},
	{
	    "spake-verifier",
	    AlMatterConfig::kConfigKey_Spake2pVerifier,
	    adm_cli_set_string,
	    BASE64_LEN_EXPAND(Crypto::kSpake2p_VerifierSerialized_Length),
	    BASE64_LEN_EXPAND(Crypto::kSpake2p_VerifierSerialized_Length)
	},
	{
	    "vendor-name",
	    AlMatterConfig::kConfigKey_VendorName,
	    adm_cli_set_string,
	    1,
	    ConfigurationManager::kMaxVendorNameLength
	},
	{
	    "rotating-uid",
	    AlMatterConfig::kConfigKey_RotatingDevIdUniqueId,
	    adm_cli_set_blob,
	    CHIP_DEVICE_CONFIG_ROTATING_DEVICE_ID_UNIQUE_ID_LENGTH,
	    CHIP_DEVICE_CONFIG_ROTATING_DEVICE_ID_UNIQUE_ID_LENGTH
	},
};

static int adm_cli_set(int argc, char **argv)
{
	int i;
	struct adm_set_item *item;

	if (argc != 4) {
		return -1;
	}

	for (i = 0; i < ARRAY_LEN(adm_set_items); i++) {
		item = &adm_set_items[i];
		if (!strcmp(argv[2], item->name)) {
			return item->handler(item, argv[3]);
		}
	}

	printcli("unknown item %s", argv[2]);
	return -1;
}

void adm_cli(int argc, char **argv)
{
	int i;
	struct adm_set_item *item;
	const char *type;

	if (argc == 1) {
		adm_cli_info_show();
		return;
	}
	if (argc < 2) {
		goto usage;
	}
	if (argc == 2) {
		if (!strcmp(argv[1], "certs")) {
			adm_cli_certs_show();
			return;
		}
		if (!strcmp(argv[1], "erase")) {
			if (!mfg_or_setup_mode_ok()) {
				return;
			}
			al_matter_config_erase();
			return;
		}
		if (!strcmp(argv[1], "help")) {
			goto usage;
		}
		if (!strcmp(argv[1], "info")) {
			adm_cli_info_show();
			return;
		}
		if (!strcmp(argv[1], "reset")) {
			al_matter_config_reset();
			return;
		}
		if (!strcmp(argv[1], "spake")) {
			adm_cli_spake_show();
			return;
		}
		if (!strcmp(argv[1], "wifi")) {
			adm_cli_wifi_show();
			return;
		}
	}
	if (argc >= 2) {
		if (!strcmp(argv[1], "log")) {
			if (adm_cli_log(argc, argv)) {
				goto usage;
			}
			return;
		}
		if (!strcmp(argv[1], "qrgen")) {
			if (!mfg_or_setup_mode_ok()) {
				return;
			}
			if (adm_cli_qr_generate(argc, argv)) {
				goto usage;
			}
			return;
		}
		if (!strcmp(argv[1], "qrshow")) {
			if (adm_cli_qr_payload_show(argc, argv)) {
				goto usage;
			}
			return;
		}
		if (!strcmp(argv[1], "set")) {
			if (!mfg_or_setup_mode_ok()) {
				return;
			}
			if (adm_cli_set(argc, argv)) {
				goto usage;
			}
			return;
		}
	}
usage:
	printcli("usage: %s", adm_cli_help);
	printcli("    certs - show Matter certificates");
	printcli("    erase - erase all Matter config, including factory");
	printcli("    info - show Matter info");
	printcli("    log - log control commands");
	printcli("    qrgen [secret] - generate and config onboarding info");
	printcli("    qrshow [secret] - show onboarding info");
	printcli("    reset - factory reset Matter config");
	printcli("    set <item> <value> - set config item value (\"\" to clear)");
	printcli("        Item\t\tType");

	for (i = 0; i < ARRAY_LEN(adm_set_items); i++) {
		item = &adm_set_items[i];
		type = "";
		if (item->handler == adm_cli_set_u32) {
			type = "number";
		} else if (item->handler == adm_cli_set_string) {
			type = "string";
		} else if (item->handler == adm_cli_set_mfg_date) {
			type = "date (YYYY-MM-DD<vendor-defined>)";
		} else {
			type = "binary object (base64 encoded)";
		}
		printcli("        %s\t%s", item->name, type);
	}

	printcli("    spake - show SPAKE2+ info");
	printcli("    wifi - show Wi-Fi info");
}

} /* extern C */
