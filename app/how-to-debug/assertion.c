/*
 * Copyright 2024-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
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
#include <assert.h>
#include <cli.h>
#include <hal/rom.h>

#if ASSERT_VERBOSITY == 0
#error "Need to set ASSERT_VERBOSITY as non-zero for this demo."
#endif

int debug_assertion(void)
{
	printf("Assertion demo.\n");

	return 0;
}

/* Simple assertion to check divide-by-zero.
 */

void check_divide_by_zero(int divisor)
{
	assert(divisor != 0);
}

int do_divide(int argc, char *argv[])
{
	int divisor, dividend;

	argc--, argv++;

	if (argc == 2) {
		dividend = strtoul(argv[0], NULL, 10);
		divisor = strtoul(argv[1], NULL, 10);
	} else {
		return CMD_RET_USAGE;
	}

	check_divide_by_zero(divisor);

	printf("Quotient is %d.\n", dividend / divisor);

	return CMD_RET_SUCCESS;
}

CMD(divide, do_divide,
	"Divide an integer",
	"divide <dividend> <divisor>"
);

#ifdef CONFIG_NET80211

/* Assertion occurring in the ROM library, in this case, net80211.
 */
#include <sys/cdefs.h>
#include <arpa/inet.h>

#include "opt_wlan.h"
#include "opt_inet.h"

#include "compat_param.h"
#include "systm.h"
#include "mbuf.h"
#include "kernel.h"

#include "compat_if.h"
#include "if_llc.h"
#include "if_media.h"

#include "cmsis_os.h"

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_input.h>

#include <netinet/in.h>
#include "ethernet.h"

static bool nullify_ni = false;

int patch_ieee80211_input_mimo(struct ieee80211_node *ni, struct mbuf *m)
{
    struct ieee80211_rx_stats *rxs;

    rxs = (struct ieee80211_rx_stats *)ieee80211_get_rx_params_ptr(m);
    if (rxs == NULL) {
        return (-1);
    }

    return ni->ni_vap->iv_input(nullify_ni ? NULL : ni, m, rxs, rxs->c_rssi, rxs->c_nf);
}

extern int (*ieee80211_input_mimo)(struct ieee80211_node * ni, struct mbuf * m);

PATCH(ieee80211_input_mimo, &ieee80211_input_mimo, &patch_ieee80211_input_mimo);

int do_nullify_ni(int argc, char *argv[])
{
	argc--, argv++;

	if (argc == 1) {
		nullify_ni = strtoul(argv[0], NULL, 10) ? true : false;
	} else {
		return CMD_RET_USAGE;
	}
	return CMD_RET_SUCCESS;
}

CMD(nullify_ni, do_nullify_ni,
	"Nullify ni for demo",
	"nullify_ni <1|0>"
);

#endif
