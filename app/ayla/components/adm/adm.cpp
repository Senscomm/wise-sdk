/*
 * Copyright 2022 Ayla Networks, Inc.  All rights reserved.
 */
#include <string.h>
#include <stdio.h>
#include <CHIPVersion.h>
#include <crypto/CHIPCryptoPAL.h>
#include <lib/support/Span.h>
#include <app/util/af.h>
#include <app/util/attribute-storage.h>
#include <platform/CHIPDeviceEvent.h>
#include <platform/PlatformManager.h>
#include <platform/CommissionableDataProvider.h>
#include <platform/DeviceInstanceInfoProvider.h>
#include <app/server/AppDelegate.h>
#include <app/clusters/identify-server/identify-server.h>
#include <app-common/zap-generated/attributes/Accessors.h>
#include <protocols/interaction_model/StatusCode.h>
#include <app/server/OnboardingCodesUtil.h>
#include <app/server/Dnssd.h>
#include <app/server/Server.h>
#include <ayla/utypes.h>
#include <ayla/assert.h>
#include <ayla/log.h>
#include <ayla/mod_log.h>
#include <ayla/conf.h>
#include <ayla/base64.h>
#include <ada/err.h>
#include <ada/ada_conf.h>
#include <al/al_hmac_sha256.h>
#ifdef AYLA_WIFI_SUPPORT
#include <ayla/conf_token.h>
#include <adw/wifi.h>
#else
#include <ada/ada_wifi.h>
#endif
#include <platform/pfm_matter.h>
#include <al/al_matter.h>
#include <adm/adm.h>
#include "adm_int.h"
#include "adm_data_provider.h"
#include <al/al_os_mem.h>

#ifndef AYLA_MATTER_DISCOVERY_MASK
#error "AYLA_MATTER_DISCOVERY_MASK not defined"
#endif

using namespace chip;
using namespace chip::app;
using namespace chip::DeviceLayer;
using namespace chip::DeviceLayer::Internal;
using namespace chip::Inet;
using namespace chip::Credentials;
using namespace chip::DeviceLayer::Ayla;
using namespace chip::Protocols::InteractionModel;

#ifndef ADM_ATTR_CHANGE_CB_MAX
#define ADM_ATTR_CHANGE_CB_MAX	10
#endif

#ifndef ADM_EVENT_CB_MAX
#define ADM_EVENT_CB_MAX	3
#endif

#define ADM_SPAKE2P_ITERATIONS	1000

#define ADM_PASSCODE_MAX	99999998UL	/* range is 1 to max */

/*
 * Length of SHA256 hash used to auto create onboarding credentials, lengths
 * of fields extracted and offsets to fields
 */
#define ADM_HASH_LEN		32	/* HMAC SHA256 output length */

#define ADM_HASH_PASSCODE_LEN	4	/* 32 bits mod'd to 8 decimal digits */
#define ADM_HASH_DISCR_LEN	2	/* 12 bit discriminator */
#define ADM_HASH_SALT_LEN	16

#define ADM_HASH_PASSCODE_OFF	0
#define ADM_HASH_DISCR_OFF	(ADM_HASH_PASSCODE_OFF + ADM_HASH_PASSCODE_LEN)
/* reserved gap */
#define ADM_HASH_SALT_OFF	(ADM_HASH_LEN - ADM_HASH_SALT_LEN)

#if ADM_HASH_SALT_OFF < (ADM_HASH_DISCR_OFF + ADM_HASH_DISCR_LEN)
#error invalid hash definition
#endif

/*
 * One of the secrets used as a factor in creating automatically generated
 * QR codes. It is recommended that OEM vendors define their own secret(s)
 * for their apps, if using the automatic QR code generation.
 *
 * It is recommended that OEM vendors who use built-in QR code generation
 * define their own secret(s).
 */
#ifndef AYLA_MATTER_QRGEN_SECRET
#define AYLA_MATTER_QRGEN_SECRET	"QXlsYS1NYXR0ZXItUVJDb2Rl"
#endif

static const char *adm_qrgen_secret = AYLA_MATTER_QRGEN_SECRET;

static u8 adm_matter_initialized;
static const struct adm_attribute_change_callback
    *attr_change_cbs[ADM_ATTR_CHANGE_CB_MAX];
static void (*adm_event_cbs[ADM_ATTR_CHANGE_CB_MAX])(enum adm_event_id id);

static const char *matter_log_mods[] = {
		/*
		 * NOTE:  The order of this list is ASCII sort order for
		 *	  strcmp based searching.
		 */
		"-",	/* None */
		"AL",	/* Alarm */
		"ASN",	/* ASN1 */
		"ATM",	/* Automation */
		"BDX",	/* BulkDataTransfer */
		"BLE",	/* BLE */
		"CR",	/* Crypto */
		"CSL",	/* chipSystemLayer */
		"CSM",	/* CASESessionManager */
		"CTL",	/* Controller */
		"DC",	/* DeviceControl */
		"DD",	/* DeviceDescription */
		"DIS",	/* Discovery */
		"DL",	/* DeviceLayer */
		"DMG",	/* DataManagement */
		"ECH",	/* Echo */
		"EM",	/* ExchangeManager */
		"EVL",	/* Event Logging */
		"FP",	/* FabricProvisioning */
		"FS",	/* FailSafe */
		"HB",	/* Heartbeat */
		"IM",	/* InteractionModel */
		"IN",	/* Inet */
		"ML",	/* MessageLayer */
		"NP",	/* NetworkProvisioning */
		"OSS",	/* OperationalSessionSetup */
		"SC",	/* SecureChannel */
		"SD",	/* ServiceDirectory */
		"SH",	/* Shell */
		"SM",	/* SecurityManager */
		"SP",	/* ServiceProvisioning */
		"SPL",	/* SetupPayload */
		"SPT",	/* Support */
		"SVR",	/* AppServer */
		"SWU",	/* SoftwareUpdate */
		"TLV",	/* TLV */
		"TOO",	/* chipTool */
		"TS",	/* TimeService */
		"TST",	/* Test */
		"ZCL",	/* Zcl */
};

