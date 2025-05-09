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

#include <version.h>
#include <hal/kernel.h>
#include <hal/console.h>
#include <hal/wlan.h>
#include <sys/sockio.h>
#include "kernel.h"
#include "compat_if.h"
#include "if_media.h"
#include <net80211/ieee80211_var.h>
#include "fwil_types.h"
#include "fwil.h"
#include "fweh.h"
#include "fws.h"
#include "sncmu_d11.h"
#include "scdc.h"
#include <at.h>
#ifdef CONFIG_SCDC_SUPPORT_LOCAL_NETIF
#include <libifconfig.h>
#include <netinet/in.h>
#endif

#ifdef CONFIG_SDIO
#include <hal/sdio.h>
#endif
#ifdef CONFIG_SCDC_SUPPORT_SCM_CHANNEL
#include "wise_channel.h"
#endif

#define FWIL_ASSERT SCDC_ASSERT
#define FWIL_LOG1   SCDC_LOG1
#define FWIL_LOG2   SCDC_LOG2
#define FWIL_LOG3   SCDC_LOG3

/* Private functions */

#define set80211param(dev, vap, op, arg)	set80211(dev, vap, op, arg, NULL, 0)
#define set80211var(dev, vap, op, arg, len)	set80211(dev, vap, op, 0, arg, len)
#define get80211var(dev, vap, op, arg, len)	get80211(dev, vap, op, arg, len)


static int set_mediaopt(struct device *dev, struct ieee80211vap *vap,
    u32 mask, u32 mode)
{
  struct ifmediareq ifmr;
  struct ifreq ifr;

  memset(&ifmr, 0, sizeof(ifmr));
  if (wlan_ctl_vap(dev, vap, SIOCGIFMEDIA, (caddr_t)&ifmr) < 0 ||
      ifmr.ifm_current < 0)
  {
    FWIL_LOG1("wlan_ctl_vap(SIOCGIFMEDIA) failed\n");
    return -1;
  }

  memset(&ifr, 0, sizeof(ifr));
  ifr.ifr_media = (ifmr.ifm_current & ~mask) | mode;

  return wlan_ctl_vap(dev, vap, SIOCSIFMEDIA, (caddr_t)&ifr);
}

static int get80211(struct device *dev, struct ieee80211vap *vap,
        int op, void *arg, int arg_len) __maybe_unused;
static int get80211(struct device *dev, struct ieee80211vap *vap,
        int op, void *arg, int arg_len)
{
  struct ieee80211req ireq, *p = &ireq;

  memset(p, 0, sizeof(*p));

  p->i_type = op;
  p->i_len = arg_len;
  p->i_data = arg;

  return wlan_ctl_vap(dev, vap, SIOCG80211, (caddr_t)p);
}

static int set80211(struct device *dev, struct ieee80211vap *vap,
                    int op, int val, const void *arg, int arg_len)
{
  struct ieee80211req ireq, *p = &ireq;

  memset(p, 0, sizeof(*p));

  p->i_type = op;
  p->i_val = val;
  p->i_data = (void *)arg;
  p->i_len = arg_len;

  return wlan_ctl_vap(dev, vap, SIOCS80211, (caddr_t)p);
}

static int send_mlme_param(struct device *dev, struct ieee80211vap *vap,
        const u8 op, const u16 reason, const u8 *addr)
{
  struct ieee80211req_mlme mlme, *p = &mlme;

  memset(p, 0, sizeof(*p));
  p->im_op = op;
  p->im_reason = reason;
  if (addr)
  {
    memcpy(p->im_macaddr, addr, IEEE80211_ADDR_LEN);
  }

  return set80211var(dev, vap, IEEE80211_IOC_MLME, p, sizeof(*p));
}

static int set_wpa_ie(struct device *dev, struct ieee80211vap *vap,
        const u8 *wpa_ie, size_t wpa_ie_len)
{
	return set80211(dev, vap, IEEE80211_IOC_APPIE, IEEE80211_APPIE_WPA,
                    wpa_ie, wpa_ie_len);
}

static int set_wpa_privacy(struct device *dev, struct ieee80211vap *vap,
		int wpa, int privacy)
{
  if (!wpa && set_wpa_ie(dev, vap, NULL, 0) < 0)
  {
    FWIL_LOG1("set_wpa_ie failed\n");
    goto failed;
  }

  if (set80211param(dev, vap, IEEE80211_IOC_PRIVACY, privacy) < 0)
  {
    FWIL_LOG1("set80211param(IEEE80211_IOC_PRIVACY) failed\n");
    goto failed;
  }

  if (set80211param(dev, vap, IEEE80211_IOC_WPA, wpa) < 0)
  {
    FWIL_LOG1("set80211param(IEEE80211_IOC_WPA) failed\n");
    goto failed;
  }

  return 0;

failed:
  return -1;
}

static inline enum ieee80211_authmode
convert_auth_alg_to_authmode(u32 auth_algo)
{
  switch (auth_algo)
  {
    case SNCMF_AUTH_ALG_OPEN_SYSTEM:
      return IEEE80211_AUTH_OPEN;
    case SNCMF_AUTH_ALG_SHARED_KEY:
      return IEEE80211_AUTH_SHARED;
#ifdef CONFIG_SAE
    case SNCMF_AUTH_ALG_SAE:
      return IEEE80211_AUTH_SAE;
#endif
    case SNCMF_AUTH_ALG_AUTO:
    default:
      return IEEE80211_AUTH_AUTO;
  }
}

void fwil_init(void)
{
  /* Nothing to do. */
}

/* bsscfg */

static int fwil_bss_ssid(fwil_bss_handler_t *fbh)
{
  fwil_handler_t *fh = fbh->fvh->fh;
  struct sncmf_mbss_ssid_le mbss_ssid;
  struct device *dev = wlandev(0);
  struct ieee80211vap *vap;
  struct sncmf_if_event ievt;

  FWIL_ASSERT(dev);

  if (fh->set)
  {
    memcpy(&mbss_ssid, fh->buf + fh->off, sizeof(mbss_ssid));

    FWIL_LOG1("[%s]  bsscfgidx: %d, SSID_len: %d, SSID: %s\n",
      __func__, mbss_ssid.bsscfgidx, mbss_ssid.SSID_len,
      mbss_ssid.SSID);

    /* XXX: what to do with mbss_ssid.SSID ? */

    vap = wlan_create_vap(dev, mbss_ssid.bsscfgidx);

    FWIL_ASSERT(vap);

    if (set_mediaopt(dev, vap, IFM_OMASK, IFM_IEEE80211_HOSTAP) < 0)
    {
      FWIL_LOG1("set_mediaopt failed\n");
      goto failed;
    }

    ievt.ifidx = ievt.bsscfgidx = mbss_ssid.bsscfgidx;
    ievt.action = SNCMF_E_IF_ADD;
    ievt.role = SNCMF_E_IF_ROLE_AP;
    ievt.flags = 0;

    if (fweh_send_if_event(fh->ifn, &ievt, vap) < 0)
    {
      FWIL_LOG1("fweh_send_if_event failed\n");
      goto failed;
    }
  }

  return 0;

failed:
  wlan_remove_vap(dev, vap);
  return -1;
}

