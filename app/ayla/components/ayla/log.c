/*
 * Copyright 2011-2012 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <string.h>
#include <stdio.h>	/* for snprintf */

#include <ayla/utypes.h>
#include <ayla/assert.h>
#include <ayla/log.h>
#include <ayla/timer.h>
#include <ayla/clock.h>
#include <ayla/nameval.h>
#include <al/al_os_thread.h>
#include <al/al_os_lock.h>

/*
 * Optionally protect the log lines with locks if desired for debugging.
 * One thing to watch out for: fatal traps taken during logging could hang
 * trying to get this lock.  So, don't use for production yet.
 */
#ifdef LOG_LOCK
static struct al_lock *log_lock;
#endif /* LOG_LOCK */

enum log_mask log_mask_minimum;
static char log_line[LOG_LINE];
const char *log_prefix = "";
static u8 log_enabled = 1;
u8 log_client_conf_enabled = 1;

static int (*log_remote_print)(void *, const char *);
static void *log_remote_arg;

void (*ada_cons_log_redir)(const char *str);	/* console redirect for logs */

void print_remote_set(int (*print_func)(void *, const char *), void *arg)
{
	log_remote_print = print_func;
	log_remote_arg = arg;
}

const char log_sev_chars[] = LOG_SEV_CHARS;

static const char * const log_sevs[] = LOG_SEVS;

#define LOG_THREAD_CT	16	/* XXX move to abstraction layer */

static void *os_curthread(void)
{
	return al_os_thread_self();
}

#ifdef LOG_THREAD_CT
static struct name_val log_tasks[LOG_THREAD_CT + 1];	/* NULL-terminated */

/*
 * Set thread ID string which will be put in log lines for the current thread.
 * Keep it short.  Name string is not copied.
 */
void log_thread_id_set(const char *name)
{
	struct name_val *nv;
	int task_id;

	task_id = (int)os_curthread();

#ifdef LOG_LOCK
	al_os_lock_lock(log_lock);
#endif
	for (nv = log_tasks; nv < &log_tasks[ARRAY_LEN(log_tasks) - 1]; nv++) {
		if (!nv->name || nv->val == task_id) {
			nv->name = name;
			nv->val = task_id;
			break;
		}
	}
#ifdef LOG_LOCK
	al_os_lock_unlock(log_lock);
#endif
	if (nv >= &log_tasks[ARRAY_LEN(log_tasks) - 1]) {
		log_err(LOG_MOD_DEFAULT, "log_tasks table is full");
	}
}

/*
 * Unset thread ID string
 */
void log_thread_id_unset(void)
{
	struct name_val *nv;
	int task_id;

	task_id = (int)os_curthread();

#ifdef LOG_LOCK
	al_os_lock_lock(log_lock);
#endif
	for (nv = log_tasks; nv < &log_tasks[ARRAY_LEN(log_tasks) - 1]; nv++) {
		if (!nv->name || nv->val == task_id) {
			break;
		}
	}
	for (; nv < &log_tasks[ARRAY_LEN(log_tasks) - 1]; nv++) {
		*nv = *(nv + 1);
		if (!nv->name) {
			break;
		}
	}
#ifdef LOG_LOCK
	al_os_lock_unlock(log_lock);
#endif
}

const char *log_thread_id(void)
{
	return lookup_by_val(log_tasks, (int)os_curthread());
}
#endif /* LOG_THREAD_CT */

/*
 * Return the string for a log level.  Used by log_client.
 */
const char *log_sev_get(enum log_sev sev)
{
	if (sev >= LOG_SEV_LIMIT) {
		return ".";
	}
	return log_sevs[sev];
}

/*
 * Check if a log mod & sev is enabled
 */
int log_mod_sev_is_enabled(u8 mod_nr, enum log_sev sev)
{
	u32 mask;

	mod_nr &= LOG_MOD_MASK;
	mask = mod_nr < (u32)LOG_MOD_CT ? log_mods[mod_nr].mask : ~0;
	return mask & ((u32)1 << sev);
}

