/*
 * Copyright 2011 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */

#ifndef __AYLA_LOG_H__
#define __AYLA_LOG_H__

#include <stdarg.h>
#include <stddef.h>
#include <ayla/utypes.h>
#include <ayla/assert.h>
#include <ayla/snprintf.h>
#include <al/al_log.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LOG_SERIAL			/* define to do printfs to serial */

#define	LOG_SIZE	2048
#define LOG_LINE	200		/* size of log_line buf */
#define LOG_BUF_EVENT_MAX	20	/* modify based on LOG_SIZE */

/*
 * Log message severity prefix.
 */
#define LOG_INFO	"\x81"
#define LOG_WARN	"\x82"
#define LOG_ERR		"\x83"
#define LOG_DEBUG	"\x84"
#define LOG_FAIL	"\x85"
#define LOG_PASS	"\x86"
#define LOG_METRIC	"\x87"
#define LOG_DEBUG2	"\x89"
#define LOG_BASE	0x80	/* delta from log prefix to log_sev */

/*
 * Character that can appear after the severity to indicate mandatory logging
 * of messages that are expected by testing scripts.
 */
#define LOG_EXPECTED	"\xf0"	/* log this message regardless of masks */

extern u8 log_client_conf_enabled;

enum log_sev {
	LOG_SEV_NONE = 0,
	LOG_SEV_INFO = 1,
	LOG_SEV_WARN = 2,
	LOG_SEV_ERR = 3,
	LOG_SEV_DEBUG = 4,
	LOG_SEV_FAIL = 5,
	LOG_SEV_PASS = 6,
	LOG_SEV_METRIC = 7,
				/* 8 was nosend - reserved */
	LOG_SEV_DEBUG2 = 9,
	LOG_SEV_LIMIT		/* limit, must be last */
};

/*
 * Name table initializers for log severities.
 */
#define LOG_SEVS {				\
		[LOG_SEV_NONE] = "none",	\
		[LOG_SEV_FAIL] = "FAIL",	\
		[LOG_SEV_PASS] = "pass",	\
		[LOG_SEV_INFO] = "info",	\
		[LOG_SEV_DEBUG] = "debug",	\
		[LOG_SEV_DEBUG2] = "debug2",	\
		[LOG_SEV_WARN] = "warning",	\
		[LOG_SEV_ERR] = "error",	\
		[LOG_SEV_METRIC] = "metric",	\
	}

/*
 * Name table initializer for single-character version of log severity.
 */
#define LOG_SEV_CHARS {				\
		[LOG_SEV_NONE] = ' ',		\
		[LOG_SEV_FAIL] = 'F',		\
		[LOG_SEV_PASS] = 'P',		\
		[LOG_SEV_INFO] = 'i',		\
		[LOG_SEV_DEBUG] = 'd',		\
		[LOG_SEV_DEBUG2] = 'd',		\
		[LOG_SEV_WARN] = 'W',		\
		[LOG_SEV_ERR] = 'E',		\
		[LOG_SEV_METRIC] = 'm',		\
	}

PREPACKED_ENUM enum log_mask {
	LOG_DEFAULT = (BIT(LOG_SEV_NONE) |
		BIT(LOG_SEV_ERR) | BIT(LOG_SEV_WARN)),
	LOG_MAX = BIT(LOG_SEV_LIMIT) - 1
} PACKED_ENUM;
ASSERT_SIZE(enum, log_mask, 2);

PREPACKED_ENUM enum log_mod_id {
	LOG_MOD_NONE = 0,	/* log module name & time not desired */
	LOG_MOD_DEFAULT = 1,	/* default application subsystem */
	LOG_MOD_APP_BASE = 2,	/* first app-specific subsystem */
	LOG_MOD_APP_MAX = 20,	/* maximum app-specific subsystem */
	LOG_MOD_CT		/* must be last - number of subsystems */
} PACKED_ENUM;
ASSERT_SIZE(enum, log_mod_id, 1);

#define LOG_MOD_MASK	0x3f	/* mask for module number */
#define LOG_MOD_NOSEND	0x80	/* flag bit: do not send to logging service */

struct log_mod {
	enum log_mask mask;
};

enum log_event {
	LOG_EVENT_SEND = 0,		/* send logs to service */
	LOG_EVENT_NOSEND = 1,		/* do not send logs to service */
};

/*
 * Log buffer header before each message.
 * The maximum message is 255 bytes long.  Longer messages may be split or
 * truncated.
 */
