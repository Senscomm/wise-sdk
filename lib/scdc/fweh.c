/*
 * Copyright 2022-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <hal/unaligned.h>
#include <hal/kernel.h>
#include <hal/wlan.h>
#include <hal/kmem.h>

#include "kernel.h"
#include "compat_if.h"
#include "if_media.h"

#include <net80211/ieee80211_var.h>

#include "sncmu_d11.h"
#include "fwil_types.h"
#include "fweh.h"
#include "scdc.h"
#ifdef CONFIG_AT_OVER_SCDC
#include <netinet/in.h>
#include "libifconfig.h"
#include "lwip/netifapi.h"
#include "wise_event.h"
#include "fweh.h"
#include <at-wifi.h>
#endif

#define FWEH_ASSERT SCDC_ASSERT
#define FWEH_LOG1   SCDC_LOG1
#define FWEH_LOG2   SCDC_LOG2
#define FWEH_LOG3   SCDC_LOG3

typedef struct {
  u8 event_mask[SNCMF_EVENTING_MASK_LEN];
  u8 mgt_txcb[SNCMF_MAX_IFS][SNCMF_MGMT_TXCB_LEN];
  mutex mtx;
} fweh_ctx_t;

static fweh_ctx_t _fweh_ctx;

#ifdef CONFIG_AT_OVER_SCDC
void  fweh_scan_result_resp(uint8_t *result, size_t size)
{
  struct device *dev = wlandev(0);
  struct ieee80211vap *vap;
  assert(dev);

  vap = wlan_get_vap(dev, 0);

  fweh_send_event(vap, SNCMF_E_ESCAN_RESULT, 0,
      SNCMF_E_STATUS_SUCCESS, 0, 0, (u8 *)result, size);
}

void fweh_get_wlan_mac(uint8_t **mac_addr)
{
  struct device *dev = wlandev(0);
  struct ieee80211vap *vap;

  assert(dev);

  vap = wlan_get_vap(dev, 0);

  assert(vap);

  *mac_addr = &vap->iv_myaddr[0];
}

static uint8_t *_fweh_get_ipinfo(void)
{
  int ret = 0;
  ifconfig_handle_t *h = NULL;
  struct sockaddr sa, nm;
  struct sockaddr_in *addr;
  uint8_t *pbuf = NULL;

  h = ifconfig_open();
  if (h == NULL)
    return NULL;

  pbuf = kzalloc(8);
  if (!pbuf) {
    ret = -1;
    goto exit;
  }
  /* get & copy ip address */
  if (ifconfig_get_addr(h, "wlan0", &sa) < 0) {
    ret = -1;
    kfree(pbuf);
    goto exit;
  }
  addr = (struct sockaddr_in *)&sa;
  memcpy(pbuf, &(addr->sin_addr.s_addr), 4);

  /* get & copy netmask */
  if (ifconfig_get_netmask(h, "wlan0", &nm) < 0) {
    ret = -1;
    kfree(pbuf);
    goto exit;
  }
  addr = (struct sockaddr_in *)&nm;
  memcpy((uint8_t *)(pbuf + 4), &(addr->sin_addr.s_addr), 4);

exit:
  ifconfig_close(h);
  if (!ret)
    return pbuf;
  else
    return NULL;
}

static void _fweh_getip_evt_tx(struct ieee80211vap *vap)
{
  uint8_t *pbuf;

  pbuf = _fweh_get_ipinfo();

  if (pbuf) {
    fweh_send_event(vap, SNCMF_E_GOT_IP, 0,
        SNCMF_E_STATUS_SUCCESS, 0, 0, (u8 *)pbuf, 8);
    kfree(pbuf);
  }
  return;
}

static void _fweh_join_leave_evt_tx(struct ieee80211vap *vap, bool connection_flag)
{
  if (connection_flag) {
    fweh_send_event(vap, SNCMF_E_LINK, SNCMF_EVENT_MSG_LINK,
        SNCMF_E_STATUS_SUCCESS, 0, 0, (u8 *)0, 0);
  }
  else {
    fweh_send_event(vap, SNCMF_E_LINK, 0,
        SNCMF_E_STATUS_SUCCESS, 0, 0, (u8 *)0, 0);
  }
  return;
}
#endif

#ifdef CONFIG_SCDC_SUPPORT_SCM_CHANNEL

