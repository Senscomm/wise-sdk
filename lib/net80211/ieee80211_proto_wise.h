/*
 * Copyright 2018-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __IEEE80211_PROTO_WISE_H__
#define __IEEE80211_PROTO_WISE_H__

#include <hal/bitops.h>
#include <freebsd/byteorder.h>

#define is_flag_set(x, f) (((x) & (f)) == (f))
#ifndef SM
#define SM(_v, _f) (((_v) << _f##_S) & _f)
#endif
#ifndef MS
#define MS(_v, _f) (((_v) & _f) >> _f##_S)
#endif
#define OPMODE(x) SM(x, IEEE80211_HTINFO_OPMODE)


extern uint32_t (*ieee80211_txtime)(struct ieee80211_node *ni, uint32_t len,
		uint16_t rate, uint8_t gi_type, uint8_t ltf_type,
		uint8_t nominal_packet_padding);

extern uint32_t (*ieee80211_compute_ack_duration)(struct ieee80211_node *ni,
		uint16_t rate, bool isba);

extern uint32_t (*ieee80211_need_prot)(struct ieee80211_node *ni,
		struct ieee80211_frame *mh);

extern bool (*ieee80211_use_short_preamble)(struct ieee80211_node *ni);

uint32_t ieee80211_sifs(struct ieee80211_channel *chan);


#ifndef CONFIG_LINK_TO_ROM

static inline
void ieee80211_set_duration(struct ieee80211_frame *mh, uint16_t dur)
{
	/*put_unaligned_le16(dur, mh->i_dur);*/
	le16enc(mh->i_dur, dur);
}

static inline
uint16_t ieee80211_get_duration(struct ieee80211_frame *mh)
{
	/*return  get_unaligned_le16(mh->i_dur);*/
	return le16dec(mh->i_dur);
}

#else
extern void ieee80211_set_duration(struct ieee80211_frame *mh, uint16_t dur);
extern uint16_t ieee80211_get_duration(struct ieee80211_frame *mh);
#endif


static inline
void ieee80211_add_duration(struct ieee80211_frame *mh, int16_t delta)
{
	uint16_t old = ieee80211_get_duration(mh);

	ieee80211_set_duration(mh, old + delta);
}

static
inline int ieee80211_aSignalExtension(struct ieee80211_channel *c)
{
	return IEEE80211_IS_CHAN_2GHZ(c) ? 6 : 0;
}

static inline
bool ieee80211_channel_2ghz(struct ieee80211_channel *chan)
{
	if (chan == IEEE80211_CHAN_ANYC)
		return true;

	return IEEE80211_IS_CHAN_2GHZ(chan) ?  true : false;
}


static inline
bool ieee80211_use_protection(struct ieee80211_node *ni)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = vap->iv_ic;

	if (vap->iv_opmode == IEEE80211_M_STA)
		return is_flag_set(ni->ni_erp, IEEE80211_ERP_USE_PROTECTION);
	else if (vap->iv_opmode == IEEE80211_M_HOSTAP)
		return is_flag_set(ic->ic_flags, IEEE80211_F_USEPROT);
	else
		/* FIXME */

	return false;
}

