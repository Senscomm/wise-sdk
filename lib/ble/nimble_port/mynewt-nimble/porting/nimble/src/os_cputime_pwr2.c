/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "os/os_cputime.h"

#include "hal/rom.h"

/**
 * This module implements cputime functionality for timers for which:
 *     a. freq is a power of 2 Hz, and
 *     b. 256 Hz <= freq < 1 MHz
 */

#if defined(OS_CPUTIME_FREQ_PWR2)

/**
 * @addtogroup OSKernel Operating System Kernel
 * @{
 *   @defgroup OSCPUTime High Resolution Timers
 *   @{
 */

/**
 * os cputime usecs to ticks
 *
 * Converts the given number of microseconds into cputime ticks.
 *
 * @param usecs The number of microseconds to convert to ticks
 *
 * @return uint32_t The number of ticks corresponding to 'usecs'
 */
uint32_t
_os_cputime_usecs_to_ticks(uint32_t usecs)
{
    return usecs * 20;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(os_cputime_usecs_to_ticks, &os_cputime_usecs_to_ticks, &_os_cputime_usecs_to_ticks);
#else
__func_tab__ uint32_t (*os_cputime_usecs_to_ticks)(uint32_t usecs)
= _os_cputime_usecs_to_ticks;
#endif

/**
 * cputime ticks to usecs
 *
 * Convert the given number of ticks into microseconds.
 *
 * @param ticks The number of ticks to convert to microseconds.
 *
 * @return uint32_t The number of microseconds corresponding to 'ticks'
 *
 * NOTE: This calculation will overflow if the value for ticks is greater
 * than 140737488. I am not going to check that here because that many ticks
 * is about 4222 seconds, way more than what this routine should be used for.
 */
uint32_t
_os_cputime_ticks_to_usecs(uint32_t ticks)
{
    return ticks / 20;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(os_cputime_ticks_to_usecs, &os_cputime_ticks_to_usecs, &_os_cputime_ticks_to_usecs);
#else
__func_tab__ uint32_t (*os_cputime_ticks_to_usecs)(uint32_t ticks)
= _os_cputime_ticks_to_usecs;
#endif

/**
 *   @} OSCPUTime
 * @} OSKernel
 */

#endif