static int fwil_bss_interface_remove(fwil_bss_handler_t *fbh)
{
  fwil_handler_t *fh = fbh->fvh->fh;
  u32 bsscfgidx;
  struct device *dev = wlandev(0);
  struct ieee80211vap *vap;
  struct sncmf_if_event ievt;

  FWIL_ASSERT(dev);

  if (fh->set)
  {
    bsscfgidx = fbh->bsscfgidx;

    FWIL_LOG1("[%s] bsscfgidx: %d\n", __func__, bsscfgidx);

    vap = wlan_get_vap(dev, bsscfgidx);

    FWIL_ASSERT(vap);

    if (wlan_remove_vap(dev, vap) < 0) {
      goto failed;
    }

    ievt.ifidx = ievt.bsscfgidx = bsscfgidx;
    ievt.action = SNCMF_E_IF_DEL;
    ievt.role = SNCMF_E_IF_ROLE_AP;
    ievt.flags = 0;

    if (fweh_send_if_event(fh->ifn, &ievt, vap) < 0)
    {
      FWIL_LOG1("fweh_send_if_event failed\n");
      goto failed;
    }
  }

  return 0;

failed:
  return -1;
}

static int fwil_bss_mgmt_ie(fwil_bss_handler_t *fbh)
{
  fwil_handler_t *fh = fbh->fvh->fh;
  struct device *dev = wlandev(0);
  struct ieee80211vap *vap;
  u32 bsscfgidx;
  u8 subtype;
  u32 ie_len;
  u8 *ie;

  FWIL_ASSERT(dev);

  if (fh->set)
  {
    bsscfgidx = fbh->bsscfgidx;

    subtype = fh->buf[fh->off];
    memcpy(&ie_len, &fh->buf[fh->off + 1], sizeof(ie_len));
    ie = &fh->buf[fh->off + 5];

    FWIL_LOG1("[%s] bsscfgidx: %d, subtype: 0x%x\n",
                __func__, bsscfgidx, subtype);

    vap = wlan_get_vap(dev, bsscfgidx);

    FWIL_ASSERT(vap);

    if (set80211(dev, vap, IEEE80211_IOC_APPIE,
        (IEEE80211_FC0_TYPE_MGT | subtype), ie, ie_len) < 0)
    {
      FWIL_LOG1("set80211var failed\n");
      goto failed;
    }
  }

  return 0;

failed:
  return -1;
}

static int fwil_bss_auth_alg(fwil_bss_handler_t *fbh)
{
  fwil_handler_t *fh = fbh->fvh->fh;
  struct device *dev = wlandev(0);
  struct ieee80211vap *vap;
  u32 bsscfgidx;
  u32 auth_algo;
  enum ieee80211_authmode authmode;

  FWIL_ASSERT(dev);

  if (fh->set)
  {
    bsscfgidx = fbh->bsscfgidx;

    memcpy(&auth_algo, &fh->buf[fh->off], sizeof(auth_algo));

    FWIL_LOG1("[%s] bsscfgidx: %d, auth_algo: %d\n",
            __func__, bsscfgidx, auth_algo);

    vap = wlan_get_vap(dev, bsscfgidx);

    FWIL_ASSERT(vap);

    authmode = convert_auth_alg_to_authmode(auth_algo);

    if (set80211param(dev, vap, IEEE80211_IOC_AUTHMODE, authmode) < 0)
    {
      FWIL_LOG1("set80211var failed\n");
      goto failed;
    }
  }

  return 0;

failed:
  return -1;
}

static int fwil_bss_privacy(fwil_bss_handler_t *fbh)
{
  fwil_handler_t *fh = fbh->fvh->fh;
  struct device *dev = wlandev(0);
  struct ieee80211vap *vap;
  u32 bsscfgidx;
  s32 privacy;

  FWIL_ASSERT(dev);

  if (fh->set)
  {
    bsscfgidx = fbh->bsscfgidx;

    memcpy(&privacy, &fh->buf[fh->off], sizeof(privacy));

    FWIL_LOG1("[%s] bsscfgidx: %d, privacy: %d\n",
            __func__, bsscfgidx, privacy);

    vap = wlan_get_vap(dev, bsscfgidx);

    FWIL_ASSERT(vap);

    if (set80211param(dev, vap, IEEE80211_IOC_PRIVACY, privacy) < 0)
    {
      FWIL_LOG1("set80211var failed\n");
      goto failed;
    }
  }

  return 0;

failed:
  return -1;
}

static int fwil_bss_wpa(fwil_bss_handler_t *fbh)
{
  fwil_handler_t *fh = fbh->fvh->fh;
  struct device *dev = wlandev(0);
  struct ieee80211vap *vap;
  u32 bsscfgidx;
  s32 wpa;

  FWIL_ASSERT(dev);

  if (fh->set)
  {
    bsscfgidx = fbh->bsscfgidx;

    memcpy(&wpa, &fh->buf[fh->off], sizeof(wpa));

    FWIL_LOG1("[%s] bsscfgidx: %d, wpa: %d\n",
            __func__, bsscfgidx, wpa);

    vap = wlan_get_vap(dev, bsscfgidx);

    FWIL_ASSERT(vap);

    if (set80211param(dev, vap, IEEE80211_IOC_WPA, wpa) < 0)
    {
      FWIL_LOG1("set80211var failed\n");
      goto failed;
    }
  }

  return 0;

failed:
  return -1;
}

