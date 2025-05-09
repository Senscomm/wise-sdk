/* CoAP Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

/*
 * WARNING
 * libcoap is not multi-thread safe, so only this thread must make any coap_*()
 * calls.  Any external (to this thread) data transmitted in/out via libcoap
 * therefore has to be passed in/out by xQueue*() via this thread.
 */

#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/param.h>

#include <cmsis_os.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <assert.h>
#include <netdb.h>
#include <string.h>
#include <fcntl.h>
#include <limits.h>

#include <arpa/inet.h>

#include <wise_err.h>
#include <wise_wifi.h>
#include <wise_event_loop.h>

#include <scm_log.h>
#include <scm_wifi.h>

#include "protocol_common.h"

static const char *COAP_TAG = "COAP";

extern void coapclient_ip_got_flag(uint8_t flag);
extern int coap_cli_server_start(int argc, char *argv[]);
extern int coap_cli_server_stop(int argc, char *argv[]);

static wise_err_t wifi_event_handler(void *priv, system_event_t * event)
{
	switch (event->event_id) {
	case SYSTEM_EVENT_STA_GOT_IP:
    {
        char ip[IP4ADDR_STRLEN_MAX] = {0,};
        scm_wifi_get_ip("wlan0", ip, sizeof(ip), NULL, 0, NULL, 0);
		SCM_INFO_LOG(COAP_TAG, "WIFI GOT IP: <%s>\n", ip);
		coapclient_ip_got_flag(1);
		break;
    }
	case SYSTEM_EVENT_STA_LOST_IP:
		SCM_INFO_LOG(COAP_TAG, "\r\nWIFI LOST IP\r\n");
		break;
	case SYSTEM_EVENT_STA_CONNECTED:
		SCM_INFO_LOG(COAP_TAG, "\r\nWIFI CONNECTED\r\n");
		if (!event->event_info.connected.not_en_dhcp) {
			scm_wifi_status connect_status;
			netifapi_dhcp_start(scm_wifi_get_netif(WISE_IF_WIFI_STA));
			scm_wifi_sta_get_connect_info(&connect_status);
			scm_wifi_sta_dump_ap_info(&connect_status);
		}
		break;
	case SYSTEM_EVENT_STA_DISCONNECTED:
		SCM_INFO_LOG(COAP_TAG, "\r\nWIFI DISCONNECT\r\n");
		break;
	default:
		break;
	}

	return WISE_OK;
}


int main(void)
{
	scm_wifi_register_event_callback(wifi_event_handler, NULL);

#ifdef CONFIG_DEMO_WIFI_CONF
	demo_wifi_connect();
#endif

	return 0;
}

#ifdef CONFIG_CMDLINE

#include <cli.h>

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

static const struct cli_cmd coap_server_cli_cmd[] = {
	CMDENTRY(start , coap_cli_server_start, "", ""),
	CMDENTRY(stop , coap_cli_server_stop, "", ""),
};

static int do_coap_server_cli(int argc, char *argv[])
{
	const struct cli_cmd *cmd;

	argc--;
	argv++;

	cmd = cli_find_cmd(argv[0], coap_server_cli_cmd, ARRAY_SIZE(coap_server_cli_cmd));
	if (cmd == NULL)
		return CMD_RET_USAGE;

	return cmd->handler(argc, argv);
}

CMD(coap_server, do_coap_server_cli,
	"CLI for CoAP server operations",
	"coap_server start" OR
	"coap_server stop"
);

#endif