static inline
bool ieee80211_use_short_slottime(struct ieee80211vap *vap)
{
	struct ieee80211_node *ni = vap->iv_bss;
#ifdef CONFIG_WLAN_HW_SCM2010
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211vap *vap_reference = NULL;
	struct ieee80211vap *vap_updated = NULL;
	struct ieee80211vap *vap_next = NULL;

    TAILQ_FOREACH(vap_next, &ic->ic_vaps, iv_next) {
		if (vap_next != vap) {
			if ((vap_next->iv_opmode == IEEE80211_M_HOSTAP) && (vap_next->iv_state >= IEEE80211_S_RUN)) {
				vap_updated = vap_next;
				vap_reference = vap;
			}
			else if ((vap_next->iv_opmode == IEEE80211_M_STA) && (vap_next->iv_state >= IEEE80211_S_RUN)) {
				vap_reference = vap_next;
				vap_updated = vap;
			}
		  break;
		}
    }

	/*
	 * Scm2010 hw only support one option of slot time
	 * Not allow both vap be same opmode
	 */
	if ((vap_updated && vap_reference)
		&& (vap_updated->iv_opmode == vap_reference->iv_opmode))
			return false;

	/*
	 * STA has linkuped and SoftAP needs to follow the STA's slot time
	 * so we need to update iv_flags which will carry ie in beacon/probe response
	 */
	if (vap_updated && vap_reference) {
		vap_updated->iv_flags &= ~IEEE80211_CAPINFO_SHORT_SLOTTIME;
		vap_updated->iv_flags |= (vap_reference->iv_bss->ni_capinfo & IEEE80211_CAPINFO_SHORT_SLOTTIME);
	}
#endif
	if (IEEE80211_IS_CHAN_5GHZ(ni->ni_chan))
		return true;
	if (vap->iv_opmode == IEEE80211_M_STA)
        return (ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_SLOTTIME);
	else if (vap->iv_opmode == IEEE80211_M_HOSTAP)
		return (vap->iv_flags & IEEE80211_F_SHSLOT);

	return false;
}

static inline
bool ieee80211_use_ht_sgi(struct ieee80211_node *ni)
{
	uint32_t sgi_flag = 0;

	if (ni->ni_chw == 20)
		sgi_flag = IEEE80211_NODE_SGI20;
	else
		sgi_flag = IEEE80211_NODE_SGI40;

	return ni->ni_flags & sgi_flag;
}

#ifdef CONFIG_SUPPORT_VHT
static inline
bool ieee80211_use_vht_sgi(struct ieee80211_node *ni)
{
	uint32_t sgi_flag, f_table[] = {IEEE80211_NODE_SGI20, IEEE80211_NODE_SGI40, IEEE80211_NODE_SGI80, IEEE80211_NODE_SGI160};

	sgi_flag = f_table[ni->ni_txbwi];

	return ni->ni_flags & sgi_flag;
}
#endif

#ifdef CONFIG_SUPPORT_HE
static inline
bool ieee80211_use_he_sgi(struct ieee80211_node *ni)
{
	/* set for max performance */
	return ni->ni_flags & IEEE80211_NODE_SU_1X_08_GI;
}
#endif

/**
 * ieee80211_compute_prot_rate() - calculate protection frame (RTS) rate
 *
 * @ni: peer node
 * @mh: 802.11 frame to protect
 * @rate: rate with which to send @mh
 *
 * Returns: dot11 rate of protection frame, or
 *          if protection is not necessary.
 *
 */
static inline
uint16_t ieee80211_compute_prot_rate(struct ieee80211_node *ni,
		struct ieee80211_frame *mh, int rate, u32 pt_to_protect)
{
	struct ieee80211com *ic = ni->ni_ic;
	const struct ieee80211_rate_table *rt = ic->ic_rt;
	enum ieee80211_phytype pt;
	int rix, cix;

#if 0
	if (IEEE80211_IS_MULTICAST(mh->i_addr1))
		return 0;

