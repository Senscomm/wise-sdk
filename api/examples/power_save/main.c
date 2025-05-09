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

#include <stdlib.h>
#include <ctype.h>
#include <wise_event_loop.h>
#include <wise_wifi.h>
#include <scm_log.h>
#include <scm_wifi.h>
#include "cli.h"
#include "hal/kernel.h"
#include "scm_pm.h"

#define DEMO_WIFI_SSID				"Redmi_Test"
#define DEMO_WIFI_PASSWORD			"12345678"
#define DEMO_WIFI_AUTH				SCM_WIFI_SECURITY_WPA2PSK
#define DEMO_WIFI_PAIRWISE			SCM_WIFI_PAIRWISE_AES
#define DEMO_WIFI_POWERSAVE_INTV	1000 /* 1000TU, DTIM10 */

#define LOWPOWER_TAG "DEMO_LOWPOWER"
#define SCM_NEED_DHCP_START(event) ((event)->event_info.connected.not_en_dhcp == false)

enum HIB_MODE {
	HIB_MODE_OFF	= 0,
	HIB_MODE_ON	 = 1,
};

/* Preconfigured Wi-Fi association request */
scm_wifi_assoc_request g_assoc_req = {
	.ssid = DEMO_WIFI_SSID,
	.auth = DEMO_WIFI_AUTH,
	.key = DEMO_WIFI_PASSWORD,
	.pairwise = DEMO_WIFI_PAIRWISE,
};

scm_wifi_ps_param g_powersave = {
	.mode = 1,
	.interval = DEMO_WIFI_POWERSAVE_INTV,
};

bool is_all_digits(const char *str) {
	if (!str) return false;

	for (; *str; str++) {
		if (!isdigit((unsigned char)*str)) {
			return false;
		}
	}

	return true;
}

static bool isValidHibMode(int mode) {
	return (mode >= HIB_MODE_OFF) && (mode <= HIB_MODE_ON);
}

/* Event handler for Wi-Fi and system events */
wise_err_t event_handler(void *ctx, system_event_t * event)
{
	switch (event->event_id) {
	case SYSTEM_EVENT_STA_START:
		break;
	case SYSTEM_EVENT_STA_STOP:
		break;
	case SYSTEM_EVENT_STA_GOT_IP:
    {
        char ip[IP4ADDR_STRLEN_MAX] = {0,};
        scm_wifi_get_ip("wlan0", ip, sizeof(ip), NULL, 0, NULL, 0);
		SCM_INFO_LOG(LOWPOWER_TAG, "WIFI GOT IP: <%s>\n", ip);
		break;
    }
	case SYSTEM_EVENT_AP_START:
		break;
	case SYSTEM_EVENT_AP_STOP:
		break;
	case SYSTEM_EVENT_AP_STACONNECTED:
		break;
	case SYSTEM_EVENT_AP_STADISCONNECTED:
		break;
	case SYSTEM_EVENT_STA_CONNECTED:
		if (SCM_NEED_DHCP_START(event)) {
			scm_wifi_status connect_status;
			netifapi_dhcp_start(scm_wifi_get_netif(WISE_IF_WIFI_STA));
			scm_wifi_sta_get_connect_info(&connect_status);
			scm_wifi_sta_dump_ap_info(&connect_status);
		}
		break;
	case SYSTEM_EVENT_STA_DISCONNECTED:
		SCM_INFO_LOG(LOWPOWER_TAG, "WIFI DISCONNECT\n");
		break;
	case SYSTEM_EVENT_SCAN_DONE:
		SCM_INFO_LOG(LOWPOWER_TAG, "WiFi: Scan results available\n");
		break;
	default:
		break;
	}

	return WISE_OK;
}

/* Function to start Wi-Fi station connection */
static int scm_wifi_start_connect(void)
{
	if (scm_wifi_sta_set_config(&g_assoc_req, NULL)) {
		return WISE_FAIL;
	}
	SCM_INFO_LOG(LOWPOWER_TAG, "Starting wifi sta connect!\n");
	return scm_wifi_sta_connect();
}

/* Function to start the station and initiate connection */
static int station_connect(void)
{
	char ifname[WIFI_IFNAME_MAX_SIZE + 1] = {0};
	int len = sizeof(ifname);

	SCM_INFO_LOG(LOWPOWER_TAG, "Sta Hello world!\n");
	scm_wifi_register_event_callback(event_handler, NULL);
	scm_wifi_sta_start(ifname, &len);

	return scm_wifi_start_connect();
}

/* Function to log power management state changes */
static void pm_log_state_change(const char *name, uint8_t len)
{
	int i;

	for (i = 0; i < len; i++) {
		writel(name[i], 0xf0200000 + 0x20);
	}
	writel('\n', 0xf0200000 + 0x20);

	while(1) {
		if((readl(0xf0200034) & 0x60) == 0x60) {
			break;
		}
	}
	return;
}

/* Callback for power management notifications */
static void scm_cli_pm_notify(enum scm_pm_state state)
{
	static const char *pm_state_names[] = {
		[SCM_PM_STATE_ACTIVE]		= "ACTIVE     ",
		[SCM_PM_STATE_LIGHT_SLEEP]	= "LIGHT_SLEEP",
		[SCM_PM_STATE_DEEP_SLEEP]	= "DEEP_SLEEP ",
		[SCM_PM_STATE_HIBERNATION]	= "HIBERNATION",
	};

	switch (state) {
		case SCM_PM_STATE_ACTIVE:
		case SCM_PM_STATE_LIGHT_SLEEP:
		case SCM_PM_STATE_DEEP_SLEEP:
		case SCM_PM_STATE_HIBERNATION:
			pm_log_state_change(pm_state_names[state], 11);
			break;
		default:
			break;
	}
	return;
}

static void handle_hibernation_mode(int mode_onoff)
{
	if (mode_onoff == HIB_MODE_ON) {
		SCM_INFO_LOG(LOWPOWER_TAG, "lowpower on, interval=%d\n", DEMO_WIFI_POWERSAVE_INTV);
		scm_wifi_sta_disconnect();
		scm_wifi_set_options(SCM_WIFI_STA_SET_POWERSAVE, &g_powersave);
		scm_pm_register_handler(scm_cli_pm_notify);

		/* Reconnect after setting DTIM10 */
		SCM_INFO_LOG(LOWPOWER_TAG, "connecting to %s\n", DEMO_WIFI_SSID);
		if (station_connect() != WISE_OK) {
			SCM_INFO_LOG(LOWPOWER_TAG, "station re-connection fail\n");
		}

		scm_pm_enable_lowpower();
	} else {
		SCM_INFO_LOG(LOWPOWER_TAG, "lowpower off\n");
		scm_pm_unregister_handler(scm_cli_pm_notify);
		scm_pm_disable_lowpower();
	}
	return;
}

int main(void)
{
	printf("wifi powersave demo\n");
	return 0;
}

static int do_lowpower(int argc, char *argv[])
{
	if ((argc != 2) || !is_all_digits(argv[1])) {
		return CMD_RET_USAGE;
	}

	int mode_onoff = atoi(argv[1]);
	if (!isValidHibMode(mode_onoff)) {
		return CMD_RET_USAGE;
	}

	handle_hibernation_mode(mode_onoff);
	return 0;
}

CMD(lowpower, do_lowpower,
	"Lower Power Demo", "lowpower [1:0]"
);