s32 g_wsec;
static int fwil_bss_wsec(fwil_bss_handler_t *fbh)
{
  fwil_handler_t *fh = fbh->fvh->fh;
  struct device *dev = wlandev(0);
  struct ieee80211vap *vap;
  u32 bsscfgidx;
  s32 wsec = 0;

  bsscfgidx = fbh->bsscfgidx;

  memcpy(&wsec, &fh->buf[fh->off], sizeof(wsec));

  FWIL_LOG1("[%s] bsscfgidx: %d, set: %d, gwsec:0x%x, wsec: 0x%x\n",
          __func__, bsscfgidx, fh->set, g_wsec, wsec);

  FWIL_ASSERT(dev);

  vap = wlan_get_vap(dev, bsscfgidx);

  FWIL_ASSERT(vap);

  if (!fh->set) {
    wsec = g_wsec;
    memcpy(fh->buf, &wsec, sizeof(wsec));
  }
  else {
    g_wsec = wsec;
  }

  return 0;
}

static inline int is_broadcast_ether_addr(const u8 *a)
{
  return (a[0] & a[1] & a[2] & a[3] & a[4] & a[5]) == 0xff;
}

#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#define	ETHER_SET_ZERO(a) memset(a,0,IEEE80211_ADDR_LEN)

static int wsec_del_key(struct device *dev, struct ieee80211vap *vap,
    struct sncmf_wsec_key *key)
{
  struct ieee80211req_del_key wk;

  memset(&wk, 0, sizeof(wk));
  if (ETHER_IS_ZERO(key->ea)) {
    FWIL_LOG1("%s: key_idx=%d", __func__, key->index);
    wk.idk_keyix = key->index;
  } else {
    FWIL_LOG1("%s: addr=" MACSTR, __func__, MAC2STR(key->ea));
    FWIL_LOG1("%s: key_idx=%d", __func__, IEEE80211_KEYIX_NONE);
    memcpy(wk.idk_macaddr, key->ea, IEEE80211_ADDR_LEN);
    wk.idk_keyix = (u_int8_t) IEEE80211_KEYIX_NONE;	/* XXX */
  }

  return set80211var(dev, vap, IEEE80211_IOC_DELKEY, &wk, sizeof(wk));
}

static int wsec_set_key(struct device *dev, struct ieee80211vap *vap,
    struct sncmf_wsec_key *key)
{

#define CRYPTO_ALGO_OFF           0
#define CRYPTO_ALGO_WEP           1
#define CRYPTO_ALGO_TKIP          2
#define CRYPTO_ALGO_AES_CCMP      3
#define CRYPTO_ALGO_AES_CCMP_256  4
#define CRYPTO_ALGO_AES_GCMP      5
#define CRYPTO_ALGO_AES_GCMP_256  6
#define CRYPTO_ALGO_AES_CMAC      7
#define CRYPTO_ALGO_NALG          8

  struct ieee80211req_key wk;

  FWIL_LOG1("%s: alg=%d addr=%p key_idx=%d key_len=%zu\n",
    __func__, key->algo, key->ea, key->index, key->len);

  if (key->algo == CRYPTO_ALGO_OFF) {
#ifndef HOSTAPD
    if (ETHER_IS_ZERO(key->ea) || is_broadcast_ether_addr(key->ea)) {
        ETHER_SET_ZERO(key->ea);
        return wsec_del_key(dev, vap, key);
      }
    else
#endif /* HOSTAPD */
      return wsec_del_key(dev, vap, key);
  }

  memset(&wk, 0, sizeof(wk));

  switch (key->algo) {
  case CRYPTO_ALGO_WEP:
    wk.ik_type = IEEE80211_CIPHER_WEP;
    break;
  case CRYPTO_ALGO_TKIP:
    wk.ik_type = IEEE80211_CIPHER_TKIP;
    break;
  case CRYPTO_ALGO_AES_CCMP:
    wk.ik_type = IEEE80211_CIPHER_AES_CCM;
    break;
  case CRYPTO_ALGO_AES_CCMP_256:
    wk.ik_type = IEEE80211_CIPHER_AES_CCM_256;
    break;
  case CRYPTO_ALGO_AES_GCMP:
    wk.ik_type = IEEE80211_CIPHER_AES_GCM;
    break;
  case CRYPTO_ALGO_AES_GCMP_256:
    wk.ik_type = IEEE80211_CIPHER_AES_GCM_256;
    break;
  case CRYPTO_ALGO_AES_CMAC:
    wk.ik_type = IEEE80211_CIPHER_AES_CMAC;
    break;
  default:
    FWIL_LOG1("%s: unknown alg=%d", __func__, key->algo);
    return -1;
  }

  wk.ik_flags = IEEE80211_KEY_RECV | IEEE80211_KEY_XMIT;

  if (key->ea == NULL) {
    memset(wk.ik_macaddr, 0xff, IEEE80211_ADDR_LEN);
    wk.ik_keyix = key->index;
  } else {
    memcpy(wk.ik_macaddr, key->ea, IEEE80211_ADDR_LEN);
    /*
     * Deduce whether group/global or unicast key by checking
     * the address (yech).  Note also that we can only mark global
     * keys default; doing this for a unicast key is an error.
     */
    if (is_broadcast_ether_addr(key->ea)) {
      wk.ik_flags |= IEEE80211_KEY_GROUP;
      wk.ik_keyix = key->index;
    } else {
      wk.ik_keyix = key->index == 0 ? IEEE80211_KEYIX_NONE :
        key->index;
    }
  }

  if (wk.ik_keyix != IEEE80211_KEYIX_NONE)
    wk.ik_flags |= IEEE80211_KEY_DEFAULT;

  wk.ik_keylen = key->len;

  memcpy(wk.ik_keydata, key->data, key->len);

#undef CRYPTO_ALGO_OFF
#undef CRYPTO_ALGO_WEP
#undef CRYPTO_ALGO_TKIP
#undef CRYPTO_ALGO_AES_CCMP
#undef CRYPTO_ALGO_AES_CCMP_256
#undef CRYPTO_ALGO_AES_GCMP
#undef CRYPTO_ALGO_AES_GCMP_256
#undef CRYPTO_ALGO_AES_CMAC
#undef CRYPTO_ALGO_NALG

  return set80211var(dev, vap, IEEE80211_IOC_WPAKEY, &wk, sizeof(wk));
}

static int fwil_bss_wsec_key(fwil_bss_handler_t *fbh)
{
  fwil_handler_t *fh = fbh->fvh->fh;
  struct sncmf_wsec_key key;
  struct device *dev = wlandev(0);
  struct ieee80211vap *vap;
  u32 bsscfgidx;

  FWIL_ASSERT(dev);

  if (fh->set) {
    bsscfgidx = fbh->bsscfgidx;

    memcpy(&key, &fh->buf[fh->off], sizeof(key));

    FWIL_LOG1("[%s] bsscfgidx: %d, key: %d\n",
            __func__, bsscfgidx, key.algo);

    vap = wlan_get_vap(dev, bsscfgidx);

    FWIL_ASSERT(vap);

    if (key.len > 0) {
      FWIL_LOG1("wsc_set_key\n");

      if (wsec_set_key(dev, vap, &key) < 0) {
        FWIL_LOG1("wsc_set_key failed\n");
        goto failed;
      }
    }
    else {
      FWIL_LOG1("wsc_del_key\n");

      if (wsec_del_key(dev, vap, &key) < 0) {
        FWIL_LOG1("wsc_del_key failed\n");
        goto failed;
      }
    }
  }

  return 0;

failed:
  return -1;
}

