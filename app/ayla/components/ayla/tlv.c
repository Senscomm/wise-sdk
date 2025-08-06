/*
 * Copyright 2012-2021 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */

#include <string.h>
#include <ayla/utypes.h>
#include <ayla/assert.h>
#include <ayla/ayla_proto_mcu.h>
#include <ayla/endian.h>
#include <ayla/tlv.h>
#include <ayla/tlv_access.h>
#ifdef AYLA_TLV_CONF_EXPORT_SUPPORT
#include <ayla/utf8.h>
#endif

/*
 * Get the first TLV of the specified type.
 * Return 0 on success, with *tlvp set to point to the TLV in the buffer.
 */
int tlv_getp(struct ayla_tlv **tlvp, enum ayla_tlv_type type,
		void *buf, size_t len)
{
	struct ayla_cmd *cmd;
	struct ayla_tlv *tlv;
	size_t rlen;
	size_t tlen;

	if (len < sizeof(*cmd)) {
		return AERR_LEN_ERR;
	}
	rlen = len - sizeof(*cmd);
	cmd = buf;
	tlv = (struct ayla_tlv *)(cmd + 1);

	while (rlen >= sizeof(*tlv)) {
		tlen = sizeof(*tlv) + tlv->len;
		if (tlen > rlen) {
			return AERR_LEN_ERR;
		}
		if (tlv->type == type) {
			*tlvp = tlv;
			return 0;
		}
		rlen -= tlen;
		tlv = (struct ayla_tlv *)((char *)tlv + tlen);
	}
	return AERR_TLV_MISSING;
}

/*
 * Get the first TLV of the specified type from a received packet.
 */
struct ayla_tlv *tlv_get(enum ayla_tlv_type type, void *buf, size_t len)
{
	struct ayla_tlv *tlv = NULL;

	tlv_getp(&tlv, type, buf, len);
	return tlv;
}

/*
 * Get u32 from TLV.
 * Note: if TLV is an integer type (as opposed to a special unsigned TLV),
 * use the signed version and check for negative values.
 */
int tlv_u32_get(u32 *dest, const struct ayla_tlv *tlv)
{
	s32 val;

	if (tlv->type == ATLV_INT) {
		if (tlv_s32_get(&val, tlv)) {
			return -1;
		}
		if (val < 0) {
			return -1;
		}
		*dest = val;
		return 0;
	}
	switch (tlv->len) {
	case 1:
		*dest = *(u8 *)TLV_VAL(tlv);
		break;
	case 2:
		*dest = get_ua_be16(TLV_VAL(tlv));
		break;
	case 4:
		*dest = get_ua_be32(TLV_VAL(tlv));
		break;
	default:
		return -1;
	}
	return 0;
}

int tlv_s32_get(s32 *dest, const struct ayla_tlv *tlv)
{
	switch (tlv->len) {
	case 1:
		*dest = *(s8 *)TLV_VAL(tlv);
		break;
	case 2:
		*dest = (s16)get_ua_be16(TLV_VAL(tlv));
		break;
	case 4:
		*dest = (s32)(int)get_ua_be32(TLV_VAL(tlv));
		break;
	default:
		return -1;
	}
	return 0;
}

int tlv_u16_get(u16 *dest, const struct ayla_tlv *tlv)
{
	int rc;
	u32 val;

	rc = tlv_u32_get(&val, tlv);
	if (rc) {
		return rc;
	}
	if (val > MAX_U16) {
		return -1;
	}
	*dest = val;
	return 0;
}

int tlv_s16_get(s16 *dest, const struct ayla_tlv *tlv)
{
	int rc;
	s32 val;

	rc = tlv_s32_get(&val, tlv);
	if (rc) {
		return rc;
	}
	if (val > MAX_S16 || val < MIN_S16) {
		return -1;
	}
	*dest = (s16)val;
	return 0;
}

int tlv_u8_get(u8 *dest, const struct ayla_tlv *tlv)
{
	int rc;
	u32 val;

	rc = tlv_u32_get(&val, tlv);
	if (rc) {
		return rc;
	}
	if (val > MAX_U8) {
		return -1;
	}
	*dest = val;
	return 0;
}

