#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "syscfg/syscfg.h"
#include "os/os.h"

#include "controller/ble_ll.h"
#include "controller/ble_ll_adv.h"
#include "controller/ble_ll_scan.h"
#include "controller/ble_ll_sched.h"
#include "controller/ble_ll_hci.h"
#include "nimble/ble.h"

#include "hal/compiler.h"
#include "hal/rom.h"

#ifdef CONFIG_LINK_TO_ROM

#define BLE_LL_ADV_SM_FLAG_SCAN_REQ_NOTIF           0x0004
#define BLE_LL_ADV_SM_FLAG_ADV_DATA_INCOMPLETE      0x0040
#define BLE_LL_ADV_SM_FLAG_CONFIGURED               0x0080
#define BLE_LL_ADV_SM_FLAG_NEW_ADV_DATA             0x0200
#define BLE_LL_ADV_SM_FLAG_NEW_SCAN_RSP_DATA        0x0400

#define ADV_DATA_LEN(_advsm) \
                ((_advsm->adv_data) ? OS_MBUF_PKTLEN(advsm->adv_data) : 0)
#define SCAN_RSP_DATA_LEN(_advsm) \
                ((_advsm->scan_rsp_data) ? OS_MBUF_PKTLEN(advsm->scan_rsp_data) : 0)

struct ble_ll_adv_aux {
    struct ble_ll_sched_item sch;
    uint32_t start_time;
    uint16_t aux_data_offset;
    uint8_t chan;
    uint8_t ext_hdr;
    uint8_t aux_data_len;
    uint8_t payload_len;
};

struct ble_ll_adv_sm
{
    uint8_t adv_enabled;
    uint8_t adv_instance;
    uint8_t adv_chanmask;
    uint8_t adv_filter_policy;
    uint8_t own_addr_type;
    uint8_t peer_addr_type;
    uint8_t adv_chan;
    uint8_t adv_pdu_len;
    int8_t adv_rpa_index;
    int8_t adv_txpwr;
    uint16_t flags;
    uint16_t props;
    uint16_t adv_itvl_min;
    uint16_t adv_itvl_max;
    uint32_t adv_itvl_usecs;
    uint32_t adv_event_start_time;
    uint32_t adv_pdu_start_time;
    uint32_t adv_end_time;
    uint8_t adva[BLE_DEV_ADDR_LEN];
    uint8_t adv_rpa[BLE_DEV_ADDR_LEN];
    uint8_t peer_addr[BLE_DEV_ADDR_LEN];
    uint8_t initiator_addr[BLE_DEV_ADDR_LEN];
    struct os_mbuf *adv_data;
    struct os_mbuf *new_adv_data;
    struct os_mbuf *scan_rsp_data;
    struct os_mbuf *new_scan_rsp_data;
#if MYNEWT_VAL(BLE_LL_ROLE_PERIPHERAL)
    uint8_t *conn_comp_ev;
#endif
    struct ble_npl_event adv_txdone_ev;
    struct ble_ll_sched_item adv_sch;
#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LE_CSA2)
    uint16_t channel_id;
    uint16_t event_cntr;
#endif
#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LL_EXT_ADV)
    uint8_t aux_active : 1;
    uint8_t aux_index : 1;
    uint8_t aux_first_pdu : 1;
    uint8_t aux_not_scanned : 1;
    uint8_t aux_dropped : 1;
    struct ble_mbuf_hdr *rx_ble_hdr;
    struct os_mbuf **aux_data;
    struct ble_ll_adv_aux aux[2];
    struct ble_npl_event adv_sec_txdone_ev;
#if (SCM2010)
    /*
     * BLE qualification TEST
     * LL/DDI/ADV/BV-45, LL/DDI/ADV/BV-52, LL/DDI/ADV/BV-54
     */
    struct ble_npl_event adv_scan_req_recv_ev;
    uint8_t adv_scan_req_recv_proc;
    uint8_t adv_scan_req_peer_addr_type;
    uint8_t adv_scan_req_peer_addr[BLE_DEV_ADDR_LEN];
#endif
    uint16_t duration;
    uint16_t adi;
    uint8_t adv_random_addr[BLE_DEV_ADDR_LEN];
    uint8_t events_max;
    uint8_t events;
    uint8_t pri_phy;
    uint8_t sec_phy;
#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LL_PERIODIC_ADV)
    struct os_mbuf *periodic_adv_data;
    struct os_mbuf *periodic_new_data;
    uint32_t periodic_crcinit; /* only 3 bytes are used */
    uint32_t periodic_access_addr;
    uint16_t periodic_adv_itvl_min;
    uint16_t periodic_adv_itvl_max;
    uint16_t periodic_adv_props;
    uint16_t periodic_channel_id;
    uint16_t periodic_event_cntr;
    uint16_t periodic_chain_event_cntr;
    uint8_t periodic_adv_enabled : 1;
    uint8_t periodic_adv_active : 1;
    uint8_t periodic_sync_active : 1;
    uint8_t periodic_sync_index : 1;
    uint8_t periodic_num_used_chans;
    uint8_t periodic_chanmap[BLE_LL_CHAN_MAP_LEN];
    uint32_t periodic_adv_itvl_ticks;
    uint8_t periodic_adv_itvl_rem_usec;
    uint8_t periodic_adv_event_start_time_remainder;
    uint32_t periodic_adv_event_start_time;
    struct ble_ll_adv_sync periodic_sync[2];
    struct ble_npl_event adv_periodic_txdone_ev;
#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LL_PERIODIC_ADV_SYNC_TRANSFER)
    uint16_t periodic_event_cntr_last_sent;
#endif
#endif
#endif
};


