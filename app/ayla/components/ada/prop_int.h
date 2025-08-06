/*
 * Copyright 2021 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_PROP_INT_H__
#define __AYLA_PROP_INT_H__

/*
 * There are serveral internal APIs currently in <ada/prop.h> that should be
 * moved here eventually.
 */
enum ayla_tlv_type;
int prop_type_is_file_or_msg(enum ayla_tlv_type);
int prop_is_file_or_msg(struct prop *prop);

#endif /* __AYLA_PROP_INT_H__ */
