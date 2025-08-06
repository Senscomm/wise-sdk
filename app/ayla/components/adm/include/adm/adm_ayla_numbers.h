/*
 * Copyright 2022 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_ADM_AYLA_NUMBERS_H__
#define __AYLA_ADM_AYLA_NUMBERS_H__

#include <adm/adm.h>
#include <adm/adm_csa_numbers.h>

/**
 * This file defines Ayla assigned numbers used by Ayla vendor extensions to
 * the Matter specifications.
 */

/**
 * Ayla Base Cluster Ids
 */
#define ADM_AYLA_BASE_CID	ADM_ID_GEN(ADM_AYLA_NETWORKS_MID, 0xFC00)

#define ADM_AYLA_BASE_DSN_AID		ADM_ID_GEN(ADM_AYLA_NETWORKS_MID, 0)
#define ADM_AYLA_BASE_OEM_ID_AID	ADM_ID_GEN(ADM_AYLA_NETWORKS_MID, 1)
#define ADM_AYLA_BASE_OEM_MODEL_AID	ADM_ID_GEN(ADM_AYLA_NETWORKS_MID, 2)
#define ADM_AYLA_BASE_TEMPLATE_VER_AID	ADM_ID_GEN(ADM_AYLA_NETWORKS_MID, 3)
#define ADM_AYLA_BASE_SETUP_TOKEN_AID	ADM_ID_GEN(ADM_AYLA_NETWORKS_MID, 4)

/**
 * Ayla Local Control Cluster Ids
 */
#define ADM_LOCAL_CONTROL_CID	ADM_ID_GEN(ADM_AYLA_NETWORKS_MID, 0xFC01)

#define ADM_MAILBOX_MASK_AID		ADM_ID_GEN(ADM_AYLA_NETWORKS_MID, 0)
#define ADM_MAILBOX0_INUSE_AID		ADM_ID_GEN(ADM_AYLA_NETWORKS_MID, 0x100)
#define ADM_INBOX0_AID			ADM_ID_GEN(ADM_AYLA_NETWORKS_MID, 0x101)
#define ADM_OUTBOX0_AID			ADM_ID_GEN(ADM_AYLA_NETWORKS_MID, 0x102)
#define ADM_MAILBOX1_INUSE_AID		ADM_ID_GEN(ADM_AYLA_NETWORKS_MID, 0x110)
#define ADM_INBOX1_AID			ADM_ID_GEN(ADM_AYLA_NETWORKS_MID, 0x111)
#define ADM_OUTBOX1_AID			ADM_ID_GEN(ADM_AYLA_NETWORKS_MID, 0x112)
#define ADM_MAILBOX2_INUSE_AID		ADM_ID_GEN(ADM_AYLA_NETWORKS_MID, 0x120)
#define ADM_INBOX2_AID			ADM_ID_GEN(ADM_AYLA_NETWORKS_MID, 0x121)
#define ADM_OUTBOX2_AID			ADM_ID_GEN(ADM_AYLA_NETWORKS_MID, 0x122)

#endif
