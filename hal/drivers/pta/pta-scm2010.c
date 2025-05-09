
#include <stdio.h>
#include <string.h>
#include <stdint.h>


#include <hal/init.h>
#include <hal/kernel.h>
#include <hal/device.h>
#include <hal/console.h>
#include <hal/timer.h>
#include <hal/pta.h>

#include "vfs.h"

#include <cli.h>

#include "mmap.h"

/* coex register offset */
#define COEX_CFG0				0x00
#define COEX_CFG1				0x04
#define COEX_CFG2				0x08
#define COEX_CFG3				0x0C
#define COEX_CFG4				0x10
#define COEX_CFG5				0x14

/* pta register offset */
#define PTA_REVISION			0x00
#define PTA_CONFIG				0x04
#define PTA_STAT_BT_TX			0x08
#define PTA_STAT_BT_TX_ABORT	0x0C
#define PTA_STAT_BT_RX			0x10
#define PTA_STAT_BT_RX_ABORT	0x14
#define PTA_STAT_WLAN_TX		0x18
#define PTA_STAT_WLAN_TX_ABORT	0x1C
#define PTA_STAT_WLAN_RX		0x20
#define PTA_STAT_WLAN_RX_ABORT	0x24
#define PTA_MORE_CONFIG			0x28
#define PTA_FORCE_CONFIG		0x2C

#define COEX_CFG0_VAL \
					0 << 0  | /* bit_intf_mode (0: 3-wire, 1: 2-wire)*/ \
					0 << 1  | /* bt_active_polarity */ \
					1 << 2  | /* bt_priority_en */ \
					0 << 3  | /* bt_priority_polarity */ \
					1 << 4  | /* bt_state_en: if disabled, everything will be treated as TX */ \
					0 << 5  | /* bt_state_polarity: indicates Tx or Rx, inverse of Telink (FLD_RF_COEX_TRX_POL) */ \
					0 << 6  | /* bt_inband_en */ \
					0 << 7  | /* bt_inband_polarity */ \
					0 << 8  | /* wlan_active_polarity */ \
					1 << 16 | /* bt_coex_en */ \
					9 << 20   /* bt_time_slot */
#define COEX_CFG1_VAL \
					19 << 0 | /* bt_active_time: should match PTA_CHECK_TIME_US */ \
					16 << 8 | /* bt_priority_time: should match PTA_BLE_IO_TIME_US */ \
					8 << 16 | /* bt_state_time */ \
					0 << 24;  /* bt_inband_time */
#define COEX_CFG2_VAL \
					0 << 0;   /* bt_trans_intv */
#define COEX_CFG3_VAL \
					0 << 0  | /* bt_trans_time */ \
					0 << 8  | /* bt_state1_time */ \
					0 << 16;  /* bt_inband1_time */
#define COEX_CFG4_VAL \
					0x00 << 0  | /* bt_priority_tx_low */ \
					0x08 << 4  | /* bt_priority_tx_high */ \
					0x00 << 8  | /* bt_priority_rx_low */ \
					0x08 << 12;  /* bt_priority_rx_high */

#define PTA_CONFIG_VAL \
					0 << 19 | /* sw_wlan_rx_abort (not used) */ \
					0 << 18 | /* sw_wlan_tx_abort (not used) */ \
					0 << 17 | /* sw_bt_rx_abort (not used) */ \
					0 << 16 | /* sw_bt_rx_abort (not used) */ \
					0 << 8 | /* chan_margin (not used) */ \
					1 << 7 | /* wlan_pti_mode */ \
					1 << 6 | /* bt_event_mask */ \
					0 << 5 | /* chan_enable */ \
					1 << 4 | /* pti_enable */ \
					1 << 3 | /* no_sim_rx */ \
					1 << 2 | /* no_sim_tx */ \
					1 << 1 | /* basic_priority (0:bt, 1:wlan)*/ \
					1 << 0   /* pta_enable */ \

#define PTA_ABORT_ABORTRX		1

struct pta_driver_data {
	struct fops devfs_ops;
	struct device *dev;
} pta_data;

uint32_t g_pta_force_mode;

static int pta_trng_ioctl(struct file *file, unsigned int cmd, void *arg)
{
	struct pta_driver_data *priv = file->f_priv;
	struct device *dev = priv->dev;
	struct pta_force_mode_val *force_mode;
	int ret = 0;

	switch (cmd) {
	case IOCTL_PTA_GET_FORCE_MODE:
		force_mode = arg;
		force_mode->val = readl(dev->base[1] + PTA_FORCE_CONFIG);
		break;
	case IOCTL_PTA_SET_FORCE_MODE:
		force_mode = arg;
		if (force_mode->val > PTA_FORCE_MODE_BLE) {
			return -EINVAL;
		}
		writel(force_mode->val, dev->base[1] + PTA_FORCE_CONFIG);
		break;
	default:
		ret = -EINVAL;
		break;
	};

	return ret;
}

