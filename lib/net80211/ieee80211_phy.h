/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2007-2008 Sam Leffler, Errno Consulting
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _NET80211_IEEE80211_PHY_H_
#define _NET80211_IEEE80211_PHY_H_

#ifdef _KERNEL
/*
 * IEEE 802.11 PHY-related definitions.
 */

/*
 * Contention window (slots).
 */
#define IEEE80211_CW_MAX	1023	/* aCWmax */
#define IEEE80211_CW_MIN_0	31	/* DS/CCK aCWmin, ERP aCWmin(0) */
#define IEEE80211_CW_MIN_1	15	/* OFDM aCWmin, ERP aCWmin(1) */

/*
 * SIFS (microseconds).
 */
#define IEEE80211_DUR_SIFS	10	/* DS/CCK/ERP SIFS */
#define IEEE80211_DUR_OFDM_SIFS	16	/* OFDM SIFS */

/*
 * Slot time (microseconds).
 */
#define IEEE80211_DUR_SLOT	20	/* DS/CCK slottime, ERP long slottime */
#define IEEE80211_DUR_SHSLOT	9	/* ERP short slottime */
#define IEEE80211_DUR_OFDM_SLOT	9	/* OFDM slottime */

#define IEEE80211_GET_SLOTTIME(ic) \
	((ic->ic_flags & IEEE80211_F_SHSLOT) ? \
	    IEEE80211_DUR_SHSLOT : IEEE80211_DUR_SLOT)

/*
 * DIFS (microseconds).
 */
#define IEEE80211_DUR_DIFS(sifs, slot)	((sifs) + 2 * (slot))

struct ieee80211_channel;

#if defined(__WISE__) && (!defined(CONFIG_IEEE80211_MODE_VHT_5GHZ) && !defined(CONFIG_IEEE80211_MODE_HE_2GHZ))
/*
 * NB: IEEE80211_RATE_TABLE_SIZE is originally 128, but
 * the ieee80211_xxx_tables have at most 36 initialized entries.
 * They are not filled in with full MCS rates (up to 76), but
 * only partially filled up to MCS 23. I don't know why.
 *
 * Just decided to set IEEE80211_RATE_TABLE_SIZE to 36 to
 * accommodate the existing ieee80211_xxx_tables, since it
 * does not look like we are utilizaing more entries to it.
 *
 * The reason that rateCodeToIndex[] has 256 entries is because
 * that array is index with dot11Rate, whose MSB indicates if the
 * 7-LSBs corresponds to rate (in unit of 500Kbps) or MCS. That
 * being said, the lower 128 entries are for legacy rates, whereas
 * the upper 128 entries are for HT rates. What a waste of memory!
 */
#define	IEEE80211_RATE_TABLE_SIZE	36
#else
#define	IEEE80211_RATE_TABLE_SIZE	128
#endif

/*
 * __WISE__
 *
 * info[256]
 * [|0xFF...0xC0| |0xBF...0xA0| |0x9F...0x80| |0x7F...0x0|]
 * [|  HE(64)   | |  VHT(32)  | |   HT(32)  | | NHT(128) |]
 *
 */
struct ieee80211_rate_table {
	int		rateCount;		/* NB: for proper padding */
#ifdef __WISE__
	uint8_t		rateCodeToIndex[128 + IEEE80211_RATE_TABLE_SIZE];	/* back mapping */
#else
	uint8_t		rateCodeToIndex[256];	/* back mapping */
#endif
	struct {
		uint8_t		phy;		/* CCK/OFDM/TURBO */
		/* rateKbps for HT/VHT/HE can be meaningful only
		 * when compared with Non-HT reference rate because
		 * there is no need to consider NSS, CBW, and GI in
		 * such occasions. (10.6.11)
		 * Therefore we can select this value from the following
		 * tables per each corresponding PHY with 0.8 us GI.
		 * (HT)  Table 19.27 (19.5 Parameters for HT MCSs)
		 * (VHT) Table 21-30 (21.5 Parameters for VHT-MCSs)
		 * (HE)  Table 27-80 (27.5.4 HE-MCSs for 242-tone RU)
		 */
		uint32_t	rateKbps;	/* transfer rate in kbs */
		uint8_t		shortPreamble;	/* mask for enabling short
						 * preamble in CCK rate code */
		uint8_t		dot11Rate;	/* value for supported rates
						 * info element of MLME */
		uint8_t		ctlRateIndex;	/* index of next lower basic
						 * rate; used for dur. calcs */
#ifndef __WISE__
		uint16_t	lpAckDuration;	/* long preamble ACK dur. */
		uint16_t	spAckDuration;	/* short preamble ACK dur. */
#endif
	} info[IEEE80211_RATE_TABLE_SIZE];
};