static u64 chip_log_disable_mask;

static enum ada_err (*adm_identify_cb)(
				enum adm_identify_cb_type type,
				u16 endpoint_id,
				union adm_identify_cb_data *data);

const char *adm_log_mod_name(int index)
{
	if (index < 0 || index >= ARRAY_LEN(matter_log_mods)) {
		return NULL;
	}

	return matter_log_mods[index];
}

int adm_log_index(const char *module)
{
	int i;
	int result;

	for (i = 0; i < ARRAY_LEN(matter_log_mods); i++) {
		result = strcmp(module, matter_log_mods[i]);
		if (!result) {
			return i;
		}
		if (result < 0) {
			break;
		}
	}
	return -1;
}

int adm_log_enabled(u8 index)
{
	if (chip_log_disable_mask & ((u64)1 << index)) {
		return 0;
	}
	return 1;
}

void adm_log_disable(u8 index)
{
	if (index == ADM_LOG_INDEX_ALL) {
		chip_log_disable_mask = MAX_U64;
		return;
	}
	chip_log_disable_mask |= (u64)1 << index;
}

void adm_log_enable(u8 index)
{
	if (index == ADM_LOG_INDEX_ALL) {
		chip_log_disable_mask = 0;
		return;
	}
	chip_log_disable_mask &= ~((u64)1 << index);
}

static int adm_log_set(const char *module, u8 enable)
{
	int i;

	i = adm_log_index(module);
	if (i < 0) {
		return -1;
	}
	if (enable) {
		adm_log_enable(i);
	} else {
		adm_log_disable(i);
	}
	return 0;
}

static void adm_event_callback(enum adm_event_id id)
{
	int i;
	void (*cb)(enum adm_event_id);

	adm_log(LOG_DEBUG "%s: event %d",  __func__, id);

	for (i = 0; i < ARRAY_LEN(adm_event_cbs); i++) {
		cb = adm_event_cbs[i];
		if (!cb) {
			return;
		}
		cb(id);
	}
}

/*
 * Hash DSN, OEM key and device pub key as basis for Matter onboarding
 * credentials.
 *
 * The hash is done in two stages such that "secret" can be secret to the
 * OEM venodr while device_key is know to the device and the Ayla cloud.
 *
 * hash = hmac_sha256(secret, hmac_sha256(device_key, dsn));
 */
static enum ada_err adm_onboard_hash_generate(const char *secret, u8 *hash)
{
	struct al_hmac_ctx *ctx;
	u8 *pub_key;
	int pub_key_len;
	enum ada_err err = AE_OK;
	char first_hash[ADM_HASH_LEN];

	ASSERT(secret);
	ASSERT(*secret != '\0');

	if (conf_sys_dev_id[0] == '\0') {
		adm_log(LOG_WARN "%s DSN not configured", __func__);
		return AE_ERR;
	}

	pub_key = (u8 *)calloc(1, CLIENT_CONF_PUB_KEY_LEN);
	if (!pub_key) {
		return AE_ALLOC;
	}

	pub_key_len = adap_conf_pub_key_get(pub_key, CLIENT_CONF_PUB_KEY_LEN);
	if (pub_key_len <= 0) {
		err = AE_ERR;
		goto exit;
	}

	ctx = al_hmac_sha256_new();
	if (!ctx) {
		err = AE_ALLOC;
		goto exit;
	}

	/*
	 * First, inner hash.
	 */
	al_hmac_sha256_init(ctx, pub_key, pub_key_len);
	al_hmac_sha256_update(ctx, conf_sys_dev_id, strlen(conf_sys_dev_id));
	al_hmac_sha256_final(ctx, first_hash);

	/*
	 * Second, outer hash.
	 */
	al_hmac_sha256_init(ctx, secret, strlen(secret));
	al_hmac_sha256_update(ctx, first_hash, sizeof(first_hash));
	al_hmac_sha256_final(ctx, hash);

	al_hmac_sha256_free(ctx);

exit:
	free(pub_key);
	return err;
}

/*
 * Generate a unchanging unique id for the device.
 *
 * unique_id = hmac_sha256(device_key, "unique_id" + dsn));
 */
enum ada_err adm_unique_id_generate(u8 *uid, size_t uid_len)
{
	struct al_hmac_ctx *ctx;
	u8 *pub_key;
	int pub_key_len;
	enum ada_err err = AE_OK;
	char hash[ADM_HASH_LEN];
	static const char *seed = "unique_id";

	if (uid_len > sizeof(hash)) {
		return AE_LEN;
	}

	if (conf_sys_dev_id[0] == '\0') {
		adm_log(LOG_WARN "%s DSN not configured", __func__);
		return AE_ERR;
	}

	pub_key = (u8 *)calloc(1, CLIENT_CONF_PUB_KEY_LEN);
	if (!pub_key) {
		return AE_ALLOC;
	}

	pub_key_len = adap_conf_pub_key_get(pub_key, CLIENT_CONF_PUB_KEY_LEN);
	if (pub_key_len <= 0) {
		err = AE_ERR;
		goto exit;
	}

