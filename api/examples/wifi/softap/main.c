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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hal/unaligned.h>
#include <hal/kernel.h>
#include <hal/wlan.h>
#include <hal/kmem.h>

#include "kernel.h"
#include "compat_if.h"
#include "if_media.h"
#include "sncmu_d11.h"
#include "fwil_types.h"
#include "fweh.h"
#include "scdc.h"
#include "task.h"
#include "FreeRTOS.h"

#include <net80211/ieee80211_var.h>
#include <wise_err.h>
#include <wise_wifi.h>
#include <wise_event_loop.h>
#include <common.h>
#include "dhcps.h"

#include <scm_log.h>
#include <scm_wifi.h>

#define SAP_APP_TAG "SAP_APP"

/* Event handler function: manages different WiFi events. */
wise_err_t event_handler(void *ctx, system_event_t * event)
{
	switch (event->event_id) {
	case SYSTEM_EVENT_AP_START:
		/* Handle the event when WiFi Access Point (AP) mode starts. */
		{
			SCM_INFO_LOG(SAP_APP_TAG, "SYSTEM_EVENT_AP_START\n");

			/* ip addr for dhcps */
			//wifi_ip_info_t ip_info = {0};
			//IP4_ADDR(&ip_info.ip, 192, 168, 200, 1);
			//IP4_ADDR(&ip_info.nm, 255, 255, 255, 0);

			scm_wifi_set_ip("wlan1", "192.168.200.1", "255.255.255.0", NULL);
			netifapi_dhcps_start(scm_wifi_get_netif(WISE_IF_WIFI_AP));
		}
		break;
	case SYSTEM_EVENT_AP_STOP:
		/* Handle the event when WiFi Access Point (AP) mode stops. */
		SCM_INFO_LOG(SAP_APP_TAG, "SYSTEM_EVENT_AP_STOP\n");
		break;
	case SYSTEM_EVENT_AP_STACONNECTED:
		/* Handle the event when a station connects to the WiFi AP. */
		SCM_INFO_LOG(SAP_APP_TAG, "SYSTEM_EVENT_AP_STACONNECTED\n");
		SCM_INFO_LOG(SAP_APP_TAG, "Connected STA:" MACSTR "\n", MAC2STR(event->event_info.sta_connected.mac));
		break;
	case SYSTEM_EVENT_AP_STADISCONNECTED:
		/* Handle the event when a station disconnects from the WiFi AP. */
		SCM_INFO_LOG(SAP_APP_TAG, "SYSTEM_EVENT_AP_STADISCONNECTED\n");
		SCM_INFO_LOG(SAP_APP_TAG, "Disconnected STA:" MACSTR "\n", MAC2STR(event->event_info.sta_disconnected.mac));
		break;

	default:
		break;
	}

	return WISE_OK;
}

/* Define the configuration for the SoftAP. */
scm_wifi_softap_config g_sap_cfg = {
	.ssid = "sap_test",                /* SSID of the SoftAP. */
	.key = "12345678",                 /* Security key for the SoftAP. */
	.channel_num = 6,                  /* Channel number for the SoftAP. */
	.authmode = SCM_WIFI_SECURITY_WPA2PSK, /* Authentication mode for the SoftAP. */
	.pairwise = SCM_WIFI_PAIRWISE_AES,     /* Encryption method. */
};

int main(void)
{
	int ret = WISE_OK;
	char ifname[WIFI_IFNAME_MAX_SIZE + 1] = {0}; /* Interface name for the WiFi. */
	int len = sizeof(ifname);

	scm_wifi_softap_config *sap = &g_sap_cfg;

	SCM_INFO_LOG(SAP_APP_TAG, "SoftAP Hello world!\n");

	scm_wifi_register_event_callback(event_handler, NULL);

	scm_wifi_sap_set_config(sap);

	ret = scm_wifi_sap_start(ifname, &len);

	return ret;
}
