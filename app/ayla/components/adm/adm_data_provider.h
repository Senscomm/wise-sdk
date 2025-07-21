/*
 * Copyright 2023 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
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
#pragma once

#include <credentials/DeviceAttestationCredsProvider.h>
#include <platform/CommissionableDataProvider.h>
#include <platform/DeviceInstanceInfoProvider.h>
#include <credentials/CHIPCert.h>
#include <ayla/base64.h>

#define	ADM_HASH_UID_LEN	16

namespace chip {
namespace DeviceLayer {
namespace Ayla {

class AdmDataProvider :
	public Credentials::DeviceAttestationCredentialsProvider,
	public CommissionableDataProvider,
	public DeviceInstanceInfoProvider
{
public:
	/*
	 * DeviceAttestationCredentialsProvider methods
	 */
	CHIP_ERROR GetCertificationDeclaration(
	    MutableByteSpan &out_cd_buffer) override;
	CHIP_ERROR GetFirmwareInformation(
	    MutableByteSpan &out_firmware_info_buffer) override;
	CHIP_ERROR GetDeviceAttestationCert(
	    MutableByteSpan &out_dac_buffer) override;
	CHIP_ERROR GetProductAttestationIntermediateCert(
	    MutableByteSpan &out_pai_buffer) override;
	CHIP_ERROR SignWithDeviceAttestationKey(
	    const ByteSpan &message_to_sign,
	    MutableByteSpan &out_signature_buffer) override;

	/*
	 * CommissionableDataProvider methods
	 */
	CHIP_ERROR GetSetupDiscriminator(uint16_t &setupDiscriminator) override;
	CHIP_ERROR SetSetupDiscriminator(uint16_t setupDiscriminator) override
	{
		return CHIP_ERROR_NOT_IMPLEMENTED;
	}
	CHIP_ERROR GetSpake2pIterationCount(uint32_t &iterationCount) override;
	CHIP_ERROR GetSpake2pSalt(MutableByteSpan &saltBuf) override;
	CHIP_ERROR GetSpake2pVerifier(MutableByteSpan &verifierBuf,
	    size_t &outVerifierLen) override;
	CHIP_ERROR GetSetupPasscode(uint32_t &setupPasscode) override
	{
		return CHIP_ERROR_NOT_IMPLEMENTED;
	}
	CHIP_ERROR SetSetupPasscode(uint32_t setupPasscode) override
	{
		return CHIP_ERROR_NOT_IMPLEMENTED;
	}

	/*
	 * DeviceInstanceInfoProvider methods
	 */
	CHIP_ERROR GetVendorName(char *buf, size_t bufSize) override;
	CHIP_ERROR GetVendorId(uint16_t &vendorId) override;
	CHIP_ERROR GetProductName(char *buf, size_t bufSize) override;
	CHIP_ERROR GetProductId(uint16_t &productId) override;
	CHIP_ERROR GetPartNumber(char * buf, size_t bufSize) override;
	CHIP_ERROR GetProductURL(char * buf, size_t bufSize) override;
	CHIP_ERROR GetProductLabel(char * buf, size_t bufSize) override;
	CHIP_ERROR GetSerialNumber(char *buf, size_t bufSize) override;
	CHIP_ERROR GetManufacturingDate(uint16_t &year, uint8_t &month,
	    uint8_t &day) override;
	CHIP_ERROR GetHardwareVersion(uint16_t &hardwareVersion) override;
	CHIP_ERROR GetHardwareVersionString(char *buf, size_t bufSize) override;
	CHIP_ERROR GetRotatingDeviceIdUniqueId(MutableByteSpan &uniqueIdSpan)
	    override;
	void SetCertificationDeclaration(const ByteSpan &cert_decl);

	static void AdmDataProviderInit(const ByteSpan &cert_decl);
	static void AdmCredentialsLoad(void);
	static AdmDataProvider *GetAdmDataProvider();

private:
	bool factory_config_valid;
	bool initialized;
	uint8_t dac_buf[Credentials::kMaxDERCertLength];
	char uid_buf[BASE64_LEN_EXPAND(ADM_HASH_UID_LEN) + 1];
	ByteSpan dac;
	uint8_t dac_pub_key_buf[Crypto::kP256_PublicKey_Length];
	ByteSpan dac_pub_key;
	ByteSpan certification_declaration;
	uint16_t vendor_id;
	uint16_t product_id;

	AdmDataProvider();
};

Credentials::DeviceAttestationCredentialsProvider *GetAdmDACProvider();
CommissionableDataProvider *GetAdmCommissionableDataProvider();
DeviceInstanceInfoProvider *GetAdmDeviceInstanceInfoProvider();

} /* namespace Ayla */
} /* namespace DeviceLayer */
} /* namespace chip */