struct log_msg_head {
	u8 magic;	/* magic number of v2 log buffer */
	u8 len;		/* length of the message body in bytes */
	u8 mod_nr;	/* unmasked enum mod_id of sub-system */
	u8 sev;		/* severity */
	u8 event;	/* internal event flags for controlling logging */
	u8 resvd[1];
	u16 msec;
	u32 time;	/* unix time of the log message (for the log service) */
	u32 mtime;	/* module monotonic time in milliseconds */
};

#define LOG_V2_MAGIC	0xf5	/* magic "length" to distinguish v2 from v1 */

struct log_msg_tail {
	u8 len;		/* length of the message body in bytes */
};

#define LOG_ENTRY_MAX_SZ \
    (sizeof(struct log_msg_head) + LOG_LINE + sizeof(struct log_msg_tail))

/*
 * Log interfaces.
 */
void log_init(void);		/* initialize logging - required with an OS */
void log_put(const char *fmt, ...) ADA_ATTRIB_FORMAT(1, 2);
void log_put_raw(const char *fmt, ...) ADA_ATTRIB_FORMAT(1, 2);
size_t log_put_va(u8 mod_nr, const char *fmt, va_list);
size_t log_put_va_sev(u8 mod_nr, enum log_sev sev,
				const char *fmt, ADA_VA_LIST args);
void log_put_mod_sev(u8 mod_nr, enum log_sev sev, const char *fmt, ...)
				ADA_ATTRIB_FORMAT(3, 4);
const char *log_sev_get(enum log_sev);
void log_setup_udp(void);

struct log_mod *log_mod_get(u8 mod_nr);		/* lookup module by mod_nr */
const char *log_mod_get_name(u8 mod_nr);	/* lookup name by mod_nr */

extern const char log_sev_chars[];

/*
 * Setup default log masks for all subsystems.
 */
void log_mask_init(enum log_mask mask);
void log_mask_init_min(enum log_mask mask, enum log_mask mask_min);
int log_mask_change(const char *mod, enum log_mask on_mask,
			enum log_mask off_mask);
int ada_log_mask_change(unsigned int mod_nr, enum log_mask on_mask,
			enum log_mask off_mask);

/*
 * Disable/Enable inserting log messages into buffer
 */
int log_enable(int);

/*
 * Add message to log buffer.
 */
void log_buf_put(u8 mod_nr, enum log_sev, u32 mtime,
		u32 time, u32 msec, const char *buf, size_t);

/*
 * Get context for pulling messages from a log buffer.
 *
 * snapshot is the snapshot buffer number, 0 is the live log buffer
 */
struct log_buf_ctxt *log_buf_open(unsigned int snapshot);

/*
 * Close log_buf_ctxt.
 * Frees resources.
 */
void log_buf_close(struct log_buf_ctxt *ctxt);

/*
 * Set sequence number for next log_buf_get().
 * Default is to use oldest sequence in the log.
 */
void log_buf_seq_set(struct log_buf_ctxt *ctxt, u32 seq);

/*
 * Set sequence number for log_buf_get such that it is a specified number
 * of lines from the tail.
 * If there are less lines in the buffer than requested the sequence number
 * will be set to start with the oldest message in the buffer.
 */
void log_buf_seq_set_tail(struct log_buf_ctxt *ctxt, u16 lines);

/*
 * Set the sequence number to that of the last message in the buffer.
 * Use this function to skip to the end.
 */
void log_buf_seq_set_end(struct log_buf_ctxt *ctxt);

/*
 * Determine if there are newer messages in the buffer that haven't
 * been gotten.
 *
 * Returns 1 if there are more, otherwise, 0.
 */
int log_buf_has_more(struct log_buf_ctxt *ctxt);

/*
 * Get next sequence number used in context.
 */
u32 log_buf_seq_get(struct log_buf_ctxt *ctxt);

/*
 * Indicate that the context will be used to send to the cloud.
 */
void log_buf_ctxt_will_send(struct log_buf_ctxt *ctxt);

/*
 * Get the next log entry from the log buffer.
 *
 * Use the sequence number in the context (typically the last message fetched
 * from the log buffer) to find the next available message with a subsequent
 * sequence number.
 *
 * Returns:
 *	length of buffer used including null terminator on success
 *	0 if no more entries available
 *	-1 if record wouldn't fit in buffer. The caller can retry with a
 *	larger buffer to try again for the same log entry.
 *	-2 if record was corrupted - This is a fatal error. The caller
 *	should stop iterating over the buffer.
 */
ssize_t log_buf_get_next(struct log_buf_ctxt *ctxt, void *buf, size_t len);

/*
 * Initialize log_buf
 */
void log_buf_init(void);

/*
 * Modify event mask of the corresponding event.
 */
void log_buf_send_event(u8 event_mask);

/*
 * Create a new event. Called in the client thread.
 */
void log_buf_new_event(void);