extern struct ble_ll_adv_sm g_ble_ll_adv_sm[BLE_ADV_INSTANCES];
extern int8_t g_ble_ll_tx_power;

extern struct ble_ll_adv_sm * ble_ll_adv_sm_find_configured(uint8_t instance);
extern void ble_ll_adv_update_did(struct ble_ll_adv_sm *advsm);
extern void ble_ll_adv_flags_set(struct ble_ll_adv_sm *advsm, uint16_t flags);
extern void (*ble_ll_adv_update_data_mbuf)(struct os_mbuf **omp, bool new_data, uint16_t maxlen,
                            const void *data, uint16_t datalen);
extern void (*ble_ll_adv_sm_init)(struct ble_ll_adv_sm *advsm);
extern void ble_ll_adv_flags_clear(struct ble_ll_adv_sm *advsm, uint16_t flags);

extern int (*ble_ll_adv_set_scan_rsp_data)(const uint8_t *data, uint8_t datalen, uint8_t instance, uint8_t operation);
extern int (*ble_ll_adv_set_adv_data)(const uint8_t *data, uint8_t datalen, uint8_t instance, uint8_t operation);

static struct ble_ll_adv_sm *
ble_ll_adv_sm_get(uint8_t instance)
{
    struct ble_ll_adv_sm *advsm;
    int i;

    advsm = ble_ll_adv_sm_find_configured(instance);
    if (advsm) {
        return advsm;
    }

    for (i = 0; i < ARRAY_SIZE(g_ble_ll_adv_sm); i++) {
        advsm = &g_ble_ll_adv_sm[i];

        if (!(advsm->flags & BLE_LL_ADV_SM_FLAG_CONFIGURED)) {
            ble_ll_adv_sm_init(advsm);

           /* configured flag is set by caller on success config */
           advsm->adv_instance = instance;
           return advsm;
        }
    }

    return NULL;
}

static bool
pri_phy_valid(uint8_t phy)
{
    switch (phy) {
#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LE_CODED_PHY)
    case BLE_HCI_LE_PHY_CODED:
#endif
    case BLE_HCI_LE_PHY_1M:
        return true;
    default:
        return false;
    }
}

static bool
sec_phy_valid(uint8_t phy)
{
    switch (phy) {
#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LE_CODED_PHY)
    case BLE_HCI_LE_PHY_CODED:
#endif
#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LE_2M_PHY)
    case BLE_HCI_LE_PHY_2M:
#endif
    case BLE_HCI_LE_PHY_1M:
        return true;
    default:
        return false;
    }
}

