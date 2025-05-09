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

#ifndef __WIFI_CHANNEL_API_H__
#define __WIFI_CHANNEL_API_H__

#include "wise_channel.h"
#include "wise_wifi_types.h"

int scm_channel_register_rx_cb(wise_channel_cb_fn fn);
int scm_channel_send_to_host(char *buf, int len);
int scm_channel_construct_ip_msg(char *net_if, char *msg, int *msg_len);
int scm_channel_add_filter(char *filter, wifi_filter_type type);
int scm_channel_del_filter(char *filter, wifi_filter_type type);
int scm_channel_query_filter(char **filter, int *num, wifi_filter_type type);
int scm_channel_set_default_filter(wifi_packet_filter direction);
int scm_channel_reset_filter(wifi_filter_type type);
uint32_t scm_channel_host_ready(void);


#endif /* __WIFI_CHANNEL_API_H__ */
