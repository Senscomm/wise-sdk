/*
 * Copyright 2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_AL_COMMON_ASSERT_H__
#define __AYLA_AL_COMMON_ASSERT_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * Assert Interfaces.
 */

/**
 * Assert handle
 *
 * It's called when the assert occurs. Normally it should show an alert message
 * and then reboot after a short delay.
 *
 * \param file is the file name where the assert occurs.
 * \param line is the line number where the assert occurs.
 */
void al_assert_handle(const char *file, int line);

#ifdef __cplusplus
}
#endif

#endif /* __AYLA_AL_COMMON_ASSERT_H__ */
