/*
 * Copyright 2023 Ayla Networks, Inc.  All rights reserved.
 */
/*
 *    Copyright (c) 2020-2022 Project CHIP Authors
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */
#include <crypto/CHIPCryptoPAL.h>
#include <credentials/examples/ExampleDACs.h>
#include <credentials/examples/ExamplePAI.h>
#include <lib/core/CHIPError.h>
#include <lib/support/Span.h>
#include <lib/support/Base64.h>
#include <ayla/utypes.h>
#include <ayla/assert.h>
#include <ada/err.h>
#include <ayla/log.h>
#include <ayla/mod_log.h>
#include <ayla/conf.h>
#include <ada/ada_conf.h>

#include <platform/pfm_matter.h>
#include "adm_int.h"
#include "adm_data_provider.h"

using namespace chip::Crypto;
using namespace chip::Credentials;

namespace chip {
namespace DeviceLayer {
namespace Ayla {

/*
 * Example Certification Declaration from Matter SDK used for test devices
 * that haven't been fully factory configured with production Matter
 * credentials.
 *
 * -> format_version = 1
 * -> vendor_id = 0xFFF1
 * -> product_id_array = [
 *  0x8000, 0x8001, 0x8002, 0x8003, 0x8004, 0x8005, 0x8006, 0x8007,
 *  0x8008, 0x8009, 0x800A, 0x800B, 0x800C, 0x800D, 0x800E, 0x800F,
 *  0x8010, 0x8011, 0x8012, 0x8013, 0x8014, 0x8015, 0x8016, 0x8017,
 *  0x8018, 0x8019, 0x801A, 0x801B, 0x801C, 0x801D, 0x801E, 0x801F,
 *  0x8020, 0x8021, 0x8022, 0x8023, 0x8024, 0x8025, 0x8026, 0x8027,
 *  0x8028, 0x8029, 0x802A, 0x802B, 0x802C, 0x802D, 0x802E, 0x802F,
 *  0x8030, 0x8031, 0x8032, 0x8033, 0x8034, 0x8035, 0x8036, 0x8037,
 *  0x8038, 0x8039, 0x803A, 0x803B, 0x803C, 0x803D, 0x803E, 0x803F,
 *  0x8040, 0x8041, 0x8042, 0x8043, 0x8044, 0x8045, 0x8046, 0x8047,
 *  0x8048, 0x8049, 0x804A, 0x804B, 0x804C, 0x804D, 0x804E, 0x804F,
 *  0x8050, 0x8051, 0x8052, 0x8053, 0x8054, 0x8055, 0x8056, 0x8057,
 *  0x8058, 0x8059, 0x805A, 0x805B, 0x805C, 0x805D, 0x805E, 0x805F,
 *  0x8060, 0x8061, 0x8062, 0x8063
 *  ]
 * -> device_type_id = 0x0016
 * -> certificate_id = "CSA00000SWC00000-00"
 * -> security_level = 0
 * -> security_information = 0
 * -> version_number = 1
 * -> certification_type = 0
 * -> dac_origin_vendor_id is not present
 * -> dac_origin_product_id is not present
 */
static const uint8_t test_cert_declaration[539] = {
    0x30, 0x82, 0x02, 0x17, 0x06, 0x09, 0x2a, 0x86,
    0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x02, 0xa0,
    0x82, 0x02, 0x08, 0x30, 0x82, 0x02, 0x04, 0x02,
    0x01, 0x03, 0x31, 0x0d, 0x30, 0x0b, 0x06, 0x09,
    0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02,
    0x01, 0x30, 0x82, 0x01, 0x70, 0x06, 0x09, 0x2a,
    0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x01,
    0xa0, 0x82, 0x01, 0x61, 0x04, 0x82, 0x01, 0x5d,
    0x15, 0x24, 0x00, 0x01, 0x25, 0x01, 0xf1, 0xff,
    0x36, 0x02, 0x05, 0x00, 0x80, 0x05, 0x01, 0x80,
    0x05, 0x02, 0x80, 0x05, 0x03, 0x80, 0x05, 0x04,
    0x80, 0x05, 0x05, 0x80, 0x05, 0x06, 0x80, 0x05,
    0x07, 0x80, 0x05, 0x08, 0x80, 0x05, 0x09, 0x80,
    0x05, 0x0a, 0x80, 0x05, 0x0b, 0x80, 0x05, 0x0c,
    0x80, 0x05, 0x0d, 0x80, 0x05, 0x0e, 0x80, 0x05,
    0x0f, 0x80, 0x05, 0x10, 0x80, 0x05, 0x11, 0x80,
    0x05, 0x12, 0x80, 0x05, 0x13, 0x80, 0x05, 0x14,
    0x80, 0x05, 0x15, 0x80, 0x05, 0x16, 0x80, 0x05,
    0x17, 0x80, 0x05, 0x18, 0x80, 0x05, 0x19, 0x80,
    0x05, 0x1a, 0x80, 0x05, 0x1b, 0x80, 0x05, 0x1c,
    0x80, 0x05, 0x1d, 0x80, 0x05, 0x1e, 0x80, 0x05,
    0x1f, 0x80, 0x05, 0x20, 0x80, 0x05, 0x21, 0x80,
    0x05, 0x22, 0x80, 0x05, 0x23, 0x80, 0x05, 0x24,
    0x80, 0x05, 0x25, 0x80, 0x05, 0x26, 0x80, 0x05,
    0x27, 0x80, 0x05, 0x28, 0x80, 0x05, 0x29, 0x80,
    0x05, 0x2a, 0x80, 0x05, 0x2b, 0x80, 0x05, 0x2c,
    0x80, 0x05, 0x2d, 0x80, 0x05, 0x2e, 0x80, 0x05,
    0x2f, 0x80, 0x05, 0x30, 0x80, 0x05, 0x31, 0x80,
    0x05, 0x32, 0x80, 0x05, 0x33, 0x80, 0x05, 0x34,
    0x80, 0x05, 0x35, 0x80, 0x05, 0x36, 0x80, 0x05,
    0x37, 0x80, 0x05, 0x38, 0x80, 0x05, 0x39, 0x80,
    0x05, 0x3a, 0x80, 0x05, 0x3b, 0x80, 0x05, 0x3c,
    0x80, 0x05, 0x3d, 0x80, 0x05, 0x3e, 0x80, 0x05,
    0x3f, 0x80, 0x05, 0x40, 0x80, 0x05, 0x41, 0x80,
    0x05, 0x42, 0x80, 0x05, 0x43, 0x80, 0x05, 0x44,
    0x80, 0x05, 0x45, 0x80, 0x05, 0x46, 0x80, 0x05,
    0x47, 0x80, 0x05, 0x48, 0x80, 0x05, 0x49, 0x80,
    0x05, 0x4a, 0x80, 0x05, 0x4b, 0x80, 0x05, 0x4c,
    0x80, 0x05, 0x4d, 0x80, 0x05, 0x4e, 0x80, 0x05,
    0x4f, 0x80, 0x05, 0x50, 0x80, 0x05, 0x51, 0x80,
    0x05, 0x52, 0x80, 0x05, 0x53, 0x80, 0x05, 0x54,
    0x80, 0x05, 0x55, 0x80, 0x05, 0x56, 0x80, 0x05,
    0x57, 0x80, 0x05, 0x58, 0x80, 0x05, 0x59, 0x80,
    0x05, 0x5a, 0x80, 0x05, 0x5b, 0x80, 0x05, 0x5c,
    0x80, 0x05, 0x5d, 0x80, 0x05, 0x5e, 0x80, 0x05,
    0x5f, 0x80, 0x05, 0x60, 0x80, 0x05, 0x61, 0x80,
    0x05, 0x62, 0x80, 0x05, 0x63, 0x80, 0x18, 0x24,
    0x03, 0x16, 0x2c, 0x04, 0x13, 0x43, 0x53, 0x41,
    0x30, 0x30, 0x30, 0x30, 0x30, 0x53, 0x57, 0x43,
    0x30, 0x30, 0x30, 0x30, 0x30, 0x2d, 0x30, 0x30,
    0x24, 0x05, 0x00, 0x24, 0x06, 0x00, 0x24, 0x07,
    0x01, 0x24, 0x08, 0x00, 0x18, 0x31, 0x7c, 0x30,
    0x7a, 0x02, 0x01, 0x03, 0x80, 0x14, 0xfe, 0x34,
    0x3f, 0x95, 0x99, 0x47, 0x76, 0x3b, 0x61, 0xee,
    0x45, 0x39, 0x13, 0x13, 0x38, 0x49, 0x4f, 0xe6,
    0x7d, 0x8e, 0x30, 0x0b, 0x06, 0x09, 0x60, 0x86,
    0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x30,
    0x0a, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d,
    0x04, 0x03, 0x02, 0x04, 0x46, 0x30, 0x44, 0x02,
    0x20, 0x4a, 0x12, 0xf8, 0xd4, 0x2f, 0x90, 0x23,
    0x5c, 0x05, 0xa7, 0x71, 0x21, 0xcb, 0xeb, 0xae,
    0x15, 0xd5, 0x90, 0x14, 0x65, 0x58, 0xe9, 0xc9,
    0xb4, 0x7a, 0x1a, 0x38, 0xf7, 0xa3, 0x6a, 0x7d,
    0xc5, 0x02, 0x20, 0x20, 0xa4, 0x74, 0x28, 0x97,
    0xc3, 0x0a, 0xed, 0xa0, 0xa5, 0x6b, 0x36, 0xe1,
    0x4e, 0xbb, 0xc8, 0x5b, 0xbd, 0xb7, 0x44, 0x93,
    0xf9, 0x93, 0x58, 0x1e, 0xb0, 0x44, 0x4e, 0xd6,
    0xca, 0x94, 0x0b
};

/*
 * Load a key pair from private key and public key raw byte buffers.
 *
 * It should be possible to initialize a P256Keypair from byte raw bytes spans
 * for the private and public keys but it is not. This function copies
 * the raw keys directly into the P256SerializedKeypair internal representation
 * then deserialize that into the P256Keypair.
 */
static CHIP_ERROR InitP256KeypairFromRawKeys(ByteSpan priv_key,
    ByteSpan pub_key, P256Keypair &keypair)
{
	P256SerializedKeypair skp;

	ReturnErrorOnFailure(skp.SetLength(priv_key.size() + pub_key.size()));
	memcpy(skp.Bytes(), pub_key.data(), pub_key.size());
	memcpy(skp.Bytes() + pub_key.size(), priv_key.data(), priv_key.size());

	return keypair.Deserialize(skp);
}

/*
 * Copy a string including the terminator character into a buffer or
 * return a CHIP_ERROR_BUFFER_TOO_SMALL error if it won't fit.
 */
static CHIP_ERROR CopyStringToBuffer(char * buf, size_t bufSize,
    const char * str)
{
	size_t len = strlen(str) + 1;	/* include terminator character */

	ReturnErrorCodeIf(bufSize < len, CHIP_ERROR_BUFFER_TOO_SMALL);
	memcpy(buf, str, len);

	return CHIP_NO_ERROR;
}

extern "C" int adm_spake2p_config_check(int log)
{
	int rc = 1;

	if (!AlMatterConfig::ConfigValueExists(
	    AlMatterConfig::kConfigKey_SetupDiscriminator)) {
		if (log) {
			adm_log(LOG_DEBUG "discriminator not configured");
		}
		rc = 0;
	}

	if (!AlMatterConfig::ConfigValueExists(
	    AlMatterConfig::kConfigKey_Spake2pIterationCount)) {
		if (log) {
			adm_log(LOG_DEBUG
			    "SPAKE2+ iteration count not configured");
		}
		rc = 0;
	}

	if (!AlMatterConfig::ConfigValueExists(
	    AlMatterConfig::kConfigKey_Spake2pSalt)) {
		if (log) {
			adm_log(LOG_DEBUG "SPAKE2+ salt not configured");
		}
		rc = 0;
	}

	if (!AlMatterConfig::ConfigValueExists(
	    AlMatterConfig::kConfigKey_Spake2pVerifier)) {
		if (log) {
			adm_log(LOG_DEBUG "SPAKE2+ verifier not configured");
		}
		rc = 0;
	}

	return rc;
}

extern "C" int adm_cert_config_check(void)
{
	int rc = 1;

	if (!AlMatterConfig::ConfigValueExists(
	    AlMatterConfig::kConfigKey_DACPrivateKey)) {
		adm_log(LOG_DEBUG "DAC private key not configured");
		rc = 0;
	}

	if (!AlMatterConfig::ConfigValueExists(
	    AlMatterConfig::kConfigKey_DACCert)) {
		adm_log(LOG_DEBUG "DAC not configured");
		rc = 0;
	}

	if (!AlMatterConfig::ConfigValueExists(
	    AlMatterConfig::kConfigKey_PAICert)) {
		adm_log(LOG_DEBUG "PAI cert not configured");
		rc = 0;
	}

	return rc;
}

/*
 * Load and validate certificates from config. If the credentials are found
 * to be invalid, use test credentials.
 *
 * A Matter device cannot be onboarded if it doesn't have a full and consistent
 * set credentials. The vendor ID and product ID in the DAC certificate, PAI
 * certificate and QR code must all be consistent. Therefore, if the
 * certificates are not valid test SPAKE2+ credentials must also be used.
 *
 * Using a consistent set of test credentials enable the device to be onboarded
 * when the factory config is invalid, as well as early in development when
 * real credentials are not yet available.
 *
 * Rather than configuring vendor id, product id and DAC public key in the
 * factory config, these items are extracted from the DAC. This simplifies the
 * factory config and eliminates the possibility of inconsistency between
 * configured values and values in the DAC. This is different from how
 * the Matter examples have been implemented.
 */
extern "C" void adm_credentials_load(void)
{
	AdmDataProvider::AdmCredentialsLoad();
}

AdmDataProvider::AdmDataProvider() : DeviceAttestationCredentialsProvider(),
    CommissionableDataProvider()
{
}

void AdmDataProvider::AdmCredentialsLoad(void)
{
	CHIP_ERROR chip_err;
	enum ada_err err;
	size_t dac_size;
	P256PublicKey pub_key;
	AttestationCertVidPid dac_vid_pid;
	size_t len;
	u8 hash[ADM_HASH_UID_LEN];
	AdmDataProvider *dp = GetAdmDataProvider();

	chip_err = AlMatterConfig::ReadConfigValueStr(
	    AlMatterConfig::kConfigKey_UniqueId, dp->uid_buf,
	    sizeof(dp->uid_buf), len);
	if (chip_err != CHIP_NO_ERROR || len != sizeof(dp->uid_buf) - 1) {
		err = adm_unique_id_generate(hash, sizeof(hash));
		len = sizeof(dp->uid_buf);
		ayla_base64_encode(&hash, sizeof(hash), dp->uid_buf, &len);
		if (err) {
			adm_log(LOG_WARN "uid generation failed %d", err);
		} else {
			chip_err = AlMatterConfig::WriteConfigValueStr(
			    AlMatterConfig::kConfigKey_UniqueId,
			    dp->uid_buf, len);
			if (chip_err != CHIP_NO_ERROR) {
				adm_log(LOG_ERR "uid write failed %s",
				    ErrorStr(chip_err));
			}
		}
	}

	/*
	 * Check that certs and key have been configured.
	 */
	if (!adm_cert_config_check()) {
		adm_log(LOG_ERR "%s: cert not configured", __func__);
		goto validation_done;
	}

	/*
	 * Validate the Device Attestation Certificate (DAC)
	 */
	chip_err = AlMatterConfig::ReadConfigValueBin(
	    AlMatterConfig::kConfigKey_DACCert, dp->dac_buf, sizeof(dac_buf),
	    dac_size);
	if (chip_err != CHIP_NO_ERROR) {
		adm_log(LOG_ERR "Error loading DAC %s", ErrorStr(chip_err));
		goto validation_done;
	}
	dp->dac = ByteSpan(dp->dac_buf, dac_size);

	chip_err = ExtractPubkeyFromX509Cert(dp->dac, pub_key);
	if (chip_err != CHIP_NO_ERROR) {
		adm_log(LOG_ERR "Error extracting DAC public key %s",
		    ErrorStr(chip_err));
		goto validation_done;
	}
	ASSERT(pub_key.Length() <= sizeof(dp->dac_pub_key_buf));
	memcpy(dp->dac_pub_key_buf, pub_key.Bytes(), pub_key.Length());
	dp->dac_pub_key = ByteSpan(dp->dac_pub_key_buf, pub_key.Length());

	chip_err = ExtractVIDPIDFromX509Cert(dp->dac, dac_vid_pid);
	if (chip_err != CHIP_NO_ERROR) {
		adm_log(LOG_ERR
		    "Error extracting vendor and product from DAC %s",
		    ErrorStr(chip_err));
		goto validation_done;
	}

	/*
	 * Check that SPAKE2+ items have been configured.
	 */
	if (!adm_spake2p_config_check(1)) {
		goto validation_done;
	}

	dp->factory_config_valid = true;

validation_done:

	if (!dp->factory_config_valid) {
		/* invalid factory config */
		dp->dac = DevelopmentCerts::kDacCert;
		dp->dac_pub_key = DevelopmentCerts::kDacPublicKey;
		ExtractVIDPIDFromX509Cert(dp->dac, dac_vid_pid);
		dp->certification_declaration =
		    ByteSpan{ test_cert_declaration };
	}

	dp->vendor_id = dac_vid_pid.mVendorId.Value();
	dp->product_id = dac_vid_pid.mProductId.Value();

	if (dp->factory_config_valid) {
		adm_log(LOG_DEBUG "Matter factory config valid");
	} else {
		adm_log(LOG_WARN "Matter factory config is invalid - "
		    "OPERATING AS A TEST DEVICE");
	}

	log_dump(MOD_LOG_CLIENT, LOG_SEV_DEBUG2, "DAC: ", dp->dac.data(),
	    dp->dac.size(), NULL);
	adm_log(LOG_DEBUG "vendor %d product %d", dp->vendor_id, dp->product_id);
}

void AdmDataProvider::AdmDataProviderInit(const ByteSpan &cert_decl)
{
	AdmDataProvider *dp = GetAdmDataProvider();

	ASSERT(!dp->initialized);
	dp->initialized = true;

	dp->certification_declaration = cert_decl;

	AdmCredentialsLoad();
}

void AdmDataProvider::SetCertificationDeclaration(const ByteSpan &cert_decl)
{
	certification_declaration = cert_decl;
}

CHIP_ERROR AdmDataProvider::GetDeviceAttestationCert(
    MutableByteSpan &out_buffer)
{
	size_t dac_size;

	if (!factory_config_valid) {
		return CopySpanToMutableSpan(DevelopmentCerts::kDacCert,
			out_buffer);
	}

	ReturnErrorOnFailure(AlMatterConfig::ReadConfigValueBin(
	    AlMatterConfig::kConfigKey_DACCert, out_buffer.data(),
	    out_buffer.size(), dac_size));
	out_buffer.reduce_size(dac_size);

	return CHIP_NO_ERROR;
}

CHIP_ERROR AdmDataProvider::GetProductAttestationIntermediateCert(
    MutableByteSpan &out_buffer)
{
	size_t pai_cert_size;

	if (!factory_config_valid) {
		return CopySpanToMutableSpan(
		    ByteSpan(DevelopmentCerts::kPaiCert), out_buffer);
	}

	ReturnErrorOnFailure(AlMatterConfig::ReadConfigValueBin(
	    AlMatterConfig::kConfigKey_PAICert, out_buffer.data(),
	    out_buffer.size(), pai_cert_size));
	out_buffer.reduce_size(pai_cert_size);

	return CHIP_NO_ERROR;
}

CHIP_ERROR AdmDataProvider::GetCertificationDeclaration(
    MutableByteSpan &out_buffer)
{
	return CopySpanToMutableSpan(certification_declaration, out_buffer);
}

CHIP_ERROR AdmDataProvider::GetFirmwareInformation(MutableByteSpan &out_buffer)
{
	/* Not supported */
	out_buffer.reduce_size(0);

	return CHIP_NO_ERROR;
}

CHIP_ERROR AdmDataProvider::SignWithDeviceAttestationKey(
    const ByteSpan &message, MutableByteSpan &out_sig)
{
	P256ECDSASignature signature;
	P256Keypair keypair;
	uint8_t priv_key[kP256_PrivateKey_Length];
	size_t priv_key_len = sizeof(priv_key);
	P256PublicKey pub_key;

	VerifyOrReturnError(!out_sig.empty(),
	    CHIP_ERROR_INVALID_ARGUMENT);
	VerifyOrReturnError(!message.empty(),
	    CHIP_ERROR_INVALID_ARGUMENT);
	VerifyOrReturnError(out_sig.size() >= signature.Capacity(),
	    CHIP_ERROR_BUFFER_TOO_SMALL);

	if (!factory_config_valid) {
		ReturnErrorOnFailure(InitP256KeypairFromRawKeys(
		    DevelopmentCerts::kDacPrivateKey,
		    dac_pub_key,
		    keypair));
	} else {
		ReturnErrorOnFailure(AlMatterConfig::ReadConfigValueBin(
		    AlMatterConfig::kConfigKey_DACPrivateKey, priv_key,
		    priv_key_len, priv_key_len));
		ReturnErrorOnFailure(InitP256KeypairFromRawKeys(
		    ByteSpan(priv_key, priv_key_len), dac_pub_key, keypair));
		memset(priv_key, 0, sizeof(priv_key));
	}

	ReturnErrorOnFailure(keypair.ECDSA_sign_msg(message.data(),
	    message.size(), signature));

	return CopySpanToMutableSpan(ByteSpan{ signature.ConstBytes(),
	    signature.Length() }, out_sig);
}

CHIP_ERROR AdmDataProvider::GetSetupDiscriminator(uint16_t &discriminator)
{
	uint32_t val;

	if (!factory_config_valid && !adm_spake2p_config_check(0)) {
	        val = CHIP_DEVICE_CONFIG_USE_TEST_SETUP_DISCRIMINATOR;
	} else {
		ReturnErrorOnFailure(AlMatterConfig::ReadConfigValue(
		    AlMatterConfig::kConfigKey_SetupDiscriminator, val));
	}

	discriminator = static_cast<uint16_t>(val);
	return CHIP_NO_ERROR;
}

CHIP_ERROR AdmDataProvider::GetSpake2pIterationCount(uint32_t &iter_count)
{
	if (!factory_config_valid && !adm_spake2p_config_check(0)) {
		iter_count =
		    CHIP_DEVICE_CONFIG_USE_TEST_SPAKE2P_ITERATION_COUNT;
		return CHIP_NO_ERROR;
	}

	return AlMatterConfig::ReadConfigValue(
	    AlMatterConfig::kConfigKey_Spake2pIterationCount, iter_count);
}

CHIP_ERROR AdmDataProvider::GetSpake2pSalt(MutableByteSpan &out_buffer)
{
	char salt_b64[
	    BASE64_ENCODED_LEN(kSpake2p_Max_PBKDF_Salt_Length) + 1] =
	    { 0 };
	size_t salt_b64_len = 0;
	size_t salt_len;

	if (!factory_config_valid && !adm_spake2p_config_check(0)) {
		salt_b64_len = strlen(CHIP_DEVICE_CONFIG_USE_TEST_SPAKE2P_SALT);
		ReturnErrorCodeIf(salt_b64_len > sizeof(salt_b64),
		    CHIP_ERROR_BUFFER_TOO_SMALL);
		memcpy(salt_b64, CHIP_DEVICE_CONFIG_USE_TEST_SPAKE2P_SALT,
		    salt_b64_len);
	} else {
		ReturnErrorOnFailure(AlMatterConfig::ReadConfigValueStr(
		    AlMatterConfig::kConfigKey_Spake2pSalt, salt_b64,
		    sizeof(salt_b64), salt_b64_len));
	}

	salt_len = chip::Base64Decode32(salt_b64, salt_b64_len,
	    reinterpret_cast<uint8_t *>(salt_b64));

	ReturnErrorCodeIf(salt_len > out_buffer.size(),
	    CHIP_ERROR_BUFFER_TOO_SMALL);
	memcpy(out_buffer.data(), salt_b64, salt_len);
	out_buffer.reduce_size(salt_len);

	return CHIP_NO_ERROR;
}

CHIP_ERROR AdmDataProvider::GetSpake2pVerifier(MutableByteSpan &verifier_buffer,
    size_t &verifier_len)
{
	char verifier_b64[
	    BASE64_ENCODED_LEN(kSpake2p_VerifierSerialized_Length) + 1
	    ] = { 0 };
	size_t verifier_b64_len = 0;

	if (!factory_config_valid && !adm_spake2p_config_check(0)) {
	        verifier_b64_len = strlen(
	            CHIP_DEVICE_CONFIG_USE_TEST_SPAKE2P_VERIFIER);
	        ReturnErrorCodeIf(verifier_b64_len > sizeof(verifier_b64),
	            CHIP_ERROR_BUFFER_TOO_SMALL);
	        memcpy(verifier_b64,
	            CHIP_DEVICE_CONFIG_USE_TEST_SPAKE2P_VERIFIER,
		    verifier_b64_len);
	} else {
		ReturnErrorOnFailure(AlMatterConfig::ReadConfigValueStr(
		    AlMatterConfig::kConfigKey_Spake2pVerifier,
		    verifier_b64, sizeof(verifier_b64), verifier_b64_len));
	}

	verifier_len = chip::Base64Decode32(verifier_b64, verifier_b64_len,
	    reinterpret_cast<uint8_t *>(verifier_b64));
	ReturnErrorCodeIf(verifier_len > verifier_buffer.size(),
	    CHIP_ERROR_BUFFER_TOO_SMALL);
	memcpy(verifier_buffer.data(), verifier_b64, verifier_len);
	verifier_buffer.reduce_size(verifier_len);

	return CHIP_NO_ERROR;
}

/*
 * Read the vendor name from config. If it hasn't been configured and there's
 * a statically defined name, return that.
 */
CHIP_ERROR AdmDataProvider::GetVendorName(char *buf, size_t bufSize)
{
	CHIP_ERROR chip_err;
	size_t vendorNameLen = 0;

	chip_err = AlMatterConfig::ReadConfigValueStr(
	    AlMatterConfig::kConfigKey_VendorName, buf, bufSize, vendorNameLen);
	if (chip_err == CHIP_NO_ERROR) {
		return CHIP_NO_ERROR;
	}

#ifdef CHIP_DEVICE_CONFIG_DEVICE_VENDOR_NAME
	chip_err = CopyStringToBuffer(buf, bufSize,
	    CHIP_DEVICE_CONFIG_DEVICE_VENDOR_NAME);
#endif
	return chip_err;
}

CHIP_ERROR AdmDataProvider::GetVendorId(uint16_t &vendorId)
{
	vendorId = vendor_id;
	return CHIP_NO_ERROR;
}

CHIP_ERROR AdmDataProvider::GetProductName(char *buf, size_t bufSize)
{
	CHIP_ERROR chip_err;
	size_t len = 0;

	chip_err = AlMatterConfig::ReadConfigValueStr(
	    AlMatterConfig::kConfigKey_ProductName, buf, bufSize, len);
	if (chip_err == CHIP_NO_ERROR) {
		return CHIP_NO_ERROR;
	}

	return chip_err;
}

CHIP_ERROR AdmDataProvider::GetProductId(uint16_t &productId)
{
	productId = product_id;
	return CHIP_NO_ERROR;
}

CHIP_ERROR AdmDataProvider::GetPartNumber(char *buf, size_t bufSize)
{
	/* return OEM model as product name */
	return CopyStringToBuffer(buf, bufSize, oem_model);
}

CHIP_ERROR AdmDataProvider::GetProductURL(char *buf, size_t bufSize)
{
	CHIP_ERROR chip_err;
	size_t len = 0;

	chip_err = AlMatterConfig::ReadConfigValueStr(
	    AlMatterConfig::kConfigKey_ProductURL, buf, bufSize, len);
	if (chip_err == CHIP_NO_ERROR) {
		return CHIP_NO_ERROR;
	}

	return chip_err;
}

CHIP_ERROR AdmDataProvider::GetProductLabel(char *buf, size_t bufSize)
{
	CHIP_ERROR chip_err;
	size_t len = 0;

	chip_err = AlMatterConfig::ReadConfigValueStr(
	    AlMatterConfig::kConfigKey_ProductLabel, buf, bufSize, len);
	if (chip_err == CHIP_NO_ERROR) {
		return CHIP_NO_ERROR;
	}

	return chip_err;
}

CHIP_ERROR AdmDataProvider::GetSerialNumber(char *buf, size_t bufSize)
{
	/* return DSN as serial number */
	return CopyStringToBuffer(buf, bufSize, conf_sys_dev_id);
}

/*
 * Derived from GenericDeviceInstanceInfoProvider implementation
 */
CHIP_ERROR AdmDataProvider::GetManufacturingDate(uint16_t &year, uint8_t &month,
    uint8_t &day)
{
	CHIP_ERROR chip_err;
	char date_str[ConfigurationManager::kMaxManufacturingDateLength + 1];
	size_t date_len;
	char *end;

	/*
	 * Date string format is YYYY-MM-DD<vendor-defined>
	 *
	 * Length must be 10 to 32 characters.
	 */
	chip_err = AlMatterConfig::ReadConfigValueStr(
	    AlMatterConfig::kConfigKey_ManufacturingDate, date_str,
	    sizeof(date_str), date_len);
	SuccessOrExit(chip_err);

	VerifyOrExit(date_len < sizeof(date_str),
	    chip_err = CHIP_ERROR_INVALID_ARGUMENT);

	date_str[10] = '\0';	/* trim vendor defined suffix */

	year = static_cast<uint16_t>(strtoul(date_str, &end, 10));
	VerifyOrExit(end == date_str + 4,
	    chip_err = CHIP_ERROR_INVALID_ARGUMENT);

	month = static_cast<uint8_t>(strtoul(date_str + 5, &end, 10));
	VerifyOrExit(end == date_str + 7,
	    chip_err = CHIP_ERROR_INVALID_ARGUMENT);

	day = static_cast<uint8_t>(strtoul(date_str + 8, &end, 10));
	VerifyOrExit(end == date_str + 10,
	    chip_err = CHIP_ERROR_INVALID_ARGUMENT);

exit:
	if (chip_err != CHIP_NO_ERROR &&
	    chip_err != CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND)
	{
		adm_log(LOG_WARN, "invalid manufacturing date %s", date_str);
	}
	return chip_err;
}

CHIP_ERROR AdmDataProvider::GetHardwareVersion(uint16_t &version)
{
	ChipError chip_err;
	uint32_t value = 0;

	chip_err = AlMatterConfig::ReadConfigValue(
	    AlMatterConfig::kConfigKey_HardwareVersion, value);
	if (chip_err == CHIP_NO_ERROR)
	{
		version = static_cast<uint16_t>(value);
		return CHIP_NO_ERROR;
	}

#ifdef CHIP_DEVICE_CONFIG_DEFAULT_DEVICE_HARDWARE_VERSION
	version = static_cast<uint16_t>(
            CHIP_DEVICE_CONFIG_DEFAULT_DEVICE_HARDWARE_VERSION);
	chip_err = CHIP_NO_ERROR;
#endif
	return chip_err;
}

CHIP_ERROR AdmDataProvider::GetHardwareVersionString(char *buf, size_t bufSize)
{
	ChipError chip_err;
	size_t len = 0;

	chip_err = AlMatterConfig::ReadConfigValueStr(
	    AlMatterConfig::kConfigKey_HardwareVersionString, buf, bufSize,
	    len);
	if (chip_err == CHIP_NO_ERROR) {
		return CHIP_NO_ERROR;
	}

#ifdef CHIP_DEVICE_CONFIG_DEFAULT_DEVICE_HARDWARE_VERSION_STRING
	chip_err = CopyStringToBuffer(buf, bufSize,
	    CHIP_DEVICE_CONFIG_DEFAULT_DEVICE_HARDWARE_VERSION_STRING);
#endif
	return chip_err;
}

CHIP_ERROR AdmDataProvider::GetRotatingDeviceIdUniqueId(
    MutableByteSpan &uniqueIdSpan)
{
	ChipError chip_err = CHIP_ERROR_WRONG_KEY_TYPE;
	size_t uniqueIdLen = 0;

	if (uniqueIdSpan.size()
	    < CHIP_DEVICE_CONFIG_ROTATING_DEVICE_ID_UNIQUE_ID_LENGTH) {
		return CHIP_ERROR_BUFFER_TOO_SMALL;
	}

	chip_err = AlMatterConfig::ReadConfigValueBin(
	    AlMatterConfig::kConfigKey_RotatingDevIdUniqueId,
	    uniqueIdSpan.data(), uniqueIdSpan.size(), uniqueIdLen);
	if (chip_err == CHIP_NO_ERROR) {
		uniqueIdSpan.reduce_size(uniqueIdLen);
	}

	return chip_err;
}

AdmDataProvider *AdmDataProvider::GetAdmDataProvider()
{
	static AdmDataProvider adm_data_provider;	/* singleton instance */
	static bool not_first_call;

	if (not_first_call) {
		ASSERT(adm_data_provider.initialized);
	} else {
		not_first_call = true;
	}
	return &adm_data_provider;
}

DeviceAttestationCredentialsProvider *GetAdmDACProvider()
{
	return AdmDataProvider::GetAdmDataProvider();
}

CommissionableDataProvider *GetAdmCommissionableDataProvider()
{
	return AdmDataProvider::GetAdmDataProvider();
}

DeviceInstanceInfoProvider *GetAdmDeviceInstanceInfoProvider()
{
	return AdmDataProvider::GetAdmDataProvider();
}

} /* namespace Ayla */
} /* namespace DeviceLayer */
} /* namespace chip */

