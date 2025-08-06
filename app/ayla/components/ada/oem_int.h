/*
 * Copyright 2022 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_OEM_INT_H__
#define __AYLA_OEM_INT_H__
/*
 * Set the OEM ID in the running config.
 */
enum ada_err oem_id_set(char *id);

/*
 * Set the previously encrypted OEM key.
 */
enum ada_err oem_enc_key_set(u8 *key, size_t len);

#endif