#ifdef AYLA_DISABLE_LOG_OUTPUT
static u8 log_output_disable;

void log_output_disable_set(u8 disable)
{
	log_output_disable = disable;
}
#endif

/*
 * Put log message into log_line buffer.
 */
size_t log_put_va_sev(u8 mod_nr, enum log_sev sev,
	const char *fmt, ADA_VA_LIST args)
{
	size_t rlen;
	size_t len;
	size_t rc;
	size_t body_len;
	struct clock_time ct;
	char time_stamp[CLOCK_FMT_LEN];
	u32 rough_time;
	static u32 last_rough_time;
	u32 mtime;
	u32 msec = 0;
	const char *mod_name;
	char *body;
	char *msg;
#ifdef LOG_THREAD_CT
	const char *thread_id;
#endif

#ifdef AYLA_DISABLE_LOG_OUTPUT
	if (log_output_disable) {
		return 0;
	}
#endif

	if (*fmt == LOG_EXPECTED[0]) {
		fmt++;
	} else if (!log_mod_sev_is_enabled(mod_nr, sev)) {
		return 0;
	}
	rlen = sizeof(log_line) - 1;
	len = 0;
	clock_get(&ct);
	msec = ct.ct_usec / 1000;
	mtime = clock_ms();

#ifdef LOG_LOCK
	al_os_lock_lock(log_lock);
#endif
	if (mod_nr != LOG_MOD_NONE) {
		len = snprintf(log_line, rlen, "%s", log_prefix);
	}
	if (clock_source() > CS_DEF) {
		clock_fmt(time_stamp, sizeof(time_stamp), ct.ct_sec);

		/*
		 * Show full date once an hour or if full date
		 * hasn't been shown, otherwise just show hh:mm:ss + ms.
		 */
		rough_time = ct.ct_sec / (60 * 60);
		len += snprintf(log_line + len, rlen - len, "%s.%3.3lu ",
		    &time_stamp[rough_time == last_rough_time ?
		    CLOCK_FMT_TIME : 0],
		    msec);
		last_rough_time = rough_time;
	} else {
		len += snprintf(log_line + len, rlen - len, "%lu ", mtime);
	}
	if (sev != LOG_SEV_NONE) {
		len += snprintf(log_line + len, rlen - len,
#ifdef LOG_SEV_SHORT
		    "%c ",
		    log_sev_chars[sev]);
#else
		    "%s:  ",
		    log_sev_get(sev));
#endif /* LOG_SEV_SHORT */
	}

#ifdef LOG_THREAD_CT
	thread_id = log_thread_id();
	if (thread_id) {
		len += snprintf(log_line + len, rlen - len, "%s ", thread_id);
	}
#endif /* LOG_THREAD_CT */

	mod_name = log_mod_get_name(mod_nr);
	if (mod_name) {
		len += snprintf(log_line + len, rlen - len, "%s: ", mod_name);
	}
	body = log_line + len;
	body_len = vsnprintf(body, rlen - len, fmt, args);
	if (!body_len || (body_len == 1 && *body == '\n')) {
		/* no body */
		rc = 0;
		goto unlock_exit;
	}
	len += body_len;
	if (len >= rlen) {
		len = rlen - 1;
	}

#ifndef NO_LOG_BUF
	log_buf_put(mod_nr, sev, mtime, ct.ct_sec, msec, body,
	    len - (body - log_line));
#endif /* NO_LOG_BUF */

	if (sev != LOG_SEV_NONE && log_line[len - 1] != '\n' && rlen > len) {
		log_line[len++] = '\n';
	}
	log_line[len] = '\0';
	msg = log_line;
	rc = len;
#ifdef LOG_SERIAL
	if (ada_cons_log_redir) {
		ada_cons_log_redir(msg);
	} else {
		al_log_print(msg);
	}
