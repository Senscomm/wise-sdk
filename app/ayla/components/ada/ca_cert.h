/*
 * Copyright 2012 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_CA_CERT_H__
#define __AYLA_CA_CERT_H__

#include <ada/linker_text.h>

/*
 * CA_CERT file is incorporated into the build by the linker.
 * objcopy provides these symbols.
 */
#if defined(AYLA_ESP32_SUPPORT)
#define CA_CERT_FILE	ca_certs_pem_txt
#else
#define CA_CERT_FILE	ca_certs_der_txt
#endif

LINKER_TEXT_ARRAY_DECLARE(CA_CERT_FILE);
LINKER_TEXT_SIZE_DECLARE(CA_CERT_FILE);

#define CA_CERT		LINKER_TEXT_START(CA_CERT_FILE)
#define CA_CERT_SIZE	((size_t)LINKER_TEXT_SIZE(CA_CERT_FILE))

#endif /* __AYLA_CA_CERT_H__ */
