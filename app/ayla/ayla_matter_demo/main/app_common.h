/*
 * Copyright 2023 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_APP_COMMON_H__
#define __AYLA_APP_COMMON_H__

#include <u-boot/linker-lists.h>
#include "wise_err.h"

/**
 * \brief CLI information.
 */
struct app_cli_info {
	const char *cmd;                /**< The command name. */
	const char *help;               /**< The help message. */
	const char *hit;                /**< The help hint. */
	int (*func)(int, char **);     /**< The command handler. */
};

#define ayla_cmd_def(cmd)   ll_entry_declare(struct app_cli_info, cmd, _ayla_)
#define ayla_cmd_start()    ll_entry_start(struct app_cli_info, _ayla_)
#define ayla_cmd_end()      ll_entry_end(struct app_cli_info, _ayla_)

int run_ayla_cmd(int argc, char *argv[]);

/**
 * \brief Execute a CLI command.
 *
 * \param command The command string.
 */
void app_cmd_exec(const char *command);

/**
 * \brief Command handler for setup_mode.
 *
 * \param argc The number of arguments.
 * \param argv The arguments.
 */
void app_setup_mode_cmd(int argc, char **argv);

/**
 * \brief Command handler for save.
 *
 * \param argc The number of arguments.
 * \param argv The arguments.
 */
void app_save_cmd(int argc, char **argv);

/**
 * \brief Command handler for diag.
 *
 * \param argc The number of arguments.
 * \param argv The arguments.
 */
void app_diag_cmd(int argc, char **argv);

#endif /* __AYLA_APP_COMMON_H__ */
