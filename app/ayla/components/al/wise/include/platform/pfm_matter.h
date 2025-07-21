/*
 * Copyright 2022 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_PFM_MATTER_H__
#define __AYLA_PFM_MATTER_H__

#include <platform/senscomm/scm1612s/ConfigurationManagerImpl.h>

using AlMatterConfig = chip::DeviceLayer::Internal::SCM1612SConfig;

/*
 * Discovery Capabilities Bitmask (8-bit value) in QR code as defined in Matter
 * Core Spec.
 *
 * It is configured by as CONFIG_RENDEZVOUS_MODE.
 * The configured value is used in the device layer.
 */
#ifdef AYLA_MATTER_DISCOVERY_MASK
#error "AYLA_MATTER_DISCOVERY_MASK must not be defined"
#endif
#define AYLA_MATTER_DISCOVERY_MASK	CONFIG_RENDEZVOUS_MODE

#endif /* __AYLA_PFM_MATTER_H__ */
