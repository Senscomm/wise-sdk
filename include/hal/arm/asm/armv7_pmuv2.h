/*
 * ARMv7 PMUv2 Performance Monitoring Unit
 *
 * Copyright (c) 2020-2024 Senscomm Semiconductor Co., Ltd. All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef __WISE_ARMV7_PMUV2_H__
#define __WISE_ARMV7_PMUV2_H__


struct armv7_pmnc_event {
	int event;
	const char *name;
};

/* PMCR, Control Register */
static inline u32 armv7_pmnc_read(void)
{
        u32 val;
        asm volatile("mrc p15, 0, %0, c9, c12, 0" : "=r"(val));
        return val;
}

static inline void armv7_pmnc_write(u32 val)
{
        asm volatile ("isb");
        asm volatile("mcr p15, 0, %0, c9, c12, 0" : : "r"(val));
}


/* PMSELR: Event Count Selection Register */
static inline void armv7_pmnc_select_counter(int counter)
{
	asm volatile("mcr p15, 0, %0, c9, c12, 5" : : "r" (counter));
        asm volatile ("isb");
}

/* PMXEVTYPER, Event Type Select Register */
static inline void armv7_pmnc_write_evtsel(int counter, u32 event)
{
	armv7_pmnc_select_counter(counter);
	event &= 0xff;
	asm volatile("mcr p15, 0, %0, c9, c13, 1" : : "r" (event));
        asm volatile ("isb");
}

/* PMXEVCNTR, Event Count Register */
static inline u32 armv7_pmnc_read_counter(int counter)
{
	u32 val;

	armv7_pmnc_select_counter(counter);
	asm volatile("mrc p15, 0, %0, c9, c13, 2" : "=r"(val));
	return val;
}

static inline void armv7_pmnc_write_counter(int counter, u32 val)
{
	armv7_pmnc_select_counter(counter);
	asm volatile("mcr p15, 0, %0, c9, c13, 2" : : "r"(val));
}

/* PMCNTENSEL, Count Enable Set register */
static inline void armv7_pmnc_enable_counter(int counter)
{
	asm volatile("mcr p15, 0, %0, c9, c12, 1" : : "r" (BIT(counter)));
}

/* PMCNTENCLR, Count Enable Clear register */
static inline void armv7_pmnc_disable_counter(int counter)
{
	asm volatile("mcr p15, 0, %0, c9, c12, 2" : : "r" (BIT(counter)));
}


static inline void armv7_pmnc_start(struct armv7_pmnc_event *events, size_t n_events)
{
	int counter;

	for (counter = 0; counter < n_events; counter++) {
		armv7_pmnc_disable_counter(counter);
		armv7_pmnc_write_evtsel(counter, events[counter].event);
		armv7_pmnc_write_counter(counter, 0);
		armv7_pmnc_enable_counter(counter);
	}
	armv7_pmnc_write(armv7_pmnc_read() | BIT(0));
}

static inline void armv7_pmnc_stop(struct armv7_pmnc_event *events, size_t n_events)
{
	int counter;
	u32 value;

	armv7_pmnc_write(armv7_pmnc_read() & ~BIT(0));

	for (counter = 0; counter < n_events; counter++) {
		armv7_pmnc_disable_counter(counter);
	}
	for (counter = 0; counter < n_events; counter++) {
		value = armv7_pmnc_read_counter(counter);
		printk("%s: %d\n", events[counter].name, value);
	}
}


#endif /* __WISE_ARMV7_PMUV2_H__ */
