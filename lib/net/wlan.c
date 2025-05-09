/*
 * Copyright 2018-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "kernel.h"
#include "compat_types.h"
#include "compat_if.h"

#include <hal/kernel.h>
#include <hal/device.h>
#include <hal/init.h>
#include "hal/efuse.h"
#include "hal/wlan.h"

#include "if_media.h"
#include <net80211/ieee80211_var.h>
#include <freebsd/sys/sockio.h>
#include <freebsd/ethernet.h>
#include <freebsd/sys/socket.h>
#include <freebsd/sys/types.h>
#include <freebsd/netinet/in.h>
#include <freebsd/arpa/inet.h>

#include "sys/ioctl.h"
#include "vfs.h"

extern void (*ieee80211_phy_init)(void);
void ieee80211_auth_setup(void);
extern void(*ieee80211_ht_init)(void);
#ifdef CONFIG_SUPPORT_VHT
void ieee80211_vht_init(void);
#endif
#ifdef CONFIG_SUPPORT_HE
extern void (*ieee80211_he_init) (void);
#endif

const char *wlan_macaddr = CONFIG_DEFAULT_WLAN_MACADDR;

int wlan_init(void)
{
	ieee80211_phy_init();
	ieee80211_auth_setup();
	ieee80211_ht_init();
#ifdef CONFIG_SUPPORT_VHT
	ieee80211_vht_init();
#endif
#ifdef CONFIG_SUPPORT_HE
	ieee80211_he_init();
#endif

	return 0;
}
__initcall__(subsystem, wlan_init);

void wlan_down(void)
{
  struct device *dev = wlandev(0);
  struct ieee80211vap *vap;
  struct ifnet *ifp;
  int i;

  /* Initialize and remove other vaps first. */
  for (i = wlan_num_max_vaps(dev) - 1; i > -1 ; i--)
  {
    vap = wlan_get_vap(dev, i);
    if (!vap)
    {
      continue;
	}
    ifp = vap->iv_ifp;
	if (ifp->if_flags & IFF_UP)
	{
 	  ifp->if_flags &= ~IFF_UP;
      wlan_ctl_vap(dev, vap, SIOCSIFFLAGS, (caddr_t)NULL);
	}
	if (i != 0) /* vap 0 should remain. */
	{
      wlan_remove_vap(dev, vap);
	}
  }
}

extern struct ether_addr *ether_aton(const char *);
void wlan_mac_addr(uint8_t *macaddr)
{
	struct ether_addr *ethaddr;
	u16 addrh;
	u32 addrl;
	int fd;
	int ret;
	struct efuse_rw_data e_rw_dat;

	ethaddr = ether_aton(wlan_macaddr);

	fd = open("/dev/efuse", 0, 0);
	if (fd < 0) {
		printk("Can't open /dev/efuse: %s\n", strerror(errno));
		goto fixed;
	}

	e_rw_dat.row = 19;
	e_rw_dat.val = &addrl;
	ret = ioctl(fd, IOCTL_EFUSE_READ_ROW, &e_rw_dat);
	if (ret < 0) {
		printk("ioctl error: %s\n", strerror(errno));
		close(fd);
		goto fixed;
	}
	addrh = htons((u16)addrl);

	e_rw_dat.row = 18;
	e_rw_dat.val = &addrl;
	ret = ioctl(fd, IOCTL_EFUSE_READ_ROW, &e_rw_dat);
	if (ret < 0) {
		printk("ioctl error: %s\n", strerror(errno));
		close(fd);
		goto fixed;
	}
	addrl = htonl(addrl);

	memcpy(macaddr, &addrh, sizeof(addrh));
	memcpy(macaddr + 2, &addrl, sizeof(addrl));

	close(fd);

	if (memcmp(macaddr, etherzeroaddr, ETHER_ADDR_LEN)
			&& !(macaddr[0] & 0x01)) {
		/* Valid MAC in eFuse. Use it. */
		printk("Use eFuse MAC address: %02x.%02x.%02x.%02x.%02x.%02x\n",
				macaddr[0], macaddr[1], macaddr[2],
				macaddr[3], macaddr[4], macaddr[5]);
		return;
	}

fixed:
	printk("Use fixed MAC address: %02x.%02x.%02x.%02x.%02x.%02x\n",
			ethaddr->octet[0], ethaddr->octet[1], ethaddr->octet[2],
			ethaddr->octet[3], ethaddr->octet[4], ethaddr->octet[5]);
	memcpy(macaddr, ethaddr->octet, ETHER_ADDR_LEN);
}

void __weak  *wlan_default_netif(int inst)
{
	return NULL;
}

/**
 * NET CLI commands
 * FIXME: move this into a separate file
 */
#if defined(CONFIG_CMD_NET)
#include <cli.h>

#include <stdio.h>
#include <stdlib.h>
#include "hal/kernel.h"
#include "lwip/stats.h"
#include "lwip/memp.h"

int do_net_stats(int argc, char *argv[])
{
	stats_display();

	return 0;
}

int do_net_memp(int argc, char *argv[])
{
	argc--;
	argv++;

#if MEMP_OWNER_CHECK
	memp_check_owners(argv[0]);
#endif

	return 0;
}

const struct cli_cmd net_cmd[] = {
	CMDENTRY(stats, do_net_stats, "", ""),
	CMDENTRY(memp, do_net_memp, "", ""),
};

static int do_net(int argc, char *argv[])
{
	const struct cli_cmd *cmd;

	argc--;
	argv++;

	if (argc == 0)
		return CMD_RET_USAGE;

	cmd = cli_find_cmd(argv[0], net_cmd, ARRAY_SIZE(net_cmd));
	if (cmd == NULL)
		return CMD_RET_USAGE;

	return cmd->handler(argc, argv);
}

CMD(net, do_net,
	"test routines for net (lwIP/net80211/driver)",
	"net stats" OR
	"net memp"
);
#endif
