/*
 * Copyright 2025 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_ADM_TRIGGER_H__
#define __AYLA_ADM_TRIGGER_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_TEST_EVENT_TRIGGER_ENABLED

#include <app/TestEventTriggerDelegate.h>

void adm_set_test_event_trigger_delegate(
	SimpleTestEventTriggerDelegate * pDelegate);
#endif

#ifdef __cplusplus
}
#endif

#endif