static void _fweh_scm_channel_evt_tx(struct ieee80211vap *vap, system_event_t *evt)
{
  uint8_t *buf = evt->event_info.scm_channel_msg.buf;
  uint32_t buf_len = evt->event_info.scm_channel_msg.buf_len;

  fweh_send_event(vap, SNCMF_E_SNCM_CHANNEL, 0,
      SNCMF_E_STATUS_SUCCESS, 0, 0, (u8 *)buf, buf_len);

  kfree(buf);
  return;
}

int fweh_sncm_channel_tx(char *buf, int len)
{
  struct device *dev = wlandev(0);
  struct ieee80211vap *vap;

  vap = wlan_get_vap(dev, 0);

  if (!buf || len == 0)
    return -1;

  return fweh_send_event(vap, SNCMF_E_SNCM_CHANNEL, 0,
            SNCMF_E_STATUS_SUCCESS, 0, 0, (u8 *)buf, len);
}
#endif

#if defined(CONFIG_AT_OVER_SCDC) || defined(CONFIG_SCDC_SUPPORT_SCM_CHANNEL)
void fweh_event_tx(void *event, size_t size)
{
  system_event_t *evt = (system_event_t *)event;
  system_event_id_t evt_id = evt->event_id;

  struct device *dev = wlandev(0);
  struct ieee80211vap *vap;

  vap = wlan_get_vap(dev, 0);

  switch(evt_id) {

#ifdef CONFIG_AT_OVER_SCDC
    case SYSTEM_EVENT_STA_GOT_IP:
      FWEH_LOG3("## %s %d SYSTEM_EVENT_STA_GOT_IP\n", __FUNCTION__, __LINE__);
      _fweh_getip_evt_tx(vap);
    break;

    case SYSTEM_EVENT_STA_DISCONNECTED:
      FWEH_LOG3("## %s %d SYSTEM_EVENT_STA_DISCONNECTED\n", __FUNCTION__, __LINE__);
      _fweh_join_leave_evt_tx(vap, 0);
#endif
    case SYSTEM_EVENT_STA_CONNECTED:
      FWEH_LOG3("## %s %d SYSTEM_EVENT_STA_CONNECTED\n", __FUNCTION__, __LINE__);
      _fweh_join_leave_evt_tx(vap, 1);
    break;

#ifdef CONFIG_SCDC_SUPPORT_SCM_CHANNEL
    case SYSTEM_EVENT_SCM_CHANNEL:
      FWEH_LOG3("## %s %d SYSTEM_EVENT_SCM_CHANNEL\n", __FUNCTION__, __LINE__);
      _fweh_scm_channel_evt_tx(vap, evt);
    break;
    case SYSTEM_EVENT_SCM_LINK_UP:
      FWEH_LOG3("## %s %d SYSTEM_EVENT_SCM_LINK_UP\n", __FUNCTION__, __LINE__);
      _fweh_join_leave_evt_tx(vap, 1);
    break;
#endif
    default:
    break;
  }
}
#endif

#ifdef CONFIG_AT_OVER_SCDC
struct at_wifi_ops_t _scdc_at_wifi_ops = {
  .at_send_event_hdl = fweh_event_tx,
  .at_get_mac_hdl    = fweh_get_wlan_mac,
  .at_scan_result_resp = fweh_scan_result_resp,
};
#endif

static void
fweh_lock(void)
{
  mtx_lock(&_fweh_ctx.mtx);
}

static void
fweh_unlock(void)
{
  mtx_unlock(&_fweh_ctx.mtx);
}

void fweh_init(void)
{
  int i, j;
  memset(&_fweh_ctx, 0, sizeof(_fweh_ctx));
  for (i = 0; i < SNCMF_MAX_IFS; i++)
  {
    for (j = 0; j < SNCMF_MGMT_TXCB_LEN; j++)
    {
      _fweh_ctx.mgt_txcb[i][j] = 0xff;
    }
  }
  mtx_init(&_fweh_ctx.mtx, NULL, NULL, MTX_DEF);
#ifdef CONFIG_AT_OVER_SCDC
  at_wifi_ops_register(&_scdc_at_wifi_ops);
#endif
}

int fweh_set_event_mask(u8 ifidx, u8 *mask)
{
  (void)ifidx;

  fweh_lock();
  memcpy(_fweh_ctx.event_mask, mask, SNCMF_EVENTING_MASK_LEN);
  fweh_unlock();

  return 0;
}

