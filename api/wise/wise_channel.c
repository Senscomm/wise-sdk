/*
 * Copyright 2023-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "mbuf.h"
#include "compat_if.h"
#include "if_media.h"

#include <libifconfig.h>
#include <netinet/in.h>

#include "wise_err.h"
#include "wise_log.h"
#include "wise_channel.h"
#include "wise_event.h"

#define SNCM_MAX_MSG_SIZE              1500
#define SNCM_CHANNEL_STR "sncm_channel_send "

extern int (*fwil_scm_channel_cb) (char *buf, int len);

struct wise_channel_status g_host_status = {0};
int (* scm_channel_cb) (char *buf, int len) = NULL;

#define WISE_CHANNEL_IS_HOST_CARRIER_ON() (g_host_status.host_en && g_host_status.host_net_en)

static int wise_channel_update_connect_status(void)
{
	system_event_t evt;
	ifconfig_handle_t *h	 = NULL;
	int linkstate = 0;
	int ret = 0;

	h = ifconfig_open();
	if (h == NULL)
		return -1;

	if (ifconfig_get_linkstate(h, "wlan0", &linkstate) < 0) {
		ret = -1;
		goto exit;
	}

	if (linkstate & LINK_STATE_UP) {
		memset(&evt, 0, sizeof(evt));
		evt.event_id = SYSTEM_EVENT_SCM_LINK_UP;
		evt.event_info.connected.not_en_dhcp = true;
		wise_event_send(&evt);
	}

exit:
	ifconfig_close(h);

	return ret;
}

wise_err_t wise_channel_set_carrier_to_repeater(void)
{
	uint32_t on = WISE_CHANNEL_IS_HOST_CARRIER_ON();
	wifi_repeater_host_carrier_getset(true, &on);
	return WISE_OK;
}

int wise_channel_get_host_net_status(void)
{
	return g_host_status.host_net_en;
}

int wise_channel_get_host_status(void)
{
	return g_host_status.host_en;
}

wise_err_t wise_channel_host_en(int on)
{
	g_host_status.host_en = on;

	/* if host is on, send connect event to host if needed. */
	if (on)
		wise_channel_update_connect_status();

	return WISE_OK;
}

wise_err_t wise_channel_host_carrier_on(int on)
{

	g_host_status.host_net_en = on;
	if (WISE_CHANNEL_IS_HOST_CARRIER_ON())
		wise_channel_update_connect_status();

	/* no matter host carrier on or off we should update to repeater module */
	wise_channel_set_carrier_to_repeater();
	return WISE_OK;
}


static int wise_channel_hdr_check(char *buf, int len)
{
	if (!buf || !len || len > SNCM_MAX_MSG_SIZE)
		return -1;
	return memcmp(SNCM_CHANNEL_STR, buf, strlen(SNCM_CHANNEL_STR));
}

int wise_channel_hdr_remove(        char *cmd_buf, int *payload_len, char *buf, int buf_len)
{
	const char *prefix = SNCM_CHANNEL_STR;
	int prefix_len = strlen(prefix);
	char *start = strstr((char *) buf, prefix);

	if (start) {
		start += prefix_len;  // move the pointer to the end of the prefix
		*payload_len = buf_len - prefix_len;
	} else {
		return -1;
	}

	memcpy(cmd_buf, start, *payload_len);
	cmd_buf[*payload_len] = '\0';
	return 0;
}

static int wise_channel_rx_msg(char *buf, int len)
{
	wise_err_t ret = WISE_FAIL;
	char   cmd_buf[SNCM_MAX_MSG_SIZE] = {0};
	int payload_len = 0;

	if (scm_channel_cb == NULL) {
		printf("scm_channel_cb not registered \n");
		goto exit;
	}

	if (wise_channel_hdr_check(buf, len)) {
		printf("scm_channel_msg error %s %d \n", buf, len);
		goto exit;
	}

	if (wise_channel_hdr_remove(cmd_buf, &payload_len, buf, len))
		goto exit;

	ret = scm_channel_cb(cmd_buf, payload_len);

exit:

	return ret;

}

wise_err_t wise_channel_register_rx_cb(wise_channel_cb_fn fn)
{

	if (!fn)
		return WISE_FAIL;

	scm_channel_cb = fn;
	fwil_scm_channel_cb = wise_channel_rx_msg;

	return WISE_OK;
}

wise_err_t wise_wifi_set_default_filter(wifi_packet_filter direction) {

	return wifi_repeater_set_default_dir(WIFI_REPEATER_WLAN0, direction);
}

wise_err_t wise_wifi_add_filter(char *filter, wifi_filter_type type) {

	return wifi_repeater_add_filter(WIFI_REPEATER_WLAN0, filter, type);
}

wise_err_t wise_wifi_del_filter(char *filter, wifi_filter_type type) {

	return wifi_repeater_del_filter(WIFI_REPEATER_WLAN0, filter, type);
}

wise_err_t wise_wifi_query_filter(char **filter, int *num, wifi_filter_type type) {

	return wifi_repeater_query_filter(WIFI_REPEATER_WLAN0, filter, num, type);
}

extern void scdc_input(struct ifnet *ifp, struct mbuf *m);
wise_err_t wise_wifi_init_filter(wifi_filter_type type) {

	return wifi_repeater_init(WIFI_REPEATER_WLAN0, scdc_input);
}

wise_err_t wise_wifi_deinit_filter(wifi_filter_type type) {

	return wifi_repeater_deinit(WIFI_REPEATER_WLAN0);
}

wise_err_t wise_wifi_free_filters(wifi_filter_type type) {

	return wifi_repeater_free_filters(WIFI_REPEATER_WLAN0, type);
}
