#include <stdint.h>
#include <string.h>

#include "controller/ble_phy.h"
#include "controller/ble_phy_trace.h"
#include "controller/ble_ll.h"
#include "controller/ble_ll_conn.h"
#include "controller/ble_ll_adv.h"
#include "controller/ble_ll_sched.h"
#include "controller/ble_ll_scan.h"
#include "controller/ble_ll_scan_aux.h"
#include "controller/ble_ll_hci.h"
#include "controller/ble_ll_whitelist.h"
#include "controller/ble_ll_resolv.h"
#include "controller/ble_ll_rfmgmt.h"
#include "controller/ble_ll_trace.h"
#include "controller/ble_ll_sync.h"
#include "controller/ble_ll_plna.h"

#include "hal/compiler.h"
#include "hal/rom.h"


extern struct ble_ll_obj g_ble_ll_data;
extern void ble_ll_dtm_reset(void);
extern void (*ble_ll_flush_pkt_queue)(struct ble_ll_pkt_q *pktq);
extern void (*ble_ll_conn_module_reset)(void);

uint8_t rxpdu_fake[256] __attribute__((aligned(4)));
struct os_mbuf *rxpdu_tmp = (struct os_mbuf *)rxpdu_fake;

__ilm_ble__ int
_ble_ll_rx_end_preprocess(uint8_t *rxbuf, struct ble_mbuf_hdr *rxhdr,
        uint8_t crcok, uint8_t *processed)
{
#if 0
    *processed = 0;

    return 0;
#else
    int rc;

    rxpdu_tmp->om_data = rxbuf;

    memcpy(BLE_MBUF_HDR_PTR(rxpdu_tmp), rxhdr,
            sizeof(struct ble_mbuf_hdr));

    if (BLE_MBUF_HDR_RX_STATE(rxhdr) == BLE_LL_STATE_SCANNING) {
        rc = ble_ll_scan_rx_isr_end(rxpdu_tmp, crcok);
    } else {
        rc = ble_ll_scan_aux_rx_isr_end(rxpdu_tmp, crcok);
    }

    memcpy(rxhdr, BLE_MBUF_HDR_PTR(rxpdu_tmp),
            sizeof(struct ble_mbuf_hdr));

    *processed = 1;

    return rc;
#endif
}
#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_ll_rx_end_preprocess, &ble_ll_rx_end_preprocess, &_ble_ll_rx_end_preprocess);
#else
__func_tab__ int (*ble_ll_rx_end_preprocess)(uint8_t *rxbuf, struct ble_mbuf_hdr *rxhdr,
        uint8_t crcok, uint8_t *processed) = _ble_ll_rx_end_preprocess;
#endif

void patch_ble_ll_supp_feature(void)
{
	/* BUG FIX: name used in the conditional macro has been changed, but missed.
		BLE_LL_CFG_FEAT_PERIPH_INIT_FEAT_XCHG
		BLE_LL_CFG_FEAT_SLAVE_INIT_FEAT_XCHG
	*/
	g_ble_ll_data.ll_supp_features |= BLE_LL_FEAT_PERIPH_INIT;
}

int
_ble_ll_deinit(void)
{
    uint8_t phy_mask;
    os_sr_t sr;

    if (g_ble_ll_data.ll_state != BLE_LL_STATE_STANDBY) {
        return -1;
    }

    while (1) {
        if (!ble_npl_eventq_is_empty(&g_ble_ll_data.ll_evq)) {
            struct ble_npl_event *ev;

            ev = ble_npl_eventq_get(&g_ble_ll_data.ll_evq, 0);
            if (!ev) {
                break;
            } else {
                ble_npl_eventq_remove(&g_ble_ll_data.ll_evq, ev);
            }
        } else {
            break;
        }
    }

    ble_npl_eventq_deinit(&g_ble_ll_data.ll_evq);

    ble_phy_disable();
    ble_ll_sched_stop();

    OS_ENTER_CRITICAL(sr);
    ble_phy_disable();
    ble_ll_sched_stop();
#if MYNEWT_VAL(BLE_LL_ROLE_OBSERVER)
    ble_ll_scan_reset();
    ble_ll_scan_deinit();
#endif
    ble_ll_rfmgmt_reset();
    OS_EXIT_CRITICAL(sr);

#if MYNEWT_VAL(BLE_LL_ROLE_BROADCASTER)
    /* Stop any advertising */
    ble_ll_adv_reset();
#endif

#if MYNEWT_VAL(BLE_LL_DTM)
    ble_ll_dtm_reset();
#endif

    /* Stop sync */
#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LL_PERIODIC_ADV) && MYNEWT_VAL(BLE_LL_ROLE_OBSERVER)
    ble_ll_sync_reset();
#endif

    /* FLush all packets from Link layer queues */
    ble_ll_flush_pkt_queue(&g_ble_ll_data.ll_tx_pkt_q);
    ble_ll_flush_pkt_queue(&g_ble_ll_data.ll_rx_pkt_q);

    /* Reset LL stats */
    STATS_RESET(ble_ll_stats);

    /* Reset any preferred PHYs */
    phy_mask = BLE_PHY_MASK_1M;
#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LE_2M_PHY)
    phy_mask |= BLE_PHY_MASK_2M;
#endif
#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LE_CODED_PHY)
    phy_mask |= BLE_PHY_MASK_CODED;
#endif
    phy_mask &= MYNEWT_VAL(BLE_LL_CONN_PHY_DEFAULT_PREF_MASK);
    BLE_LL_ASSERT(phy_mask);
    g_ble_ll_data.ll_pref_tx_phys = phy_mask;
    g_ble_ll_data.ll_pref_rx_phys = phy_mask;

    /* Enable all channels in channel map */
    g_ble_ll_data.chan_map_num_used = BLE_PHY_NUM_DATA_CHANS;
    memset(g_ble_ll_data.chan_map, 0xff, BLE_LL_CHAN_MAP_LEN - 1);
    g_ble_ll_data.chan_map[4] = 0x1f;

#if MYNEWT_VAL(BLE_LL_ROLE_PERIPHERAL) || MYNEWT_VAL(BLE_LL_ROLE_CENTRAL)
    /* Reset connection module */
    ble_ll_conn_module_reset();

    ble_ll_conn_module_deinit();
#endif

    /* Clear the whitelist */
    ble_ll_whitelist_clear();

    /* Reset resolving list */
#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LL_PRIVACY)
    ble_ll_resolv_list_reset();

    ble_ll_resolv_deinit();
#endif

    return 0;
}
#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ble_ll_deinit, &ble_ll_deinit, &_ble_ll_deinit);
#else
__func_tab__ int (*ble_ll_deinit)(void) = _ble_ll_deinit;
#endif
