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
#include <unistd.h>
#include <errno.h>

#include <assert.h>

#include <wise_err.h>
#include <wise_wifi.h>
#include <wise_event_loop.h>

#include <scm_log.h>
#include <scm_wifi.h>

#define PROTO_COMMON_TAG "PROTO-COMMON"

#ifdef CONFIG_DEMO_WIFI_CONF

/* WiFi automatically connect with configuration */
void demo_wifi_connect(void)
{
	const char *ssid;
	const char *passwd;
	int ret;

	scm_wifi_assoc_request demo_assoc_req = {0};

	char ifname[WIFI_IFNAME_MAX_SIZE + 1] = {0};
	int len = sizeof(ifname);
	int i;

	/* Start the Wi-Fi station interface */
	if (scm_wifi_sta_start(ifname, &len)){
		SCM_ERR_LOG(PROTO_COMMON_TAG, "WiFi station starting failed\n");
		return;
	}

	/* Retrieve SSID and password from the configuration */
	ssid = CONFIG_DEMO_WIFI_SSID;
	passwd = CONFIG_DEMO_WIFI_PASSWORD;

	/* Copy SSID and password into the Wi-Fi association request structure */
	memcpy(demo_assoc_req.ssid, ssid, strlen(ssid));
	memcpy(demo_assoc_req.key, passwd, strlen(passwd));
	demo_assoc_req.auth = CONFIG_DEMO_WIFI_AUTH; /* Set authentication type from config */
	demo_assoc_req.pairwise = CONFIG_DEMO_WIFI_PAIRWISE; /* Set pairwise cipher type from config */

	for (i = 0 ; i < SCM_WIFI_MAC_LEN ; i++)
		demo_assoc_req.bssid[0] = 0;

	/* Set Wi-Fi station configuration */
	if (scm_wifi_sta_set_config(&demo_assoc_req, NULL)){
		SCM_ERR_LOG(PROTO_COMMON_TAG, "WiFi configuration is failed\n");
		return;
	}
	SCM_DBG_LOG(PROTO_COMMON_TAG, "WiFi connecting... %s\n", demo_assoc_req.ssid);

	/* Attempt to connect to the Wi-Fi network */
	ret = scm_wifi_sta_connect();
	if (ret) {
		SCM_ERR_LOG(PROTO_COMMON_TAG, "WiFi Connection is failed(%d)\n", ret);
	}
}

#endif