	/*
	 * In case associated to 11b AP, both prot_erp and prot_ht will be 0,
	 * and protection is not applied. Similarly, for 11g AP, prot_ht will
	 * be 0, and only prot_erp takes effect. These are correct behaviors.
	 */
	if (vap->iv_opmode == IEEE80211_M_STA) {
		prot_erp |= is_flag_set(ni->ni_erp, IEEE80211_ERP_USE_PROTECTION);
		prot_ht |= OPMODE(ni->ni_htopmode) != IEEE80211_HTINFO_OPMODE_PURE;

	} else if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
		prot_erp |= is_flag_set(ic->ic_flags, IEEE80211_F_USEPROT);
		prot_ht |= OPMODE(ic->ic_htprotmode) != IEEE80211_HTINFO_OPMODE_PURE;

	} else {
		/* FIXME */
	}

	/*
	 * prot_erp prot_ht PHY type   RTS rate
	 *
	 * 0        0       X            -
	 * 0        1       HT           OFDM
	 * 1        0       OFDM/HT      CCK
	 * 1        1       OFDM/HT      CCK
	 *
	 * In the presence of non-HT STAs, either member or not,, we protect
	 * atomic frame exchange sequence involving HT-mixed PPDUs with legacy
	 * OFDM RTS/CTS.
	 */
	if (prot_erp)
		pt_to_protect |= BIT(IEEE80211_T_OFDM) | BIT(IEEE80211_T_HT);
	if (prot_ht)
		pt_to_protect |= BIT(IEEE80211_T_HT);
#ifndef __WISE__
	/*
	 * The following has been done in FreeBSD net80211, but seems not make sense.
	 * Why should all VHT PPDUs be protected?
	 */
#ifdef CONFIG_SUPPORT_VHT
	pt_to_protect |= BIT(IEEE80211_T_VHT);
#endif
#endif
#endif

	pt = ieee80211_rate2phytype(rt, rate);
	if (!isset(&pt_to_protect, pt))
		return 0; /* no need to protect */

	rix = rt->rateCodeToIndex[rate];
	cix = rt->info[rix].ctlRateIndex;
	while (cix >= 0 &&
	       rt->info[cix].rateKbps > rt->info[rix].rateKbps &&
	       isset(&pt_to_protect, rt->info[cix].phy))
		cix--;

	return rt->info[cix].dot11Rate;
}

/**
 * ieee80211_fixup_duration() - fixes up the duration
 * @ni: peer receiver
 * @rate: transmission rate
 * @duratiion: returned value of ieee80211_compute_duration()
 *
 * Returns: corrected duration
 *
 * net80211 ieee80211_compute_duration() uses wrong SIFS time
 * for OFDM at 2.4GHz (11g). This function fixes it up.
 */
static __maybe_unused
uint32_t ieee80211_fixup_duration(struct ieee80211_node *ni,
		uint16_t rate, uint32_t duration)
{
	struct ieee80211com *ic = ni->ni_ic;

	if (ieee80211_rate2phytype(ic->ic_rt, rate) == IEEE80211_T_OFDM) {
		duration -= IEEE80211_DUR_OFDM_SIFS;
		duration += ieee80211_sifs(ni->ni_ic->ic_curchan);
	}

	return duration;
}

static inline
uint32_t ieee80211_peer_ampdu_max_size_ht(struct ieee80211_node *ni)
{
	uint8_t exp;
	/*
	 * HT Capabilities::A-MPDU Parameters
	 *	 ::Maximum A-MPDU Length Exponent (0-3)
	 */
	if (ni->ni_flags & IEEE80211_NODE_HT) {
		exp = MS(ni->ni_htparam, IEEE80211_HTCAP_MAXRXAMPDU);
		return (1 << (13 + exp)) - 1;
	}

	return 0;
}

#ifdef CONFIG_SUPPORT_VHT
static inline
uint32_t ieee80211_peer_ampdu_max_size_vht(struct ieee80211_node *ni)
{
	uint8_t exp;
	/*
	 * VHT Capabilities Info[23:25] (0-7)
	 */
	if (ni->ni_flags & IEEE80211_NODE_VHT) {
		exp = MS(ni->ni_vhtcap, IEEE80211_VHTCAP_MAX_A_MPDU_LENGTH_EXPONENT_MASK);
		return (1 << (13 + exp)) - 1;
	}

	return 0;
}
#endif