#endif
unlock_exit:
#ifdef LOG_LOCK
	al_os_lock_unlock(log_lock);
#endif
	return rc;
}

/*
 * Put log message into log_line buffer.
 */
size_t log_put_va(u8 mod_nr, const char *fmt, ADA_VA_LIST args)
{
	enum log_sev sev;
	u8 sev_byte;

	/* first char of fmt may be severity */
	sev_byte = *(u8 *)fmt;
	if (sev_byte >= LOG_BASE && sev_byte < LOG_BASE + (u8)LOG_SEV_LIMIT) {
		fmt++;
		sev = (enum log_sev)(sev_byte - LOG_BASE);
	} else {
		sev = LOG_SEV_NONE;
	}
	return log_put_va_sev(mod_nr, sev, fmt, args);
}

/*
 * Put into log from LwIP thread.
 */
void log_put_raw(const char *fmt, ...)
{
	ADA_VA_LIST args;
	ADA_VA_START(args, fmt);
	log_put_va(LOG_MOD_DEFAULT, fmt, args);
	ADA_VA_END(args);
}

void log_put(const char *fmt, ...)
{
	ADA_VA_LIST args;
	ADA_VA_START(args, fmt);
	log_put_va(LOG_MOD_DEFAULT, fmt, args);
	ADA_VA_END(args);
}

void log_put_mod(u8 mod_nr, const char *fmt, ...)
{
	ADA_VA_LIST args;
	ADA_VA_START(args, fmt);
	log_put_va(mod_nr, fmt, args);
	ADA_VA_END(args);
}

void log_put_mod_sev(u8 mod_nr, enum log_sev sev, const char *fmt, ...)
{
	ADA_VA_LIST args;
	ADA_VA_START(args, fmt);
	log_put_va_sev(mod_nr, sev, fmt, args);
	ADA_VA_END(args);
}

void log_mask_init(enum log_mask mask)
{
	log_mask_change(NULL, mask, (enum log_mask)0);
}

void log_mask_init_min(enum log_mask mask, enum log_mask min_mask)
{
	log_mask_minimum = min_mask;
	log_mask_init(mask);
}

void log_init(void)
{
#ifdef LOG_LOCK
	log_lock = al_os_lock_create();
#endif
	al_clock_init();
}

/*
 * Enabling/disabling inserting log messages into buffer
 */
int log_enable(int enable)
{
	int old_val = log_enabled;

	log_enabled = enable;
	return old_val;
}

/*
 * Print out bytes as hex
 */
void log_bytes_in_hex_sev(u8 mod_nr, enum log_sev sev, const void *buf, int len)
{
	char tmpbuf[48 + 12 + 1]; /* 16 * 3 + 12 */
	int i, j, off;

	for (i = 0; i < len; ) {
		off = 0;
		for (j = 0; j < 16 && i < len; j++) {
			off += snprintf(tmpbuf + off,
			    sizeof(tmpbuf) - off, "%2.2x ",
			    ((u8 *)buf)[i]);
			if ((j + 1) % 4 == 0) {
				off += snprintf(tmpbuf + off,
				    sizeof(tmpbuf) - off, " ");
			}
			i++;
		}
		log_put_mod_sev(mod_nr, sev, "%s", tmpbuf);
	}
}

void log_bytes_in_hex(u8 mod_nr, const void *buf, int len)
{
	log_bytes_in_hex_sev(mod_nr, LOG_SEV_DEBUG, buf, len);
}

