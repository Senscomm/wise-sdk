/*
 * Copyright 2022 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_ADM_AYLA_BASE_H__
#define __AYLA_ADM_AYLA_BASE_H__

#include <adm/adm.h>
#include <adm/adm_ayla_numbers.h>

/**
 * This file provides definitions for Ayla Base Cluster vendor extension to the
 * the Matter specifications.
 */

#ifdef __cplusplus

using namespace chip;

/*
 * ZCL define required by ZAP generated code.
 */
static constexpr ClusterId ZCL_AYLA_BASE_CLUSTER_ID = ADM_AYLA_BASE_CID;

/*
 * Ayla Cluster definition
 */
namespace chip {
namespace app {
namespace Clusters {
namespace AylaBase {

static constexpr ClusterId Id = ADM_AYLA_BASE_CID;

namespace Attributes {

namespace Dsn {
static constexpr AttributeId Id = ADM_AYLA_BASE_DSN_AID;
}

namespace OemId {
static constexpr AttributeId Id = ADM_AYLA_BASE_OEM_ID_AID;
}

namespace OemModel {
static constexpr AttributeId Id = ADM_AYLA_BASE_OEM_MODEL_AID;
}

namespace TemplateVersion {
static constexpr AttributeId Id = ADM_AYLA_BASE_TEMPLATE_VER_AID;
}

namespace SetupToken {
static constexpr AttributeId Id = ADM_AYLA_BASE_SETUP_TOKEN_AID;
}

} /* Attributes */
} /* AylaBase */
} /* Clusters */
} /* app */
} /* chip */

using namespace chip;

class AylaBaseServer
{
public:
    void initAylaBaseServer(EndpointId endpoint);
};

void emberAfAylaBaseClusterInitCallback(EndpointId endpoint);

void emberAfPluginAylaBaseClusterServerPostInitCallback(EndpointId endpoint);

void MatterAylaBasePluginServerInitCallback();

#endif /* __cplusplus */

#endif
