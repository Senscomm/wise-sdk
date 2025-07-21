/*
 * Copyright 2022 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_ADM_INT_H__
#define __AYLA_ADM_INT_H__

#ifdef __cplusplus
extern "C" {
#endif

#define ADM_LOG_INDEX_ALL	0xff
#define ADM_TEST_VENDOR_ID	0xFFF1	/* assigned by CSA */

void adm_log(const char *fmt, ...);
const char *adm_log_mod_name(int index);
int adm_log_index(const char *module);
int adm_log_enabled(u8 index);
void adm_log_disable(u8 index);
void adm_log_enable(u8 index);
int adm_spake2p_config_check(int log);
void adm_credentials_load(void);
enum ada_err adm_unique_id_generate(u8 *uid, size_t uid_len);

#ifdef __cplusplus
}
#endif

#endif
