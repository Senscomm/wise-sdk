/*
 * Copyright 2021-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <hal/kernel.h>
#include <hal/device.h>
#include <hal/ble.h>
#include <hal/rf.h>

#include "rf.h"
#include "rf_reg.h"
#include "dma.h"
#include "sys.h"

#ifndef CONFIG_WLAN
#ifndef CONFIG_SYS_FPGA
static int ble_phy_rf_reg_write(unsigned int addr, unsigned int data)
{
	writel(data, PHY_BASE_START + addr);

	return 0;
}

static unsigned int ble_phy_rf_reg_read(unsigned int addr)
{
	return readl(PHY_BASE_START + addr);
}
#endif
#endif

static int scm2010_ble_probe(struct device *dev)
{
#ifndef CONFIG_WLAN
#ifdef CONFIG_SYS_FPGA
	/* FPGA with mango RF: use direct call for BLE RF initialization */
	ble_sys_init();
#else
	/* SOC with XRC RF: use common RF init only if not done by WLAN */
	struct device *rf = device_get_by_name("rf");

	if (rf) {
		rf_config(rf, ble_phy_rf_reg_write, ble_phy_rf_reg_read);
		rf_init(rf, 0);
	}
#endif
#endif

	return 0;
}

#ifdef CONFIG_PM_DM
static int scm2010_ble_suspend(struct device *dev, u32 *idle)
{
	return 0;
}

static int scm2010_ble_resume(struct device *dev)
{
	rf_ll_resume();

#ifdef CONFIG_PTA
	rf_set_pta_t1_time(PTA_CHECK_TIME_US);
	rf_set_pta_t2_time(PTA_BLE_IO_TIME_US);
	rf_3wire_pta_init(PTA_BLE_STATUS_TX);
#endif

	return 0;
}
#endif

static declare_driver(ble) = {
	.name       = "ble",
	.probe      = scm2010_ble_probe,
#ifdef CONFIG_PM_DM
	.suspend    = scm2010_ble_suspend,
	.resume     = scm2010_ble_resume,
#endif
};
