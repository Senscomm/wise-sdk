/*
 * Copyright 2022 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_ADM_CLI_H__
#define __AYLA_ADM_CLI_H__

/**
 * This file provides CLI interfaces for ADM.
 */

#ifdef __cplusplus
extern "C" {
#endif

void adm_cli(int argc, char **argv);
extern const char adm_cli_help[];

#ifdef __cplusplus
}
#endif

#endif