int tlv_s8_get(s8 *dest, const struct ayla_tlv *tlv)
{
	int rc;
	s32 val;

	rc = tlv_s32_get(&val, tlv);
	if (rc) {
		return rc;
	}
	if (val > MAX_S8 || val < MIN_S8) {
		return -1;
	}
	*dest = (s8)val;
	return 0;
}

int tlv_utf8_get(char *out, size_t out_len, const struct ayla_tlv *tlv)
{
	if (tlv->len >= out_len) {
		return -1;
	}
	memcpy(out, TLV_VAL(tlv), tlv->len);
	out[tlv->len] = '\0';
	return tlv->len;
}

#ifdef AYLA_TLV_CONF_EXPORT_SUPPORT
static ssize_t tlv_put_conf(void *buf, size_t buflen, enum conf_token *token,
		int ntoken)
{
	u8 *data;
	ssize_t len;
	ssize_t tmp;
	int i;

	if (buflen < 2 || ntoken > 0xff) {
		return -1;
	}

	data = ((u8 *)buf) + 2;
	len = 0;
	buflen -= 2;
	for (i = 0; i < ntoken; i++) {
		tmp = utf8_encode(data, buflen, token[i]);
		if (tmp <= 0) {
			return -1;
		}
		data += tmp;
		len += tmp;
		buflen -= tmp;
	}
	if (len > 0xff) {
		return -1;
	}
	((u8 *)buf)[0] = ATLV_CONF;
	((u8 *)buf)[1] = len;
	return len + 2;
}
#endif

enum conf_error tlv_import(u8 *buf, size_t *buflen,
		enum ayla_tlv_type type, const void *data, size_t len)
{
	u32 val;
	s32 sval;
	ssize_t size;

	if (!buf || !buflen || !data) {
		return CONF_ERR_NONE;
	}
	if (*buflen <= 2) {
		return CONF_ERR_LEN;
	}
	switch (type) {
	case ATLV_BOOL:
	case ATLV_UINT:
		switch (len) {
		case sizeof(u8):
			val = *(u8 *)data;
			break;
		case sizeof(u16):
			val = *(u16 *)data;
			break;
		case sizeof(u32):
			val = *(u32 *)data;
			break;
		default:
			return CONF_ERR_RANGE;
		}
		size = tlv_put_uint(buf, *buflen, val);
		((struct ayla_tlv *)buf)->type = type;
		break;
	case ATLV_INT:
		switch (len) {
		case sizeof(s8):
			sval = *(s8 *)data;
			break;
		case sizeof(s16):
			sval = *(s16 *)data;
			break;
		case sizeof(s32):
			sval = *(s32 *)data;
			break;
		default:
			return CONF_ERR_RANGE;
		}
		size = tlv_put_int(buf, *buflen, sval);
		break;
	case ATLV_UTF8:
		size = tlv_put_str(buf, *buflen, data);
		break;
#ifdef AYLA_TLV_CONF_EXPORT_SUPPORT
	case ATLV_CONF:
		size = tlv_put_conf(buf, *buflen, (enum conf_token *)data,
		    len / sizeof(enum conf_token));
		break;
#endif
	default:
		size = tlv_put(buf, *buflen, type, data, len);
		break;
	}
	if (size < 0) {
		return CONF_ERR_LEN;
	}

	*buflen = size;
	return CONF_ERR_NONE;
}

size_t tlv_info(const void *tlv, enum ayla_tlv_type *type,
		const u8 **data, size_t *len)
{
	size_t _len;
	_len = ((u8 *)tlv)[1];
	if (((u8 *)tlv)[0] & ATLV_FILE) {
		_len |= (((u8 *)tlv)[0] & (~ATLV_FILE)) << 8;
	}

	if (type) {
		if (((u8 *)tlv)[0] & ATLV_FILE) {
			*type = ATLV_FILE;
		} else {
			*type = ((u8 *)tlv)[0];
		}
	}
	if (len) {
		*len = _len;
	}
	if (data) {
		*data = ((u8 *)tlv) + 2;
	}
	return _len + 2;
}

