/*
 * Copyright 2024 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_CLIENT_BATCH_H__
#define __AYLA_CLIENT_BATCH_H__

void client_batch_conf_load(void);
void client_batch_conf_export(void);
void client_batch_info(void);
enum ada_err client_batch_cli(int argc, char **argv);
enum ada_err client_batch_add(const char *name, enum ayla_tlv_type type,
    const void *val, size_t val_len, u8 echo, struct prop_dp_meta *metadata);
u8 client_batch_enabled(void);
void client_batch_reset(void);
void client_batch_init(void);

#endif /* __AYLA_ADA_BATCH_H__ */
