/*
 * Copyright 2018-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <hal/kernel.h>
#include <hal/clk.h>
#include <hal/console.h>
#include <hal/rom.h>

#include <string.h>
#include <stdio.h>

#define dbg(f...) printk(f)

#define clk_start() ll_entry_start(struct clk, clk)
#define clk_end() ll_entry_end(struct clk, clk)

__iram__ struct clk *
clk_get(struct device *dev, const char *id)
{
	struct clk *clk;
	char name[64];

	if (dev)
		sprintf(name, "%s/%s", dev_name(dev), id);
	else
		strcpy(name, id);

	for (clk = clk_start(); clk < clk_end(); clk++) {
		if (!strcmp(name, clk->name))
			return clk;
	}

	return NULL;
}

int clk_enable(struct clk* clk, int enable)
{
	if (!clk)
		return -1;

	clk->enable = enable;

	if (clk->ops && clk->ops->enable)
		return clk->ops->enable(clk, enable);

	return 0;
}

u32 clk_get_rate(struct clk *clk)
{
	u32 rate;

	assert(clk);

	rate = clk->rate;

	if (/*clk->rate == 0 */ clk->ops && clk->ops->get_rate)
		rate = clk->ops->get_rate(clk);
	else if (rate == 0
			&& clk->parent
			&& clk->parent != clk)
		rate = clk_get_rate(clk->parent);

	return rate;
}

int clk_set_rate(struct clk *clk, u32 rate)
{
	int ret = -1;

	if (!clk)
		return ret;

	if (clk->ops && clk->ops->set_rate) {
		ret = clk->ops->set_rate(clk, rate);
		if (ret > 0)
			clk->rate = ret;
	}

	return (ret > 0) ? 0: -1;
}

u32 clk_get_div(struct clk *clk)
{
	u32 div;

	assert(clk);

	div = clk->div;

	if (/*clk->div == 0 */ clk->ops && clk->ops->get_div)
		div = clk->ops->get_div(clk);

	return div;
}

int clk_set_div(struct clk *clk, u32 div)
{
	int ret = -1;

	if (!clk)
		return ret;

	if (clk->ops && clk->ops->set_div) {
		ret = clk->ops->set_div(clk, div);
		if (ret > 0)
			clk->div = ret;
	}

	return (ret > 0) ? 0: -1;
}


int clk_set_parent(struct clk *clk, struct clk *parent)
{
	int ret = 0;

	if (!clk || !parent)
		return -1;

	if (clk->parent == parent)
		goto out;

	if (clk->ops && clk->ops->set_parent)
		ret = clk->ops->set_parent(clk, parent);

	if (!ret)
		clk->parent = parent;

out:
	list_del_init(&clk->list);
	list_add_tail(&clk->list, &parent->children);
	return ret;
}

struct clk *clk_get_parent(struct clk *clk)
{
	if (!clk)
		return 0;

	if (!clk->parent && clk->ops && clk->ops->get_parent)
		clk->parent = clk->ops->get_parent(clk);

	if (0 && clk->parent && list_empty(&clk->list))
		list_add_tail(&clk->list, &clk->parent->children);
	return clk->parent;
}

/*
 * clk_num_source() - number of input clocks
 *
 */
int clk_num_source(struct clk *clk)
{
	if (!clk)
		return 0;

	if (clk->nr_input)
		return clk->nr_input;
	else if (clk->parent)
		return 1;
	else
		return 0; /* root clock */
}

/*
 * clk_get_source() - a particular clock source
 *
 * @clk: clock
 * @i: index to the source clock
 *
 * Returns: @i-th parent
 */
struct clk *clk_get_source(struct clk* clk, int i)
{
	if (!clk)
		return 0;

	if (clk->cmx == NULL
		|| clk->nr_input == 0)
			return clk->parent;

	if (i > clk->nr_input)
		return NULL;

	return clk->cmx[i];
}

#if 0
void clk_print(struct clk *clk, int verbose)
{
	if (verbose == 0) {
		printf("%s:\n", clk->name);
	} else {
		printf("%s: freq=%lu Hz, parent=%s\n", clk->name, clk_get_rate(clk),
			   clk->parent ? clk->parent->name : "none");
	}
}

void clk_debug_dump(void)
{
	struct clk *clk;

	for (clk = clk_start(); clk < clk_end(); clk++) {
		clk_print(clk, 1);
	}
}
#endif

__dram__
static struct clk root = {
	.name = "root",
	.parent = &root,
	.list = LIST_HEAD_INIT(root.list),
	.children = LIST_HEAD_INIT(root.children),
};

__iram__ void
clock_init(void)
{
	struct clk *clk;

	for (clk = clk_start(); clk < clk_end(); clk++) {
		INIT_LIST_HEAD(&clk->list);
		INIT_LIST_HEAD(&clk->children);
	}

	for (clk = clk_start(); clk < clk_end(); clk++) {
		struct clk *parent __maybe_unused;

		parent = clk_get_parent(clk);
		clk_set_parent(clk, parent? parent : &root);
		clk_set_rate(clk, clk_get_rate(clk));
		clk_enable(clk, true);
	}
}

__attribute__((weak)) void clock_postinit(void) {}

#ifdef CONFIG_CMD_CLK

#include <cli.h>

#define toMHz(x) ((x)/1000000)
#define toKHz(x) ((x)/1000)

__iram__ static int
clk_show(struct clk *clk, int level, int bitmap)
{
	struct clk *child;
	u32 rate;
	const char *unit;
	int i, is_last;

	for (i = level; i >= 0; i--) {
		is_last = (bitmap >> i) & 1;
		if (i)
			printf("%c   ", is_last ? ' ' : '|');
		else
			printf("%c-- ", is_last ? '`' : '|');
	}

	rate = clk_get_rate(clk);
	if (toMHz(rate) > 0) {
			rate = toMHz(rate);
			unit = "MHz";
	} else if (toKHz(rate) > 0) {
			rate = toKHz(rate);
			unit = "KHz";
	} else
			unit = "Hz";

	printf("%s\x1b[70G[ %c ]   %d%s\n",
			clk->name, clk->enable? '+' : ' ', rate, unit);

	list_for_each_entry(child, &clk->children, list) {
		is_last = list_is_last(&child->list, &clk->children);
		clk_show(child, level + 1, (bitmap << 1) | is_last);
	}

	return 0;
}


__iram__ int
clk_tree(int argc, char *argv[])
{
	printf("Clock tree\n");

	clk_show(&root, -1, 0);

	return CMD_RET_SUCCESS;
}

__dram__
CMD(clk, clk_tree,
	"display clock tree",
	"clk"
);
#endif