int fweh_set_mgt_txcb(u8 ifidx, u8 *subtype)
{
  fweh_lock();
  memcpy(&_fweh_ctx.mgt_txcb[ifidx], subtype, SNCMF_MGMT_TXCB_LEN);
  fweh_unlock();

  return 0;
}

int fweh_send_event(struct ieee80211vap *vap, u32 code, u16 flags,
    u32 status, u32 reason, u32 auth_type, u8 *data,
    u32 datalen)
{
  struct scdc_buffer *scdc_buf;
  struct sncmf_event *evt;
  struct device *dev = wlandev(0);
  u8 ifidx = get_vap_idx(dev, vap);

  FWEH_ASSERT(vap);
  FWEH_ASSERT(ifidx != -1);

  fweh_lock();
  if (isclr(_fweh_ctx.event_mask, code))
  {
    fweh_unlock();
    return -ENOTSUP;
  }
  fweh_unlock();

  scdc_buf = scdc_alloc_buffer(sizeof(*evt) + datalen);
  evt = (struct sncmf_event *)scdc_buf->body;

  /*
   * struct sncmf_event {
   *   struct ether_header eth;
   *   struct sncm_ethhdr hdr;
   *   struct sncmf_event_msg_be msg;
   * } __packed;
   */

  /*
   * struct ether_header {
   *   u_char	ether_dhost[ETHER_ADDR_LEN];
   *   u_char	ether_shost[ETHER_ADDR_LEN];
   *   u_short	ether_type;
   * } __packed;
  */

  memcpy(evt->eth.ether_dhost, vap->iv_myaddr, ETHER_ADDR_LEN);
  memcpy(evt->eth.ether_shost, vap->iv_myaddr, ETHER_ADDR_LEN);
  put_unaligned_be16(ETHERTYPE_P_LINK_CTL, &evt->eth.ether_type);

  /*
   * struct sncm_ethhdr {
   *   __be16 subtype;
   *   __be16 length;
   *   u8 version;
   *   u8 oui[3];
   *   __be16 usr_subtype;
   * } __packed;
   */

  put_unaligned_be16(SCMILCP_SUBTYPE_VENDOR_LONG, &evt->hdr.subtype);
  memcpy(evt->hdr.oui, SNCM_OUI, sizeof(evt->hdr.oui));
  put_unaligned_be16(SCMILCP_SCM_SUBTYPE_EVENT, &evt->hdr.usr_subtype);

  /*
   * struct sncmf_event_msg_be {
   * __be16 version;
   * __be16 flags;
   * __be32 event_type;
   * __be32 status;
   * __be32 reason;
   * __be32 auth_type;
   * __be32 datalen;
   * u8 addr[ETH_ALEN];
   * char ifname[IFNAMSIZ];
   * u8 ifidx;
   * u8 bsscfgidx;
   * } __packed;
   */

  put_unaligned_be16(flags, &evt->msg.flags);
  put_unaligned_be32(code, &evt->msg.event_type);
  put_unaligned_be32(status, &evt->msg.status);
  put_unaligned_be32(reason, &evt->msg.reason);
  put_unaligned_be32(auth_type, &evt->msg.auth_type);
  put_unaligned_be32(datalen, &evt->msg.datalen);
  evt->msg.ifidx = evt->msg.bsscfgidx = ifidx;
  memcpy(evt->msg.addr, vap->iv_myaddr, ETHER_ADDR_LEN);

  memcpy(&evt[1], data, datalen);

  return scdc_event(ifidx, scdc_buf, 0);
}

int fweh_send_if_event(u8 ifidx, struct sncmf_if_event *ievt,
    struct ieee80211vap *vap)
{
  struct scdc_buffer *scdc_buf;
  struct sncmf_event *evt;
  struct device *dev;
  struct ieee80211vap *if_vap;
  int vap_idx;
  int datalen = sizeof(*ievt);

  FWEH_ASSERT(ifidx < SNCMF_MAX_IFS);

  fweh_lock();
  if (isclr(_fweh_ctx.event_mask, SNCMF_E_IF))
  {
    fweh_unlock();
    return -ENOTSUP;
  }
  fweh_unlock();

  dev = wlandev(0);

  vap_idx = get_vap_idx(dev, vap);

  FWEH_ASSERT(vap_idx != -1);

  if_vap = wlan_get_vap(dev, ifidx);

  scdc_buf = scdc_alloc_buffer(sizeof(*evt) + datalen);
  evt = (struct sncmf_event *)scdc_buf->body;

