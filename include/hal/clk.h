/*
 * Copyright 2021-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef __CLK_H__
#define __CLK_H__

#include <hal/device.h>

struct clk;

struct clk_ops {
	int (*enable)(struct clk *clk, int enable);
	int (*set_rate)(struct clk* clk, u32 rate);
	u32 (*get_rate)(struct clk *clk);
	int (*set_parent)(struct clk *clk, struct clk *parent);
	struct clk *(*get_parent)(struct clk *clk);
	/* XXX: Must add new callback at the end. */
	int (*set_div)(struct clk* clk, u32 div);
	u32 (*get_div)(struct clk *clk);
};

struct clk_hw {
	u32 *base;
	u32 shift; /* start bit position */
    u32 mask;  /* bit mask */
};

struct clk {
	struct clk *parent;
	const char *name;
	u32 rate; /* in HZ */

	bool enable;
	u32 div;
	int sel;

	struct clk **cmx;
	int nr_input; /* also used for a divider as 2 ^ (# of bits) */

	struct clk_ops *ops;

	struct clk_hw hw;

	struct list_head list;
	struct list_head children;
};

#define CLK(name) ll_entry_declare(struct clk, name, clk)
#define CLK_ARRAY(name) ll_entry_declare_list(struct clk, name, clk)

#define clksym(name) llsym(struct clk, name, clk)

/**
 * clk_get() - find clock
 * @dev: clock consumer
 * @id: clock id
 *
 * Return: clk object if found, NULL otherwise
 */
struct clk *clk_get(struct device *dev, const char *id);

/**
 * clk_enable() - enables/disables clock
 * @clk: clock
 * @enable: true/false
 *
 * Return: 0 if successful, negative otherwise
 */
int clk_enable(struct clk* clk, int enable);

/**
 * clk_get_rate() - get clk frequency
 * @clk: clock
 *
 * Return: current clock frequency. 0 if invalid or unknown.
 */
u32 clk_get_rate(struct clk *clk);

/**
 * clk_set_rate() - set clock rate
 * @clk: clock to set rate
 * @rate: target rate
 *
 * Return: 0 if successful, negative otherwise
 *
 * Note:
 * The actual set rate can be different from @rate.
 */
int clk_set_rate(struct clk *clk, u32 rate);


/**
 * clk_get_div() - get clk divider
 * @clk: clock
 *
 * Return: current clock divider. 0 if invalid or unknown.
 */
u32 clk_get_div(struct clk *clk);

/**
 * clk_set_div() - set clock divider
 * @clk: clock to set divider
 * @div: target divider
 *
 * Return: 0 if successful, negative otherwise
 */
int clk_set_div(struct clk *clk, u32 div);

/**
 * clk_set_parent() - set parent clock
 * @clk: clock of which parent to change
 * @parent: parent clock
 *
 * Return: 0 if successful, negative otherwise
 */
int clk_set_parent(struct clk *clk, struct clk *parent);
struct clk *clk_get_parent(struct clk *clk);

int clk_num_source(struct clk *clk);
struct clk* clk_get_source(struct clk *clk, int i);

void clk_print(struct clk *clk, int verbose);
void clock_init(void);
void clock_postinit(void);

#endif