const struct ieee80211_rate_table *_ieee80211_get_ratetable(
		struct ieee80211_channel *);
extern const struct ieee80211_rate_table * (*ieee80211_get_ratetable) (struct ieee80211_channel * c);

static __inline__ uint8_t
ieee80211_ack_rate(const struct ieee80211_rate_table *rt, uint8_t rate)
{
#ifdef __fully_updated__
	uint8_t cix;
	/*
	 * XXX Assert this is for a legacy rate; not for an MCS rate.
	 * If the caller wishes to use it for a basic rate, they should
	 * clear the high bit first.
	 */
	KASSERT(! (rate & 0x80), ("[%s] rate %d is basic/mcs?\n", __func__, rate));

	cix = rt->info[rt->rateCodeToIndex[rate & IEEE80211_RATE_VAL]].ctlRateIndex;
#else
	uint8_t cix = rt->info[rt->rateCodeToIndex[rate]].ctlRateIndex;
#endif
	KASSERT(cix != (uint8_t)-1, ("[%s] rate %d has no info\n", __func__, rate));
	return rt->info[cix].dot11Rate;
}

static __inline__ uint8_t
ieee80211_ctl_rate(const struct ieee80211_rate_table *rt, uint8_t rate)
{
#ifdef __fully_updated__
	uint8_t cix;
	/*
	 * XXX Assert this is for a legacy rate; not for an MCS rate.
	 * If the caller wishes to use it for a basic rate, they should
	 * clear the high bit first.
	 */
	KASSERT(! (rate & 0x80), ("[%s] rate %d is basic/mcs\n", __func__, rate));

	cix = rt->info[rt->rateCodeToIndex[rate & IEEE80211_RATE_VAL]].ctlRateIndex;
#else
	uint8_t cix = rt->info[rt->rateCodeToIndex[rate]].ctlRateIndex;
#endif
	KASSERT(cix != (uint8_t)-1, ("[%s] rate %d has no info\n", __func__, rate));
	return rt->info[cix].dot11Rate;
}

static __inline__ enum ieee80211_phytype
ieee80211_rate2phytype(const struct ieee80211_rate_table *rt, uint8_t rate)
{
#ifdef __fully_updated__
	uint8_t rix;
	/*
	 * XXX Assert this is for a legacy rate; not for an MCS rate.
	 * If the caller wishes to use it for a basic rate, they should
	 * clear the high bit first.
	 */
	KASSERT(! (rate & 0x80), ("[%s] rate %d is basic/mcs\n", __func__, rate));

	rix = rt->rateCodeToIndex[rate & IEEE80211_RATE_VAL];
#else
	uint8_t rix = rt->rateCodeToIndex[rate];
#endif

	KASSERT(rix != (uint8_t)-1, ("[%s] rate %d has no info\n", __func__, rate));
	return (enum ieee80211_phytype)rt->info[rix].phy;
}

#ifndef CONFIG_LINK_TO_ROM
static __inline__ uint8_t
ieee80211_legacy_rate_lookup(const struct ieee80211_rate_table *rt,
    uint8_t rate)
{

	return (rt->rateCodeToIndex[rate & IEEE80211_RATE_VAL]);
}

static __inline__ int
ieee80211_isratevalid(const struct ieee80211_rate_table *rt, uint8_t rate)
{
#ifdef __fully_updated__
	/*
	 * XXX Assert this is for a legacy rate; not for an MCS rate.
	 * If the caller wishes to use it for a basic rate, they should
	 * clear the high bit first.
	 */
	KASSERT(! (rate & 0x80), ("[%s] rate %d is basic/mcs\n", __func__, rate));
#endif

	return rt->rateCodeToIndex[rate] != (uint8_t)-1;
}
#else
extern int ieee80211_isratevalid(const struct ieee80211_rate_table *rt, uint8_t rate);
extern uint8_t ieee80211_legacy_rate_lookup(const struct ieee80211_rate_table *rt, uint8_t rate);
#endif
#ifndef __WISE__
/*
 * Calculate ACK field for
 * o  non-fragment data frames
 * o  management frames
 * sent using rate, phy and short preamble setting.
 */