static int dump_hex_ascii(const u8 *buf, size_t length, int offset, char *hex,
    size_t hlen, char *ascii, size_t alen, u8 (*redact_cb)(int offset, u8 c))
{
	int i = 0;
	int j;
	int k;
	u8 c;

	ASSERT(hlen >= 50);
	ASSERT(alen >= 18);
	for (j = 0, k = 0; k < 16 && offset < length; i++, k++, offset++) {
		c = buf[offset];
		if (redact_cb) {
			c = redact_cb(offset, c);
		}
		if (c >= ' ' && c <= '~') {
			ascii[k] = c;
		} else {
			ascii[k] = '.';
		}
		j += snprintf(&hex[j], hlen - j, "%02x ", c);
	}
	ascii[k] = '\0';
	while (k++ < 16) {
		j += snprintf(&hex[j], hlen - j, "   ");
	}
	return i;
}

void log_dump(u8 mod_nr, enum log_sev sev, const char *prefix,
    const void *buffer, u16 length, u8 (*redact_cb)(int offset, u8 c))
{
	int offset = 0;
	char hex[50];
	char ascii[18];
	const u8 *buf = buffer;

	while (offset < length) {
		offset += dump_hex_ascii(buf, length, offset, hex, sizeof(hex),
		    ascii, sizeof(ascii), redact_cb);
		log_put_mod_sev(mod_nr, sev, "%s%s %s", prefix, hex, ascii);
	}
}

static int printcli_va(int add_nl, const char *fmt, ADA_VA_LIST args)
{
	char buf[512];
	size_t len;
	char *line;
	char *next;
	char saved = 0;

	len = vsnprintf(buf, sizeof(buf), fmt, args);

	if (len > sizeof(buf) - 2) {
		len = sizeof(buf) - 2;
	}
	if (add_nl && (len == 0 || buf[len - 1] != '\n')) {
		buf[len++] = '\n';
	}
	buf[len] = '\0';

	/*
	 * Split output at each newline.
	 */
	for (line = buf; line < &buf[len]; line = next) {
		next = strchr(line, '\n');
		if (next) {
			next++;
			saved = *next;
			*next = '\0';
		} else {
			next = NULL;
		}
		if (log_remote_print) {
			log_remote_print(log_remote_arg, line);
		} else {
#ifdef LOG_LOCK
			al_os_lock_lock(log_lock);
			al_log_print(line);
			al_os_lock_unlock(log_lock);
#else
			al_log_print(line);
#endif
		}
		if (!next) {
			break;
		}
		*next = saved;
	}
	return len;
}

int printcli(const char *fmt, ...)
{
	ADA_VA_LIST  args;
	int len;

	ADA_VA_START(args, fmt);
	len = printcli_va(1, fmt, args);
	ADA_VA_END(args);
	return len;
}

/*
 * Note: printcli_s should be avoided.
 * It won't work on SDKs that don't provide for printing
 * less than one line at a time.
 */
int printcli_s(const char *fmt, ...)
{
	ADA_VA_LIST args;
	size_t len;

	ADA_VA_START(args, fmt);
	len = printcli_va(0, fmt, args);
	ADA_VA_END(args);
	return len;
}

void dumpcli(const char *prefix, const void *buffer, size_t length)
{
	int offset = 0;
	char hex[50];
	char ascii[18];
	const u8 *buf = buffer;
	const char *pfp = prefix;	/* prefix pointer */
	char ps[32];			/* prefix for subsequent lines */
	char *p;
	size_t prefix_len;

	while (1) {
		offset += dump_hex_ascii(buf, length, offset, hex, sizeof(hex),
		    ascii, sizeof(ascii), NULL);
		printcli("%s%s %s", pfp, hex, ascii);
		if (offset >= length) {
			break;
		}

		/*
		 * For subsequent lines, change the prefix to all whitespace.
		 */
		if (pfp != ps) {
			/*
			 * Copy the prefix, then replace all characters except
			 * tab with a blank character.
			 */
			prefix_len = strlen(prefix) + 1;
			if (prefix_len <= sizeof(ps)) {
				memcpy(ps, prefix, prefix_len);
				pfp = ps;
				p = ps;
				while (*p) {
					if (*p != '\t') {
						*p = ' ';
					}
					p++;
				}
			}
		}
	}
}

