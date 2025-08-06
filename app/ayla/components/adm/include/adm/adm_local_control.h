/*
 * Copyright 2022 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_ADM_LOCAL_CONTROL_H__
#define __AYLA_ADM_LOCAL_CONTROL_H__

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
static constexpr ClusterId ZCL_AYLA_LOCAL_CONTROL_CLUSTER_ID =
    ADM_LOCAL_CONTROL_CID;

/*
 * Ayla Cluster definition
 */
namespace chip {
namespace app {
namespace Clusters {
namespace AylaLocalControl {

static constexpr ClusterId Id = ADM_LOCAL_CONTROL_CID;

namespace Attributes {

namespace MailboxMask {
static constexpr AttributeId Id = ADM_MAILBOX_MASK_AID;
}

namespace Mailbox0InUse {
static constexpr AttributeId Id = ADM_MAILBOX0_INUSE_AID;
}

namespace Inbox0 {
static constexpr AttributeId Id = ADM_INBOX0_AID;
}

namespace Outbox0 {
static constexpr AttributeId Id = ADM_OUTBOX0_AID;
}

namespace Mailbox1InUse {
static constexpr AttributeId Id = ADM_MAILBOX1_INUSE_AID;
}

namespace Inbox1 {
static constexpr AttributeId Id = ADM_INBOX1_AID;
}

namespace Outbox1 {
static constexpr AttributeId Id = ADM_OUTBOX1_AID;
}

namespace Mailbox2InUse {
static constexpr AttributeId Id = ADM_MAILBOX2_INUSE_AID;
}

namespace Inbox2 {
static constexpr AttributeId Id = ADM_INBOX2_AID;
}

namespace Outbox2 {
static constexpr AttributeId Id = ADM_OUTBOX2_AID;
}

} /* Attributes */
} /* AylaLocalControl */
} /* Clusters */
} /* app */
} /* chip */

using namespace chip;

class AylaLocalControlServer
{
public:
    void initAylaLocalControlServer(EndpointId endpoint);
};

void emberAfAylaLocalControlClusterInitCallback(EndpointId endpoint);

void emberAfPluginAylaLocalControlClusterServerPostInitCallback(
    EndpointId endpoint);

void MatterAylaLocalControlPluginServerInitCallback();

#endif /* __cplusplus */

#endif
