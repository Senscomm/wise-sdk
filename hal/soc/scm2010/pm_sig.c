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

#include <stdint.h>
#include <linker.h>

#include <hal/device.h>
#include <hal/console.h>
#include <hal/irq.h>

#include "pm_rram.h"
#include "pm_sig.h"

static int pm_sig_irq(int irq, void *data)
{
	struct pm_sig_msg *msg = (struct pm_sig_msg *)SCM2010_PM_RRAM_SIG_DATA;
	pm_sig_cb cb = data;

	if (cb) {
		cb(msg);
	}

	return 0;
}

int pm_sig_init(pm_sig_cb cb)
{
	if (request_irq(CONFIG_PM_SIG_IRQ, pm_sig_irq, "pm_sig", 1, cb)) {
		printk("failed to request irq\n");
		return -1;
	}

	return 0;
}
