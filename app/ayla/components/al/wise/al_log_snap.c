/*
 * Copyright 2021 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <ayla/log.h>
#include <al/al_log.h>
#include <al/al_err.h>

/*
 * Log snapshots are saved in a flash partition.
 * Each snapshot is a fixed size and aligned on an eraseable block boundary.
 */
#define PFM_LOG_SNAP_COUNT		16
#define PFM_LOG_SNAP_LEN 		4096
#define PFM_LOG_SNAP_ALIGN		16
#define PFM_LOG_SNAP_OFF(i) 	(((i) - 1) * PFM_LOG_SNAP_LEN)
/* TODO: what is going to be the snap partition address? */
#define PFM_LOG_SNAP_START_ADDR	0x803f0000
#define PFM_LOG_SNAP_END_ADDR	(PFM_LOG_SNAP_START_ADDR + (PFM_LOG_SNAP_LEN * PFM_LOG_SNAP_COUNT))


extern int flash_erase(off_t addr, size_t size, unsigned int how);
extern int flash_write(off_t addr, void *buf, size_t size);
extern int flash_read(off_t addr, void *buf, size_t size);

size_t al_log_snap_size_get(void)
{
	return PFM_LOG_SNAP_LEN;
}

/*
 * Read data from log snapshot.
 */
int al_log_snap_read(unsigned int snapshot, size_t offset, void *buf,
    size_t len)
{
	off_t addr;

	snapshot--;
	addr = PFM_LOG_SNAP_START_ADDR + (PFM_LOG_SNAP_LEN * snapshot) + offset;
	if (!(addr >= PFM_LOG_SNAP_START_ADDR && addr <= PFM_LOG_SNAP_END_ADDR)) {
		/* caller may use this function to check the validity of the snapshot */
		return -1;
	}

	flash_read(addr, buf, len);

	return len;
}

/*
 * Erase individual snapshot or all.
 *
 * The first snapshot is 1.
 * A snapshot index of 0 means to erase all.
 */
int al_log_snap_erase(unsigned int snapshot)
{
	off_t addr;

	if (snapshot == 0) {
		int i;
		for (i = 0; i < PFM_LOG_SNAP_COUNT; i++) {
			addr = PFM_LOG_SNAP_START_ADDR + (PFM_LOG_SNAP_LEN * i);
			if (addr > PFM_LOG_SNAP_END_ADDR) {
				return -1;
			}
			flash_erase(addr, PFM_LOG_SNAP_LEN, 0);
		}
	} else {
		snapshot--;
		addr = PFM_LOG_SNAP_START_ADDR + (PFM_LOG_SNAP_LEN * snapshot);
		flash_erase(addr, PFM_LOG_SNAP_LEN, 0);
		if (!(addr >= PFM_LOG_SNAP_START_ADDR && addr <= PFM_LOG_SNAP_END_ADDR)) {
			ASSERT(0);
			return -1;
		}
	}

	return AL_ERR_OK;
}

/*
 * Erase and write snapshot.
 * Do not log here except on error.
 * This is usually called with interrupts disabled and sometimes called when
 * crashing.
 */
int al_log_snap_save(unsigned int snapshot, void *hdr, size_t hdr_len,
    void *buf, size_t buf_len)
{
	off_t addr;
	void *fbuf;

	if (al_log_snap_erase(snapshot)) {
		return -1;
	}

	ASSERT(hdr_len + buf_len < PFM_LOG_SNAP_LEN);

	snapshot--;
	addr = PFM_LOG_SNAP_START_ADDR + (PFM_LOG_SNAP_LEN * snapshot);
	if (!(addr >= PFM_LOG_SNAP_START_ADDR && addr <= PFM_LOG_SNAP_END_ADDR)) {
		ASSERT(0);
		return -1;
	}

	/* TODO: any nicer way of doing this instead of malloc ? */
	fbuf = (void *)malloc(PFM_LOG_SNAP_LEN);
	if (!fbuf) {
		return -1;
	}

	memcpy(fbuf, hdr, hdr_len);
	memcpy(fbuf + hdr_len, buf, buf_len);
	flash_write(addr, fbuf, PFM_LOG_SNAP_LEN);

	free(fbuf);

	return AL_ERR_OK;
}

/*
 * Return count of snapshots that can be saved.
 * Since an explicit erase is not needed, this can be total space for the
 * snapshot partition divided by the size of the individual snapshots.
 *
 * auto_overwrite returns whether the implementation overwrites older snapshots
 * automatically (1) to make room for new ones or stops saving snapshots (0)
 * when there isn't sufficient space to record a new one without deleting an
 * old one.
 */
int al_log_snap_space(size_t single_snapshot_len, int *auto_overwrite)
{
	*auto_overwrite = 0;
	return PFM_LOG_SNAP_COUNT;
}
