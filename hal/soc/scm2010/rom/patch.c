/*
 * Copyright 2021-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include <stdlib.h>
#include <hal/rom.h>
#include <strings.h>
#include <FreeRTOS.h>
#ifdef CONFIG_FREEBSD
#include <freebsd/compat_param.h>
#include <freebsd/mbuf.h>
#endif

extern int g_freertos_use_timer_pool;
extern int g_freertos_use_queue_pool;
extern int g_serial_console_port;
extern int g_scn_active_dwell_time;
extern int g_scn_passive_dwell_time;
extern int MSIZE;
extern int MLEN;
extern int MHLEN;
extern int MINCLSIZE;
extern int MCLBYTES;
extern int MLEN2;
extern int MHLEN2;
extern int max_linkhdr;
extern int max_protohdr;
extern int max_hdr;
extern int max_datalen;

#ifdef CONFIG_SUPPORT_AMPDU_RX
#ifdef CONFIG_IEEE80211_AMPDU_AGE_LOOKUP
extern uint8_t g_ieee80211_max_reorder_mbuf_count;
#endif
#endif

extern uint8_t g_cfg_support_scan_entry_notify;
extern uint8_t g_cfg_eliminate_weak_rssi_entry;
extern uint8_t g_cfg_support_min_scan_table;

extern int time_hz;

void patch(void)
{
    const struct fcopy *start, *end, *fcp;

	g_freertos_use_timer_pool = configUSE_TIMER_POOL;
	g_freertos_use_queue_pool = configUSE_QUEUE_POOL;
	g_serial_console_port = CONFIG_SERIAL_CONSOLE_PORT;
#ifdef CONFIG_FREEBSD
	MSIZE 			= __MSIZE__;
	MLEN			= __MLEN__;
	MHLEN			= __MHLEN__;
	MINCLSIZE		= __MINCLSIZE__;
	MCLBYTES		= __MCLBYTES__;
	MLEN2			= __MLEN2__;
	MHLEN2			= __MHLEN2__;
	max_linkhdr		= 16;
	max_protohdr	= (40 + 20);
	max_hdr			= (16 + 40 + 20); /* max_linkhdr + max_protohdr */
	max_datalen		= (__MHLEN__ - (16 + 40 + 20));
#endif
#ifdef CONFIG_SUPPORT_AMPDU_RX
#ifdef CONFIG_IEEE80211_AMPDU_AGE_LOOKUP
#ifdef CONFIG_MEMP_NUM_MBUF_CACHE
    g_ieee80211_max_reorder_mbuf_count = CONFIG_MEMP_NUM_MBUF_CACHE - 1;
#endif
#endif
#endif
#ifdef CONFIG_ACTIVE_SCAN_DWELL_TIME
	g_scn_active_dwell_time = CONFIG_ACTIVE_SCAN_DWELL_TIME;
#endif
#ifdef CONFIG_PASSIVE_SCAN_DWELL_TIME
	g_scn_passive_dwell_time = CONFIG_PASSIVE_SCAN_DWELL_TIME;
#endif

	time_hz = CONFIG_TIMER_HZ;

#ifdef CONFIG_SUPPORT_SCAN_ENTRY_NOTIFY
	g_cfg_support_scan_entry_notify = CONFIG_SUPPORT_SCAN_ENTRY_NOTIFY;
#else
	g_cfg_support_scan_entry_notify = 0;
#endif

#ifdef CONFIG_ELIMINATE_WEAK_RSSI_SCAN_ENTRY
	g_cfg_eliminate_weak_rssi_entry = CONFIG_ELIMINATE_WEAK_RSSI_SCAN_ENTRY;
#else
	g_cfg_eliminate_weak_rssi_entry = 0;
#endif

#ifdef CONFIG_SUPPORT_MINIMUM_SCAN_TABLE
	g_cfg_support_min_scan_table = CONFIG_SUPPORT_MINIMUM_SCAN_TABLE;
#else
	g_cfg_support_min_scan_table = 0;
#endif

	start = patch_start();
	end = patch_end();

	for (fcp = start; fcp < end; fcp++) {
		*(unsigned long *)fcp->dst = fcp->src;
	}
}
