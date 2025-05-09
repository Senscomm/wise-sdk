/*
 * Copyright 2020-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <soc.h>
#include <hal/cmsis.h>
#include <hal/kernel.h>
#include <hal/device.h>
#include <hal/sw-irq.h>
#include <hal/console.h>
#include <hal/kmem.h>

#include <stdio.h>
#include <string.h>


#if 0
#define warn(args...) printk(args);
#define dbg(args...) printk(args);
#else
#define warn(args...)
#define dbg(args...)
#endif

/* XXX: PLIC int_src starts from 1. */
#define is_valid_irq(x) (((x) > 0) && ((x) <= CONFIG_NR_SW_IRQ))

#define set_intprio(i, prio) 		do { 		\
	__nds__plic_sw_set_priority(i, prio);		\
} while(0)

#define enable_int(i)		        do {		\
	__nds__plic_sw_enable_interrupt(i);			\
} while (0)

#define disable_int(i)		        do {		\
	__nds__plic_sw_disable_interrupt(i);		\
} while (0)

struct sw_irq_entry {
	const char *name;
	int (*handler)(int, void *);
	int priority;
	int count;
	void *priv;
	struct sw_irq_entry *next;
};

static struct sw_irq_entry *sw_irq_table[CONFIG_NR_SW_IRQ];

__ilm__ void sw_irq_handler(int irqnr)
{
	struct sw_irq_entry *irq;
	int ret = -1;

	if (!is_valid_irq(irqnr))
		return;

	irq = sw_irq_table[irqnr - 1];
	if (irq == NULL) {
		warn("IRQ: no handler found for irq %d\n", irqnr);
		return;
	}
	while (irq) {
		if (irq->handler)
			ret = irq->handler(irqnr, irq->priv);

		if (ret < 0)
			warn("IRQ: irq %d not properly handled (ret=%d)\n", irqnr, ret);

		irq->count++;
		irq = irq->next;
	}
}

int request_sw_irq(int irq, int (*handler)(int, void *), const char *name,
		unsigned priority, void *priv)
{
	struct sw_irq_entry *i, *p = NULL;

	dbg("IRQ: %s request irq=%d (priority=%d)\n", name, irq, priority);

	if (!is_valid_irq(irq))
		return -1;

	for (i = sw_irq_table[irq - 1]; i != NULL; i = i->next) {
		if (strcmp(i->name, name) == 0 && i->handler == handler)
			goto out;
		p = i;
	}

	i = kmalloc(sizeof(*i));
	if (i == NULL)
		return -ENOMEM;

	i->name = name;
	i->handler = handler;
	i->priority = priority;
	i->priv = priv;
	i->count = 0;
	i->next = NULL;

	if (p == NULL)
		sw_irq_table[irq - 1] = i;
	else
		p->next = i;

 out:
	set_intprio(irq, priority);

	enable_int(irq);

	return 0;
}

void free_sw_irq(int irq, const char *name)
{
	struct sw_irq_entry *i, *p = NULL;

	if (!is_valid_irq(irq))
		return;
	dbg("IRQ: %s free irq=%d\n", name, irq);

	disable_int(irq);

	for (i = sw_irq_table[irq - 1]; i != NULL; i = p) {
		if (strcmp(i->name, name) == 0)
			goto found;
		p = i;
	}
	warn("IRQ: handler not found for irq=%d, name=%s\n", irq, name);
	return;

 found:
	if (p == NULL) {
		sw_irq_table[irq - 1] = NULL;
	} else {
		p->next = NULL;
		/* There is still a driver handling this interrupt */
		enable_int(irq);
	}
	kfree(i);
}

/* At the time of writing, interrupt nesting is not supported, so do not use
the default mswi_handler() implementation as that enables interrupts.  A
version that does not enable interrupts is provided below.  THIS INTERRUPT
HANDLER IS SPECIFIC TO FREERTOS WHICH USES PLIC! */
__ilm__ void mswi_handler(void)
{
	unsigned irqnr = __nds__plic_sw_claim_interrupt();

	/* Jump to the software interrupt handler */
	sw_irq_handler(irqnr);

	__nds__plic_sw_complete_interrupt(irqnr);
}

#include <hal/console.h>

int get_sw_irq_stat(char *buf, size_t size)
{
	struct sw_irq_entry *irq;
	int i;
	char *end = buf + size;

	buf += snprintk(buf, end - buf, "SWI:%8s%8s   %8s\n", "CPU0", "PRIO", "");

	for (i = 1; i <= CONFIG_NR_SW_IRQ; i++) {
		irq = sw_irq_table[i - 1];
		if (irq == NULL || irq->name == NULL)
			continue;

		buf += snprintk(buf, end - buf, "%3d:%8d%8d   %s",
				i, irq->count, irq->priority, irq->name);

		while ((irq = irq->next)) {
			buf += snprintk(buf, end - buf, ", %s", irq->name);
			if (buf > end)
				return -1;
		}
		buf += snprintk(buf, end - buf, "\n");
		if (buf > end)
			return -1;
	}

	return 0;
}
