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

#ifndef H_BLEHOGP_
#define H_BLEHOGP_

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdbool.h>
#include "nimble/ble.h"
#include "modlog/modlog.h"
#include "cli.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#include "../misc.h"

#ifdef __cplusplus
extern "C"
{
#endif

	/* GAP */
	#define BLE_HOGP_DEVICE_NAME	"Xiaohu HID"
	#define BLE_HOGP_APPEARANCE		0x0180  /* generic remote control */

	/* GATT DIS */
	#define BLE_SVC_DIS_MODEL_NUMBER_DEFAULT        "MODEM 0001"
	#define BLE_SVC_DIS_SERIAL_NUMBER_DEFAULT       "SERIAL 0002"
	#define BLE_SVC_DIS_FIRMWARE_REVISION_DEFAULT   "FIRMWARE 0003"
	#define BLE_SVC_DIS_HARDWARE_REVISION_DEFAULT   "HARDWARE 0004"
	#define BLE_SVC_DIS_SOFTWARE_REVISION_DEFAULT   "SOFTWARE 0005"
	#define BLE_SVC_DIS_MANUFACTURER_NAME_DEFAULT   "MANUFACTURER 0006"
	#define BLE_SVC_DIS_SYSTEM_ID_DEFAULT           "\x12\x34\x56\xFF\xFE\x9A\xBC\xDE"
	#define BLE_SVC_DIS_PNP_INFO_DEFAULT            "\x01\x12\x34\x56\x78\x90\xab"

	struct ble_hs_cfg;
	struct ble_gatt_register_ctxt;

	/** GATT server. */

	#define BLE_SVC_CHR_DSC_UUID16_PRESENTATION_FORMAT  0x2904
	#define BLE_SVC_CHR_DSC_UUID16_EXTERNAL_REPORT_REF  0x2907
	#define BLE_SVC_CHR_DSC_UUID16_REPORT_REF           0x2908

	#define gatt_svr_register_cb blehogp_gatt_svr_register_cb
	#define gatt_svr_init blehogp_gatt_svr_init

	void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg);
	int gatt_svr_init(void);

	#define HOGP_CMD_DEF(cmd)   ll_entry_declare(struct cli_cmd, cmd, _hogp_)
	#define HOGP_CMD_START()    ll_entry_start(struct cli_cmd, _hogp_)
	#define HOGP_CMD_END()      ll_entry_end(struct cli_cmd, _hogp_)
	#define HOGP_CMD(cmd, fn, d, u) \
		HOGP_CMD_DEF(cmd) = {       \
		.name = #cmd,               \
		.handler = fn,              \
		.desc = d,                  \
		.usage = u,                 \
	}


#ifdef __cplusplus
}
#endif

#endif