/*
 * Set a function to be called when the log buffer has messages for the cloud.
 */
void log_buf_notify_set(void (*notify)(int push));

/*
 * Save log to non-volatile storage if possible.
 */
void log_save(void);

/*
 * Check if a log mod & sev is enabled
 */
int log_mod_sev_is_enabled(u8 mod_nr, enum log_sev sev);

/*
 * Log info/debug messages.  These automatically append newlines.
 */
void log_put_mod(u8 mod, const char *fmt, ...)
		 ADA_ATTRIB_FORMAT(2, 3);

#define log_info(...)	log_put_mod(LOG_MOD_DEFAULT, LOG_INFO __VA_ARGS__)
#define log_err(mod_nr, ...)	log_put_mod(mod_nr, LOG_ERR __VA_ARGS__)
#define log_warn(mod_nr, ...)	log_put_mod(mod_nr, LOG_WARN __VA_ARGS__)
#define log_debug(mod_nr, ...)	log_put_mod(mod_nr, LOG_DEBUG __VA_ARGS__)

/*
 * Change global log settings from CLI.
 */
void ada_log_cli(int argc, char **argv);
extern const char ada_log_cli_help[];

extern int _write(int file, const char *ptr, int len);

/*
 * upper-layer-provided log_module settings and names.
 */
extern struct log_mod log_mods[LOG_MOD_CT];
extern const char *const log_mod_names[LOG_MOD_CT];
extern const char *log_prefix;
extern enum log_mask log_mask_minimum;

void log_client_init(void);
void ada_log_client_cli(int argc, char **argv);
int log_client_enable(int);
void log_client_reset(void);

int print(const char *);	/* low level print string for cli */
void log_print(const char *);	/* low level print string for logs */
#ifndef __cplusplus
/* app console redirect for logs */
extern void (*ada_cons_log_redir)(const char *str);
#endif

int printcli(const char *fmt, ...) ADA_ATTRIB_FORMAT(1, 2);
int printcli_s(const char *fmt, ...) ADA_ATTRIB_FORMAT(1, 2);
void print_remote_set(int (*print_func)(void *, const char *), void *arg);

/*
 * Dump a buffer to the CLI in conventional hex dump format.
 * Primarily useful for debugging.
 */
void dumpcli(const char *prefix, const void *buffer, size_t length);

/*
 * Log module I/O as printable ASCII.
 */
void log_io(const void *buf, size_t len, const char *fmt, ...)
	ADA_ATTRIB_FORMAT(3, 4);

/*
 * Print out bytes as hex
 */
void log_bytes_in_hex(u8 mod_nr, const void *buf, int len);
void log_bytes_in_hex_sev(u8 mod_nr, enum log_sev, const void *buf, int len);

/*
 * Dump a buffer in hex dump format with hex bytes and ASCII dump to the
 * right.
 */
void log_dump(u8 mod_nr, enum log_sev sev, const char *prefix,
    const void *buffer, u16 length, u8 (*redact_cb)(int offset, u8 c));

/*
 * Log bytes from buffer, with message.
 */
void log_bytes(u8 mod_nr, enum log_sev sev, const void *buf, size_t len,
		const char *fmt, ...) ADA_ATTRIB_FORMAT(5, 6);

/*
 * Set the log-line ID for the current thread.
 */
void log_thread_id_set(const char *id);

/*
 * Unset the log-line ID for the current thread.
 */
void log_thread_id_unset(void);

/*
 * Return the log-line ID for the current thread.
 * Returns NULL if none.
 */
const char *log_thread_id(void);

/*
 * Manage saved log snapshots.
 */
void ada_log_snap_cli(int argc, char **argv);
extern const char ada_log_snap_cli_help[];

/*
 * Get info on log snapshots.
 */
struct log_snap {
	u32 time;
	u32 size;
};
int log_snap_stat(struct log_snap *buf, unsigned int count);

int log_snap_erase(void);

/*
 * Return count of snapshots that have been saved, the maximum number of
 * snapshots that can be saved, whether old snapshots will automatically
 * be overwritten by newer snapshots.
 */
int log_snap_count(size_t *max, int *auto_overwrite);
void log_snap_status(void);
void log_snap_show(unsigned long snapshot, int time_only);

extern u8 log_snap_saved;	/* cached result from log_snap_count */

/*
 * "core" CLI, available only on some platforms.
 */
void ada_log_core_cli(int argc, char **argv);
extern const char ada_log_core_help[];

#ifdef AYLA_DISABLE_LOG_OUTPUT
void log_output_disable_set(u8 disable);
#endif

#ifdef __cplusplus
}
#endif

#endif /*  __AYLA_LOG_H__ */