	ctx = al_hmac_sha256_new();
	if (!ctx) {
		err = AE_ALLOC;
		goto exit;
	}

	/*
	 * First, inner hash.
	 */
	al_hmac_sha256_init(ctx, pub_key, pub_key_len);
	al_hmac_sha256_update(ctx, seed, strlen(seed));
	al_hmac_sha256_update(ctx, conf_sys_dev_id, strlen(conf_sys_dev_id));
	al_hmac_sha256_final(ctx, hash);

	al_hmac_sha256_free(ctx);

	memcpy(uid, hash, uid_len);
exit:
	free(pub_key);
	return err;
}

/*
 * Generate the passcode by mod'ing a 32-bit unsigned integer down to an 8
 * decimal digit number and adding 1 (range is 1 to max). 32 bits wraps close to
 * 43 times the 8 digit range. mod'ing this range should result in an
 * approximately uniform distribution of passcode values over a population of
 * devices.
 *
 * If the value generated from the hash isn't a legal passcode, increment it
 * and wrap until a legal value is found. The second try should always be
 * a legal value but loop just in case.
 */
static u32 adm_passcode_from_hash(const u8 *hash)
{
	u32 passcode = (((u32)(hash[ADM_HASH_PASSCODE_OFF] << 24) |
	    (hash[ADM_HASH_PASSCODE_OFF+1] << 16) |
	    (hash[ADM_HASH_PASSCODE_OFF+2] << 8) |
	    hash[ADM_HASH_PASSCODE_OFF+3])) % ADM_PASSCODE_MAX + 1;

	while (!PayloadContents::IsValidSetupPIN(passcode)) {
		passcode = (passcode + 1) % ADM_PASSCODE_MAX;
	}

	return passcode;
}

static u16 adm_discriminator_from_hash(const u8 *hash)
{
	return (((u16)hash[ADM_HASH_DISCR_OFF] << 8) |
	    hash[ADM_HASH_DISCR_OFF+1]) & 0xfff; /* 12-bit value */
}

static enum ada_err adm_salt_from_hash(const u8 *hash, u8 *salt, size_t len)
{
	if(len != ADM_HASH_SALT_LEN) {
		return AE_ERR;
	}

	memcpy(salt, &hash[ADM_HASH_SALT_OFF], ADM_HASH_SALT_LEN);

	return AE_OK;
}

static enum ada_err adm_salt_b64_from_hash(const u8 *hash, char *salt, size_t len)
{
	if (ayla_base64_encode(&hash[ADM_HASH_SALT_OFF], ADM_HASH_SALT_LEN,
	    salt, &len)) {
		return AE_ERR;
	}

	return AE_OK;
}

static void adm_connectivity_change(const ChipDeviceEvent * event)
{
	if (event->InternetConnectivityChange.IPv4 ==
	    kConnectivity_Established) {
		adm_log(LOG_DEBUG "IPv4 up");
		DnssdServer::Instance().StartServer();
		adm_event_callback(ADM_EVENT_IPV4_UP);
	} else if (event->InternetConnectivityChange.IPv4 ==
	    kConnectivity_Lost)	{
		adm_log(LOG_DEBUG "IPv4 down");
		adm_event_callback(ADM_EVENT_IPV4_DOWN);
	}

	if (event->InternetConnectivityChange.IPv6 ==
	    kConnectivity_Established) {
		adm_log(LOG_DEBUG "IPv6 up");
		DnssdServer::Instance().StartServer();
		adm_event_callback(ADM_EVENT_IPV6_UP);
	} else if (event->InternetConnectivityChange.IPv6 ==
	    kConnectivity_Lost)	{
		adm_log(LOG_DEBUG "IPv6 down");
		adm_event_callback(ADM_EVENT_IPV6_DOWN);
	}
}

static void adm_device_event(const ChipDeviceEvent * event, intptr_t arg)
{
	adm_log(LOG_DEBUG "%s type 0x%04x", __func__, event->Type);

	switch (event->Type) {
	case DeviceEventType::kInternetConnectivityChange:
		adm_connectivity_change(event);
		break;

	case DeviceEventType::kCHIPoBLEConnectionEstablished:
		adm_log(LOG_DEBUG "CHIPoBLE connection established");
		break;

	case DeviceEventType::kCHIPoBLEConnectionClosed:
		adm_log(LOG_DEBUG "CHIPoBLE disconnected");
		break;

	case DeviceEventType::kCommissioningComplete:
		adm_log(LOG_DEBUG "Commissioning complete");
		al_matter_fully_provisioned();
		adm_event_callback(ADM_EVENT_COMMISSIONING_COMPLETE);
		break;

	case DeviceEventType::kServiceProvisioningChange:
	case DeviceEventType::kWiFiConnectivityChange:
		break;

	case DeviceEventType::kInterfaceIpAddressChanged:
		if ((event->InterfaceIpAddressChanged.Type ==
		    InterfaceIpChangeType::kIpV4_Assigned) ||
		    (event->InterfaceIpAddressChanged.Type ==
		    InterfaceIpChangeType::kIpV6_Assigned)) {
			/*
			 * Restart MDNS server on any ip assignment event
			 */
			DnssdServer::Instance().StartServer();
		}
		if (event->InterfaceIpAddressChanged.Type ==
		    InterfaceIpChangeType::kIpV6_Assigned) {
			al_matter_ipv6_assigned();
		}
		break;
	}
}

extern "C" void adm_log(const char *fmt, ...)
{
	ADA_VA_LIST args;

	ADA_VA_START(args, fmt);
	log_put_va(MOD_LOG_CLIENT, fmt, args);
	ADA_VA_END(args);
}