static void pta_config(struct device *dev)
{
	/* coex */
	writel(COEX_CFG0_VAL, dev->base[0] + COEX_CFG0);
	writel(COEX_CFG1_VAL, dev->base[0] + COEX_CFG1);
	writel(COEX_CFG2_VAL, dev->base[0] + COEX_CFG2);
	writel(COEX_CFG3_VAL, dev->base[0] + COEX_CFG3);
	writel(COEX_CFG4_VAL, dev->base[0] + COEX_CFG4);

	/* pta config */
	writel(PTA_CONFIG_VAL, dev->base[1] + PTA_CONFIG);

	/* pta more */
	writel(PTA_ABORT_ABORTRX, dev->base[1] + PTA_MORE_CONFIG);
}

static int scm2010_pta_probe(struct device *dev)
{
	struct pta_driver_data *priv;
	char buf[32];
	struct file *file;

	pta_config(dev);

	/* pta force off */
	writel(PTA_FORCE_MODE_OFF, dev->base[1] + PTA_FORCE_CONFIG);

	priv = &pta_data;

	priv->dev = dev;
	dev->driver_data = priv;

	sprintf(buf, "/dev/pta");

	priv->devfs_ops.ioctl = pta_trng_ioctl;

	file = vfs_register_device_file(buf, &priv->devfs_ops, priv);
	if (!file) {
		printk("%s: failed to register as %s\n", dev_name(dev), buf);
		return -1;
	}

	printk("PTA: %s registered as %s\n", dev_name(dev), buf);

	return 0;
}

#ifdef CONFIG_PM_DM
static int scm2010_pta_suspend(struct device *dev, u32 *idle)
{
	g_pta_force_mode = readl(dev->base[1] + PTA_FORCE_CONFIG);

	return 0;
}

static int scm2010_pta_resume(struct device *dev)
{
	pta_config(dev);

	writel(g_pta_force_mode, dev->base[1] + PTA_FORCE_CONFIG);

	return 0;
}
#endif

static declare_driver(pta) = {
	.name		= "pta",
	.probe		= scm2010_pta_probe,
#ifdef CONFIG_PM_DM
	.suspend	= scm2010_pta_suspend,
	.resume		= scm2010_pta_resume,
#endif
};

#ifdef CONFIG_CMD_PTA

static int do_pta_stats_show(int argc, char *argv[])
{
	struct device *dev = device_get_by_name("pta");
	printf("BT TX   : %d\n", readl(dev->base[1] + PTA_STAT_BT_TX));
	printf("BT TXA  : %d\n", readl(dev->base[1] + PTA_STAT_BT_TX_ABORT));
	printf("BT RX   : %d\n", readl(dev->base[1] + PTA_STAT_BT_RX));
	printf("BT RXA  : %d\n", readl(dev->base[1] + PTA_STAT_BT_RX_ABORT));
	printf("WLAN TX : %d\n", readl(dev->base[1] + PTA_STAT_WLAN_TX));
	printf("WLAN TXA: %d\n", readl(dev->base[1] + PTA_STAT_WLAN_TX_ABORT));
	printf("WLAN RX : %d\n", readl(dev->base[1] + PTA_STAT_WLAN_RX));
	printf("WLAN RXA: %d\n", readl(dev->base[1] + PTA_STAT_WLAN_RX_ABORT));
	return CMD_RET_SUCCESS;
}

static int do_pta_stats_clear(int argc, char *argv[])
{
	struct device *dev = device_get_by_name("pta");
	writel(0, dev->base[1] + PTA_STAT_BT_TX);
	writel(0, dev->base[1] + PTA_STAT_BT_TX_ABORT);
	writel(0, dev->base[1] + PTA_STAT_BT_RX);
	writel(0, dev->base[1] + PTA_STAT_BT_RX_ABORT);
	writel(0, dev->base[1] + PTA_STAT_WLAN_TX);
	writel(0, dev->base[1] + PTA_STAT_WLAN_TX_ABORT);
	writel(0, dev->base[1] + PTA_STAT_WLAN_RX);
	writel(0, dev->base[1] + PTA_STAT_WLAN_RX_ABORT);
	return CMD_RET_SUCCESS;
}

CMD(ptas, do_pta_stats_show,
	"PTA stats show", ""
);
CMD(ptac, do_pta_stats_clear,
	"PTA stats clear", ""
);

#endif

