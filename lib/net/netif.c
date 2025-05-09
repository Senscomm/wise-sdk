/*
 * Copyright 2023-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <sys/cdefs.h>
#include <stdio.h>

#include "compat_if.h"
#include "if_arp.h"
#include "if_llc.h"
#include "ethernet.h"
#include "if_dl.h"
#include "if_media.h"
#include "compat_if_types.h"
#include "if_ether.h"
#include "lwip-glue.h"

__weak void nuttx_ifattach(struct ifnet *ifp)
{
}

__weak void nuttx_ifdetach(struct ifnet *ifp)
{
}

__weak void scdc_ifattach(struct ifnet *ifp)
{
}

__weak void scdc_ifdetach(struct ifnet *ifp)
{
}

void netif_ifattach(struct ifnet *ifp)
{
	nuttx_ifattach(ifp);
#ifndef CONFIG_SUPPORT_WIFI_REPEATER
	scdc_ifattach(ifp);
#endif
}

void netif_ifdetach(struct ifnet *ifp)
{
	nuttx_ifdetach(ifp);
#ifndef CONFIG_SUPPORT_WIFI_REPEATER
	scdc_ifdetach(ifp);
#endif
}
