/*
 * Copyright 2017 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_COMPILER_GCC_H__
#define __AYLA_COMPILER_GCC_H__

#ifndef PREPACKED
#define PREPACKED
#endif

#ifndef PACKED
#define PACKED __attribute__((__packed__))
#endif

#ifndef PREPACKED_ENUM
#define PREPACKED_ENUM
#endif

#ifndef PACKED_ENUM
#define PACKED_ENUM PACKED
#endif

#ifndef ADA_ATTRIB_FORMAT
#define ADA_ATTRIB_FORMAT(a, b) __attribute__ ((format (printf, a, b)))
#endif

#ifndef ADA_VA_LIST
#define ADA_VA_LIST __builtin_va_list
#endif

#ifndef ADA_VA_START
#define ADA_VA_START __builtin_va_start
#endif

#ifndef ADA_VA_END
#define ADA_VA_END __builtin_va_end
#endif

#ifndef FALL_THROUGH
#define FALL_THROUGH __attribute__((fallthrough))
#endif

#ifndef WEAK
#define WEAK __attribute__((weak))
#endif

#ifndef USED
#define USED __attribute__((used))
#endif

#endif /* __AYLA_COMPILER_GCC_H__ */
