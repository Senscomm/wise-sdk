/*
 * Copyright 2023 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <stdlib.h>
#include <string.h>
#include <ayla/utypes.h>
#include <ayla/assert.h>
#include <al/al_os_mem.h>
#include <ada/err.h>
#include "ada_mbuf.h"

struct ada_mbuf {		/* semi-opaque to users of this API */
	struct ada_mbuf *next;
	size_t tot_len;		/* total valid length of chain */
	size_t len;		/* valid data in buffer */
	size_t start;		/* starting offset in buffer */
	size_t alloc_len;	/* allocated size of buffer that follows */
};

void *ada_mbuf_payload(struct ada_mbuf *am)
{
	return (char *)(am + 1) + am->start;
}

size_t ada_mbuf_tot_len(struct ada_mbuf *am)
{
	return am->tot_len;
}

size_t ada_mbuf_len(struct ada_mbuf *am)
{
	return am->len;
}

struct ada_mbuf *ada_mbuf_alloc(unsigned int size)
{
	struct ada_mbuf *am;

	am = (struct ada_mbuf *)al_os_mem_calloc(sizeof(*am) + size);
	if (!am) {
		return am;
	}
	am->alloc_len = size;
	am->tot_len = size;
	am->len = size;
	return am;
}

struct ada_mbuf *ada_mbuf_new(unsigned int size)
{
	struct ada_mbuf *am;

	am = ada_mbuf_alloc(size);
	if (!am) {
		return am;
	}
	ada_mbuf_trim(am, 0);
	return am;
}

/*
 * Free entire chain of mbufs.
 * This is perfectly fine to call with a NULL pointer (saves code space).
 */
void ada_mbuf_free(struct ada_mbuf *am)
{
	struct ada_mbuf *next = am;

	while (next) {
		am = next;
		next = am->next;
		al_os_mem_free(am);
	}
}

void ada_mbuf_trim(struct ada_mbuf *am, int len)
{
	ASSERT(len <= am->tot_len);
	ASSERT(len <= am->len);
	am->tot_len = len;
	am->len = len;
}

/*
 * Add len to the front of the mbuf.
 * Currently len must be negative, which removes data from the front
 * Expects a contiguous buffer, not a chain.
 */
int ada_mbuf_header(struct ada_mbuf *am, int len)
{
	if (len > 0 || -len > am->len) {
		return -1;
	}
	am->tot_len += len;
	am->len += len;
	am->start -= len;
	return 0;
}

void ada_mbuf_cat(struct ada_mbuf *head, struct ada_mbuf *tail)
{
	struct ada_mbuf *prev;

	ASSERT(head);
	ASSERT(tail);

	prev = head;
	while (prev->next) {
		prev = prev->next;
	}
	prev->next = tail;
	head->tot_len += tail->tot_len;
}

struct ada_mbuf *ada_mbuf_coalesce(struct ada_mbuf *am)
{
	struct ada_mbuf *mb;
	struct ada_mbuf *new;
	size_t len;
	size_t tlen;
	char *out;

	len = am->tot_len;
	if (len == ada_mbuf_len(am)) {
		return am;
	}

	/*
	 * Note: the first mbuf (or others) may have enough room, but
	 * for simplicity, we just allocate a new one.  Improve later.
	 */
	new = ada_mbuf_alloc(len);
	if (!new) {
		return NULL;
	}
	out = ada_mbuf_payload(new);

	for (mb = am; mb && len > 0; mb = mb->next) {
		tlen = ada_mbuf_len(mb);	/* length to append to new */
		ASSERT(tlen <= len);

		memcpy(out, ada_mbuf_payload(mb), tlen);

		out += tlen;
		len -= tlen;
	}
	ada_mbuf_free(am);		/* free entire old chain */
	return new;
}

static struct ada_mbuf *ada_mbuf_tail(struct ada_mbuf *am)
{
	while (am->next) {
		am = am->next;
	}
	return am;
}

size_t ada_mbuf_chain_tail_room(struct ada_mbuf *am)
{
	if (!am) {
		return 0;
	}
	am = ada_mbuf_tail(am);
	return am->alloc_len - am->len - am->start;
}

int ada_mbuf_chain_append_data(struct ada_mbuf *head, void *data, size_t len)
{
	struct ada_mbuf *am;

	if (ada_mbuf_chain_tail_room(head) < len) {
		return -1;
	}
	am = ada_mbuf_tail(head);
	memcpy((char *)ada_mbuf_payload(am) + am->len, data, len);
	am->len += len;
	head->tot_len += len;
	return 0;
}
