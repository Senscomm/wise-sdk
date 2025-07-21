/*
 * Copyright 2015 Ayla Networks, Inc.  All rights reserved.
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
#include <ayla/mod_log.h>
#include <ayla/parse.h>
#include <ayla/http.h>

/*
 * Structure used to hide sensitive data.
 */
struct log_bytes_prot {
	const char *start_key;
	const char *end_key;
	u16	match_len;
	u8	in_prot;
};

static void log_bytes_prot_init(struct log_bytes_prot *prot,
				const char *start_key, const char *end_key)
{
	memset(prot, 0, sizeof(*prot));
	prot->start_key = start_key;
	prot->end_key = end_key;
}

static void log_bytes_prot(struct log_bytes_prot *prot, char *str)
{
	char *cp;
	char in;
	const char *match;

	for (cp = str; *cp; cp++) {
		in = *cp;
		if (prot->in_prot) {
			match = prot->end_key;
			*cp = 'x';
		} else {
			match = prot->start_key;
		}
		if (in == match[prot->match_len]) {
			*cp = in;
			prot->match_len++;
			if (!match[prot->match_len]) {
				prot->in_prot = !prot->in_prot;
				prot->match_len = 0;
			}
		} else {
			prot->match_len = 0;
		}
	}
}

static void log_bytes_va(u8 mod, enum log_sev sev, const void *buf, size_t len,
			const char *fmt, va_list args)
{
	size_t rem = len;
	size_t chunk;
	const char *bp = buf;
	char msg[40];
	char tmpbuf[48 + 1];
	struct log_bytes_prot key1;
	struct log_bytes_prot key2;

	if (!log_mod_sev_is_enabled(mod, sev)) {
		return;
	}

	log_bytes_prot_init(&key1, HTTP_CLIENT_INIT_AUTH_HDR "\":\"", "\"");
	log_bytes_prot_init(&key2, HTTP_CLIENT_KEY_FIELD "\":\"", "\"");

	vsnprintf(msg, sizeof(msg), fmt, args);

	for (bp = buf, rem = len; rem; bp += chunk, rem -= chunk) {
		chunk = rem;
		if (chunk > sizeof(tmpbuf) - 1) {
			chunk = sizeof(tmpbuf) - 1;
		}

	memcpy(tmpbuf, bp, chunk);
	tmpbuf[chunk] = '\0';
	log_bytes_prot(&key1, tmpbuf);
	log_bytes_prot(&key2, tmpbuf);
	format_string(tmpbuf, sizeof(tmpbuf), tmpbuf, chunk);
	log_put_mod_sev(mod, sev, "%s %u \"%s\"",
	    msg, (unsigned int)len, tmpbuf);
	}
}

void log_bytes(u8 mod, enum log_sev sev,
		const void *buf, size_t len, const char *fmt, ...)
{
	ADA_VA_LIST args;

	ADA_VA_START(args, fmt);
	log_bytes_va(mod, sev, buf, len, fmt, args);
	ADA_VA_END(args);
}

void log_io(const void *buf, size_t len, const char *fmt, ...)
{
	ADA_VA_LIST args;

	ADA_VA_START(args, fmt);
	log_bytes_va(MOD_LOG_IO, LOG_SEV_DEBUG, buf, len, fmt, args);
	ADA_VA_END(args);
}