static int fwil_bss_drop_unencrypted(fwil_bss_handler_t *fbh)
{
  fwil_handler_t *fh = fbh->fvh->fh;
  struct device *dev = wlandev(0);
  struct ieee80211vap *vap;
  u32 bsscfgidx;
  s32 dropunencrypted;

  FWIL_ASSERT(dev);

  if (fh->set) {
    bsscfgidx = fbh->bsscfgidx;

    memcpy(&dropunencrypted, &fh->buf[fh->off], sizeof(dropunencrypted));

    FWIL_LOG1("[%s] bsscfgidx: %d, drop unencrypted: %d\n",
      __func__, bsscfgidx, dropunencrypted);

    vap = wlan_get_vap(dev, bsscfgidx);

    FWIL_ASSERT(vap);

    if (set80211param(dev, vap, IEEE80211_IOC_DROPUNENCRYPTED, dropunencrypted) < 0) {
      FWIL_LOG1("set80211param failed\n");
      goto failed;
    }
  }

  return 0;

failed:
  return -1;
}

static int fwil_bss_ltfgi_ctl(fwil_bss_handler_t *fbh)
{
  fwil_handler_t *fh = fbh->fvh->fh;
  struct device *dev = wlandev(0);
  struct ieee80211vap *vap;
  u32 bsscfgidx;
  s32 ltfgi;

  FWIL_ASSERT(dev);

  if (fh->set) {
    bsscfgidx = fbh->bsscfgidx;

    memcpy(&ltfgi, &fh->buf[fh->off], sizeof(ltfgi));

    FWIL_LOG1("[%s] bsscfgidx: %d, ltfgi: 0x%x\n",
            __func__, bsscfgidx, ltfgi);

    vap = wlan_get_vap(dev, bsscfgidx);

    FWIL_ASSERT(vap);

    if (set80211var(dev, vap, IEEE80211_IOC_HE_LTFGI, &ltfgi, sizeof(ltfgi)) < 0) {
      FWIL_LOG1("set80211param failed\n");
      goto failed;
    }
  }

  return 0;

failed:
  return -1;
}

static int fwil_bss_powersave_ctl(fwil_bss_handler_t *fbh)
{
  fwil_handler_t *fh = fbh->fvh->fh;
  struct device *dev = wlandev(0);
  struct ieee80211vap *vap;
  u32 bsscfgidx;
  s32 powersave;

  FWIL_ASSERT(dev);

  if (fh->set) {
    bsscfgidx = fbh->bsscfgidx;

    memcpy(&powersave, &fh->buf[fh->off], sizeof(powersave));

    FWIL_LOG1("[%s] bsscfgidx: %d, powersave: %d\n",
            __func__, bsscfgidx, powersave);

    vap = wlan_get_vap(dev, bsscfgidx);

    FWIL_ASSERT(vap);

    if (set80211param(dev, vap, IEEE80211_IOC_POWERSAVE, powersave) < 0) {
      FWIL_LOG1("set80211param failed\n");
      goto failed;
    }
  }

  return 0;

failed:
  return -1;
}

struct wpa_twt_params {
  u8	twt_wpa_action;			/* private definition */
  u8	twt_ndppading;				/*1 bits*/
  u8	twt_resp_pm_mode;			/*1 bits*/
  u8	twt_negotiation_type;		/*2 bits	0: individual, 1: broadcast*/
  u8	twt_setup; 				/* 0:request, 1:suggestion, 2:demand ...*/
  u8	twt_min_sp_dur;			/* 1 Octets Unit: 256us*/
  u8	twt_wake_int_exp;		/* 5 bits*/
  u8	twt_trigger;			/* 3 bits*/
  u8	twt_implicit;			/*1: wakeup periodic*/
  u8	twt_flowtype;			/*0: announce(ps-poll), 1: unannounce(null)*/
  u8	twt_prot;				/* 1 bits*/
  u8	twt_flow_id;			/* 3 bits*/
  u32	twt_wake_int_mantissa;	/* 2 Octets*/
  u8	twt_channel;			/* 1 Octets*/
}__packed;

static int fwil_bss_twt_ctl(fwil_bss_handler_t *fbh)
{
  fwil_handler_t *fh = fbh->fvh->fh;
  struct device *dev = wlandev(0);
  struct ieee80211vap *vap;
  u32 bsscfgidx;
  struct wpa_twt_params twt_params;

  FWIL_ASSERT(dev);

  if (fh->set) {
    bsscfgidx = fbh->bsscfgidx;

    memcpy(&twt_params, &fh->buf[fh->off], sizeof(struct wpa_twt_params));

    vap = wlan_get_vap(dev, bsscfgidx);

    FWIL_ASSERT(vap);

    FWIL_LOG1("[%s] bsscfgidx: %d, twt: %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
              __func__, bsscfgidx, twt_params.twt_wpa_action,
              twt_params.twt_ndppading,twt_params.twt_resp_pm_mode,
              twt_params.twt_negotiation_type,twt_params.twt_setup,
              twt_params.twt_min_sp_dur,twt_params.twt_wake_int_exp,
              twt_params.twt_trigger,twt_params.twt_implicit,
              twt_params.twt_flowtype,twt_params.twt_prot,
              twt_params.twt_flow_id,twt_params.twt_wake_int_mantissa,
              twt_params.twt_channel);

    if (set80211var(dev, vap, IEEE80211_IOC_TWT, &twt_params, sizeof(struct wpa_twt_params)) < 0) {
      FWIL_LOG1("set80211param failed\n");
      goto failed;
    }
  }

  return 0;

failed:
  return -1;
}

FWIL_BSS_HANDLER(ssid, fwil_bss_ssid);
FWIL_BSS_HANDLER(interface_remove, fwil_bss_interface_remove);
FWIL_BSS_HANDLER(mgmt_ie, fwil_bss_mgmt_ie);
FWIL_BSS_HANDLER(auth_alg, fwil_bss_auth_alg);
FWIL_BSS_HANDLER(privacy, fwil_bss_privacy);
FWIL_BSS_HANDLER(wpa, fwil_bss_wpa);
FWIL_BSS_HANDLER(wsec_key, fwil_bss_wsec_key);
FWIL_BSS_HANDLER(wsec, fwil_bss_wsec);
FWIL_BSS_HANDLER(drop_unencrypted, fwil_bss_drop_unencrypted);