static uint16_t
ble_ll_adv_aux_calculate_payload(struct ble_ll_adv_sm *advsm, uint16_t props,
                                 struct os_mbuf *data, uint32_t data_offset,
                                 uint8_t *data_len_o, uint8_t *ext_hdr_flags_o)
{
    uint16_t rem_data_len;
    uint8_t data_len;
    uint8_t ext_hdr_flags;
    uint8_t ext_hdr_len;
    bool chainable;
    bool first_pdu;

    /* Note: advsm shall only be used to check if periodic advertising is
     *       enabled, other parameters in advsm may have different values than
     *       those we want to check (e.g. when reconfiguring instance).
     */

    rem_data_len = (data ? OS_MBUF_PKTLEN(data) : 0) - data_offset;
    BLE_LL_ASSERT((int16_t)rem_data_len >= 0);

    first_pdu = (data_offset == 0);
    chainable = !(props & BLE_HCI_LE_SET_EXT_ADV_PROP_CONNECTABLE);

    ext_hdr_flags = 0;
    ext_hdr_len = BLE_LL_EXT_ADV_HDR_LEN;

    /* ADI for anything but scannable */
    if (!(props & BLE_HCI_LE_SET_EXT_ADV_PROP_SCANNABLE)) {
        ext_hdr_flags |= (1 << BLE_LL_EXT_ADV_DATA_INFO_BIT);
        ext_hdr_len += BLE_LL_EXT_ADV_DATA_INFO_SIZE;
    }

    /* AdvA in 1st PDU, except for anonymous */
    if (first_pdu &&
        !(props & BLE_HCI_LE_SET_EXT_ADV_PROP_ANON_ADV)) {
        ext_hdr_flags |= (1 << BLE_LL_EXT_ADV_ADVA_BIT);
        ext_hdr_len += BLE_LL_EXT_ADV_ADVA_SIZE;
    }

    /* TargetA in 1st PDU, if directed
     *
     * Note that for scannable this calculates AUX_SCAN_RSP which shall not
     * include TargetA (see: Core 5.3, Vol 6, Part B, 2.3.2.3). For scannable
     * TargetA is included in AUX_ADV_IND which is in that case calculated in
     * ble_ll_adv_aux_schedule_first().
     */
    if (first_pdu &&
        (props & BLE_HCI_LE_SET_EXT_ADV_PROP_DIRECTED) &&
        !(props & BLE_HCI_LE_SET_EXT_ADV_PROP_SCANNABLE)) {
        ext_hdr_flags |= (1 << BLE_LL_EXT_ADV_TARGETA_BIT);
        ext_hdr_len += BLE_LL_EXT_ADV_TARGETA_SIZE;
    }

    /* TxPower in 1st PDU, if configured */
    if (first_pdu &&
        (props & BLE_HCI_LE_SET_EXT_ADV_PROP_INC_TX_PWR)) {
        ext_hdr_flags |= (1 << BLE_LL_EXT_ADV_TX_POWER_BIT);
        ext_hdr_len += BLE_LL_EXT_ADV_TX_POWER_SIZE;
    }

#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LL_PERIODIC_ADV)
    /* SyncInfo in 1st PDU, if periodic advertising is enabled */
    if (first_pdu && advsm->periodic_adv_active) {
        ext_hdr_flags |= (1 << BLE_LL_EXT_ADV_SYNC_INFO_BIT);
        ext_hdr_len += BLE_LL_EXT_ADV_SYNC_INFO_SIZE;
    }
#endif

    /* Flags, if any field is present in header
     *
     * Note that this does not account for AuxPtr which is added later if
     * remaining data does not fit in single PDU.
     */
    if (ext_hdr_flags) {
        ext_hdr_len += BLE_LL_EXT_ADV_FLAGS_SIZE;
    }

    /* AdvData */
    data_len = min(BLE_LL_MAX_PAYLOAD_LEN - ext_hdr_len, rem_data_len);

    /* AuxPtr if there are more AdvData remaining that we can fit here */
    if (chainable && (rem_data_len > data_len)) {
        /* Add flags if not already added */
        if (!ext_hdr_flags) {
            ext_hdr_len += BLE_LL_EXT_ADV_FLAGS_SIZE;
            data_len -= BLE_LL_EXT_ADV_FLAGS_SIZE;
        }

        ext_hdr_flags |= (1 << BLE_LL_EXT_ADV_AUX_PTR_BIT);
        ext_hdr_len += BLE_LL_EXT_ADV_AUX_PTR_SIZE;

        data_len -= BLE_LL_EXT_ADV_AUX_PTR_SIZE;

        /* PDU payload should be full if adding AuxPtr */
        BLE_LL_ASSERT(ext_hdr_len + data_len == BLE_LL_MAX_PAYLOAD_LEN);
    }

    *data_len_o = data_len;
    *ext_hdr_flags_o = ext_hdr_flags;

    return ext_hdr_len + data_len;
}

static bool
ble_ll_adv_aux_check_data_itvl(struct ble_ll_adv_sm *advsm, uint16_t props,
                               uint8_t pri_phy, uint8_t sec_phy,
                               struct os_mbuf *data, uint32_t interval_us)
{
    uint32_t max_usecs;
    uint16_t data_offset;
    uint16_t pdu_len;
    uint8_t data_len;
    uint8_t ext_hdr_flags;

    /* FIXME:
     * We should include PDUs on primary channel when calculating advertising
     * event duration, but the actual time varies a bit in our case due to
     * scheduling. For now let's assume we always schedule all PDUs 300us apart
     * and we use shortest possible payload (ADI+AuxPtr, no AdvA).
     *
     * Note that calculations below do not take channel map and max skip into
     * account, but we do not support max skip anyway for now.
     */

    max_usecs = 3 * (ble_ll_pdu_tx_time_get(7, pri_phy) + 300) +
                BLE_LL_MAFS + MYNEWT_VAL(BLE_LL_SCHED_AUX_MAFS_DELAY);

    data_offset = 0;

    do {
        pdu_len = ble_ll_adv_aux_calculate_payload(advsm, props, data, data_offset,
                                                   &data_len, &ext_hdr_flags);
        max_usecs += ble_ll_pdu_tx_time_get(pdu_len, sec_phy);
        max_usecs += BLE_LL_MAFS + MYNEWT_VAL(BLE_LL_SCHED_AUX_CHAIN_MAFS_DELAY);

        data_offset += data_len;

    } while (ext_hdr_flags & (1 << BLE_LL_EXT_ADV_AUX_PTR_BIT));

    return max_usecs < interval_us;
}