static __inline__ uint16_t
ieee80211_ack_duration(const struct ieee80211_rate_table *rt,
    uint8_t rate, int isShortPreamble)
{
	uint8_t rix = rt->rateCodeToIndex[rate];

	KASSERT(rix != (uint8_t)-1, ("[%s] rate %d has no info\n", __func__, rate));
	if (isShortPreamble) {
		KASSERT(rt->info[rix].spAckDuration != 0,
			("shpreamble ack dur is not computed!\n"));
		return rt->info[rix].spAckDuration;
	} else {
		KASSERT(rt->info[rix].lpAckDuration != 0,
			("lgpreamble ack dur is not computed!\n"));
		return rt->info[rix].lpAckDuration;
	}
}
#endif


/*
 * Compute the time to transmit a frame of length frameLen bytes
 * using the specified 802.11 rate code, phy, and short preamble
 * setting.
 *
 * NB: SIFS is included.
 */
uint16_t
_ieee80211_compute_duration(const struct ieee80211_rate_table *,
                            uint32_t frameLen, uint16_t rate,
                            int isShortPreamble);
extern uint16_t (*ieee80211_compute_duration) (const struct ieee80211_rate_table * rt,
	uint32_t frameLen, uint16_t rate, int isShortPreamble);

/*
 * Convert PLCP signal/rate field to 802.11 rate code (.5Mbits/s)
 */
uint8_t		ieee80211_plcp2rate(uint8_t, enum ieee80211_phytype);
/*
 * Convert 802.11 rate code to PLCP signal.
 */
uint8_t		ieee80211_rate2plcp(int, enum ieee80211_phytype);

#define IEEE80211_IS_LEGACY_RATE(_rc)	( !((_rc) & IEEE80211_BEYOND_HT_MASK))

/*
 * 802.11n rate manipulation.
 */

#define	IEEE80211_HT_RC_2_MCS(_rc)	((_rc) & 0x1f)
#define	IEEE80211_HT_RC_2_STREAMS(_rc)	((((_rc) & 0x78) >> 3) + 1)
#define	IEEE80211_IS_HT_RATE(_rc)	( (_rc) & IEEE80211_RATE_MCS)
#if defined(__WISE__) && defined(CONFIG_SUPPORT_HE)
uint32_t
_ieee80211_compute_duration_ht(uint32_t frameLen, uint16_t rate,
                               struct ieee80211_node *ni,
                               int isShortGI, int streams, int isht40);
extern uint32_t
    (*ieee80211_compute_duration_ht) (uint32_t frameLen, uint16_t rate,
                                      struct ieee80211_node * ni,
                                      int isShortGI, int streams,
                                      int isht40);
#else
uint32_t
_ieee80211_compute_duration_ht(uint32_t frameLen,
                               uint16_t rate, int streams,
                               int isht40, int isShortGI);

extern uint32_t
    (*ieee80211_compute_duration_ht) (uint32_t frameLen, uint16_t rate,
                                      int streams, int isht40,
                                      int isShortGI);
#endif
#if defined(__WISE__) && defined(CONFIG_SUPPORT_VHT)
#define	IEEE80211_VHT_RC_2_MCS(_rc)	((_rc) & 0x0f)
#define	IEEE80211_IS_VHT_RATE(_rc)	( ((_rc) & IEEE80211_BEYOND_HT_MASK) \
					== IEEE80211_RATE_AC)
uint32_t
ieee80211_compute_duration_vht(uint32_t frameLen, uint16_t rate,
		struct ieee80211_node *ni, int isShortGI);
#endif

#ifdef CONFIG_SUPPORT_HE
struct he_txtime_vector {
	uint8_t mcs;
	uint8_t num_sts;
	uint8_t stbc;
	uint8_t fec_coding;

	uint8_t ch_bandwidth;
	uint8_t dcm;
	uint8_t nominal_packet_padding;
	uint8_t he_ltf_type;

	uint8_t gi_type;
	uint8_t doppler;
	uint8_t midamble_periodicity;
	uint8_t rsvd1;

	uint32_t apep_length;
	uint8_t he_er_su;
};

#define	IEEE80211_HE_RC_2_MCS(_rc)	((_rc) & 0x0f)
#define	IEEE80211_IS_HE_RATE(_rc)	( ((_rc) & IEEE80211_BEYOND_HT_MASK) \
					== IEEE80211_RATE_AX)

uint32_t
_ieee80211_compute_duration_he(uint32_t frameLen, uint16_t rate,
                               struct ieee80211_node *ni, uint8_t gi_type,
                               uint8_t ltf_type, uint8_t nominal_packet_padding);
extern uint32_t (*ieee80211_compute_duration_he) (uint32_t frameLen, uint16_t rate,
	struct ieee80211_node * ni, uint8_t gi_type, uint8_t ltf_type,
	uint8_t nominal_packet_padding);

#endif
#endif	/* _KERNEL */
#endif	/* !_NET80211_IEEE80211_PHY_H_ */
