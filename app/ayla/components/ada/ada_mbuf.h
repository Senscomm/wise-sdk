/*
 * Copyright 2023 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_ADA_MBUF_H__
#define __AYLA_ADA_MBUF_H__

struct ada_mbuf;

/*
 * allocate new empty mbuf with specified size, but no data.
 */
struct ada_mbuf *ada_mbuf_new(unsigned int size);

struct ada_mbuf *ada_mbuf_alloc(unsigned int size);

void ada_mbuf_free(struct ada_mbuf *am);

void *ada_mbuf_payload(struct ada_mbuf *am);

size_t ada_mbuf_tot_len(struct ada_mbuf *am);

size_t ada_mbuf_len(struct ada_mbuf *am);

void ada_mbuf_trim(struct ada_mbuf *am, int len);

/*
 * Add len to the front of the mbuf.
 * Currently len must be negative, which removes data from the front
 */
int ada_mbuf_header(struct ada_mbuf *am, int len);

/*
 * Add an mbuf to a chain of mbufs.
 */
void ada_mbuf_cat(struct ada_mbuf *head, struct ada_mbuf *tail);

/*
 * Combine a chain of mbufs into a single mbuf.
 */
struct ada_mbuf *ada_mbuf_coalesce(struct ada_mbuf *);

/*
 * Return the amount of data that can be appended to the chain.
 * If a NULL mbuf is passed, return 0.
 */
size_t ada_mbuf_chain_tail_room(struct ada_mbuf *am);

/*
 * Append data to the mbuf chain.
 * Returns 0 on success, -1 if there isn't enough room.
 */
int ada_mbuf_chain_append_data(struct ada_mbuf *am, void *data, size_t len);

#endif /* __AYLA_ADA_MBUF_H__ */