int
patch_ble_ll_adv_set_scan_rsp_data(const uint8_t *data, uint8_t datalen,
                             uint8_t instance, uint8_t operation)
{
    struct ble_ll_adv_sm *advsm;
    bool new_data;

    advsm = ble_ll_adv_sm_find_configured(instance);
    if (!advsm) {
        return BLE_ERR_UNK_ADV_INDENT;
    }

    /* check if type of advertising support scan rsp */
    if (!(advsm->props & BLE_HCI_LE_SET_EXT_ADV_PROP_SCANNABLE)) {
        if (!(advsm->props & BLE_HCI_LE_SET_EXT_ADV_PROP_LEGACY)) {
            return BLE_ERR_INV_HCI_CMD_PARMS;
        }
    }

    switch (operation) {
    case BLE_HCI_LE_SET_DATA_OPER_COMPLETE:
        if (advsm->props & BLE_HCI_LE_SET_EXT_ADV_PROP_LEGACY) {
            if (datalen > BLE_SCAN_RSP_LEGACY_DATA_MAX_LEN) {
                return BLE_ERR_INV_HCI_CMD_PARMS;
            }
        }

        break;
#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LL_EXT_ADV)
    case BLE_HCI_LE_SET_DATA_OPER_LAST:
        /* TODO mark scan rsp as complete? */
        /* fall through */
    case BLE_HCI_LE_SET_DATA_OPER_INT:
        if (!advsm->scan_rsp_data) {
            return BLE_ERR_INV_HCI_CMD_PARMS;
        }

        if (advsm->props & BLE_HCI_LE_SET_EXT_ADV_PROP_LEGACY) {
            return BLE_ERR_INV_HCI_CMD_PARMS;
        }

        if (advsm->adv_enabled) {
            return BLE_ERR_CMD_DISALLOWED;
        }

        if (!datalen) {
            return BLE_ERR_INV_HCI_CMD_PARMS;
        }
        break;
    case BLE_HCI_LE_SET_DATA_OPER_FIRST:
        if (advsm->props & BLE_HCI_LE_SET_EXT_ADV_PROP_LEGACY) {
            return BLE_ERR_INV_HCI_CMD_PARMS;
        }

        if (advsm->adv_enabled) {
            return BLE_ERR_CMD_DISALLOWED;
        }

        if (!datalen) {
            return BLE_ERR_INV_HCI_CMD_PARMS;
        }

        break;
#endif
    default:
        return BLE_ERR_INV_HCI_CMD_PARMS;
    }

    new_data = (operation == BLE_HCI_LE_SET_DATA_OPER_COMPLETE) ||
               (operation == BLE_HCI_LE_SET_DATA_OPER_FIRST);

    if (advsm->adv_enabled) {
        if (advsm->new_scan_rsp_data) {
            ble_ll_adv_flags_clear(advsm, BLE_LL_ADV_SM_FLAG_NEW_SCAN_RSP_DATA);
            os_mbuf_free_chain(advsm->new_scan_rsp_data);
            advsm->new_scan_rsp_data = NULL;
        }

        ble_ll_adv_update_data_mbuf(&advsm->new_scan_rsp_data, new_data,
                                    BLE_ADV_DATA_MAX_LEN, data, datalen);
        if (!advsm->new_scan_rsp_data) {
            return BLE_ERR_MEM_CAPACITY;
        }

#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LL_EXT_ADV)
        if (!(advsm->props & BLE_HCI_LE_SET_EXT_ADV_PROP_LEGACY) &&
            !ble_ll_adv_aux_check_data_itvl(advsm, advsm->props, advsm->pri_phy,
                                            advsm->sec_phy,
                                            advsm->new_scan_rsp_data,
                                            advsm->adv_itvl_usecs)) {
            os_mbuf_free_chain(advsm->new_scan_rsp_data);
            advsm->new_scan_rsp_data = NULL;
            return BLE_ERR_PACKET_TOO_LONG;
        }
#endif

        ble_ll_adv_flags_set(advsm, BLE_LL_ADV_SM_FLAG_NEW_SCAN_RSP_DATA);
    } else {
        ble_ll_adv_update_data_mbuf(&advsm->scan_rsp_data, new_data,
                                    BLE_SCAN_RSP_DATA_MAX_LEN, data, datalen);
        if (!advsm->scan_rsp_data) {
            return BLE_ERR_MEM_CAPACITY;
        }

#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LL_EXT_ADV)
        if (!(advsm->props & BLE_HCI_LE_SET_EXT_ADV_PROP_LEGACY) &&
            !ble_ll_adv_aux_check_data_itvl(advsm, advsm->props, advsm->pri_phy,
                                            advsm->sec_phy, advsm->adv_data,
                                            advsm->adv_itvl_usecs)) {
            os_mbuf_free_chain(advsm->adv_data);
            advsm->adv_data = NULL;
            return BLE_ERR_PACKET_TOO_LONG;
        }

        /* DID shall be updated when host provides new scan response data */
        ble_ll_adv_update_did(advsm);
#endif
    }

    return BLE_ERR_SUCCESS;
}
#ifndef CONFIG_MFG
PATCH(ble_ll_adv_set_scan_rsp_data, &ble_ll_adv_set_scan_rsp_data, &patch_ble_ll_adv_set_scan_rsp_data);
#endif