extern "C" enum ada_err adm_write_attribute(u16 endpoint, u32 cluster,
    u32 attribute, u8 *value, u8 type)
{
	Protocols::InteractionModel::Status status;

	if (!PlatformMgr().TryLockChipStack()) {
		adm_log(LOG_DEBUG "%s could not get lock", __func__);
		return AE_BUSY;
	}

	status = emberAfWriteAttribute(endpoint, cluster,
	    attribute, value, type);

	PlatformMgr().UnlockChipStack();

	if (status != Status::Success) {
		adm_log(LOG_ERR "Write Attribute %u %#X %#X %#X status %d",
		    endpoint, cluster, attribute, type, status);
		return AE_ERR;
	}
	return AE_OK;
}

extern "C" enum ada_err adm_write_boolean(u16 endpoint, u32 cluster,
    u32 attribute, u8 value)
{
	return adm_write_attribute(endpoint, cluster, attribute, &value,
	    ZCL_BOOLEAN_ATTRIBUTE_TYPE);
}

extern "C" enum ada_err adm_write_u8(u16 endpoint, u32 cluster,
    u32 attribute, u8 value)
{
	return adm_write_attribute(endpoint, cluster, attribute, &value,
	    ZCL_INT8U_ATTRIBUTE_TYPE);
}

extern "C" enum ada_err adm_write_u16(u16 endpoint, u32 cluster,
    u32 attribute, u16 value)
{
	return adm_write_attribute(endpoint, cluster, attribute, (u8 *)&value,
	    ZCL_INT16U_ATTRIBUTE_TYPE);
}

extern "C" enum ada_err adm_write_u32(u16 endpoint, u32 cluster,
    u32 attribute, u32 value)
{
	return adm_write_attribute(endpoint, cluster, attribute, (u8 *)&value,
	    ZCL_INT32U_ATTRIBUTE_TYPE);
}

extern "C" enum ada_err adm_write_s8(u16 endpoint, u32 cluster,
    u32 attribute, s8 value)
{
	return adm_write_attribute(endpoint, cluster, attribute, (u8 *)&value,
	    ZCL_INT8S_ATTRIBUTE_TYPE);
}

extern "C" enum ada_err adm_write_s16(u16 endpoint, u32 cluster,
    u32 attribute, s16 value)
{
	return adm_write_attribute(endpoint, cluster, attribute, (u8 *)&value,
	    ZCL_INT16S_ATTRIBUTE_TYPE);
}

extern "C" enum ada_err adm_write_s32(u16 endpoint, u32 cluster,
    u32 attribute, s32 value)
{
	return adm_write_attribute(endpoint, cluster, attribute, (u8 *)&value,
	    ZCL_INT32S_ATTRIBUTE_TYPE);
}

extern "C" enum ada_err adm_write_string(u16 endpoint, u32 cluster,
    u32 attribute, char *value)
{
	enum ada_err err;
	size_t len = strlen(value);
	u8 *buf = (u8 *)al_os_mem_alloc(1 + len + 1);

	if (!buf) {
		adm_log(LOG_ERR "%s: buffer alloc failed", __func__);
		return AE_ALLOC;
	}

	buf[0] = (u8)len;
	strcpy((char *)&buf[1], value);

	err = adm_write_attribute(endpoint, cluster, attribute, buf,
	    ZCL_CHAR_STRING_ATTRIBUTE_TYPE);

	al_os_mem_free(buf);
	return err;
}

extern "C" enum ada_err adm_get_boolean(u8 type, u16 size, u8 *data,
    u8 *value)
{
	if (type != ZCL_BOOLEAN_ATTRIBUTE_TYPE) {
		return AE_INVAL_TYPE;
	}

	if (size != sizeof(u8)) {
		return AE_LEN;
	}

	*value = (*data != 0);

	return AE_OK;
}

extern "C" enum ada_err adm_get_u8(u8 type, u16 size, u8 *data, u8 *value)
{
	if (type != ZCL_INT8U_ATTRIBUTE_TYPE) {
		return AE_INVAL_TYPE;
	}

	if (size != sizeof(u8)) {
		return AE_LEN;
	}

	*value = *data;

	return AE_OK;
}

extern "C" enum ada_err adm_get_u16(u8 type, u16 size, u8 *data, u16 *value)
{
	if (type != ZCL_INT16U_ATTRIBUTE_TYPE) {
		return AE_INVAL_TYPE;
	}

	if (size != sizeof(u16)) {
		return AE_LEN;
	}

	*value = *(u16 *)data;

	return AE_OK;
}

extern "C" enum ada_err adm_get_u32(u8 type, u16 size, u8 *data, u32 *value)
{
	if (type != ZCL_INT16U_ATTRIBUTE_TYPE) {
		return AE_INVAL_TYPE;
	}

	if (size != sizeof(u32)) {
		return AE_LEN;
	}

	*value = *(u32 *)data;

	return AE_OK;
}

extern "C" enum ada_err adm_get_s8(u8 type, u16 size, u8 *data, s8 *value)
{
	if (type != ZCL_INT8S_ATTRIBUTE_TYPE) {
		return AE_INVAL_TYPE;
	}

	if (size != sizeof(s8)) {
		return AE_LEN;
	}

	*value = *(s8 *)data;

	return AE_OK;
}

extern "C" enum ada_err adm_get_s16(u8 type, u16 size, u8 *data, s16 *value)
{
	if (type != ZCL_INT16S_ATTRIBUTE_TYPE) {
		return AE_INVAL_TYPE;
	}

	if (size != sizeof(s16)) {
		return AE_LEN;
	}

	*value = *(s16 *)data;

	return AE_OK;
}