#ifdef CONFIG_SUPPORT_HE
static inline
uint32_t ieee80211_peer_ampdu_max_size_he(struct ieee80211_node *ni)
{
	uint32_t len = 0;
	uint8_t ext = MS(ni->ni_hemaccap, IEEE80211_HEMACCAP_MAX_AMPDU_LEN_EXP_MASK);

	/* P802.11ax_D7.0 26.6.1 */
	if (ext == 0) {
#ifdef CONFIG_SUPPORT_VHT
		len = ieee80211_peer_ampdu_max_size_vht(ni);
#endif
		if (len == 0) {
			len = ieee80211_peer_ampdu_max_size_ht(ni);
		}
#ifdef CONFIG_SUPPORT_VHT
	} else if (ni->ni_flags & IEEE80211_NODE_VHT) {
		len = min((1 << (20 + ext)) - 1, 6500631);
#endif
	} else if (ni->ni_flags & IEEE80211_NODE_HT) {
		len = (1 << (16 + ext)) - 1;
	} else {
		printk("HE STA without both VHT and HT capabilities\n");
	}

	return len;
}
#endif

static inline
uint32_t ieee80211_peer_ampdu_max_size(struct ieee80211_node *ni, uint16_t rate)
{
#ifdef CONFIG_SUPPORT_HE
	if (IEEE80211_IS_HE_RATE(rate)) {
		return ieee80211_peer_ampdu_max_size_he(ni);

	} else
#endif
#ifdef CONFIG_SUPPORT_VHT
	if (IEEE80211_IS_VHT_RATE(rate)) {
		return ieee80211_peer_ampdu_max_size_vht(ni);
	} else
#endif
	if (IEEE80211_IS_HT_RATE(rate)) {
		return ieee80211_peer_ampdu_max_size_ht(ni);
	}

	return 0;
}

static inline
uint32_t ieee80211_peer_ampdu_mmss(struct ieee80211_node *ni, uint16_t rate,
		uint8_t gi_type, uint8_t ltf_type, uint8_t nominal_packet_padding)
{
	int nstream, duration, rateMbps = 0, mmss, min_mpdu_bits;
	bool cbw40 = ni->ni_chw == 40;

	if (!IEEE80211_IS_HT_RATE(rate))
		return 0;

#ifdef CONFIG_SUPPORT_HE
	if (IEEE80211_IS_HE_RATE(rate)) {
		duration = ieee80211_compute_duration_he(125000, rate, ni,
				gi_type, ltf_type, nominal_packet_padding);
	} else
#endif
#ifdef CONFIG_SUPPORT_VHT
	if (IEEE80211_IS_VHT_RATE(rate)) {
		duration = ieee80211_compute_duration_vht(125000, rate, ni,
				gi_type);
	} else
#endif
	{
		nstream = IEEE80211_HT_RC_2_STREAMS(rate);
#if defined(__WISE__) && defined(CONFIG_SUPPORT_HE)
		duration = ieee80211_compute_duration_ht(125000, rate, ni,
				gi_type, nstream, cbw40);
#else
		duration = ieee80211_compute_duration_ht(125000, rate, nstream,
				cbw40, gi_type);
#endif
	}
	if (duration)
		rateMbps = 125000 / duration;

	mmss = MS(ni->ni_htparam, IEEE80211_HTCAP_MPDUDENSITY);
	if (mmss == 0)
		return 0;

	min_mpdu_bits = rateMbps * (1 << (mmss - 1)) / 4; /* in bits */
	min_mpdu_bits = roundup(min_mpdu_bits, 32); /* in multiple of 32-bits */

	return min_mpdu_bits / 8; /* in byte */
}

#if 0
/* Wrong implementation because m->m_pkthdr.len is not the MDPU length */
static inline
int ieee80211_ampdu_pad_size(struct mbuf *m, int mmss)
{
	int pad, len;

	len = IEEE80211_AMPDU_DELIMITER_LEN + m->m_pkthdr.len;
	pad = (4 - (len & 3)) & 3; /* for 4-byte alignment */
	if (mmss && (len + pad < mmss)) /* to honor receiver mmss */
		pad += mmss - (len + pad);

	return pad;
}
#endif

#endif
