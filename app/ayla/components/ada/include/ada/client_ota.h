/*
 * Copyright 2015 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_CLIENT_OTA_H__
#define __AYLA_CLIENT_OTA_H__

#include <ayla/patch.h>

/*
 * Over-the-air (OTA) Firmware update interfaces.
 */

/*
 * Size of each request to fetch part of the firmware image.
 *
 * The host app can request another size, within reason.
 * Too small may cause the OTA to take a long time.
 * Too large will mean the device is unresponsive to properties for too long.
 */
#define CLIENT_OTA_FETCH_LEN_DEF	4096		/* default size */

#define CLIENT_OTA_FETCH_LEN_MIN	(255 * 8)	/* small for MCU */
#define CLIENT_OTA_FETCH_LEN_MED	(64 * 1024)	/* reasonable length */
#define	CLIENT_OTA_FETCH_LEN_MAX	(256 * 1024)	/* very large */

/*
 * Number of times to retry a fetch.
 */
#define OTA_CHUNK_RETRIES	5

/*
 * Types of OTA.
 */
PREPACKED_ENUM enum ada_ota_type {
	OTA_MODULE = 0,		/* Wi-Fi module (may include application) */
	OTA_HOST = 1,		/* host MCU/application */
	OTA_TYPE_CT		/* number of OTA types */
} PACKED_ENUM;

/*
 * Information about an OTA.
 */
struct ada_ota_info {
	enum ada_ota_type type; /* type of OTA */
	u32 length;             /* length of OTA image */
#ifndef ADA_BUILD_OTA_LEGACY
	const char *label;      /* optional label associated with the OTA,
				   NULL if no label provided */
#endif
	const char *version;    /* image version string */
};

/*
 * OTA operations.
 */
struct ada_ota_ops {
#ifndef ADA_BUILD_OTA_LEGACY
	/*
	 * The notify() function indicates to the handler that an OTA is
	 * available. ota_info provides information about the OTA. notify()
	 * may return an error. If there is no error, the handler must start
	 * the download at its earliest convenience by calling ada_ota_start().
	 *
	 * Note: The application must start the OTA as soon as possible
	 * rather than deferring it.
	 */
	enum patch_state (*notify)(const struct ada_ota_info *ota_info);
#else
	/*
	 * Deprecated legacy notify interface.
	 *
	 * The notify() function indicates to the handler that an OTA is
	 * available. The length and version string are given. If the length
	 * or version are unacceptable, notify() may return an error. If there
	 * is no error, the handler must start the download at its earliest
	 * convenience by calling ada_ota_start().
	 */
	enum patch_state (*notify)(unsigned int len, const char *version);
#endif

	/*
	 * Receive a portion of the OTA update.
	 */
	enum patch_state (*save)(unsigned int offset, const void *, size_t);

	/*
	 * Handle the completion of the OTA update.
	 * Will report status via ada_ota_report() immediately if image was bad.
	 */
	void (*save_done)(void);

	/*
	 * Clear status of the OTA - it has been reported to the service.
	 * Optional.
	 */
	void (*status_clear)(void);
};

/*
 * Register handler for OTA
 */
void ada_ota_register(enum ada_ota_type, const struct ada_ota_ops *);

#ifndef ADA_BUILD_OTA_LEGACY
/*
 * Start the pending OTA download.
 */
void ada_ota_start(void);
#else
/*
 * Give permission for OTA to start.
 */
void ada_ota_start(enum ada_ota_type);
#endif

#ifndef ADA_BUILD_OTA_LEGACY
/*
 * Report status of OTA download. Call this in the handler to report the status
 * of an OTA download after it has been completely downloaded. It may be called
 * during in done() handler or at a time later, even when the new image has
 * booted.
 *
 * \param status is the status of OTA download.
 */
void ada_ota_report(enum patch_state status);
#else
/*
 * Report status of OTA update.
 */
void ada_ota_report(enum ada_ota_type type, enum patch_state status);
#endif

/*
 * Continue OTA saves after reporting a stall.
 */
void ada_ota_continue(void);

/*
 * Set the size for each piece of the firmware image download.
 */
void ada_ota_fetch_len_set(size_t len);

#endif /* __AYLA_CLIENT_OTA_H__ */