FWIL_BSS_HANDLER(ltfgi, fwil_bss_ltfgi_ctl);
FWIL_BSS_HANDLER(powersave, fwil_bss_powersave_ctl);
FWIL_BSS_HANDLER(twt, fwil_bss_twt_ctl);

/* iovar */
static int fwil_var_fwmachdrlen(fwil_var_handler_t *fvh)
{
  fwil_handler_t *fh = fvh->fh;
  int fwmachdrlen = SNCMF_FW_MAC_HDR_LEN;

  if (!fh->set)
  {
    memcpy(fh->buf, &fwmachdrlen, sizeof(int));
  }

  return 0;
}

static int fwil_var_bsscfg(fwil_var_handler_t *fvh)
{
  fwil_handler_t *fh = fvh->fh;
  fwil_bss_handler_t *fbh;

  FWIL_LOG1("[%s] name:%s\n", __func__, (const char *)fh->buf);

  fbh = fwil_find_bss_handler(fvh, (char *)(fh->buf + 7));
  if (fbh)
  {
    if (fbh->cb)
    {
      return fbh->cb(fbh);
    }
  }
  else
  {
    FWIL_LOG1("%s: %d: Failed\r\n", __FUNCTION__, __LINE__);
  }

  return 0;
}

static int fwil_var_cur_etheraddr(fwil_var_handler_t *fvh)
{
  fwil_handler_t *fh = fvh->fh;
  struct device *dev = wlandev(0);
  struct ieee80211vap *vap;

  FWIL_ASSERT(dev);

  vap = wlan_get_vap(dev, fh->ifn);

  FWIL_ASSERT(vap);

  if (!fh->set)
  {
    memcpy(fh->buf, vap->iv_myaddr, IEEE80211_ADDR_LEN);
  }

  return 0;
}

static int fwil_var_event_msgs(fwil_var_handler_t *fvh)
{
  fwil_handler_t *fh = fvh->fh;
  if (fh->set)
  {
    fweh_set_event_mask(fh->ifn, fh->buf + fh->off);
  }

  return 0;
}

extern struct ieee80211_scan_freq_list g_scan_freq;
extern u32 freq_24GHz[STA_24G_SCAN_CH_MAX];

static void chanspec_to_freq(__le16         *ch_list, u16 n_channels)
{
  int i;
  u16 ch_idx;
  for (i = 0; i < n_channels; i++) {
    if(ch_list[i] & SNCMU_CHSPEC_D11N_BND_2G) {
      ch_idx = ch_list[i] & SNCMU_CHSPEC_CH_MASK;
      g_scan_freq.scan_freq[i] = freq_24GHz[ch_idx - 1];
    } else {
      /* not support 5G */
      assert(0);
    }
    g_scan_freq.number = n_channels;
  }

}

static int fwil_var_escan(fwil_var_handler_t *fvh)
{
  fwil_handler_t *fh = fvh->fh;
  struct device *dev = wlandev(0);
  struct ieee80211vap *vap;
  struct sncmf_escan_params_le *eparams_le;
  struct sncmf_scan_params_le *params_le;
  struct sncmf_ssid_le ssid_le;
  struct ieee80211_scan_req sr;
  u16 n_ssids, n_channels;
  int i;
  s16 offset;
  char *ptr;

  FWIL_ASSERT(dev);

  vap = wlan_get_vap(dev, fh->ifn);

  FWIL_ASSERT(vap);

  if (fh->set)
  {

  eparams_le = (struct sncmf_escan_params_le *)&fh->buf[fh->off];
  if (eparams_le->version != SNCMF_ESCAN_REQ_VERSION
      || eparams_le->sync_id != 0x1234)
  {
    FWIL_LOG1("invalid escan params\n");
    goto failed;
  }
  params_le = &eparams_le->params_le;

  FWIL_LOG1("[%s]  scan_type: 0x%x, flags: 0x%x\n",
          __func__, params_le->scan_type, params_le->flags);

  if (set_mediaopt(dev, vap, IFM_OMASK, 0 /* STA */) < 0)
  {
    FWIL_LOG1("set_mediaopt(IFM_OMASK) failed\n");
    goto failed;
  }

  /* XXX: why do we need this? */
  if (set_wpa_privacy(dev, vap, 3, 1) < 0)
  {
    FWIL_LOG1("set_wpa_privacy failed\n");
    goto failed;
  }

#ifdef IEEE80211_IOC_SCAN_MAX_SSID
  memset(&sr, 0, sizeof(sr));
  sr.sr_flags = IEEE80211_IOC_SCAN_ONCE | IEEE80211_IOC_SCAN_NOJOIN;
#if IEEE80211_IOC_SCAN_MAX_SSID > 1
  sr.sr_flags |= IEEE80211_IOC_SCAN_NOBCAST;
#endif
  if (params_le->scan_type != SNCMF_SCANTYPE_PASSIVE)
  {
    sr.sr_flags |= IEEE80211_IOC_SCAN_ACTIVE;
  }
  sr.sr_duration = IEEE80211_IOC_SCAN_FOREVER;
  n_ssids = (params_le->channel_num >> SNCMF_SCAN_PARAMS_NSSID_SHIFT) \
    & SNCMF_SCAN_PARAMS_COUNT_MASK;
  n_channels = params_le->channel_num & SNCMF_SCAN_PARAMS_COUNT_MASK;

  if (n_channels > 0) {
    chanspec_to_freq(params_le->channel_list, n_channels);
  }

   /* it's a scan request for specific channels so cannot cancel */
  if (n_channels < SCAN_CH_MAX) {
    vap->iv_flags_ext |= IEEE80211_FEXT_NON_CANCEL_SCAN;
  }

  if (n_ssids > 0)
  {
    sr.sr_nssid = n_ssids;
    if (sr.sr_nssid > IEEE80211_IOC_SCAN_MAX_SSID) {
      sr.sr_nssid = IEEE80211_IOC_SCAN_MAX_SSID;
    }

    /* NB: check scan cache first */
    sr.sr_flags |= IEEE80211_IOC_SCAN_CHECK;
    offset = offsetof(struct sncmf_scan_params_le, channel_list) \
              + n_channels * sizeof(u16);
    offset = roundup(offset, sizeof(u32));
    ptr = (char *)params_le + offset;

    for (i = 0; i < sr.sr_nssid; i++) {
      memcpy(&ssid_le, ptr, sizeof(ssid_le));
      sr.sr_ssid[i].len = ssid_le.SSID_len;
      memcpy(sr.sr_ssid[i].ssid, ssid_le.SSID, sr.sr_ssid[i].len);
      ptr += sizeof(ssid_le);
    }
  }

  if (params_le->flags & SNCMF_SCAN_FLAG_FLUSH)
  {
    sr.sr_flags &= ~IEEE80211_IOC_SCAN_CHECK;
    sr.sr_flags |= IEEE80211_IOC_SCAN_FLUSH;
  }

  if (params_le->scan_type == SNCMF_SCANTYPE_PASSIVE)
  {
    sr.sr_maxdwell = params_le->passive_time;
  }
  else
  {
    sr.sr_maxdwell = params_le->active_time;
  }

  if (set80211var(dev, vap, IEEE80211_IOC_SCAN_REQ, &sr, sizeof(sr)) < 0)
  {
    FWIL_LOG1("set80211var(IEEE80211_IOC_SCAN_REQ) failed\n");
    goto failed;
  }
#else
  if (set80211var(dev, vap, IEEE80211_IOC_SSID, params_le->ssid_le.SSID,
        params_le->ssid_le.SSID_len) < 0)
  {
    FWIL_LOG1("set80211var(IEEE80211_IOC_SSID) failed\n");
    goto failed;
  }
#endif
  }

  return 0;

failed:
  return -1;
}

