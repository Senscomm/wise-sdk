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


#ifndef __WISE_CHANNEL_H__
#define __WISE_CHANNEL_H__

#include <stdint.h>
#include <stdbool.h>
#include "wise_wifi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_WIFI_FILTER_IPV4_CNT CONFIG_SUPPORT_WIFI_REPEATER_IPV4_CNT

typedef int (*wise_channel_cb_fn) (char *buf, int len);

struct wise_channel_status {
	int host_en;
	int host_net_en;
};

#ifdef CONFIG_API_SCMCHANNEL

wise_err_t wise_channel_host_en(int on);
wise_err_t wise_channel_host_carrier_on(int on);
int wise_channel_get_host_status(void);
int wise_channel_get_host_net_status(void);

wise_err_t wise_channel_register_rx_cb(wise_channel_cb_fn fn);
wise_err_t wise_wifi_set_default_filter(wifi_packet_filter direction);
wise_err_t wise_wifi_add_filter(char *filter, wifi_filter_type type);
wise_err_t wise_wifi_del_filter(char *filter, wifi_filter_type type);
wise_err_t wise_wifi_query_filter(char **filter, int *num, wifi_filter_type type);
wise_err_t wise_wifi_init_filter(wifi_filter_type type);
wise_err_t wise_wifi_deinit_filter(wifi_filter_type type);
wise_err_t wise_wifi_free_filters(wifi_filter_type type);
#else
inline wise_err_t wise_channel_host_en(int on) {return WISE_OK; }
inline wise_err_t wise_channel_host_carrier_on(int on) {return WISE_OK; }

#endif

#ifdef __cplusplus
}
#endif

#endif /* __WISE_CHANNEL_H__ */