  /*
   * struct sncmf_event {
   *   struct ether_header eth;
   *   struct sncm_ethhdr hdr;
   *   struct sncmf_event_msg_be msg;
   * } __packed;
   */

  /*
   * struct ether_header {
   *   u_char	ether_dhost[ETHER_ADDR_LEN];
   *   u_char	ether_shost[ETHER_ADDR_LEN];
   *   u_short	ether_type;
   * } __packed;
  */

  memcpy(evt->eth.ether_dhost, if_vap->iv_myaddr, ETHER_ADDR_LEN);
  memcpy(evt->eth.ether_shost, if_vap->iv_myaddr, ETHER_ADDR_LEN);
  put_unaligned_be16(ETHERTYPE_P_LINK_CTL, &evt->eth.ether_type);

  /*
   * struct sncm_ethhdr {
   *   __be16 subtype;
   *   __be16 length;
   *   u8 version;
   *   u8 oui[3];
   *   __be16 usr_subtype;
   * } __packed;
   */

  put_unaligned_be16(SCMILCP_SUBTYPE_VENDOR_LONG, &evt->hdr.subtype);
  memcpy(evt->hdr.oui, SNCM_OUI, sizeof(evt->hdr.oui));
  put_unaligned_be16(SCMILCP_SCM_SUBTYPE_EVENT, &evt->hdr.usr_subtype);

  /*
   * struct sncmf_event_msg_be {
   *   __be16 version;
   *   __be16 flags;
   *   __be32 event_type;
   *   __be32 status;
   *   __be32 reason;
   *   __be32 auth_type;
   *   __be32 datalen;
   *   u8 addr[ETH_ALEN];
   *   char ifname[IFNAMSIZ];
   *   u8 ifidx;
   *   u8 bsscfgidx;
   * } __packed;
   */

  put_unaligned_be32(SNCMF_E_IF, &evt->msg.event_type);
  put_unaligned_be32(datalen, &evt->msg.datalen);
  evt->msg.ifidx = evt->msg.bsscfgidx = vap_idx;
  memcpy(evt->msg.addr, vap->iv_myaddr, ETHER_ADDR_LEN);

  memcpy(&evt[1], ievt, sizeof(*ievt));

  return scdc_event(ifidx, scdc_buf, 0);
}

static void fweh_send_mlme_mgmt_frame(struct ieee80211vap *vap)
{
  struct mbuf *m;

  m = (struct mbuf *)vap->iv_rx_mgmt;

  if (m == NULL)
  {
    return;
  }

  fweh_send_event(vap, SNCMF_E_MLME_MGMT_RX, 0,
      SNCMF_E_STATUS_SUCCESS, 0, 0, mtod(m, u8 *), m->m_len);
}

void ieee80211_tx_mgt_cb_fweh(struct ieee80211_node *ni, void *arg,
    int status)
{
  struct ieee80211vap *vap = ni->ni_vap;
  struct device *dev = wlandev(0);
  u8 ifidx;
  struct mbuf *m = arg;
  struct ieee80211_frame *mh;
  int len;
  u8 subtype;
  int i;
  bool ignored = true;

  if (!vap)
    return;

  ifidx = get_vap_idx(dev, vap);

  mh = (struct ieee80211_frame *)M_AGE_GET(m);
  subtype = mh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
  len = (int)M_SEQNO_GET(m);

  if (!IEEE80211_IS_MGMT(mh))
    return;

  FWEH_LOG1("[%s] subtype: 0x%x\n", __func__, subtype);

  fweh_lock();
  for (i = 0; i < ARRAY_SIZE(_fweh_ctx.mgt_txcb[ifidx]); i++)
  {
    if (_fweh_ctx.mgt_txcb[ifidx][i] == subtype)
    {
      ignored = false;
      break;
    }
  }
  fweh_unlock();

  if (ignored)
    return;

  FWEH_LOG1("[%s] Sent\n", __func__);

  fweh_send_event(vap, SNCMF_E_MLME_MGMT_TX, 0,
      (status == 0 ? SNCMF_E_STATUS_SUCCESS : SNCMF_E_STATUS_FAIL),
      0, 0, (u8 *)mh, len);
}


static inline void
maskset16(u16 *var, u16 mask, u8 shift, u16 value)
{
  value = (value << shift) & mask;
  *var = (*var & ~mask) | value;
}