static int fwil_var_action(fwil_var_handler_t *fvh)
{
  fwil_handler_t *fh = fvh->fh;
  struct device *dev = wlandev(0);
  struct ieee80211vap *vap;
  struct sncmf_fil_action_frame_le *action_frame;
  struct sncmf_fil_af_params_le *af_params;
  struct ieee80211req_send_action *act;
  u32 len;

  FWIL_ASSERT(dev);

  vap = wlan_get_vap(dev, fh->ifn);

  FWIL_ASSERT(vap);

  if (fh->set)
  {
    af_params = (struct sncmf_fil_af_params_le *)&fh->buf[fh->off];
    action_frame = &af_params->action_frame;

    len = action_frame->len;

    act = os_zalloc(sizeof(*act) + len);

    if (act == NULL)
      return -1;

    memset(act, 0, sizeof(*act));

    act->freq = af_params->freq;
    memcpy(act->dst_addr, action_frame->da, IEEE80211_ADDR_LEN);
    memcpy(act->src_addr, vap->iv_myaddr, IEEE80211_ADDR_LEN);
    memcpy(act->bssid, af_params->bssid, IEEE80211_ADDR_LEN);
    memcpy(act + 1, action_frame->data, len);

    FWIL_LOG1("%s: freq=%lu, wait=%u, dst=" MACSTR ", src="
     MACSTR ", bssid=" MACSTR,
     __func__, act->freq, af_params->dwell_time, MAC2STR(act->dst_addr),
     MAC2STR(act->src_addr), MAC2STR(act->bssid));

    len += sizeof(*act);

    if (set80211var(dev, vap, IEEE80211_IOC_SEND_ACTION, act, len) < 0)
    {
      FWIL_LOG1("set80211var(IEEE80211_IOC_SEND_ACTION) failed\n");
      goto failed;
    }
  }

  return 0;

failed:
  return -1;
}

static int fwil_freq_get(fwil_handler_t *fh)
{
  struct device *dev = wlandev(0);
  struct ieee80211vap *vap;
  u32 freq;

  FWIL_ASSERT(dev);

  vap = wlan_get_vap(dev, fh->ifn);

  FWIL_ASSERT(vap);

  if (!fh->set)
  {
    freq = vap->iv_ic->ic_curchan->ic_freq;
    memcpy(fh->buf, &freq, sizeof(freq));
  }

  return 0;
}

extern int g_hosted_reassoc;

static int fwil_var_mlme(fwil_var_handler_t *fvh)
{
  fwil_handler_t *fh = fvh->fh;
  struct device *dev = wlandev(0);
  struct ieee80211vap *vap;
  struct sncmf_fil_mlme_params_le *mlme_params_le;
  struct ieee80211req_mlme mlme;
  u8 null_bssid[ETHER_ADDR_LEN] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0};

  FWIL_ASSERT(dev);

  vap = wlan_get_vap(dev, fh->ifn);

  FWIL_ASSERT(vap);

  if (fh->set)
  {
    mlme_params_le = (struct sncmf_fil_mlme_params_le *)&fh->buf[fh->off];
    memset(&mlme, 0, sizeof(mlme));
    mlme.im_op = mlme_params_le->op;
    mlme.im_reason = mlme_params_le->reason;
    memcpy(mlme.im_macaddr, mlme_params_le->macaddr, IEEE80211_ADDR_LEN);
    g_hosted_reassoc = (memcmp(mlme_params_le->pre_bssid, null_bssid, ETHER_ADDR_LEN)) ? 1 : 0;

    FWIL_LOG1("[%s] im_op: 0x%x, im_reason:%d\n", __func__,
            mlme.im_op, mlme.im_reason);

    if (set80211var(dev, vap, IEEE80211_IOC_MLME, &mlme, sizeof(mlme)) < 0)
    {
      FWIL_LOG1("set80211var(IEEE80211_IOC_MLME) failed\n");
      goto failed;
    }
  }

  return 0;

failed:
  return -1;
}

static int fwil_var_mgt_txcb(fwil_var_handler_t *fvh)
{
  fwil_handler_t *fh = fvh->fh;
  if (fh->set)
  {
    FWIL_LOG1("[%s] txcb: %x.%x.%x.%x\n", __func__,
            fh->buf[fh->off], fh->buf[fh->off + 1],
            fh->buf[fh->off + 2], fh->buf[fh->off + 3]);

    fweh_set_mgt_txcb(fh->ifn, fh->buf + fh->off);
  }

  return 0;
}

static int fwil_var_roam_off(fwil_var_handler_t *fvh)
{
  fwil_handler_t *fh = fvh->fh;
  struct device *dev = wlandev(0);
  struct ieee80211vap *vap;
  u32 roamoff;

  FWIL_ASSERT(dev);

  vap = wlan_get_vap(dev, fh->ifn);

  FWIL_ASSERT(vap);

  if (fh->set)
  {
    memcpy(&roamoff, &fh->buf[fh->off], sizeof(roamoff));

    FWIL_LOG1("[%s] roamoff: %u\n", __func__, roamoff);

    if (set80211param(dev, vap, IEEE80211_IOC_ROAMING,
        roamoff ? IEEE80211_ROAMING_MANUAL : IEEE80211_ROAMING_AUTO) < 0)
    {
      FWIL_LOG1("set80211param(IEEE80211_IOC_ROAMING) failed\n");
      goto failed;
    }
  }

  return 0;

failed:
  return -1;
}

