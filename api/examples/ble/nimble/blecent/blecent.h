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

#ifndef H_BLECENT_
#define H_BLECENT_

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "../misc.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

	struct ble_hs_adv_fields;
	struct ble_gap_conn_desc;
	struct ble_hs_cfg;
	union ble_store_value;
	union ble_store_key;

	#define BLECENT_SVC_ALERT_UUID              0x1811
	#define BLECENT_CHR_SUP_NEW_ALERT_CAT_UUID  0x2A47
	#define BLECENT_CHR_NEW_ALERT               0x2A46
	#define BLECENT_CHR_SUP_UNR_ALERT_CAT_UUID  0x2A48
	#define BLECENT_CHR_UNR_ALERT_STAT_UUID     0x2A45
	#define BLECENT_CHR_ALERT_NOT_CTRL_PT       0x2A44

	/** Peer. */

	struct peer_dsc
	{
		SLIST_ENTRY(peer_dsc) next;
		struct ble_gatt_dsc dsc;
	};

	SLIST_HEAD(peer_dsc_list, peer_dsc);

	struct peer_chr
	{
		SLIST_ENTRY(peer_chr) next;
		struct ble_gatt_chr chr;

		struct peer_dsc_list dscs;
	};

	SLIST_HEAD(peer_chr_list, peer_chr);

	struct peer_svc
	{
		SLIST_ENTRY(peer_svc) next;
		struct ble_gatt_svc svc;

		struct peer_chr_list chrs;
	};

	SLIST_HEAD(peer_svc_list, peer_svc);

	struct peer;
	typedef void peer_disc_fn(const struct peer *peer, int status, void *arg);

	struct peer
	{
		SLIST_ENTRY(peer) next;

		uint16_t conn_handle;

		/** List of discovered GATT services. */

		struct peer_svc_list svcs;

		/** Keeps track of where we are in the service discovery process. */

		uint16_t disc_prev_chr_val;
		struct peer_svc *cur_svc;

		/** Callback that gets executed when service discovery completes. */

		peer_disc_fn *disc_cb;
		void *disc_cb_arg;
	};

	int peer_disc_all(uint16_t conn_handle, peer_disc_fn *disc_cb,
			void *disc_cb_arg);
	const struct peer_dsc * peer_dsc_find_uuid(const struct peer *peer,
			const ble_uuid_t *svc_uuid,
			const ble_uuid_t *chr_uuid,
			const ble_uuid_t *dsc_uuid);
	const struct peer_chr * peer_chr_find_uuid(const struct peer *peer,
			const ble_uuid_t *svc_uuid,
			const ble_uuid_t *chr_uuid);
	const struct peer_svc * peer_svc_find_uuid(const struct peer *peer,
			const ble_uuid_t *uuid);
	int peer_delete(uint16_t conn_handle);
	int peer_add(uint16_t conn_handle);
	int peer_init(int max_peers, int max_svcs, int max_chrs, int max_dscs);

#ifdef __cplusplus
}
#endif

#endif
