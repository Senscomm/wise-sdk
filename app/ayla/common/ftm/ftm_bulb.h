/*
 * Copyright 2011-2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_FTM_BULB_H__
#define __AYLA_FTM_BULB_H__

enum {
    FTM_BTYPE_COLOR         = 1,
    FTM_BTYPE_TUNABLE_WHITE = 2,
    FTM_BTYPE_FIXED_WHITE   = 3,
    FTM_BTYPE_GROW          = 4,
    FTM_BTYPE_NONE          = 0,
};

void ftm_bulb_init(void);

#endif /* __AYLA_FTM_BULB_H__ */
