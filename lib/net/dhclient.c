#include <unistd.h>

#include <hal/types.h>

#include "lwip/opt.h"
#include "lwip/netif.h"
#include "lwip/netifapi.h"

#include "cli.h"

static int do_dhclient(int argc, char *argv[])
{
	int opt;
	bool release = false;
    bool kill = false;
	char *ifname = NULL;
	struct netif *netif;
	err_t ret;

	optind = 0;
	while ((opt = getopt(argc, argv, "r:k:")) != -1) {
		switch (opt) {
		case 'r':
			release = true;
			break;
		case 'k':
			kill = true;
			break;
		default:
			return CMD_RET_USAGE;
		}
	}

    if (optind > argc)
		return CMD_RET_USAGE;

	ifname = argv[argc - 1];
	netif = netifapi_netif_find(ifname);
	if (netif == NULL) {
		printf("Unknown interface %s\n", ifname);
		return CMD_RET_USAGE;
	}

    if (kill) {
        netifapi_dhcp_stop_coarse_tmr(netif);
		printf("DHCP client's renew stopped on interface %s\n", ifname);
    } else if (!release) {
		ret = netifapi_dhcp_start(netif);
		printf("DHCP client %s on interface %s\n", ifname,
			   ret == 0 ? "successfully started": "failed to start");
		return (ret != 0) ? CMD_RET_FAILURE : CMD_RET_SUCCESS;
	} else {
		netifapi_dhcp_stop(netif);
		printf("DHCP client stopped on interface %s\n", ifname);
	}

	return 0;
}

CMD(dhclient, do_dhclient,
	"Dynamic Host Configuration Protocol Client",
	"dhclient [-(r|k)] ifname");
