/*
 * Copyright (c) 2018-2024 Senscomm Semiconductor Co., Ltd. All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */
#ifndef _SCM2020_TRANSMITACTION_H
#define _SCM2020_TRANSMITACTION_H

#include <hal/types.h>
#include <u-boot/list.h>
#include <cmsis_os.h>
#include <transmitaction.h>

//#include "task.h"                       // ARM.FreeRTOS::RTOS:Core
#include "../include/FreeRTOS/event_groups.h"               // ARM.FreeRTOS::RTOS:Event Groups
//#include <include/FreeRTOS/FreeRTOS.h>                   // ARM.FreeRTOS::RTOS:Core

#define MACEXT_SIGNATURE	(0x07714197)
struct scm2020_test_payload {
	u32 signature;
	u32 chksum;
	u32 pkt_idx;
};

struct psdu_params {
	u32 leng;
	u32 mpdu;
	u32 prot;
	u32 nack;
	u32 txac;
	u32 numq;
};

struct scm2020_transmitaction {
	struct transmitaction base;
	void *sc;
	osEventFlagsId_t txdone;
	struct tx_vector tvec;
	struct psdu_params psdu;

	struct ieee80211_key key;
	int pkt_idx;
	ieee80211_seq seqno[SC_NR_TXQ];
};

struct scm2020_transmitaction *create_scm2020_transmitaction(void *sc);
void destroy_scm2020_transmitaction(struct scm2020_transmitaction *this);

#define m_set_repeat(i, r)	base.m_set_repeat((struct transmitaction *)i, r)

extern bool scm2020_validate_cs(struct changeset *cs);

#endif /* _SCM2020_TRANSMITACTION_H */
