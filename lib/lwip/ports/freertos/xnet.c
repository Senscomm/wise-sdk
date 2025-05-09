/*
 * Copyright 2018-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/**
 * @file
 * Interface Identification APIs from:
 *              RFC 3493: Basic Socket Interface Extensions for IPv6
 *                  Section 4: Interface Identification
 *
 * @defgroup if_api Interface Identification API
 * @ingroup socket
 */

/*
 * Copyright (c) 2017 Joel Cunningham, Garmin International, Inc. <joel.cunningham@garmin.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Joel Cunningham <joel.cunningham@me.com>
 *
 */
#include "lwip/opt.h"
#include "lwip/errno.h"
#include "lwip/netifapi.h"
#include "lwip/priv/sockets_priv.h"

#undef set_errno
#define set_errno(err) do { \
	errno = err; \
	} while (0);

#if LWIP_NETIF_API

char* os_if_indextoname(unsigned int ifindex, char *ifname)
{
	if (ifindex <= 0xff) {
		err_t err = netifapi_netif_index_to_name((u8_t)ifindex, ifname);
		if (!err && ifname[0] != '\0') {
			return ifname;
		}
	}
	set_errno(ENXIO);
	return NULL;
}

unsigned char os_if_nametoflags(char *ifname)
{
	err_t err;
	u8_t flags;

	err = netifapi_netif_name_to_flags(ifname, &flags);
	if (!err) {
		return flags;
	}
	return 0;
}

unsigned int os_if_nametoindex(const char *ifname)
{
	err_t err;
	u8_t idx;

	err = netifapi_netif_name_to_index(ifname, &idx);
	if (!err) {
		return idx;
	}
	return 0; /* invalid index */
}

#include <freebsd/net/if.h>
#include <string.h>

void os_if_freenameindex(struct if_nameindex *ptr)
{
	struct if_nameindex *ifs = ptr;

	while (ifs->if_name) {
		mem_free(ifs->if_name);
		ifs->if_name = NULL;
		ifs++;
	}

	mem_free(ptr);
}

/* Simple implementation of if_nameindex */
struct if_nameindex *os_if_nameindex(void)
{
	int n_ifs = 0, i;
	char ifname[IFNAMSIZ + 10];
	struct if_nameindex *ifs;

	/* We assume there is no hole in ifindex space */
	while (if_indextoname(n_ifs + 1, ifname)) {
		n_ifs++;
	}

	ifs = mem_malloc(sizeof(*ifs) * (n_ifs + 1));
	if (ifs == NULL)
		return NULL;

	memset(ifs, 0, sizeof(*ifs) * (n_ifs + 1));

	for (i = 0; i < n_ifs && if_indextoname(i + 1, ifname); i++) {
		if (if_nametoflags(ifname) & NETIF_FLAG_ROUTE)
			continue;

		ifs[i].if_index = i + 1;
		ifs[i].if_name = mem_malloc(IFNAMSIZ + 1);
		if (ifs[i].if_name == NULL)
			goto fail;
		strncpy(ifs[i].if_name, ifname, IFNAMSIZ);
	}
	/* Terminate the array */
	ifs[i].if_name = NULL;

	return ifs;

 fail:
	if_freenameindex(ifs);
	return NULL;
}

#endif

#include <hal/init.h>
#include <lwip/init.h>

static int net_init(void)
{
#if NO_SYS
    lwip_init();
#else
	tcpip_init(NULL, NULL);
#endif
	return 0;
}
__initcall__(subsystem, net_init);
