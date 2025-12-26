/*
 * Copyright 2011-2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_FTM_H__
#define __AYLA_FTM_H__

#include <stdbool.h>
#include "u-boot/list.h"

extern u8 ftm;
extern u8 ftm_wifi;
extern u8 ftm_bulb;
extern u8 ftm_bulb_type;

typedef void (*ftm_exec)(void);

void ftm_init(void);
void ftm_register(ftm_exec);
bool ftm_is_enabled(void);
void ftm_run(u32);
void ftm_error(int on, int off, int blinks, int period);
void ftm_success(void);

void ftm_cli(int argc, char **argv);

#endif /* __AYLA_FTM_H__ */
