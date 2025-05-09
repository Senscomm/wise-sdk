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

#ifndef H_BLEPRPH_
#define H_BLEPRPH_

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdbool.h>
#include "nimble/ble.h"
#include "modlog/modlog.h"

#include "../misc.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

	struct ble_hs_cfg;
	struct ble_gatt_register_ctxt;

	/** GATT server. */

	#define GATT_SVR_SVC_ALERT_UUID               0x1811
	#define GATT_SVR_CHR_SUP_NEW_ALERT_CAT_UUID   0x2A47
	#define GATT_SVR_CHR_NEW_ALERT                0x2A46
	#define GATT_SVR_CHR_SUP_UNR_ALERT_CAT_UUID   0x2A48
	#define GATT_SVR_CHR_UNR_ALERT_STAT_UUID      0x2A45
	#define GATT_SVR_CHR_ALERT_NOT_CTRL_PT        0x2A44

	#define gatt_svr_register_cb bleprph_gatt_svr_register_cb
	#define gatt_svr_init bleprph_gatt_svr_init

	void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg);
	int gatt_svr_init(void);

#ifdef __cplusplus
}
#endif

#endif