enum conf_error tlv_export(enum ayla_tlv_type type, void *data,
		size_t *data_len, const void *tlv, size_t len)
{
	enum ayla_tlv_type tlv_type;
	const u8 *tlv_data;
	size_t tlv_len;
	size_t bytes;
	u32 val;
	s32 sval;
#ifdef AYLA_TLV_CONF_EXPORT_SUPPORT
	long unsigned int utf8_val;
	enum conf_token *token;
	size_t ntoken;
#endif

	if (len < 2) {
		return CONF_ERR_LEN;
	}
	if (!tlv || !data) {
		return CONF_ERR_NONE;
	}
	bytes = tlv_info(tlv, &tlv_type, &tlv_data, &tlv_len);
	if (bytes > len) {
		return CONF_ERR_LEN;
	}

	switch (type) {
	case ATLV_BOOL:
	case ATLV_UINT:
		switch (tlv_len) {
		case sizeof(u8):
			val = *tlv_data;
			break;
		case sizeof(u16):
			val = get_ua_be16(tlv_data);
			break;
		case sizeof(u32):
			val = get_ua_be32(tlv_data);
			break;
		default:
			return CONF_ERR_LEN;
		}
		switch (*data_len) {
		case sizeof(u8):
			if (val > MAX_U8) {
				return CONF_ERR_RANGE;
			}
			*(u8 *)data = val;
			break;
		case sizeof(u16):
			if (val > MAX_U16) {
				return CONF_ERR_RANGE;
			}
			*(u16 *)data = val;
			break;
		case sizeof(u32):
			*(u32 *)data = val;
			break;
		default:
			return CONF_ERR_LEN;
		}
		break;
	case ATLV_INT:
		switch (tlv_len) {
		case sizeof(s8):
			sval = *(s8 *)tlv_data;
			break;
		case sizeof(s16):
			sval = (s16)get_ua_be16(tlv_data);;
			break;
		case sizeof(s32):
			sval = (s32)get_ua_be32(tlv_data);
			break;
		default:
			return CONF_ERR_LEN;
		}
		switch (*data_len) {
		case sizeof(s8):
			if (sval < MIN_S8 || sval > MAX_S8) {
				return CONF_ERR_RANGE;
			}
			*(s8 *)data = sval;
			break;
		case sizeof(s16):
			if (sval < MIN_S16 || sval > MAX_S16) {
				return CONF_ERR_RANGE;
			}
			*(s16 *)data = sval;
			break;
		case sizeof(s32):
			*(s32 *)data = sval;
			break;
		default:
			return CONF_ERR_LEN;
		}
		break;
	case ATLV_UTF8:
		if (tlv_len + 1 > *data_len) {
			return CONF_ERR_LEN;
		}
		memcpy(data, tlv_data, tlv_len);
		*((char *)data + tlv_len) = '\0';
		*data_len = tlv_len + 1;
		break;
#ifdef AYLA_TLV_CONF_EXPORT_SUPPORT
	case ATLV_CONF:
		token = (enum conf_token *)data;
		ntoken = 0;
		*data_len /= sizeof(enum conf_token);
		while (tlv_len) {
			bytes = tlv_utf8_get(&utf8_val, tlv_data, tlv_len);
			if (bytes == 0 || bytes > tlv_len) {
				return CONF_ERR_UTF8;
			}
			tlv_data += bytes;
			tlv_len -= bytes;
			if (ntoken > *data_len) {
				return CONF_ERR_LEN;
			}
			*(token++) = (enum conf_token)utf8_val;
			ntoken++;
		}
		*data_len = ntoken * sizeof(enum conf_token);
		break;
#endif
	default:
		if (tlv_len > *data_len) {
			return CONF_ERR_LEN;
		}
		memcpy(data, tlv_data, tlv_len);
		*data_len = tlv_len;
		break;
	}
	return CONF_ERR_NONE;
}
