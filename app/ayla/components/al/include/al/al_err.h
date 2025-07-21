/*
 * Copyright 2015-2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_AL_ERR_H__
#define __AYLA_AL_ERR_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * Error numbers.
 */

/**
 * Error numbers.
 *
 * These numbers are used by the platform adaptation layer and ADA.
 *
 * The numbers must not be changed because they correspond to the enum
 * ada_err values, which are used by the application layer.
 */
enum al_err {
	AL_ERR_OK = 0,		/**< no error */
	AL_ERR_BUF = 1,		/**< network buf shortage - retry later */
	AL_ERR_ALLOC = 2,	/**< resource shortage */
	AL_ERR_ERR = 3,		/**< non-specific error */
	AL_ERR_NOT_FOUND = 4,	/**< object (e.g., property) not found */
	AL_ERR_INVAL_VAL = 5,	/**< invalid value */
	AL_ERR_INVAL_TYPE = 6,	/**< invalid type */
	AL_ERR_IN_PROGRESS = 7,	/**< successfully started, but not finished */
	AL_ERR_BUSY = 8,	/**< another operation is in progress */
	AL_ERR_LEN = 9,		/**< invalid length */
	AL_ERR_INVAL_STATE = 10, /**< called without correct prerequisites */
	AL_ERR_TIMEOUT = 11,	/**< operation timed out */
	AL_ERR_ABRT = 12,	/**< connection aborted */
	AL_ERR_RST = 13,	/**< connection reset */
	AL_ERR_CLSD = 14,	/**< connection closed */
	AL_ERR_NOTCONN = 15,	/**< not connected */
	AL_ERR_INVAL_NAME = 16,	/**< invalid property name */
	AL_ERR_RDONLY = 17,	/**< tried to set a read-only value */
	AL_ERR_CERT_EXP = 18,	/**< SSL certificate not valid due to time */
	AL_ERR_INVAL_OFF = 19,	/**< invalid file offset */
	AL_ERR_FILE = 20,	/**< a file error while processing */
	AL_ERR_PARSE = 21,	/**< parsing failure */
	AL_ERR_UNEXP_END = 22,	/**< unexpectedly ran out of data */
	AL_ERR_AUTH_FAIL = 23,	/**< device authentication failure */

	/*
	 * Note: when adding new values, insert them before this comment and
	 * update the AL_ERR_STRINGS define below, as well as <ayla/err.h>.
	 */
	AL_ERR_COUNT		/**< count of enums, unused as an error code */
};

/**
 * Initializer for error strings array.
 * Keep this in sync with the enum al_err definition.
 */
#define AL_ERR_STRINGS {			\
	[AL_ERR_OK] = "none",			\
	[AL_ERR_BUF] = "buf",			\
	[AL_ERR_ALLOC] = "alloc failed",	\
	[AL_ERR_ERR] = "error",			\
	[AL_ERR_NOT_FOUND] = "not found",	\
	[AL_ERR_INVAL_VAL] = "inv val",		\
	[AL_ERR_INVAL_TYPE] = "inv type",	\
	[AL_ERR_IN_PROGRESS] = "in progress",	\
	[AL_ERR_BUSY] = "busy",			\
	[AL_ERR_LEN] = "len",			\
	[AL_ERR_INVAL_STATE] = "inv state",	\
	[AL_ERR_TIMEOUT] = "timeout",		\
	[AL_ERR_ABRT] = "conn abrt",		\
	[AL_ERR_RST] = "conn reset",		\
	[AL_ERR_CLSD] = "conn closed",		\
	[AL_ERR_NOTCONN] = "not conn",		\
	[AL_ERR_INVAL_NAME] = "inv name",	\
	[AL_ERR_RDONLY] = "read-only property",	\
	[AL_ERR_CERT_EXP] = "cert time",	\
	[AL_ERR_PARSE] = "parsing failure",	\
	[AL_ERR_UNEXP_END] = "unexp end",	\
	[AL_ERR_INVAL_OFF] = "inv offset",	\
	[AL_ERR_FILE] = "file",			\
	[AL_ERR_AUTH_FAIL] = "auth failed",	\
}

/**
 * Lookup a human-readable definition of an error number.
 *
 * \param err the error number or the negative of the error number.
 * \returns a pointer to a string describing the error.
 */
const char *al_err_string(enum al_err err);

#ifdef __cplusplus
}
#endif

#endif /* __AYLA_AL_ERR_H__ */
