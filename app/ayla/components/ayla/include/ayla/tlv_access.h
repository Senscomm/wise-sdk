/*
 * Copyright 2019-2021 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_TLV_ACCESS_H__
#define __AYLA_TLV_ACCESS_H__

/*
 * Get the first TLV of the specified type from a received packet.
 */
struct ayla_tlv *tlv_get(enum ayla_tlv_type type, void *buf, size_t len);

/*
 * Get the first TLV of the specified type from a received packet,
 * The return value is 0 on success, the MCU error code on failure.
 */
int tlv_getp(struct ayla_tlv **tlvp, enum ayla_tlv_type type,
	void *buf, size_t len);


/*
 * Get values from TLV.
 * These functions return 0 on success.
 * Return -1 if the TLV value is out of range for the type requested.
 */
int tlv_u32_get(u32 *val, const struct ayla_tlv *tlv);
int tlv_s32_get(s32 *val, const struct ayla_tlv *tlv);
int tlv_u16_get(u16 *val, const struct ayla_tlv *tlv);
int tlv_s16_get(s16 *val, const struct ayla_tlv *tlv);
int tlv_u8_get(u8 *val, const struct ayla_tlv *tlv);
int tlv_s8_get(s8 *val, const struct ayla_tlv *tlv);
int tlv_utf8_get(char *out, size_t out_len, const struct ayla_tlv *tlv);

#endif /* __AYLA_TLV_ACCESS_H__ */