static u16
d11n_bw(const struct ieee80211_channel *chan)
{
  if (IEEE80211_IS_CHAN_HT40(chan))
  {
    return SNCMU_CHSPEC_D11N_BW_40;
  }
  else
  {
    return SNCMU_CHSPEC_D11N_BW_20;
  }
}

static u16
d11n_band(const struct ieee80211_channel *chan)
{
  if (IEEE80211_IS_CHAN_5GHZ(chan))
  {
    return SNCMU_CHSPEC_D11N_BND_5G;
  }
  else
  {
    return SNCMU_CHSPEC_D11N_BND_2G;
  }
}

static u16
encode_chanspec(const struct ieee80211_channel *chan)
{
  u16 chspec;

  chspec = 0;
  maskset16(&chspec, SNCMU_CHSPEC_CH_MASK,
      SNCMU_CHSPEC_CH_SHIFT, chan->ic_ieee);
  maskset16(&chspec, SNCMU_CHSPEC_D11N_BW_MASK,
      0, d11n_bw(chan));
  maskset16(&chspec, SNCMU_CHSPEC_D11N_BND_MASK,
      0, d11n_band(chan));

  return chspec;
}

static struct sncmf_escan_result_le *escan_result_le = NULL;

static void
notify_scan_entry(void *arg, const struct ieee80211_scan_entry *se)
{
  struct ieee80211vap *vap = arg;
  struct sncmf_bss_info_le *bss_info_le;
  int ielen = se->se_ies.len;
  int ssid_len;
  int nr, nxr;

  /* Do not indicate nontransmitted entry but
   * enable support mbssid in host driver.
   * Cfg80211 process mbssid via ie
   */
  if (se->se_nontransmitted == 1)
    return;

  if (escan_result_le != NULL
        && escan_result_le->buflen < sizeof(*escan_result_le) + ielen)
  {
    kfree(escan_result_le);
    escan_result_le = NULL;
  }
  if (escan_result_le == NULL)
  {
    escan_result_le = kzalloc(sizeof(*escan_result_le) + ielen);
  }
  else
  {
    /* clean up and reuse */
    memset(escan_result_le, 0, sizeof(*escan_result_le) + ielen);
  }
  escan_result_le->buflen = sizeof(*escan_result_le) + ielen;
  escan_result_le->bss_count = 1; /* report a single BSS at a time */

  bss_info_le = &escan_result_le->bss_info_le;
  bss_info_le->version = SNCMF_BSS_INFO_VERSION;
  bss_info_le->length = sizeof(*bss_info_le) + ielen;
  IEEE80211_ADDR_COPY(bss_info_le->BSSID, se->se_bssid);
  bss_info_le->beacon_period = se->se_intval;
  bss_info_le->capability = se->se_capinfo;
  bss_info_le->SSID_len = se->se_ssid[1];
  ssid_len = min(sizeof(bss_info_le->SSID), bss_info_le->SSID_len);
  memcpy(bss_info_le->SSID, &se->se_ssid[2], ssid_len);
  nr = min(se->se_rates[1], IEEE80211_RATE_MAXSIZE);
  memcpy(&bss_info_le->rateset.rates[0], se->se_rates + 2, nr);
  nxr = min(se->se_xrates[1], IEEE80211_RATE_MAXSIZE - nr);
  memcpy(&bss_info_le->rateset.rates[nr], se->se_xrates + 2, nxr);
  bss_info_le->rateset.count = nr + nxr;
  bss_info_le->dtim_period = se->se_dtimperiod;
  bss_info_le->RSSI = se->se_rssi;
  bss_info_le->phy_noise = se->se_noise;
  bss_info_le->ctl_ch = se->se_chan->ic_ieee;
  bss_info_le->chanspec = encode_chanspec(se->se_chan);

  /* IEs go right after the common fields for now. */
  bss_info_le->ie_offset = sizeof(*bss_info_le);
  bss_info_le->ie_length = ielen;
  memcpy((char *)bss_info_le + bss_info_le->ie_offset,
            se->se_ies.data, ielen);

  fweh_send_event(vap, SNCMF_E_ESCAN_RESULT, 0,
                    SNCMF_E_STATUS_PARTIAL, 0, 0,
                    (u8 *)escan_result_le, escan_result_le->buflen);
}

#if CONFIG_SUPPORT_SCAN_ENTRY_NOTIFY