int
patch_ble_ll_adv_set_adv_data(const uint8_t *data, uint8_t datalen, uint8_t instance,
                        uint8_t operation)
{
    struct ble_ll_adv_sm *advsm;
    bool new_data;

    advsm = ble_ll_adv_sm_find_configured(instance);
    if (!advsm) {
        return BLE_ERR_UNK_ADV_INDENT;
    }

    /* check if type of advertising support adv data */
    if (advsm->props & BLE_HCI_LE_SET_EXT_ADV_PROP_LEGACY) {
        if (advsm->props & BLE_HCI_LE_SET_EXT_ADV_PROP_DIRECTED) {
            if (ble_ll_hci_adv_mode_ext()) {
                return BLE_ERR_INV_HCI_CMD_PARMS;
            }
        }
    } else {
        if (advsm->props & BLE_HCI_LE_SET_EXT_ADV_PROP_SCANNABLE) {
            return BLE_ERR_INV_HCI_CMD_PARMS;
        }
    }

    switch (operation) {
    case BLE_HCI_LE_SET_DATA_OPER_COMPLETE:
        if (advsm->props & BLE_HCI_LE_SET_EXT_ADV_PROP_LEGACY) {
            if (datalen > BLE_ADV_LEGACY_DATA_MAX_LEN) {
                return BLE_ERR_INV_HCI_CMD_PARMS;
            }
        }

        ble_ll_adv_flags_clear(advsm, BLE_LL_ADV_SM_FLAG_ADV_DATA_INCOMPLETE);

        break;
#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LL_EXT_ADV)
    case BLE_HCI_LE_SET_DATA_OPER_UNCHANGED:
        if (advsm->props & BLE_HCI_LE_SET_EXT_ADV_PROP_LEGACY) {
            return BLE_ERR_INV_HCI_CMD_PARMS;
        }

        if (!advsm->adv_enabled || !ADV_DATA_LEN(advsm) || datalen) {
            return BLE_ERR_INV_HCI_CMD_PARMS;
        }

        /* update DID only */
        ble_ll_adv_update_did(advsm);
        return BLE_ERR_SUCCESS;
    case BLE_HCI_LE_SET_DATA_OPER_LAST:
        ble_ll_adv_flags_clear(advsm, BLE_LL_ADV_SM_FLAG_ADV_DATA_INCOMPLETE);
        /* fall through */
    case BLE_HCI_LE_SET_DATA_OPER_INT:
        if (!advsm->adv_data) {
            return BLE_ERR_INV_HCI_CMD_PARMS;
        }

        if (advsm->props & BLE_HCI_LE_SET_EXT_ADV_PROP_LEGACY) {
            return BLE_ERR_INV_HCI_CMD_PARMS;
        }

        if (!datalen) {
            return BLE_ERR_INV_HCI_CMD_PARMS;
        }

        if (advsm->adv_enabled) {
            return BLE_ERR_CMD_DISALLOWED;
        }
        break;
    case BLE_HCI_LE_SET_DATA_OPER_FIRST:
        if (advsm->props & BLE_HCI_LE_SET_EXT_ADV_PROP_LEGACY) {
            return BLE_ERR_INV_HCI_CMD_PARMS;
        }

        if (advsm->adv_enabled) {
            return BLE_ERR_CMD_DISALLOWED;
        }

        if (!datalen) {
            return BLE_ERR_INV_HCI_CMD_PARMS;
        }

        ble_ll_adv_flags_set(advsm, BLE_LL_ADV_SM_FLAG_ADV_DATA_INCOMPLETE);
        break;
#endif
    default:
        return BLE_ERR_INV_HCI_CMD_PARMS;
    }

    new_data = (operation == BLE_HCI_LE_SET_DATA_OPER_COMPLETE) ||
               (operation == BLE_HCI_LE_SET_DATA_OPER_FIRST);

    if (advsm->adv_enabled) {
        if (advsm->new_adv_data) {
            ble_ll_adv_flags_clear(advsm, BLE_LL_ADV_SM_FLAG_NEW_ADV_DATA);
            os_mbuf_free_chain(advsm->new_adv_data);
            advsm->new_adv_data = NULL;
        }

        ble_ll_adv_update_data_mbuf(&advsm->new_adv_data, new_data,
                                    BLE_ADV_DATA_MAX_LEN, data, datalen);
        if (!advsm->new_adv_data) {
            return BLE_ERR_MEM_CAPACITY;
        }

#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LL_EXT_ADV)
        if (!(advsm->props & BLE_HCI_LE_SET_EXT_ADV_PROP_LEGACY) &&
            !ble_ll_adv_aux_check_data_itvl(advsm, advsm->props, advsm->pri_phy,
                                            advsm->sec_phy, advsm->new_adv_data,
                                            advsm->adv_itvl_usecs)) {
            os_mbuf_free_chain(advsm->new_adv_data);
            advsm->new_adv_data = NULL;
            return BLE_ERR_PACKET_TOO_LONG;
        }
#endif

        ble_ll_adv_flags_set(advsm, BLE_LL_ADV_SM_FLAG_NEW_ADV_DATA);
    } else {
        ble_ll_adv_update_data_mbuf(&advsm->adv_data, new_data,
                                    BLE_ADV_DATA_MAX_LEN, data, datalen);
        if (!advsm->adv_data) {
            return BLE_ERR_MEM_CAPACITY;
        }

#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LL_EXT_ADV)
        if (!(advsm->props & BLE_HCI_LE_SET_EXT_ADV_PROP_LEGACY) &&
            !ble_ll_adv_aux_check_data_itvl(advsm, advsm->props, advsm->pri_phy,
                                            advsm->sec_phy, advsm->adv_data,
                                            advsm->adv_itvl_usecs)) {
            os_mbuf_free_chain(advsm->adv_data);
            advsm->adv_data = NULL;
            return BLE_ERR_PACKET_TOO_LONG;
        }

        /* DID shall be updated when host provides new advertising data */
        ble_ll_adv_update_did(advsm);
#endif
        }

    return BLE_ERR_SUCCESS;
}
#ifndef CONFIG_MFG
PATCH(ble_ll_adv_set_adv_data, &ble_ll_adv_set_adv_data, &patch_ble_ll_adv_set_adv_data);
#endif

