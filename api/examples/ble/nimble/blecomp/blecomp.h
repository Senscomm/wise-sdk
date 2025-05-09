/*
 * Copyright 2023-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef H_BLECOMP_
#define H_BLECOMP_

#endif //H_BLECOMP_

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "../misc.h"

#ifdef __cplusplus
extern "C"
{
#endif

	#define BLECOMP_SVC_TEST_UUID16		0x00FF
	#define BLECOMP_CHR1_TEST_UUID16	0xFF01
	#define BLECOMP_CHR2_TEST_UUID16	0xFF02
	#define BLECOMP_CHR3_TEST_UUID16	0xFF03
	#define BLECOMP_USER_DESC_UUID16	0x2901

	#define gatt_svr_register_cb blecomp_gatt_svr_register_cb
	#define gatt_svr_init blecomp_gatt_svr_init

	void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg);
    void gatt_svt_val_clear(void);
	int gatt_svr_init(void);
	int gatt_svr_notify(uint16_t conn_handle);

#ifdef __cplusplus
}
#endif