void
ieee80211_notify_scan_entry_fweh(struct ieee80211vap *vap,
                       struct ieee80211_scan_entry *se)
{
  notify_scan_entry(vap, se);
}

#endif

extern void (*ieee80211_restore_privacye)(struct ieee80211vap *vap);

void
ieee80211_notify_scan_done_fweh(struct ieee80211vap *vap)
{
  struct ieee80211com *ic = vap->iv_ic;
  struct ieee80211_scan_state *ss = ic->ic_scan;
  /* XXX: truly ugly! */
#define ISCAN_INTERRUPT	0x0004		/* interrupt current scan */
#define	ISCAN_CANCEL	0x0008		/* cancel current scan */
#define ISCAN_PAUSE		(ISCAN_INTERRUPT | ISCAN_CANCEL)
#define	ISCAN_ABORT		0x0010		/* end the scan immediately */
  u32 ss_iflags = *(u32 *)(ss + 1);

  IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN, "%s\n", "notify scan done");

  vap->iv_flags_ext &= ~IEEE80211_FEXT_NON_CANCEL_SCAN;

  if (ss_iflags & ISCAN_PAUSE)
  {
    fweh_send_event(vap, SNCMF_E_ESCAN_RESULT, 0,
        SNCMF_E_STATUS_ABORT, 0, 0, (u8 *)0, 0);
    return;
  }

#ifndef CONFIG_SUPPORT_SCAN_ENTRY_NOTIFY

  ieee80211_scan_iterate(vap, notify_scan_entry, vap);

#endif

  ieee80211_restore_privacye(vap);

  if (escan_result_le)
  {
    kfree(escan_result_le);
    escan_result_le = NULL;
  }

  fweh_send_event(vap, SNCMF_E_ESCAN_RESULT, 0,
      SNCMF_E_STATUS_SUCCESS, 0, 0, (u8 *)0, 0);
}

void
ieee80211_notify_auth_fweh(struct ieee80211vap *vap,
                       const struct ieee80211_frame *wh, uint16_t algo,
                       uint16_t seq, uint16_t status, uint8_t * auth_data,
                       uint16_t auth_data_len)
{
  IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_AUTH, wh->i_addr2, "recv auth");

  fweh_send_mlme_mgmt_frame(vap);
}

void
ieee80211_notify_action_fweh(struct ieee80211vap *vap)
{
  fweh_send_mlme_mgmt_frame(vap);
}


void
ieee80211_notify_node_join_fweh(struct ieee80211_node *ni, int newassoc)
{
  struct ieee80211vap* vap = ni->ni_vap;

  IEEE80211_NOTE(vap, IEEE80211_MSG_NODE, ni, "%snode join",
      (ni == vap->iv_bss) ? "bss " : "");

  if (ni == vap->iv_bss) {
    fweh_send_mlme_mgmt_frame(vap);
    fweh_send_event(vap, SNCMF_E_LINK, SNCMF_EVENT_MSG_LINK,
        SNCMF_E_STATUS_SUCCESS, 0, 0, (u8 *)0, 0);
  }
}

void
ieee80211_notify_node_leave_fweh(struct ieee80211_node *ni)
{
  struct ieee80211vap* vap = ni->ni_vap;

  IEEE80211_NOTE(vap, IEEE80211_MSG_NODE, ni, "%snode leave",
        (ni == vap->iv_bss) ? "bss " : "");

  if (ni == vap->iv_bss) {
    fweh_send_event(vap, SNCMF_E_LINK, 0,
        SNCMF_E_STATUS_SUCCESS, 0, 0, (u8 *)0, 0);

  }
}

void
ieee80211_notify_assoc_reject_fweh(struct ieee80211vap *vap,
                       const struct ieee80211_frame *wh, uint16_t status,
                       u8 *assoc_resp, u16 assoc_resp_len)
{
  (void)assoc_resp;
  (void)assoc_resp_len;

  IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_ASSOC, wh->i_addr2, "recv assoc reject");

  fweh_send_mlme_mgmt_frame(vap);
}

void
fweh_send_fws_credit(uint32_t *fifo_credit, int len)
{
  struct ieee80211vap *if_vap;
  struct device *dev;

  dev = wlandev(0);
  if_vap = wlan_get_vap(dev, 0);
  fweh_send_event(if_vap, SNCMF_E_FIFO_CREDIT_MAP, 0,
      SNCMF_E_STATUS_SUCCESS, 0, 0, (u8 *)fifo_credit, len);
}
