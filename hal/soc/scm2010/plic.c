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
#include <hal/irq.h>
#include <hal/console.h>
#include <hal/kmem.h>
#include <hal/init.h>

#include <stdio.h>
#include <string.h>


#if 0
#define warn(args...) printk(args);
#define dbg(args...) printk(args);
#else
#define warn(args...)
#define dbg(args...)
#endif

//#define DEBUG_CLAIM_COMPL

/* XXX: PLIC int_src starts from 1. */
#define is_valid_irq(x) (((x) > 0) && ((x) <= CONFIG_NR_IRQ))

#define set_intprio(i, prio) 		do { 				\
	__nds__plic_set_priority(i, prio);				\
} while(0)

#define enable_int(i)		do {				\
	__nds__plic_enable_interrupt(i);			\
} while (0)

#define disable_int(i)		do {				\
	__nds__plic_disable_interrupt(i);			\
} while (0)

struct irq_entry {
	const char *name;
	int (*handler)(int, void *);
	int priority;
	int count;
#ifdef DEBUG_CLAIM_COMPL
	int claim;
	int completion;
#endif
	void *priv;
	struct irq_entry *next;
};

static struct irq_entry irq_id_0 = { .name = "no interrupt" }; /* an entry for 'no interrupt' interrupt ID */
static struct irq_entry *irq_table[CONFIG_NR_IRQ];

__ilm__ void irq_handler(int irqnr)
{
	struct irq_entry *irq;
	int ret = -1;

	if (!is_valid_irq(irqnr))
		return;

	irq = irq_table[irqnr];
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

int request_irq(int irq, int (*handler)(int, void *), const char *name,
		unsigned priority, void *priv)
{
	struct irq_entry *i, *p = NULL;

	dbg("IRQ: %s request irq=%d (priority=%d)\n", name, irq, priority);

	if (!is_valid_irq(irq))
		return -1;

	for (i = irq_table[irq]; i != NULL; i = i->next) {
		if (strcmp(i->name, name) == 0 && i->handler == handler)
			goto out;
		p = i;
	}

	i = zalloc(sizeof(*i));
	if (i == NULL)
		return -ENOMEM;

	i->name = name;
	i->handler = handler;
	i->priority = priority;
	i->priv = priv;
	i->count = 0;
	i->next = NULL;

	if (p == NULL)
		irq_table[irq] = i;
	else
		p->next = i;

 out:
	set_intprio(irq, priority);

	enable_int(irq);

	return 0;
}

void free_irq(int irq, const char *name)
{
	struct irq_entry *i, *p = NULL;

	if (!is_valid_irq(irq))
		return;
	dbg("IRQ: %s free irq=%d\n", name, irq);

	disable_int(irq);

	for (i = irq_table[irq]; i != NULL; i = p) {
		if (strcmp(i->name, name) == 0)
			goto found;
		p = i;
	}
	warn("IRQ: handler not found for irq=%d, name=%s\n", irq, name);
	return;

 found:
	if (p == NULL) {
		irq_table[irq] = NULL;
	} else {
		p->next = NULL;
		/* There is still a driver handling this interrupt */
		enable_int(irq);
	}
	kfree(i);
}

/* At the time of writing, interrupt nesting is not supported, so do not use
the default mext_interrupt() implementation as that enables interrupts.  A
version that does not enable interrupts is provided below.  THIS INTERRUPT
HANDLER IS SPECIFIC TO FREERTOS WHICH USES PLIC! */

__ilm__ void mext_interrupt(void)
{
	unsigned irqnr = __nds__plic_claim_interrupt();
	struct irq_entry *ie __maybe_unused = irq_table[irqnr];

#ifdef DEBUG_CLAIM_COMPL
	if (ie) ie->claim++;
#endif

	/* Do interrupt handler */
	irq_handler(irqnr);

	__nds__plic_complete_interrupt(irqnr);

#ifdef DEBUG_CLAIM_COMPL
	if (ie) ie->completion++;
#endif
}

#include <hal/console.h>

int get_irq_stat(char *buf, size_t size)
{
	struct irq_entry *irq;
	int i;
	char *end = buf + size;

#ifdef DEBUG_CLAIM_COMPL
	buf += snprintk(buf, end - buf, "IRQ:%8s%8s%10s%8s   %8s\n", "Claimed", "Served", "Completed", "PRIO", "");
#else
	buf += snprintk(buf, end - buf, "IRQ:%8s%8s   %8s\n", "CPU0", "PRIO", "");
#endif

	for (i = 0; i < CONFIG_NR_IRQ; i++) {
		irq = irq_table[i];
		if (irq == NULL)
			continue;

#ifdef DEBUG_CLAIM_COMPL
		buf += snprintk(buf, end - buf, "%3d:%8d%8d%10d%8d   %s",
				i, irq->claim, irq->count, irq->completion, irq->priority, irq->name);
#else
		buf += snprintk(buf, end - buf, "%3d:%8d%8d   %s",
				i, irq->count, irq->priority, irq->name);
#endif

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

void enable_irq(int irq)
{
    enable_int(irq);
}

void disable_irq(int irq)
{
    disable_int(irq);
}

int init_irq(void)
{
	int i;

	for (i = 0; i < CONFIG_NR_IRQ; i++) {
		disable_int(i);
	}

	irq_table[0] = &irq_id_0;

	return 0;
}

__initcall__(early, init_irq);
