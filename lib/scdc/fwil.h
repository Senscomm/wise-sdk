/*
 * SPDX-License-Identifier: ISC
 *
 * Copyright (c) 2022 Senscomm Semiconductor Co., Ltd.
 *
 */

// SPDX-License-Identifier: ISC
/*
 * Copyright (c) 2012 Broadcom Corporation
 */

#ifndef _fwil_h_
#define _fwil_h_

#include <hal/types.h>

/*******************************************************************************
 * Dongle command codes that are interpreted by firmware
 ******************************************************************************/
#define SNCMF_C_GET_VERSION			1
#define SNCMF_C_UP					2
#define SNCMF_C_DOWN				3
#define SNCMF_C_SET_PROMISC			10
#define SNCMF_C_GET_RATE			12
#define SNCMF_C_GET_INFRA			19
#define SNCMF_C_SET_INFRA			20
#define SNCMF_C_GET_AUTH			21
#define SNCMF_C_SET_AUTH			22
#define SNCMF_C_GET_BSSID			23
#define SNCMF_C_GET_SSID			25
#define SNCMF_C_SET_SSID			26
#define SNCMF_C_TERMINATED			28
#define SNCMF_C_GET_CHANNEL			29
#define SNCMF_C_SET_CHANNEL			30
#define SNCMF_C_GET_SRL				31
#define SNCMF_C_SET_SRL				32
#define SNCMF_C_GET_LRL				33
#define SNCMF_C_SET_LRL				34
#define SNCMF_C_GET_RADIO			37
#define SNCMF_C_SET_RADIO			38
#define SNCMF_C_GET_PHYTYPE			39
#define SNCMF_C_SET_KEY				45
#define SNCMF_C_GET_REGULATORY			46
#define SNCMF_C_SET_REGULATORY			47
#define SNCMF_C_SET_PASSIVE_SCAN		49
#define SNCMF_C_SCAN				50
#define SNCMF_C_SCAN_RESULTS			51
#define SNCMF_C_DISASSOC			52
#define SNCMF_C_REASSOC				53
#define SNCMF_C_SET_ROAM_TRIGGER		55
#define SNCMF_C_SET_ROAM_DELTA			57
#define SNCMF_C_GET_BCNPRD			75
#define SNCMF_C_SET_BCNPRD			76
#define SNCMF_C_GET_DTIMPRD			77
#define SNCMF_C_SET_DTIMPRD			78
#define SNCMF_C_SET_COUNTRY			84
#define SNCMF_C_GET_PM				85
#define SNCMF_C_SET_PM				86
#define SNCMF_C_GET_REVINFO			98
#define SNCMF_C_GET_MONITOR			107
#define SNCMF_C_SET_MONITOR			108
#define SNCMF_C_GET_CURR_RATESET		114
#define SNCMF_C_GET_AP				117
#define SNCMF_C_SET_AP				118
#define SNCMF_C_SET_SCB_AUTHORIZE		121
#define SNCMF_C_SET_SCB_DEAUTHORIZE		122
#define SNCMF_C_GET_RSSI			127
#define SNCMF_C_GET_WSEC			133
#define SNCMF_C_SET_WSEC			134
#define SNCMF_C_GET_PHY_NOISE			135
#define SNCMF_C_GET_BSS_INFO			136
#define SNCMF_C_GET_GET_PKTCNTS			137
#define SNCMF_C_GET_BANDLIST			140
#define SNCMF_C_SET_SCB_TIMEOUT			158
#define SNCMF_C_GET_ASSOCLIST			159
#define SNCMF_C_GET_PHYLIST			180
#define SNCMF_C_SET_SCAN_CHANNEL_TIME		185
#define SNCMF_C_SET_SCAN_UNASSOC_TIME		187
#define SNCMF_C_SCB_DEAUTHENTICATE_FOR_REASON	201
#define SNCMF_C_SET_ASSOC_PREFER		205
#define SNCMF_C_GET_VALID_CHANNELS		217
#define SNCMF_C_SET_FAKEFRAG			219
#define SNCMF_C_GET_KEY_PRIMARY			235
#define SNCMF_C_SET_KEY_PRIMARY			236
#define SNCMF_C_SET_SCAN_PASSIVE_TIME		258
#define SNCMF_C_GET_VAR				262
#define SNCMF_C_SET_VAR				263
#define SNCMF_C_SET_WSEC_PMK			268
#define SNCMF_C_AT_CMD					269
#define SNCMF_C_CHANNEL_MSG				270

