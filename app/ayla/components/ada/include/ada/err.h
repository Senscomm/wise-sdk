/*
 * Copyright 2015 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_ADA_ERR_H__
#define __AYLA_ADA_ERR_H__

#include <al/al_err.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ada_err {
	AE_OK = 0,
	AE_BUF = -AL_ERR_BUF,
	AE_ALLOC = -AL_ERR_ALLOC,
	AE_ERR = -AL_ERR_ERR,
	AE_NOT_FOUND = -AL_ERR_NOT_FOUND,
	AE_INVAL_VAL = -AL_ERR_INVAL_VAL,
	AE_INVAL_TYPE = -AL_ERR_INVAL_TYPE,
	AE_IN_PROGRESS = -AL_ERR_IN_PROGRESS,
	AE_BUSY = -AL_ERR_BUSY,
	AE_LEN = -AL_ERR_LEN,
	AE_INVAL_STATE = -AL_ERR_INVAL_STATE,
	AE_TIMEOUT = -AL_ERR_TIMEOUT,
	AE_ABRT = -AL_ERR_ABRT,
	AE_RST = -AL_ERR_RST,
	AE_CLSD = -AL_ERR_CLSD,
	AE_NOTCONN = -AL_ERR_NOTCONN,
	AE_INVAL_NAME = -AL_ERR_INVAL_NAME,
	AE_RDONLY = -AL_ERR_RDONLY,
	AE_CERT_EXP = -AL_ERR_CERT_EXP,
	AE_PARSE = -AL_ERR_PARSE,
	AE_UNEXP_END = -AL_ERR_UNEXP_END,
	AE_INVAL_OFF = -AL_ERR_INVAL_OFF,
	AE_FILE = -AL_ERR_FILE,
	AE_AUTH_FAIL = -AL_ERR_AUTH_FAIL,
	/* Note: add values in <al/err.h> */
};

#define ADA_ERR_STRINGS AL_ERR_STRINGS

static inline enum ada_err ada_err_from_al_err(enum al_err al_err)
{
	return (enum ada_err)(-(int)al_err);
}

static inline enum al_err ada_err_to_al_err(enum ada_err ada_err)
{
	return (enum al_err)(-(int)ada_err);
}

const char *ada_err_string(enum ada_err);

#ifdef __cplusplus
}
#endif

#endif /* __AYLA_ADA_ERR_H__ */
