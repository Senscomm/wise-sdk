/*
 * Copyright 2014 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_PFM_LINKER_TEXT_H__
#define __AYLA_PFM_LINKER_TEXT_H__

#define LINKER_TEXT_ARRAY_DECLARE(file) \
	extern const char LINKER_TEXT_START(file)[]

#ifdef XXD_BIN_TO_C

#define	LINKER_TEXT_START(file) file
#define	LINKER_TEXT_SIZE(file) file ## _len

#define LINKER_TEXT_SIZE_DECLARE(file) \
	extern unsigned int LINKER_TEXT_SIZE(file)

#else

#define LINKER_TEXT_PATH(file, suffix) _binary_ ## file ## suffix

#define	LINKER_TEXT_START(file) LINKER_TEXT_PATH(file, _start)

#ifdef CONFIG_IDF_CMAKE
#define LINKER_TEXT_END(file) LINKER_TEXT_PATH(file, _end)
#define LINKER_TEXT_END_DECLARE(file) \
	extern const char LINKER_TEXT_END(file)[]
#define LINKER_TEXT_SIZE_DECLARE(file) LINKER_TEXT_END_DECLARE(file)

/*
 * This macro cannot be used for structure initialization, but can still be
 * handy in code.
 */
#define LINKER_TEXT_SIZE(file) ((size_t)LINKER_TEXT_END(file) - \
	(size_t)LINKER_TEXT_START(file))

#else

#define	LINKER_TEXT_SIZE(file) LINKER_TEXT_PATH(file, _size)
#define LINKER_TEXT_SIZE_DECLARE(file) \
	extern const char LINKER_TEXT_SIZE(file)[]
#endif
#endif /* XXD_BIN_TO_C */

#endif /* __AYLA_PFM_LINKER_TEXT_H__ */