typedef struct _fwil_handler {
  uint32_t cmd;
  uint32_t len;
  uint8_t *buf;
  uint8_t  off;
  uint8_t  ifn;
  bool 	   set;
  int (*cb) (struct _fwil_handler *);
} fwil_handler_t;

#define FWIL_HANDLER(_name_, _cmd_, _cb_) \
  ll_entry_declare(fwil_handler_t, _name_, fwil) = { \
    .cmd = _cmd_, \
	.cb = _cb_, \
  }

#define _fwil_handler_start() ll_entry_start(fwil_handler_t, fwil)
#define _fwil_handler_end()   ll_entry_end(fwil_handler_t, fwil)

static inline fwil_handler_t *fwil_find_handler(uint32_t cmd)
{
  fwil_handler_t *start, *end, *fh;

  start = _fwil_handler_start();
  end = _fwil_handler_end();

  for (fh = start; fh < end; fh++)
  {
    if (fh->cmd == cmd)
	{
	  fh->off = 0;
	  return fh;
	}
  }

  return NULL;
}

typedef struct _fwil_var_handler {
  fwil_handler_t *fh;
  char *name;
  int (*cb) (struct _fwil_var_handler *fvh);
} fwil_var_handler_t;

#define FWIL_VAR_HANDLER(_name_, _cb_) \
  ll_entry_declare(fwil_var_handler_t, _name_, fwil_var) = { \
    .name = #_name_, \
	.cb = _cb_, \
  }

#define _fwil_var_handler_start() ll_entry_start(fwil_var_handler_t, fwil_var)
#define _fwil_var_handler_end()   ll_entry_end(fwil_var_handler_t, fwil_var)

static inline fwil_var_handler_t *fwil_find_var_handler(fwil_handler_t *fh,
		char *name)
{
  fwil_var_handler_t *start, *end, *var;

  start = _fwil_var_handler_start();
  end = _fwil_var_handler_end();

  for (var = start; var < end; var++)
  {
	/* compare up to the declared length */
  if (!strncmp(var->name, name, strlen(var->name)))
	{
	  fh->off = strlen(name) + 1;
	  var->fh = fh;
	  return var;
	}
  }

  return NULL;
}

typedef struct _fwil_bss_handler {
  fwil_var_handler_t *fvh;
  char *name; /* without prefix */
  int (*cb) (struct _fwil_bss_handler *fh);
  u32 bsscfgidx;
} fwil_bss_handler_t;

#define FWIL_BSS_HANDLER(_name_, _cb_) \
  ll_entry_declare(fwil_bss_handler_t, _name_, fwil_bss) = { \
    .name = #_name_, \
	.cb = _cb_, \
  }

#define _fwil_bss_handler_start() ll_entry_start(fwil_bss_handler_t, fwil_bss)
#define _fwil_bss_handler_end()   ll_entry_end(fwil_bss_handler_t, fwil_bss)

static inline fwil_bss_handler_t *fwil_find_bss_handler(fwil_var_handler_t *fvh,
		char *name)
{
  fwil_bss_handler_t *start, *end, *bss;
  fwil_handler_t *fh;

  start = _fwil_bss_handler_start();
  end = _fwil_bss_handler_end();

  for (bss = start; bss < end; bss++)
  {
	/* compare up to the declared length */
  if (!strncmp(name, bss->name, strlen(name)))
	{
	  bss->fvh = fvh;
	  fh = fvh->fh;
	  fh->off = 7 + strlen(name) + 1;
      memcpy(&bss->bsscfgidx, fh->buf + fh->off, sizeof(bss->bsscfgidx));
	  fh->off += sizeof(bss->bsscfgidx);
	  return bss;
	}
  }

  return NULL;
}

void fwil_init(void);

#endif /* _fwil_h_ */