static int fwil_var_tlv(fwil_var_handler_t *fvh)
{
  fwil_handler_t *fh = fvh->fh;
  struct device *dev = wlandev(0);
  struct ieee80211vap *vap;
  u32 tlv;

  FWIL_ASSERT(dev);

  vap = wlan_get_vap(dev, fh->ifn);

  FWIL_ASSERT(vap);

  if (fh->set)
  {
    memcpy(&tlv, &fh->buf[fh->off], sizeof(tlv));
    fws_set_features(tlv);
  }
  return 0;
}

#ifdef CONFIG_SCDC_SUPPORT_SCM_CHANNEL

static int fwil_var_host_carrier_on(fwil_var_handler_t *fvh)
{
  fwil_handler_t *fh = fvh->fh;
  struct device *dev = wlandev(0);
  struct ieee80211vap *vap;
  u32 ready;


  FWIL_ASSERT(dev);

  vap = wlan_get_vap(dev, fh->ifn);

  FWIL_ASSERT(vap);

  if (fh->set)
  {
    memcpy(&ready, &fh->buf[fh->off], sizeof(ready));

    FWIL_LOG1("[%s] host net ready: %u\n", __func__, ready);

    wise_channel_host_carrier_on(ready);
  }

  return 0;

}

static int fwil_var_host_ready(fwil_var_handler_t *fvh)
{
  fwil_handler_t *fh = fvh->fh;
  struct device *dev = wlandev(0);
  struct ieee80211vap *vap;
  u32 ready;


  FWIL_ASSERT(dev);

  vap = wlan_get_vap(dev, fh->ifn);

  FWIL_ASSERT(vap);

  if (fh->set)
  {
    memcpy(&ready, &fh->buf[fh->off], sizeof(ready));

    FWIL_LOG1("[%s] host ready: %u\n", __func__, ready);

    wise_channel_host_en(ready);
  }



  return 0;

}
#endif


#ifdef CONFIG_SCDC_SUPPORT_LOCAL_NETIF
static int fwil_var_cur_ip(fwil_var_handler_t *fvh)
{
  fwil_handler_t      *fh  = fvh->fh;
  struct device       *dev = wlandev(0);
  struct ieee80211vap *vap;
  int                  ret = 0;
  ifconfig_handle_t   *h   = NULL;
  struct sockaddr      sa, nm;
  struct sockaddr_in  *addr;
  uint8_t             *pbuf;

  assert(dev);

  vap = wlan_get_vap(dev, fh->ifn);

  assert(vap);

  if (!fh->set) {
    FWIL_LOG3("fwil_var_cur_ip \n");

    h = ifconfig_open();
    if (h == NULL)
      return -1;

    pbuf = fh->buf;
    /* get & copy ip address */
    if (ifconfig_get_addr(h, "wlan0", &sa) < 0) {
      ret = -1;
      goto exit;
    }
    addr = (struct sockaddr_in *)&sa;
    memcpy(pbuf, &(addr->sin_addr.s_addr), 4);
    FWIL_LOG3("ip %x %x %x %x\n", pbuf[0], pbuf[1], pbuf[2], pbuf[3]);

    /* get & copy netmask */
    if (ifconfig_get_netmask(h, "wlan0", &nm) < 0) {
      ret = -1;
      goto exit;
    }
    addr = (struct sockaddr_in *)&nm;
    memcpy((uint8_t *)(pbuf + 4), &(addr->sin_addr.s_addr), 4);
    FWIL_LOG3("netmask %x %x %x %x\n", pbuf[4], pbuf[5], pbuf[6], pbuf[7]);
  }
exit:
  ifconfig_close(h);

  return ret;
}
#endif



FWIL_VAR_HANDLER(fwmachdrlen, fwil_var_fwmachdrlen);
FWIL_VAR_HANDLER(bsscfg, fwil_var_bsscfg);
FWIL_VAR_HANDLER(cur_etheraddr, fwil_var_cur_etheraddr);
FWIL_VAR_HANDLER(event_msgs, fwil_var_event_msgs);
FWIL_VAR_HANDLER(escan, fwil_var_escan);
FWIL_VAR_HANDLER(action, fwil_var_action);
FWIL_VAR_HANDLER(mlme, fwil_var_mlme);
FWIL_VAR_HANDLER(mgt_txcb, fwil_var_mgt_txcb);
FWIL_VAR_HANDLER(roam_off, fwil_var_roam_off);
FWIL_VAR_HANDLER(tlv, fwil_var_tlv);
#ifdef CONFIG_SCDC_SUPPORT_LOCAL_NETIF
FWIL_VAR_HANDLER(cur_ip, fwil_var_cur_ip);
#endif
#ifdef CONFIG_SCDC_SUPPORT_SCM_CHANNEL
FWIL_VAR_HANDLER(host_ready, fwil_var_host_ready);
FWIL_VAR_HANDLER(host_carrier_on, fwil_var_host_carrier_on);
#endif
/* Etc. */

static int fwil_iovar_data_getset(fwil_handler_t *fh)
{
  fwil_var_handler_t *fvh;

  fvh = fwil_find_var_handler(fh, (char *)fh->buf);
  if (fvh)
  {
    if (fvh->cb)
    {
      return fvh->cb(fvh);
    }
  }
  else
  {
    FWIL_LOG1("%s: %d: Failed\r\n", __FUNCTION__, __LINE__);
  }
  return 0;
}

static int fwil_revinfo_get(fwil_handler_t *fh)
{
  struct sncmf_rev_info revinfo = {0,};

  sncmf_get_revinfo(&revinfo);

  if (!fh->set)
  {
    memcpy(fh->buf, &revinfo, sizeof(revinfo));
  }

  return 0;
}

static int fwil_version_get(fwil_handler_t *fh)
{
  s32 io_type;

  if (!fh->set)
  {
    io_type = SNCMU_D11N_IOTYPE;
    memcpy(fh->buf, &io_type, sizeof(io_type));
  }

  return 0;
}

