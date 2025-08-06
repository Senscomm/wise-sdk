/*
 * Copyright 2011-2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ayla/utypes.h>
#include <ayla/clock.h>
#include <ayla/log.h>
#include <ayla/mod_log.h>
#include <ayla/tlv.h>
#include <ayla/conf.h>

#include <ada/libada.h>
#include <ada/client_ota.h>

#include "wise_err.h"
#include "wise_system.h"
#include "flash_map_backend/flash_map_backend.h"
#include "sysflash/sysflash.h"
#include "bootutil/bootutil_public.h"

#include <cmsis_os.h>

#define CRC32_INIT 0xffffffffUL

#define TICK_PERIOD_MS ((uint32_t) 1000 / osKernelGetTickFreq())


static const u32 crc32_table[16] = {
	0, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
	0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
	0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
	0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c,
};

static const struct flash_area *fap;

/*
 * Compute CRC-8 with IEEE polynomial
 * LSB-first.  Use 4-bit table.
 */
static u32 ota_crc32(const void *buf, size_t len, u32 crc)
{
	const u8 *bp = buf;

	while (len-- > 0) {
		crc ^= *bp++;
		crc = (crc >> 4) ^ crc32_table[crc & 0xf];
		crc = (crc >> 4) ^ crc32_table[crc & 0xf];
	}
	return crc;
}

struct demo_ota {
	u32 exp_len;
	u32 rx_len;
	u32 crc;
};
static struct demo_ota demo_ota;

static enum patch_state demo_ota_notify(const struct ada_ota_info *ota_info)
{
	int ret;

	log_put(LOG_DEBUG
	    "OTA notify: label=\"%s\" length=%lu version=\"%s\"",
	    ota_info->label ? ota_info->label : "", ota_info->length,
	    ota_info->version);
	demo_ota.exp_len = ota_info->length;
	demo_ota.rx_len = 0;
	demo_ota.crc = CRC32_INIT;

	ret = flash_area_open(FLASH_AREA_IMAGE_SECONDARY(0), &fap);
	if (ret < 0) {
		log_put(LOG_ERR "OTA flash open failed\n");
		return PB_ERR_OPEN;
	}
	log_put(LOG_INFO "OTA flash area %08lx, size %ld\n", fap->fa_off, fap->fa_size);
	log_put(LOG_INFO "OTA begin\n");

	ada_ota_start();
	return PB_DONE;
}

/*
 * Save the OTA image chunk by chunk as it is received.
 */
static enum patch_state demo_ota_save(unsigned int offset,
		const void *buf, size_t len)
{
	struct demo_ota *ota = &demo_ota;
	int ret;

	if (offset != ota->rx_len) {
		log_put(LOG_WARN "OTA save: offset skip at %u", offset);
		return PB_ERR_FATAL;
	}
	ota->rx_len += len;
	if (ota->rx_len > ota->exp_len) {
		log_put(LOG_WARN "OTA save: rx at %lu past len %lu",
				ota->rx_len, ota->exp_len);
		return PB_ERR_FATAL;
	}
	ota->crc = ota_crc32(buf, len, ota->crc);

	ret = flash_area_write(fap, offset, (const void *)buf, len);
	if (ret < 0) {
		log_put(LOG_ERR "OTA write failed %d\n", ret);
		return PB_ERR_WRITE;
	}

	log_put(LOG_INFO "OTA flash save %08x, len %d\n", offset, len);

	return PB_DONE;
}

static void ota_tmr_task(void *arg)
{
	wise_restart();
}

static void demo_ota_save_done(void)
{
	struct demo_ota *ota = &demo_ota;
	int tmr_id = 0;
	osTimerId_t tmr;
	osTimerAttr_t attr;

	if (ota->rx_len != ota->exp_len) {
		log_put(LOG_WARN "OTA save_done: rx len %lu not "
				"expected len %lu", ota->rx_len, ota->exp_len);
	}
	log_put(LOG_INFO "OTA save_done len %lu crc %lx\r\n",
			ota->rx_len, ota->crc);

	flash_area_close(fap);
	/* always mark the image as confirmed */
	boot_set_pending_multi(0, 1);

#ifndef ADA_BUILD_OTA_LEGACY
	ada_ota_report(PB_DONE);
#else
	ada_ota_report(OTA_HOST, PB_DONE);
#endif

	memset(&attr, 0, sizeof(attr));

	attr.name = "otaTmr";
	tmr = osTimerNew(ota_tmr_task, osTimerOnce, (void *)tmr_id, &attr);
	while (1) {
		if (osTimerStart(tmr, (20000 / TICK_PERIOD_MS)) != osErrorResource) {
			break;
		}
	}
}

static struct ada_ota_ops demo_ota_ops = {
	.notify = demo_ota_notify,
	.save = demo_ota_save,
	.save_done = demo_ota_save_done,
};

void demo_ota_init(void)
{
	ada_ota_register(OTA_HOST, &demo_ota_ops);
}
