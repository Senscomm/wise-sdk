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
#include "FreeRTOS.h"
#include "task.h"
#include <wise_err.h>
#include <wise_wifi.h>
#include <wise_event_loop.h>
#include <common.h>

#include <scm_log.h>
#include <scm_wifi.h>

#define CLI_APP_TAG "CLI_APP"

/* Event handler function: manages different WiFi events. */
wise_err_t event_handler(void *ctx, system_event_t * event)
{
	switch (event->event_id) {
	case SYSTEM_EVENT_STA_START:
		break;
	case SYSTEM_EVENT_STA_STOP:
		break;
	case SYSTEM_EVENT_STA_GOT_IP:
		SCM_INFO_LOG(CLI_APP_TAG, "WIFI GOT IP\n");
		break;
	case SYSTEM_EVENT_AP_START:
		SCM_INFO_LOG(CLI_APP_TAG, "SYSTEM_EVENT_AP_START\n");
		break;
	case SYSTEM_EVENT_AP_STOP:
		SCM_INFO_LOG(CLI_APP_TAG, "SYSTEM_EVENT_AP_STOP\n");
		break;
	case SYSTEM_EVENT_AP_STACONNECTED:
		SCM_INFO_LOG(CLI_APP_TAG, "SYSTEM_EVENT_AP_STACONNECTED\n");
		SCM_INFO_LOG(CLI_APP_TAG, "Connected STA:" MACSTR "\r\n", MAC2STR(event->event_info.sta_connected.mac));
		break;
	case SYSTEM_EVENT_AP_STADISCONNECTED:
		SCM_INFO_LOG(CLI_APP_TAG, "SYSTEM_EVENT_AP_STADISCONNECTED\n");
		SCM_INFO_LOG(CLI_APP_TAG, "Disconnected STA:" MACSTR "\n", MAC2STR(event->event_info.sta_disconnected.mac));
		break;
	case SYSTEM_EVENT_STA_CONNECTED:
		{
			scm_wifi_status connect_status;

			SCM_INFO_LOG(CLI_APP_TAG, "WIFI CONNECTED\n");

			netifapi_dhcp_start(scm_wifi_get_netif(WISE_IF_WIFI_STA));
			scm_wifi_sta_get_connect_info(&connect_status);

			scm_wifi_sta_dump_ap_info(&connect_status);
		}
		break;
	case SYSTEM_EVENT_STA_DISCONNECTED:
		SCM_INFO_LOG(CLI_APP_TAG, "WIFI DISCONNECT\n");
		break;
	case SYSTEM_EVENT_SCAN_DONE:
		SCM_INFO_LOG(CLI_APP_TAG, "WiFi: Scan results available\n");
		break;
	case SYSTEM_EVENT_SCM_CHANNEL:
		SCM_INFO_LOG(CLI_APP_TAG, "WiFi: Scm channel send msg\n");
		scm_wifi_event_send(event, sizeof(system_event_t));
		break;
	default:
		break;
	}

	return WISE_OK;
}

int main(void)
{
	SCM_INFO_LOG(CLI_APP_TAG, "Hello world!\n");

	scm_wifi_register_event_callback(event_handler, NULL);

	return 0;
}