int
patch_ble_ll_adv_ext_set_param(const uint8_t *cmdbuf, uint8_t len,
                         uint8_t *rspbuf, uint8_t *rsplen)
{
    const struct ble_hci_le_set_ext_adv_params_cp *cmd = (const void *) cmdbuf;
    struct ble_hci_le_set_ext_adv_params_rp *rsp = (void *) rspbuf;
    struct ble_ll_adv_sm *advsm;
    uint32_t adv_itvl_min;
    uint32_t adv_itvl_max;
    uint32_t adv_itvl_usecs;
    uint16_t props;
    int rc;

    if (len != sizeof(*cmd )) {
        rc = BLE_ERR_INV_HCI_CMD_PARMS;
        goto done;
    }

    advsm = ble_ll_adv_sm_get(cmd->adv_handle);
    if (!advsm) {
        rc = BLE_ERR_MEM_CAPACITY;
        goto done;
    }

    if (advsm->adv_enabled) {
        rc = BLE_ERR_CMD_DISALLOWED;
        goto done;
    }

    props = le16toh(cmd->props);

#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LL_PERIODIC_ADV)
    /* If the Host issues this command when periodic advertising is enabled for
     * the specified advertising set and connectable, scannable, legacy, or
     * anonymous advertising is specified, the Controller shall return the
     * error code Invalid HCI Command Parameters (0x12).
     */
    if (advsm->flags & BLE_LL_ADV_SM_FLAG_PERIODIC_CONFIGURED) {
        if (advsm->periodic_adv_enabled) {
            if (props & (BLE_HCI_LE_SET_EXT_ADV_PROP_SCANNABLE |
                         BLE_HCI_LE_SET_EXT_ADV_PROP_CONNECTABLE |
                         BLE_HCI_LE_SET_EXT_ADV_PROP_LEGACY |
                         BLE_HCI_LE_SET_EXT_ADV_PROP_ANON_ADV)) {
                rc = BLE_ERR_INV_HCI_CMD_PARMS;
                goto done;
            }
        }
    }
#endif

    adv_itvl_min = cmd->pri_itvl_min[2] << 16 | cmd->pri_itvl_min[1] << 8 |
                   cmd->pri_itvl_min[0];
    adv_itvl_max = cmd->pri_itvl_max[2] << 16 | cmd->pri_itvl_max[1] << 8 |
                   cmd->pri_itvl_max[0];

    if (props & ~BLE_HCI_LE_SET_EXT_ADV_PROP_MASK) {
        rc = BLE_ERR_INV_HCI_CMD_PARMS;
        goto done;
    }

    if (props & BLE_HCI_LE_SET_EXT_ADV_PROP_LEGACY) {
        if (ADV_DATA_LEN(advsm) > BLE_ADV_LEGACY_DATA_MAX_LEN ||
            SCAN_RSP_DATA_LEN(advsm) > BLE_SCAN_RSP_LEGACY_DATA_MAX_LEN) {
            rc = BLE_ERR_INV_HCI_CMD_PARMS;
            goto done;
        }

        /* if legacy bit is set possible values are limited */
        switch (props) {
#if MYNEWT_VAL(BLE_LL_ROLE_PERIPHERAL)
        case BLE_HCI_LE_SET_EXT_ADV_PROP_LEGACY_IND:
        case BLE_HCI_LE_SET_EXT_ADV_PROP_LEGACY_LD_DIR:
        case BLE_HCI_LE_SET_EXT_ADV_PROP_LEGACY_HD_DIR:
#endif
        case BLE_HCI_LE_SET_EXT_ADV_PROP_LEGACY_SCAN:
        case BLE_HCI_LE_SET_EXT_ADV_PROP_LEGACY_NONCONN:
            break;
        default:
            rc = BLE_ERR_INV_HCI_CMD_PARMS;
            goto done;
        }
    } else {
#if !MYNEWT_VAL(BLE_LL_ROLE_PERIPHERAL)
        if (props & BLE_HCI_LE_SET_EXT_ADV_PROP_CONNECTABLE) {
            rc = BLE_ERR_INV_HCI_CMD_PARMS;
            goto done;
        }
#endif
        /* HD directed advertising allowed only on legacy PDUs */
        if (props & BLE_HCI_LE_SET_EXT_ADV_PROP_HD_DIRECTED) {
            rc = BLE_ERR_INV_HCI_CMD_PARMS;
            goto done;
        }

        /* if ext advertising PDUs are used then it shall not be both
         * connectable and scanable
         */
        if ((props & BLE_HCI_LE_SET_EXT_ADV_PROP_CONNECTABLE) &&
            (props & BLE_HCI_LE_SET_EXT_ADV_PROP_SCANNABLE)) {
            rc = BLE_ERR_INV_HCI_CMD_PARMS;
            goto done;
        }
    }

    /* High Duty Directed advertising is special */
    if (props & BLE_HCI_LE_SET_EXT_ADV_PROP_HD_DIRECTED) {
        if (ADV_DATA_LEN(advsm) || SCAN_RSP_DATA_LEN(advsm)) {
            rc = BLE_ERR_INV_HCI_CMD_PARMS;
            goto done;
        }

        /* Ignore min/max interval */
        adv_itvl_min = 0;
        adv_itvl_max = 0;
    } else {
        /* validate intervals for non HD-directed advertising */
        if ((adv_itvl_min > adv_itvl_max) ||
                (adv_itvl_min < BLE_HCI_ADV_ITVL_MIN) ||
                (adv_itvl_max < BLE_HCI_ADV_ITVL_MIN)) {
            rc = BLE_ERR_INV_HCI_CMD_PARMS;
            goto done;
        }

        /* TODO for now limit those to values from legacy advertising
         *
         * If the primary advertising interval range is outside the advertising
         * interval range supported by the Controller, then the Controller shall
         * return the error code Unsupported Feature or Parameter Value (0x11).
         */
        if ((adv_itvl_min > BLE_HCI_ADV_ITVL_MAX) ||
                (adv_itvl_max > BLE_HCI_ADV_ITVL_MAX)) {
            rc = BLE_ERR_UNSUPPORTED;
            goto done;
        }
    }

    /* There are only three adv channels, so check for any outside the range */
    if (((cmd->pri_chan_map & 0xF8) != 0) || (cmd->pri_chan_map == 0)) {
        rc = BLE_ERR_INV_HCI_CMD_PARMS;
        goto done;
    }

    if (cmd->own_addr_type > BLE_HCI_ADV_OWN_ADDR_MAX) {
        rc = BLE_ERR_INV_HCI_CMD_PARMS;
        goto done;
    }

#if !MYNEWT_VAL(BLE_LL_CFG_FEAT_LL_PRIVACY)
    /* If we dont support privacy some address types won't work */
    if (cmd->own_addr_type > BLE_HCI_ADV_OWN_ADDR_RANDOM) {
        rc = BLE_ERR_UNSUPPORTED;
        goto done;
    }
#endif

    /* peer address type is only valid for directed */
    if ((props & BLE_HCI_LE_SET_EXT_ADV_PROP_DIRECTED) &&
            (cmd->peer_addr_type > BLE_HCI_ADV_PEER_ADDR_MAX)) {
        rc = BLE_ERR_INV_HCI_CMD_PARMS;
        goto done;
    }

    /* Check filter policy (valid only for undirected) */
    if (!(props & BLE_HCI_LE_SET_EXT_ADV_PROP_DIRECTED) &&
         cmd->filter_policy > BLE_HCI_ADV_FILT_MAX) {
        rc = BLE_ERR_INV_HCI_CMD_PARMS;
        goto done;
    }

    if (!pri_phy_valid(cmd->pri_phy)) {
        rc = BLE_ERR_INV_HCI_CMD_PARMS;
        goto done;
    }

    /* check secondary phy only if not using legacy PDUs */
    if (!(props & BLE_HCI_LE_SET_EXT_ADV_PROP_LEGACY) &&
            !sec_phy_valid(cmd->sec_phy)) {
        rc = BLE_ERR_INV_HCI_CMD_PARMS;
        goto done;
    }

    if (cmd->sid > 0x0f) {
        rc = BLE_ERR_INV_HCI_CMD_PARMS;
        goto done;
    }

    if (cmd->scan_req_notif > 0x01) {
        rc = BLE_ERR_INV_HCI_CMD_PARMS;
        goto done;
    }

    /* Determine the advertising interval we will use */
    if (props & BLE_HCI_LE_SET_EXT_ADV_PROP_HD_DIRECTED) {
        /* Set it to max. allowed for high duty cycle advertising */
        adv_itvl_usecs = BLE_LL_ADV_PDU_ITVL_HD_MS_MAX;
    } else {
        adv_itvl_usecs = adv_itvl_max * BLE_LL_ADV_ITVL;
    }

    if (!(props & BLE_HCI_LE_SET_EXT_ADV_PROP_LEGACY)) {
		if (!ble_ll_adv_aux_check_data_itvl(advsm, props, cmd->pri_phy, cmd->sec_phy,
					                        advsm->adv_data, adv_itvl_usecs) ||
			!ble_ll_adv_aux_check_data_itvl(advsm, props, cmd->pri_phy, cmd->sec_phy,
				                            advsm->scan_rsp_data, adv_itvl_usecs)) {
			return BLE_ERR_PACKET_TOO_LONG;
		}
	}

    rc = BLE_ERR_SUCCESS;

    if (cmd->tx_power == 127) {
        /* no preference */
        advsm->adv_txpwr = g_ble_ll_tx_power;
    } else {
        advsm->adv_txpwr = ble_phy_txpower_round(cmd->tx_power);
    }

    /* we can always store as those are validated and used only when needed */
    advsm->peer_addr_type = cmd->peer_addr_type;
    memcpy(advsm->peer_addr, cmd->peer_addr, BLE_DEV_ADDR_LEN);
    advsm->own_addr_type = cmd->own_addr_type;
    advsm->adv_filter_policy = cmd->filter_policy;
    advsm->adv_chanmask = cmd->pri_chan_map;
    advsm->adv_itvl_min = adv_itvl_min;
    advsm->adv_itvl_max = adv_itvl_max;
    advsm->adv_itvl_usecs = adv_itvl_usecs;
    advsm->pri_phy = cmd->pri_phy;
    advsm->sec_phy = cmd->sec_phy;
    /* Update SID only */
    advsm->adi = (advsm->adi & 0x0fff) | ((cmd->sid << 12));

    advsm->props = props;

    /* Set proper mbuf chain for aux data */
    if (props & BLE_HCI_LE_SET_EXT_ADV_PROP_LEGACY) {
        advsm->aux_data = NULL;
    } else if (props & BLE_HCI_LE_SET_EXT_ADV_PROP_SCANNABLE) {
        advsm->aux_data = &advsm->scan_rsp_data;
    } else {
        advsm->aux_data = &advsm->adv_data;
    }

    if (cmd->scan_req_notif) {
        ble_ll_adv_flags_set(advsm, BLE_LL_ADV_SM_FLAG_SCAN_REQ_NOTIF);
    } else {
        ble_ll_adv_flags_clear(advsm, BLE_LL_ADV_SM_FLAG_SCAN_REQ_NOTIF);
    }

    ble_ll_adv_flags_set(advsm, BLE_LL_ADV_SM_FLAG_CONFIGURED);

done:
    /* Update TX power */
    rsp->tx_power = rc ? 0 : advsm->adv_txpwr;

    *rsplen = sizeof(*rsp);
    return rc;
}
#ifndef CONFIG_MFG
PATCH(ble_ll_adv_ext_set_param, &ble_ll_adv_ext_set_param, &patch_ble_ll_adv_ext_set_param);
#endif

