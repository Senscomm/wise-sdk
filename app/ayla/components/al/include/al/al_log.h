/*
 * Copyright 2017 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_AL_COMMON_LOG_H__
#define __AYLA_AL_COMMON_LOG_H__

#include <al/al_utypes.h>

/**
 * @file
 * Platform Logging Interfaces
 */

/**
 * This prints a single line on the console.
 *
 * \param line string to print.
 *
 * The supplied single line will be NUL-terminated and will not have a
 * newline character (\n) at the end.
 *
 * The output will be terminated with a carriage return and line feed,
 * as appropriate.
 *
 * Where possible, the adaptation layer or SDK should buffer output but
 * if buffers are full, it should block the thread (and the entire system
 * if necessary) until the output can be added to the buffer.
 *
 * Discussion:
 * This would be called from the logging system in lib/ayla for log lines
 * or for printcli().
 * Ideally the port would never drop output lines.
 */
void al_log_print(const char *line);

/**
 * Get the log mod name.
 *
 * \param mod_id is log module ID.
 */
const char *al_log_get_mod_name(u8 mod_id);

/**
 * Get the log mod id given the name.
 *
 * \param name is the module name.
 *
 * \returns id on success or -1 if name does not exist.
 */
int al_log_get_mod_id(const char *name);

/**
 * Summarize the latest core dump info, if any, using log_put().
 */
int al_log_core_dump_info(void);

/**
 * Print the latest core dump, if any, using printcli().
 */
int al_log_core_dump(void);

/**
 * Erase the latest core dump, if any.
 */
void al_log_core_dump_erase(void);

/**
 * Return the size of a log snapshot.
 *
 * /return the maximum size of a single log snapshot, or 0 if snapshots not
 * provided.
 */
size_t al_log_snap_size_get(void);

/**
 * Read a log snapshot from flash.
 *
 * \param snapshot is the snapshot number (1-based).
 * \param offset is the byte offset into the snapshot from which to read.
 * \param buf is the destination buffer for the read.
 * \param len is the length of the buffer.
 * \return the length read or a negative error number.
 */
int al_log_snap_read(unsigned int snapshot, size_t offset, void *buf,
    size_t len);

/**
 * Save a log snapshot to flash.
 */
int al_log_snap_save(unsigned int snapshot, void *hdr, size_t hdr_len,
    void *buf, size_t buf_len);

/**
 * Erase a log snapshot.
 */
int al_log_snap_erase(unsigned int snapshot);

/**
 * Return count of snapshots that can be saved.
 *
 * If an explicit erase is not needed, this can be total space for the
 * snapshot partition divided by the size of the individual snapshots.
 *
 * auto_overwrite returns whether the implementation overwrites older snapshots
 * automatically (1) to make room for new ones or stops saving snapshots (0)
 * when there isn't sufficient space to record a new one without deleting an
 * old one.
 *
 */
int al_log_snap_space(size_t single_snapshot_len, int *auto_overwrite);

/**
 * Allocate a buffer for logging.
 *
 * This may use special memory requiring word aligned access if the symbol
 * AYLA_LOG_BUF_ALIGN4 is defined.
 *
 * /param size the size of the allocation in bytes.
 * /return pointer to buffer, or NULL on failure.
 */
void *al_log_buf_alloc(size_t size);

#endif /* __AYLA_AL_COMMON_LOG_H__ */
