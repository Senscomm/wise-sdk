/*
 * Copyright 2022 Ayla Networks, Inc.  All rights reserved.
 */
#include <string.h>
#include <stdio.h>
#include <app-common/zap-generated/attributes/Accessors.h>
#include <app-common/zap-generated/cluster-objects.h>
#include <app/EventLogging.h>
#include <app/InteractionModelEngine.h>
#include <app/util/attribute-storage.h>
#include <app/AttributeAccessInterface.h>
#include <app/data-model/Nullable.h>
#include <app/reporting/reporting.h>
#include <app/util/af.h>
#include <app/util/util.h>
#include <ayla/utypes.h>
#include <ayla/assert.h>
#include <ayla/conf.h>
#include <ayla/clock.h>
#include <ayla/log.h>
#include <ayla/mod_log.h>
#include <ayla/wifi_error.h>
#include <ada/err.h>
#include <ada/ada_conf.h>
#include <ada/prop.h>
#include <ada/client.h>
#include <adm/adm.h>
#include <adm/adm_ayla_base.h>

extern "C" void adm_log(const char *fmt, ...);

using namespace chip;
using namespace chip::app;
using namespace chip::app::Clusters;
using namespace chip::app::Clusters::AylaBase;
using namespace chip::app::Clusters::AylaBase::Attributes;

namespace {

class AylaBaseAttrAccess : public AttributeAccessInterface
{
public:
	AylaBaseAttrAccess() : AttributeAccessInterface(
	    Optional<EndpointId>::Missing(), AylaBase::Id) {}

	CHIP_ERROR Read(const ConcreteReadAttributePath &aPath,
	    AttributeValueEncoder &aEncoder) override;
	CHIP_ERROR Write(const ConcreteDataAttributePath &aPath,
	    AttributeValueDecoder &aDecoder) override;

private:
	CHIP_ERROR WriteSetupToken(AttributeValueDecoder &aDecoder);
};

static AylaBaseAttrAccess gAylaBaseAttrAccess;

CHIP_ERROR AylaBaseAttrAccess::Read(const ConcreteReadAttributePath &aPath,
    AttributeValueEncoder &aEncoder)
{
	CHIP_ERROR status = CHIP_NO_ERROR;
	char token[CLIENT_SETUP_TOK_LEN];

	if (aPath.mClusterId != AylaBase::Id)
	{
		return CHIP_ERROR_INVALID_ARGUMENT;
	}

	switch (aPath.mAttributeId) {
	case Dsn::Id:
		status = aEncoder.Encode(chip::CharSpan(conf_sys_dev_id,
		    strlen(conf_sys_dev_id)));
		break;
	case OemId::Id:
		status = aEncoder.Encode(chip::CharSpan(oem,
		    strlen(oem)));
		break;
	case OemModel::Id:
		status = aEncoder.Encode(chip::CharSpan(oem_model,
		    strlen(oem_model)));
		break;
	case TemplateVersion::Id:
		status = aEncoder.Encode(chip::CharSpan(template_version,
		    strlen(template_version)));
		break;
	case SetupToken::Id:
		if (client_get_setup_token(token, sizeof(token)) < 0) {
			adm_log(LOG_ERR "%s get setup token error", __func__);
			return CHIP_ERROR_BUFFER_TOO_SMALL;
		}
		status = aEncoder.Encode(chip::CharSpan(token,
		    strlen(token)));
		break;
	default:
		/* not readable, return nothing */
		break;
	}

	return status;
}

CHIP_ERROR AylaBaseAttrAccess::WriteSetupToken(AttributeValueDecoder &aDecoder)
{
	chip::CharSpan span;
	char token[CLIENT_SETUP_TOK_LEN];
	size_t length;

	ReturnErrorOnFailure(aDecoder.Decode(span));

	length = span.size();
	if (length >= sizeof(token)) {
		adm_log(LOG_WARN "%s invalid setup token", __func__);
		return CHIP_ERROR_INVALID_ARGUMENT;
	}
	memcpy(token, span.data(), length);
	token[length] = '\0';

	client_set_setup_token(token);

	return CHIP_NO_ERROR;
}

CHIP_ERROR AylaBaseAttrAccess::Write(const ConcreteDataAttributePath &aPath,
    AttributeValueDecoder &aDecoder)
{
	CHIP_ERROR status = CHIP_NO_ERROR;

	VerifyOrDie(aPath.mClusterId == AylaBase::Id);

	switch (aPath.mAttributeId) {
	case SetupToken::Id:
		status = WriteSetupToken(aDecoder);
		break;
	default:
		break;
	}

	return status;
}

} /* namespace */

void emberAfAylaBaseClusterServerInitCallback(chip::EndpointId endpoint)
{
}

void emberAfPluginAylaBaseClusterServerPostInitCallback(chip::EndpointId endpoint)
{
}

void MatterAylaBasePluginServerInitCallback()
{
	registerAttributeAccessOverride(&gAylaBaseAttrAccess);
}