extern "C" enum ada_err adm_get_s32(u8 type, u16 size, u8 *data, s32 *value)
{
	if (type != ZCL_INT16S_ATTRIBUTE_TYPE) {
		return AE_INVAL_TYPE;
	}

	if (size != sizeof(s32)) {
		return AE_LEN;
	}

	*value = *(s32 *)data;

	return AE_OK;
}

static enum ada_err adm_onboard_autogen_internal(const char *secret)
{
	u16 discr;
	uint32_t passcode;
	enum ada_err err;
	CHIP_ERROR chip_err;
	Spake2pVerifier verifier;
	Spake2pVerifierSerialized serialized_verifier;
	MutableByteSpan verifier_span(serialized_verifier);
	u8 hash[ADM_HASH_LEN];
	char salt_b64[BASE64_LEN_EXPAND(ADM_HASH_SALT_LEN) + 1];
	char verifier_b64[
	    BASE64_LEN_EXPAND(kSpake2p_VerifierSerialized_Length) + 1];
	size_t verifier_b64_len = sizeof(verifier_b64);

	if (!secret) {
		/* Use compiled in secret if none supplied */
		secret = adm_qrgen_secret;
	}

	err = adm_onboard_hash_generate(secret, hash);
	if (err) {
		adm_log(LOG_ERR "credential hash generate failed %s", err);
		return AE_ERR;
	}

	err = adm_salt_b64_from_hash(hash, salt_b64, sizeof(salt_b64));
	if (err) {
		adm_log(LOG_ERR "SPAKE2+ salt generation failed %d", err);
		return AE_ERR;
	}

	passcode = adm_passcode_from_hash(hash);

	chip_err = PASESession::GeneratePASEVerifier(
	    verifier, ADM_SPAKE2P_ITERATIONS, ByteSpan(&hash[ADM_HASH_SALT_OFF],
	    ADM_HASH_SALT_LEN), false, passcode);
	if (chip_err != CHIP_NO_ERROR) {
		adm_log(LOG_ERR "SPAKE2+ verifier generation failed %s",
		    ErrorStr(chip_err));
		return AE_ERR;
	}

	chip_err = verifier.Serialize(verifier_span);
	if (chip_err != CHIP_NO_ERROR) {
		adm_log(LOG_ERR "SPAKE2+ verifier serialize failed %s",
		    ErrorStr(chip_err));
		return AE_ERR;
	}

	if (ayla_base64_encode(verifier_span.data(),
	    verifier_span.size(), verifier_b64, &verifier_b64_len)) {
		adm_log(LOG_ERR "SPAKE2+ verifier b64 encode failed");
		return AE_ERR;
	}

	al_matter_spake2p_config_set(ADM_SPAKE2P_ITERATIONS, salt_b64,
	    verifier_b64);

	discr = adm_discriminator_from_hash(hash);
	al_matter_discriminator_config_set(discr, 1);

	return AE_OK;
}

class AdmAppDelegateImpl : public AppDelegate
{
public:
	void OnCommissioningSessionStarted()
	{
		adm_event_callback(ADM_EVENT_COMMISSIONING_SESSION_STARTED);
	}

	void OnCommissioningSessionStopped()
	{
		adm_event_callback(ADM_EVENT_COMMISSIONING_SESSION_STOPPED);
	}

	void OnCommissioningWindowOpened()
	{
		adm_event_callback(ADM_EVENT_COMMISSIONING_WINDOW_OPENED);
	}

	void OnCommissioningWindowClosed()
	{
		adm_event_callback(ADM_EVENT_COMMISSIONING_WINDOW_CLOSED);
	}
};

static void adm_init_server(intptr_t context)
{
	static CommonCaseDeviceServerInitParams initParams;
	static AdmAppDelegateImpl adm_app_delegate;
	CHIP_ERROR chip_err;
	int ret;

	/*
	 * This executes on the Matter thread. Give the thread a log id.
	 */
	log_thread_id_set("t");

	/*
	 * Init cluster data model and CHIP App Server
	 */
	chip_err = initParams.InitializeStaticResourcesBeforeServerInit();
	ASSERT(chip_err == CHIP_NO_ERROR);
	initParams.appDelegate = &adm_app_delegate;

	chip_err = Server::GetInstance().Init(initParams);
	ASSERT(chip_err == CHIP_NO_ERROR);

	ret = al_matter_server_init();
	ASSERT(!ret);

	adm_matter_initialized = 1;
	adm_event_callback(ADM_EVENT_INITIALIZED);

	if (chip::Server::GetInstance().GetFabricTable().FabricCount() > 0) {
		al_matter_fully_provisioned();
	}
}

int adm_initialized(void)
{
	return adm_matter_initialized;
}

enum ada_err adm_onboard_config_generate(const char *secret)
{
	return adm_onboard_autogen_internal(secret);
}

#ifdef AYLA_WIFI_SUPPORT
static void adm_ip_up_send(void)
{
	IPAddress addr;
	ChipDeviceEvent event;
	char ip[30];

	adm_log(LOG_DEBUG "%s", __func__);

	memset(&event, 0, sizeof(event));

	ipaddr_ntoa_r(&netif_default->ip_addr, ip, sizeof(ip));
	adm_log(LOG_INFO "%s IP %s", __func__, ip);

	event.Type = DeviceEventType::kInterfaceIpAddressChanged;
	event.InterfaceIpAddressChanged.Type =
	    InterfaceIpChangeType::kIpV4_Assigned;
	PlatformMgr().PostEventOrDie(&event);

	event.Type = DeviceEventType::kInterfaceIpAddressChanged;
	event.InterfaceIpAddressChanged.Type =
	    InterfaceIpChangeType::kIpV6_Assigned;
	PlatformMgr().PostEventOrDie(&event);

	IPAddress::FromString(ip, addr);
	event.Type = DeviceEventType::kInternetConnectivityChange;
	event.InternetConnectivityChange.IPv4 =
	    ConnectivityChange::kConnectivity_Established;
	event.InternetConnectivityChange.IPv6 =
	    ConnectivityChange::kConnectivity_Established;
	// event.InternetConnectivityChange.ipAddress = addr;
	PlatformMgr().PostEventOrDie(&event);
}
#endif

