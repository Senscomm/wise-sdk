#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "os/os.h"
#include "nimble/ble.h"
#include "nimble/hci_common.h"
#include "controller/ble_phy.h"
#include "controller/ble_hw.h"
#include "controller/ble_ll.h"
#include "controller/ble_ll_sched.h"
#include "controller/ble_ll_scan.h"
#include "controller/ble_ll_scan_aux.h"
#include "controller/ble_ll_hci.h"

#include "hal/compiler.h"
#include "hal/rom.h"

#ifdef CONFIG_LINK_TO_ROM

struct ble_ll_scan_aux_data {
    uint16_t flags;
    uint8_t hci_state;

    uint8_t scan_type;

    uint8_t pri_phy;
    uint8_t sec_phy;
    uint8_t chan;
    uint8_t offset_unit : 1;
    uint32_t aux_ptr;
    struct ble_ll_sched_item sch;
    struct ble_npl_event break_ev;
    struct ble_hci_ev *hci_ev;

    uint16_t adi;

    uint8_t adva[6];
    uint8_t targeta[6];
    uint8_t adva_type : 1;
    uint8_t targeta_type : 1;

#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LL_PRIVACY)
    int8_t rpa_index;
#endif
};

#define BLE_LL_SCAN_AUX_F_RESOLVED_ADVA     0x0100

extern int (*ble_ll_hci_ev_send_ext_adv_report)(struct os_mbuf *rxpdu,
                                  struct ble_mbuf_hdr *rxhdr,
                                  struct ble_hci_ev **hci_ev);
extern void *(*ble_transport_alloc_evt)(int discardable);
extern int ble_ll_hci_event_send(struct ble_hci_ev *hci_ev);

__ilm_ble__ static struct ble_hci_ev *
ble_ll_hci_ev_dup_ext_adv_report(struct ble_hci_ev *hci_ev_src)
{
    struct ble_hci_ev_le_subev_ext_adv_rpt *hci_subev;
    struct ext_adv_report *report;
    struct ble_hci_ev *hci_ev;

    hci_ev = ble_transport_alloc_evt(1);
    if (!hci_ev) {
        return NULL;
    }

    memcpy(hci_ev, hci_ev_src, sizeof(*hci_ev) + sizeof(*hci_subev) +
                               sizeof(*report));
    hci_ev->length = sizeof(*hci_subev) + sizeof(*report);

    hci_subev = (void *)hci_ev->data;

    report = hci_subev->reports;
    report->data_len = 0;

    return hci_ev;
}


__ilm_ble__ static int
patch_ble_ll_hci_ev_send_ext_adv_report(struct os_mbuf *rxpdu,
                                  struct ble_mbuf_hdr *rxhdr,
                                  struct ble_hci_ev **hci_ev)
{
    struct ble_mbuf_hdr_rxinfo *rxinfo = &rxhdr->rxinfo;
    struct ble_hci_ev_le_subev_ext_adv_rpt *hci_subev;
    struct ble_hci_ev *hci_ev_next;
    struct ext_adv_report *report;
    bool truncated = false;
    int max_data_len;
    int data_len;
    int offset;

    data_len = OS_MBUF_PKTLEN(rxpdu);
    max_data_len = BLE_LL_MAX_EVT_LEN -
                   sizeof(**hci_ev) - sizeof(*hci_subev) - sizeof(*report);
    offset = 0;
    hci_ev_next = NULL;


    do {
        hci_subev = (void *)(*hci_ev)->data;
        report = hci_subev->reports;

        report->rssi = rxinfo->rssi;

#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LL_PRIVACY)
		if (!rxinfo->user_data) {
			if (!(rxinfo->flags & BLE_MBUF_HDR_F_RESOLVED)) {
				if (report->addr_type == BLE_ADDR_PUBLIC_ID) {
					report->addr_type = BLE_ADDR_PUBLIC;
				} else if (report->addr_type == BLE_ADDR_RANDOM_ID) {
					report->addr_type = BLE_ADDR_RANDOM;
				}
			}
		} else {
			struct ble_ll_scan_aux_data *aux = rxinfo->user_data;

			if (!(aux->flags & BLE_LL_SCAN_AUX_F_RESOLVED_ADVA)) {
				if (report->addr_type == BLE_ADDR_PUBLIC_ID) {
					report->addr_type = BLE_ADDR_PUBLIC;
				} else if (report->addr_type == BLE_ADDR_RANDOM_ID) {
					report->addr_type = BLE_ADDR_RANDOM;
				}
			}
		}

		if (report->dir_addr_type == BLE_HCI_ADV_PEER_ADDR_RANDOM) {
			if (ble_ll_is_rpa(report->dir_addr, report->dir_addr_type)) {
				/* Resovable Private Address(Controller unable to resolve) */
				report->dir_addr_type = 0xfe;
			}
		}
#endif

        report->data_len = min(max_data_len, data_len - offset);
        os_mbuf_copydata(rxpdu, offset, report->data_len, report->data);
        (*hci_ev)->length += report->data_len;

        offset += report->data_len;

        /*
         * We need another event if either there are still some data left in
         * this PDU or scan for next aux is scheduled.
         */
        if ((offset < data_len) ||
            (rxinfo->flags & BLE_MBUF_HDR_F_AUX_PTR_WAIT)) {
            hci_ev_next = ble_ll_hci_ev_dup_ext_adv_report(*hci_ev);
            if (hci_ev_next) {
                report->evt_type |= BLE_HCI_ADV_DATA_STATUS_INCOMPLETE;
            } else {
                report->evt_type |= BLE_HCI_ADV_DATA_STATUS_TRUNCATED;
            }
        }

        switch (report->evt_type & BLE_HCI_ADV_DATA_STATUS_MASK) {
        case BLE_HCI_ADV_DATA_STATUS_TRUNCATED:
            truncated = true;
            /* no break */
        case BLE_HCI_ADV_DATA_STATUS_COMPLETE:
            BLE_LL_ASSERT(!hci_ev_next);
            break;
        case BLE_HCI_ADV_DATA_STATUS_INCOMPLETE:
            BLE_LL_ASSERT(hci_ev_next);
            break;
        default:
            BLE_LL_ASSERT(0);
        }

        ble_ll_hci_event_send(*hci_ev);

        *hci_ev = hci_ev_next;
        hci_ev_next = NULL;
    } while ((offset < data_len) && *hci_ev);

    return truncated ? -1 : 0;
}
#ifndef CONFIG_MFG
PATCH(ble_ll_hci_ev_send_ext_adv_report, &ble_ll_hci_ev_send_ext_adv_report, &patch_ble_ll_hci_ev_send_ext_adv_report);
#endif

#endif
