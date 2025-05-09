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

#include <mbuf.h>
#include <kernel.h>
#include <wifi_repeater.h>
#include <hal/console.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

struct wifi_ipv4_filter filter_def_setting[] = {
	/* DHCP */
	{
		0,							 /* remote ip */
		68,							 /* local port */
		0,							 /* localp_min */
		0,							 /* localp_max */
		0,							 /* remote_port */
		0,							 /* remotep_min */
		0,							 /* remotep_max */
		17,							 /* packet type */
		WIFI_FILTER_TO_LWIP,					 /* config_type */
		WIFI_FILTER_MASK_LOCAL_PORT | WIFI_FILTER_MASK_PROTOCOL, /* match_mask */
	},
	/* Iperf tcp server*/
	{
		0,							 /* remote ip */
		5201, 0, 0,						 /* local port */
		0, 0, 0,						 /* remote port */
		6,							 /* packet type */
		WIFI_FILTER_TO_HOST,					 /* config_type */
		WIFI_FILTER_MASK_LOCAL_PORT | WIFI_FILTER_MASK_PROTOCOL, /* match_mask */
	},

	/* Iperf tcp client */
	{
		0,							  /* remote ip */
		0, 0, 0,						  /* local port */
		5201, 0, 0,						  /* remote port */
		6,							  /* packet type */
		WIFI_FILTER_TO_HOST,					  /* config_type */
		WIFI_FILTER_MASK_REMOTE_PORT | WIFI_FILTER_MASK_PROTOCOL, /* match_mask */
	},

	/* Iperf udp server */
	{
		0,							 /* remote ip */
		5201, 0, 0,						 /* local port */
		0, 0, 0,						 /* remote port */
		17,							 /* packet type */
		WIFI_FILTER_TO_HOST,					 /* config_type */
		WIFI_FILTER_MASK_LOCAL_PORT | WIFI_FILTER_MASK_PROTOCOL, /* match_mask */
	},

	/* Iperf udp client */
	{
		0,							  /* remote ip */
		0, 0, 0,						  /* local port */
		5201, 0, 0,						  /* remote port */
		17,							  /* packet type */
		WIFI_FILTER_TO_HOST,					  /* config_type */
		WIFI_FILTER_MASK_REMOTE_PORT | WIFI_FILTER_MASK_PROTOCOL, /* match_mask */
	},
	/* Testing*/
	{
		0,		     /* remote ip */
		1234, 0, 0,	     /* localp_max */
		0, 1000, 60000,	     /* remote port */
		6,		     /* packet type */
		WIFI_FILTER_TO_HOST, /* config_type */
		WIFI_FILTER_MASK_LOCAL_PORT | WIFI_FILTER_MASK_PROTOCOL |
			WIFI_FILTER_MASK_REMOTE_PORT_RANGE, /* match_mask */
	},

};

extern void scdc_input(struct ifnet *ifp, struct mbuf *m);

int tusb_wifi_filter_init(void)
{
	int index;

	if (wifi_repeater_init(WIFI_REPEATER_WLAN0, scdc_input)) {
		return -1;
	}

	wifi_repeater_set_default_dir(WIFI_REPEATER_WLAN0, WIFI_FILTER_TO_HOST);
	for (index = 0; index < ARRAY_SIZE(filter_def_setting); index++) {
		if (wifi_repeater_add_filter(WIFI_REPEATER_WLAN0,
			(char *) &filter_def_setting[index], WIFI_FILTER_TYPE_IPV4)) {
			printk("add filter failed \n");
		}
	}

	return 0;
}