/*
 * Functions called from C code
 */
extern "C" {

#ifdef AYLA_WIFI_SUPPORT
/*
 * Event handler for various Wi-Fi events.
 */
static void adm_wifi_event_handler(enum adw_wifi_event_id id,
    void *arg)
{
	switch (id) {
	case ADW_EVID_STA_DHCP_UP:
		adm_ip_up_send();
		break;
	/* TODO: handle down events */
	default:
		break;
	}
}
#else
int adm_network_configured(void)
{
	return ConfigurationMgr().IsFullyProvisioned();
}

enum ada_err adm_onboarding_config_get(const char *secret, u32 *passcode,
    u16 *vendor, u16 *product, u8 *discovery_mask, u16 *discriminator,
    char *pairing_str, size_t ps_len, char *qr_str, size_t qr_len)
{
	CHIP_ERROR chip_err;
	enum ada_err err;
	PayloadContents pc;
	uint16_t discr;
	MutableCharSpan ps_span(pairing_str, ps_len);
	MutableCharSpan qr_span(qr_str, qr_len);
	DeviceInstanceInfoProvider *diip = GetDeviceInstanceInfoProvider();
	CommissionableDataProvider *cdp = GetCommissionableDataProvider();
	uint8_t salt[kSpake2p_Max_PBKDF_Salt_Length] = { 0 };
	u8 salt2[ADM_HASH_SALT_LEN];
	MutableByteSpan saltSpan{ salt };
	u8 hash[ADM_HASH_LEN];

	pc.version = 0;
	pc.rendezvousInformation.SetValue(
	    RendezvousInformationFlags(AYLA_MATTER_DISCOVERY_MASK));

	chip_err = diip->GetVendorId(pc.vendorID);
	if (chip_err != CHIP_NO_ERROR) {
		return AE_ERR;
	}

	chip_err = diip->GetProductId(pc.productID);
	if (chip_err != CHIP_NO_ERROR) {
		return AE_ERR;
	}

	chip_err = cdp->GetSetupDiscriminator(discr);
	if (chip_err != CHIP_NO_ERROR) {
		return AE_ERR;
	}
	pc.discriminator.SetLongValue(discr);

	chip_err = cdp->GetSpake2pSalt(saltSpan);
	if (chip_err != CHIP_NO_ERROR) {
		return AE_ERR;
	}

	*vendor = pc.vendorID;
	*product = pc.productID;
	*discovery_mask = pc.rendezvousInformation.Value().Raw();
	*discriminator = discr;

	if (!adm_spake2p_config_check(0)) {
		/* Test mode */
		pc.setUpPINCode = CHIP_DEVICE_CONFIG_USE_TEST_SETUP_PIN_CODE;
	} else {
		if (!secret) {
			/* Use compiled in secret if none supplied */
			secret = adm_qrgen_secret;
		}

		/*
		 * Attempt to generate the passcode using the secret. If the secret
		 * is not correct or the credentials were not generated by
		 * adm_onboard_config_generate, the salt will not match.
		 */
		err = adm_onboard_hash_generate(secret, hash);
		if (err) {
			goto invalid_secret;
		}

		err = adm_salt_from_hash(hash, salt2, sizeof(salt2));
		if (err) {
			goto invalid_secret;
		}

		if ((saltSpan.size() != sizeof(salt2)) ||
		    memcmp(saltSpan.data(), salt2, sizeof(salt2))) {
			goto invalid_secret;
		}

		pc.setUpPINCode = adm_passcode_from_hash(hash);
	}

	chip_err = GetManualPairingCode(ps_span, pc);
	if (chip_err != CHIP_NO_ERROR) {
		goto invalid_secret;
	}

	chip_err = GetQRCode(qr_span, pc);
	if (chip_err != CHIP_NO_ERROR) {
		goto invalid_secret;
	}

	*passcode = pc.setUpPINCode;
	return AE_OK;

invalid_secret:
	*passcode = 0;
	return AE_OK;
}

int adap_wifi_in_ap_mode(void)
{
	return 0;
}

int adap_wifi_get_ssid(void *buf, size_t len)
{
	return 0;
}

int adap_net_get_signal(int *signal)
{
	return -1;
}

enum ada_wifi_features adap_wifi_features_get(void)
{
	enum ada_wifi_features features = (enum ada_wifi_features)0;
	return features;
}

void adap_wifi_stayup(void)
{
}

void adap_wifi_show_hist(int to_log)
{
}
#endif

void adm_identify_cb_register(enum ada_err (*callback)(
				enum adm_identify_cb_type type,
				u16 endpoint_id,
				union adm_identify_cb_data *data))
{
	adm_identify_cb = callback;
}

static enum ada_err adm_identify_execute_cb(
		enum adm_identify_cb_type type,
		u16 endpoint_id, union adm_identify_cb_data *data)
{
	adm_log(LOG_DEBUG "%s Call identify callback", __func__);
	if (adm_identify_cb) {
		return adm_identify_cb(type, endpoint_id, data);
	}

	return AE_OK;
}

static void adm_identify_start_cb(Identify *identify)
{
	u16 identify_time = 0;
	Protocols::InteractionModel::Status ret;
	union adm_identify_cb_data data;

	adm_log(LOG_DEBUG "%s Start callback", __func__);

	ret = Clusters::Identify::Attributes::IdentifyTime::Get(
	    identify->mEndpoint, &identify_time);
	if (ret != Status::Success)
	{
		adm_log(LOG_ERR "%s Get IdentifyTime return error 0x%X",
		    __func__, ret);
		return;
	}

	data.time.identify_type = static_cast<u8>(identify->mIdentifyType);
	data.time.identify_time = identify_time;

	adm_identify_execute_cb(ADM_IDENTIFY_START,
	    identify->mEndpoint, &data);
}

static void adm_identify_stop_cb(Identify *identify)
{
	u16 identify_time = 0;
	Protocols::InteractionModel::Status ret;
	union adm_identify_cb_data data;

	adm_log(LOG_DEBUG "%s Stop callback", __func__);

	ret = Clusters::Identify::Attributes::IdentifyTime::Get(
	    identify->mEndpoint, &identify_time);
	if (ret != Status::Success)
	{
		adm_log(LOG_ERR "%s Get IdentifyTime return error 0x%X",
		    __func__, ret);
		return;
	}

	data.time.identify_type = static_cast<u8>(identify->mIdentifyType);
	data.time.identify_time = identify_time;

	adm_identify_execute_cb(ADM_IDENTIFY_STOP,
	    identify->mEndpoint, &data);
}

static void adm_identify_effect_cb(Identify *identify)
{
	union adm_identify_cb_data data;

	adm_log(LOG_DEBUG "%s Effect callback", __func__);

	data.effect.effect_identify = static_cast<u8>(identify->mCurrentEffectIdentifier);
	data.effect.effect_variant = static_cast<u8>(identify->mEffectVariant);

	adm_identify_execute_cb(ADM_IDENTIFY_EFFECT,
	    identify->mEndpoint, &data);
}

enum ada_err adm_identify_init(u16 endpoint_id,
			enum adm_identify_type identify_type)
{
	Identify *identify = new Identify(endpoint_id, adm_identify_start_cb,
	    adm_identify_stop_cb,
	    static_cast<Clusters::Identify::IdentifyTypeEnum>(identify_type),
	    adm_identify_effect_cb);
	if (!identify) {
		adm_log(LOG_ERR "%s Fail to create identify object", __func__);
		return AE_ALLOC;
	}
	return AE_OK;
}

enum ada_err adm_attribute_change_cb_register(
    const struct adm_attribute_change_callback *entry)
{
	int i;

	for (i = 0; i < ADM_ATTR_CHANGE_CB_MAX; ++i) {
		if (!attr_change_cbs[i]) {
			attr_change_cbs[i] = entry;
			return AE_OK;
		}
	}

	return AE_ALLOC;
}

enum ada_err adm_event_cb_register(void (*cb)(enum adm_event_id id))
{
	int i;

	for (i = 0; i < ARRAY_LEN(adm_event_cbs); i++) {
		if (!adm_event_cbs[i]) {
			adm_event_cbs[i] = cb;
			return AE_OK;
		}
	}

	return AE_ALLOC;
}

void adm_init(void)
{
	CHIP_ERROR error;

	/*
	 * Disable some of the chatty debug logs.
	 */
	adm_log_set("DMG", 0);
	adm_log_set("EM", 0);
	adm_log_set("IM", 0);
	adm_log_set("IN", 0);

	error = Platform::MemoryInit();
	if (error != CHIP_NO_ERROR) {
		adm_log(LOG_ERR "%s memory init failed: %s",
		    __func__, ErrorStr(error));
		return;
	}

	// Initialize the CHIP stack.
	error = PlatformMgr().InitChipStack();
	if (error != CHIP_NO_ERROR) {
		adm_log(LOG_ERR "%s init CHIP stack failed: %s",
		    __func__, ErrorStr(error));
		return;
	}
}

void adm_start(const u8 *cert_declaration, size_t cd_len)
{
	CHIP_ERROR error;
	RendezvousInformationFlags flags =
	    RendezvousInformationFlags(AYLA_MATTER_DISCOVERY_MASK);

	AdmDataProvider::AdmDataProviderInit(ByteSpan(cert_declaration, cd_len));

	SetCommissionableDataProvider(GetAdmCommissionableDataProvider());
	SetDeviceAttestationCredentialsProvider(GetAdmDACProvider());
	SetDeviceInstanceInfoProvider(GetAdmDeviceInstanceInfoProvider());

	if (flags.Has(RendezvousInformationFlag::kBLE)) {
		ConnectivityMgr().SetBLEAdvertisingEnabled(true);
	} else if (flags.Has(RendezvousInformationFlag::kSoftAP)) {
		ConnectivityMgr().SetBLEAdvertisingEnabled(false);
		ConnectivityMgr().SetWiFiAPMode(
		    ConnectivityManager::kWiFiAPMode_Enabled);
	}

	PlatformMgr().AddEventHandler(adm_device_event, 0);

	/*
	 * Start a task to run the CHIP Device event loop.
	 */
	error =  PlatformMgr().StartEventLoopTask();
	if (error != CHIP_NO_ERROR) {
		adm_log(LOG_ERR "%s platform manager start failed: %s",
		    __func__, ErrorStr(error));
		return;
	}

#ifdef AYLA_WIFI_SUPPORT
	adw_wifi_event_register(adm_wifi_event_handler, NULL);
#endif

	PlatformMgr().ScheduleWork(adm_init_server,
	    reinterpret_cast<intptr_t>(nullptr));
}

enum ada_err adm_endpoint_type_set(u16 endpoint, u16 device_id, u8 version)
{
	CHIP_ERROR err;
	EmberAfDeviceType *list;

	list = (EmberAfDeviceType *)calloc(1, sizeof(EmberAfDeviceType));
	if (!list) {
		adm_log(LOG_ERR "%s alloc failed", __func__);
		return AE_ALLOC;
	}
	list->deviceId = device_id;
	list->deviceVersion = version;

	err = emberAfSetDeviceTypeList(endpoint,
	    Span<const EmberAfDeviceType>(list, 1));
	if (err != CHIP_NO_ERROR) {
		adm_log(LOG_ERR "%s failed: %s", __func__, ErrorStr(err));
		return AE_ERR;
	}

	return AE_OK;
}

void adm_cd_set(const u8 *cert_declaration, size_t cd_len)
{
	AdmDataProvider *dp = AdmDataProvider::GetAdmDataProvider();
	dp->SetCertificationDeclaration(ByteSpan(cert_declaration, cd_len));
}

int adm_cd_get(u8 *cert_declaration, size_t cd_len)
{
	AdmDataProvider *dp = AdmDataProvider::GetAdmDataProvider();
	MutableByteSpan mutable_span;
	CHIP_ERROR ret;

	memset(cert_declaration, 0, cd_len);
	mutable_span = MutableByteSpan(cert_declaration, cd_len);
	ret = dp->GetCertificationDeclaration(mutable_span);
	if (ret == CHIP_NO_ERROR) {
		return mutable_span.size();
	} else {
		return -1;
	}
}

} /* extern "C" */