/* #define TEST_ADV_COUNTS */
#ifdef TEST_ADV_COUNTS

#define BLE_LL_ADV_SM_FLAG_TX_ADD                   0x0001
#define BLE_LL_ADV_SM_FLAG_RX_ADD                   0x0002

static uint8_t g_adv_count;

__ilm_ble__ static uint8_t
patch_ble_ll_adv_legacy_pdu_make(uint8_t *dptr, void *pducb_arg, uint8_t *hdr_byte)
{
    struct ble_ll_adv_sm *advsm;
    uint8_t     adv_data_len;
    uint8_t     pdulen;
    uint8_t     pdu_type;

    advsm = pducb_arg;

    /* assume this is not a direct ind */
    adv_data_len = ADV_DATA_LEN(advsm);
    pdulen = BLE_DEV_ADDR_LEN + adv_data_len;

    if (advsm->props & BLE_HCI_LE_SET_EXT_ADV_PROP_DIRECTED) {
        pdu_type = BLE_ADV_PDU_TYPE_ADV_DIRECT_IND;

#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LE_CSA2)
        pdu_type |= BLE_ADV_PDU_HDR_CHSEL;
#endif

        if (advsm->flags & BLE_LL_ADV_SM_FLAG_RX_ADD) {
            pdu_type |= BLE_ADV_PDU_HDR_RXADD_RAND;
        }

        adv_data_len = 0;
        pdulen = BLE_ADV_DIRECT_IND_LEN;
    } else if (advsm->props & BLE_HCI_LE_SET_EXT_ADV_PROP_CONNECTABLE) {
        pdu_type = BLE_ADV_PDU_TYPE_ADV_IND;

#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LE_CSA2)
        pdu_type |= BLE_ADV_PDU_HDR_CHSEL;
#endif
    } else if (advsm->props & BLE_HCI_LE_SET_EXT_ADV_PROP_SCANNABLE) {
        pdu_type = BLE_ADV_PDU_TYPE_ADV_SCAN_IND;
    } else {
        pdu_type = BLE_ADV_PDU_TYPE_ADV_NONCONN_IND;
    }

    /* An invalid advertising data length indicates a memory overwrite */
    BLE_LL_ASSERT(adv_data_len <= BLE_ADV_LEGACY_DATA_MAX_LEN);

    /* Set the PDU length in the state machine (includes header) */
    advsm->adv_pdu_len = pdulen + BLE_LL_PDU_HDR_LEN;

    /* Set TxAdd to random if needed. */
    if (advsm->flags & BLE_LL_ADV_SM_FLAG_TX_ADD) {
        pdu_type |= BLE_ADV_PDU_HDR_TXADD_RAND;
    }

    *hdr_byte = pdu_type;

    /* Construct advertisement */
    memcpy(dptr, advsm->adva, BLE_DEV_ADDR_LEN);
    dptr += BLE_DEV_ADDR_LEN;

    /* For ADV_DIRECT_IND add inita */
    if (advsm->props & BLE_HCI_LE_SET_EXT_ADV_PROP_DIRECTED) {
        memcpy(dptr, advsm->initiator_addr, BLE_DEV_ADDR_LEN);
    }

    /* Copy in advertising data, if any */
    if (adv_data_len != 0) {
        os_mbuf_copydata(advsm->adv_data, 0, adv_data_len, dptr);
        dptr[2] = g_adv_count;
        if (advsm->adv_chan == 39) { /* per a adv event */
            g_adv_count++;
        }
    }

    return pdulen;
}

extern uint8_t (*ble_ll_adv_legacy_pdu_make)(uint8_t *dptr, void *pducb_arg, uint8_t *hdr_byte);
PATCH(ble_ll_adv_legacy_pdu_make, &ble_ll_adv_legacy_pdu_make, &patch_ble_ll_adv_legacy_pdu_make);


#endif /* TEST_ADV_COUNTS */

#endif