static int fwil_ctrl_iface(struct device *dev, struct ieee80211vap *vap,
		bool enable)
{
  struct ifnet *ifp = vap->iv_ifp;

  FWIL_LOG1("[%s] enable: %u\n", __func__, enable);

  if (enable) {
    if (ifp->if_flags & IFF_UP)
      return 0;
    ifp->if_flags |= IFF_UP;
  } else {
    if (!(ifp->if_flags & IFF_UP))
      return 0;
    ifp->if_flags &= ~IFF_UP;
  }

  return wlan_ctl_vap(dev, vap, SIOCSIFFLAGS, (caddr_t)NULL);
}

static int fwil_updown(fwil_handler_t *fh)
{
  struct device *dev = wlandev(0);
  struct ieee80211vap *vap;

  FWIL_ASSERT(dev);

  if (fh->set)
  {
    vap = wlan_get_vap(dev, fh->ifn);

    FWIL_ASSERT(vap);

    FWIL_LOG1("[%s] %s\n", __func__,
          (fh->cmd == SNCMF_C_UP ? "up" : "down"));

    fwil_ctrl_iface(dev, vap, fh->cmd == SNCMF_C_UP ? true : false);

    if(fh->cmd == SNCMF_C_UP)
    {
      fws_enable_credit_mgmt();
    }
  }

  return 0;
}

static int fwil_disassoc(fwil_handler_t *fh)
{
  struct device *dev = wlandev(0);
  struct ieee80211vap *vap;
  u16 reason;

  FWIL_ASSERT(dev);

  if (fh->set)
  {
    memcpy(&reason, fh->buf, sizeof(reason));

    vap = wlan_get_vap(dev, fh->ifn);

    FWIL_ASSERT(vap);

    FWIL_LOG1("[%s] reason:%u\n", __func__, reason);

    send_mlme_param(dev, vap, IEEE80211_MLME_DEAUTH, reason, NULL);
  }

  return 0;
}

static int fwil_scan(fwil_handler_t *fh)
{
  struct device *dev = wlandev(0);
  struct ieee80211vap *vap;
  struct sncmf_scan_params_le *params_le;

  FWIL_ASSERT(dev);

  if (fh->set)
  {
    vap = wlan_get_vap(dev, fh->ifn);

    FWIL_ASSERT(vap);

    params_le = (struct sncmf_scan_params_le *)&fh->buf[fh->off];
    if (params_le->channel_num == 1 && params_le->channel_list[0] == -1)
    {
      FWIL_LOG1("[%s] abort scanning\n", __func__);

      /* Abort on-going scan. */
      ieee80211_cancel_scan(vap);
    }
  }

  return 0;
}

static int fwil_terminated(fwil_handler_t *fh)
{
  __maybe_unused struct device *dev = wlandev(0);
  u8 all_zero[SNCMF_EVENTING_MASK_LEN];
  __maybe_unused struct ieee80211vap *vap;
  __maybe_unused int i;

  FWIL_ASSERT(dev);

  if (fh->set)
  {
    FWIL_LOG1("[%s]\n", __func__);

    /* Disable further fweh events not to confuse the host. */
    memset(all_zero, 0, SNCMF_EVENTING_MASK_LEN);
    fweh_set_event_mask(fh->ifn, all_zero);

#ifdef CONFIG_SCDC_SUPPORT_SCM_CHANNEL
    wise_channel_host_carrier_on(0);
    wise_channel_host_en(0);
#endif
#ifdef CONFIG_SDIO
    struct device *sdiodev = device_get_by_name("sdio");
    sdio_stop(sdiodev);
#endif
#ifdef CONFIG_IEEE80211_HOSTED
    /* Initialize and remove other vaps first. */
    for (i = wlan_num_max_vaps(dev) - 1; i > -1 ; i--)
    {
        vap = wlan_get_vap(dev, i);
      if (!vap)
      {
        continue;
      }
      fwil_ctrl_iface(dev, vap, false);
      if (i != fh->ifn)
      {
          wlan_remove_vap(dev, vap);
      }
    }
#endif
  }

  return 0;
}

#ifdef CONFIG_AT
static int fwil_at_cmd(fwil_handler_t *fh)
{
  char   cmd_buf[256] = {0};
  char  *tmp;
  int    argc;
  AT_CAT type;
  int    resp_result = 0;
  char  *argv[AT_MAX_ARGV];

  memcpy(cmd_buf, fh->buf, fh->len);

  /* fh->buf overlapped, ERROR or Fault is happened, so use print_cmd */
  tmp           = &cmd_buf[0];
  argc        = at_parse_line(&tmp, argv, &type);
  resp_result = at_process_cmd(argc, argv, type);

  return resp_result;
}
#else
static int fwil_at_cmd(fwil_handler_t *fh)
{

  printk("AT Cmd should be enabled\n");

  return 0;
}

#endif

#ifdef CONFIG_SCDC_SUPPORT_SCM_CHANNEL
int (* fwil_scm_channel_cb) (char *buf, int len) = NULL;

static int fwil_scm_channel_msg(fwil_handler_t *fh)
{
  int    resp_result = -1;

  if (!fwil_scm_channel_cb) {
    FWIL_LOG1("cb not register \n");
    goto exit;
  }

  resp_result = fwil_scm_channel_cb((char *) fh->buf, fh->len);

exit:

  return resp_result;
}
#endif

FWIL_HANDLER(get_var, SNCMF_C_GET_VAR, fwil_iovar_data_getset);
FWIL_HANDLER(set_var, SNCMF_C_SET_VAR, fwil_iovar_data_getset);
FWIL_HANDLER(get_revinfo, SNCMF_C_GET_REVINFO, fwil_revinfo_get);
FWIL_HANDLER(get_version, SNCMF_C_GET_VERSION, fwil_version_get);
FWIL_HANDLER(up, SNCMF_C_UP, fwil_updown);
FWIL_HANDLER(down, SNCMF_C_DOWN, fwil_updown);
FWIL_HANDLER(disassoc, SNCMF_C_DISASSOC, fwil_disassoc);
FWIL_HANDLER(scan, SNCMF_C_SCAN, fwil_scan);
FWIL_HANDLER(terminated, SNCMF_C_TERMINATED, fwil_terminated);
FWIL_HANDLER(at_cmd, SNCMF_C_AT_CMD, fwil_at_cmd);
FWIL_HANDLER(get_freq, SNCMF_C_GET_CHANNEL, fwil_freq_get);

#ifdef CONFIG_SCDC_SUPPORT_SCM_CHANNEL
FWIL_HANDLER(scm_channel, SNCMF_C_CHANNEL_MSG, fwil_scm_channel_msg);
#endif