static chip::Protocols::InteractionModel::Status
adm_attr_change_callback(u8 post_change, const ConcreteAttributePath &path,
    uint8_t type, uint16_t size, uint8_t * value)
{
	int i;
	const struct adm_attribute_change_callback *entry;
	u16 endpoint = path.mEndpointId;
	u32 cluster = path.mClusterId;
	u32 attribute = path.mAttributeId;
	enum ada_err err;

	for (i = 0; i < ADM_ATTR_CHANGE_CB_MAX; ++i) {
		entry = attr_change_cbs[i];
		if (!entry) {
			return chip::Protocols::InteractionModel::
			    Status::Success;
		}
		if (post_change) {
			if (!(entry->flags & ADM_ACCE_POST_CHANGE) ) {
				continue;
			}
		} else if (!(entry->flags & ADM_ACCE_PRE_CHANGE) ) {
			continue;
		}
		if (!(entry->flags & ADM_ACCE_ANY_ENDPOINT) &&
		    endpoint != entry->endpoint) {
			continue;
		}
		if (!(entry->flags & ADM_ACCE_ANY_CLUSTER) &&
		    cluster != entry->cluster) {
			continue;
		}
		if (!(entry->flags & ADM_ACCE_ANY_ATTRIBUTE) &&
		    attribute != entry->attribute) {
			continue;
		}
		err = entry->callback(post_change, endpoint, cluster, attribute,
		    type, size, value);
		if (err) {
			/* TODO: better map ada errs of CHIP errors */
			adm_log(LOG_ERR "%s callback ret err %d",
			    __func__, err);
			return chip::Protocols::InteractionModel::
			    Status::Failure;
		}
	}
	return chip::Protocols::InteractionModel::Status::Success;
}

