/*
 * Copyright 2017 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_PFM_CONST_H__
#define __AYLA_PFM_CONST_H__

/* Platform OS Stack Size definitions */
#define PFM_STACKSIZE_MIN	(2 * 1024)
#define PFM_STACKSIZE_MAX	(10 * 1024)
#define PFM_STACKSIZE_DFT	(4 * 1024)

/* APP layer stack size definitions */
#define PFM_STACKSIZE_TC610328	PFM_STACKSIZE_DFT
#define PFM_STACKSIZE_TESTFW	PFM_STACKSIZE_MAX

/* Examples' stack size definitions */
#define PFM_STACKSIZE_LEDEVB	PFM_STACKSIZE_DFT
#define PFM_STACKSIZE_WIFISETUP	PFM_STACKSIZE_DFT
#define PFM_STACKSIZE_APPTEST	PFM_STACKSIZE_DFT

/* ADA thread stack size */
/* TODO Linux doesn't have this. Add there or change API to eliminate this */
#define PFM_STACKSIZE_ADA		5200

/* Platform persist size definitions */
#define PFM_PERSIST_MAX_DATA_LEN	(1 * 1024)

#endif /* __AYLA_PFM_CONST_H__ */