chip::Protocols::InteractionModel::Status
MatterPreAttributeChangeCallback(const ConcreteAttributePath &path,
    uint8_t type, uint16_t size, uint8_t * value)
{
	adm_log(LOG_DEBUG2
	    "%s endpoint %u cluster %04lx attribute %lu",
	    __func__, path.mEndpointId, path.mClusterId, path.mAttributeId);

	return adm_attr_change_callback(0, path, type, size, value);
;
}

void MatterPostAttributeChangeCallback(const ConcreteAttributePath &path,
    uint8_t type, uint16_t size, uint8_t * value)
{
	adm_log(LOG_DEBUG2
	    "%s endpoint %u cluster %#X attribute %#X",
	    __func__, path.mEndpointId, path.mClusterId, path.mAttributeId);

	adm_attr_change_callback(1, path, type, size, value);
}

/*
 * Define chip::Logging::Platform::LogV to redirect CHIP log messages into
 * the ADA logging system.
 *
 * This function is also defined in the platform library but the linker will
 * find this one first if it is found by the linker first when searching
 * libraries.
 */
namespace chip {
namespace Logging {
namespace Platform {

void LogV(const char * module, uint8_t category, const char * msg, va_list v)
{
	enum log_sev sev;
	char format[100];
	int i;

	switch (category) {
	case kLogCategory_Error:
		sev = LOG_SEV_ERR;
		break;
	case kLogCategory_Progress:
	default:
		sev = LOG_SEV_DEBUG;
		break;
	case kLogCategory_Detail:
		sev = LOG_SEV_DEBUG2;
		break;
	}

	if (!log_mod_sev_is_enabled(MOD_LOG_CLIENT, sev)) {
		return;
	}

	if (sev > LOG_SEV_ERR) {
		i = adm_log_index(module);
		if (i >= 0 && !adm_log_enabled(i)) {
			return;
		}
	}

	snprintf(format, sizeof(format), "matter[%s]: %s", module, msg);
	log_put_va_sev(MOD_LOG_CLIENT, sev, format, v);
}

} // namespace Platform
} // namespace Logging
} // namespace chip
