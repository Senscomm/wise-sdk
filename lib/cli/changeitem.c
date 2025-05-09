/*
 * Copyright (c) 2018-2024 Senscomm Semiconductor Co., Ltd. All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */
#include <stdio.h>
#include <string.h>
#include <hal/kernel.h>
#include <hal/kmem.h>
#include <changeitem.h>

static
bool changeitem_advance(struct changeitem *this)
{
	int lower, upper, next;
	/*
	 * case 1 (step > 0) : [start, current, end]
	 * case 2 (step < 0) : [end, current, start]
	 */
	if (this->step > 0) {
		lower = this->start;
		upper = this->end;
	} else if (this->step < 0) {
		lower = this->end;
		upper = this->start;
	} else
		return false;

	next = this->current + this->step;
	if (next >= lower && next <= upper) { /* inclusive */
		this->current = next;
		return true;
	}

	return false;
}

static
struct changeitem *changeitem_next(struct changeitem *this)
{
	if (!list_is_last(&this->list, this->head))
		return list_entry(this->list.next, struct changeitem, list);

	return NULL;
}

static
const char *changeitem_name(struct changeitem *this)
{
	return this->name;
}

static
int changeitem_current(struct changeitem *this)
{
	return this->current;
}

static
void changeitem_init(struct changeitem *this)
{
	this->current = this->start;
}

static
bool changeitem_change(struct changeitem *this)
{
	bool ret;
	struct changeitem *next = this->p_next(this);

	if (next) {
		ret = next->m_change(next);
		if (ret == false) {
			next->m_init(next);
			ret = changeitem_advance(this);
		}
	} else
		ret = changeitem_advance(this);

	return ret;
}

static
void changeitem_print(struct changeitem *this)
{
	printf("%-28s : %4d [%4d:%4d,%4d]\n",
			this->m_name(this),
			this->m_current(this),
			this->start,
			this->end,
			this->step);
}

static
bool changeitem_is_included(struct changeitem *this,
		struct changeitem *other)
{
	if (strncmp(this->name, other->name, ARRAY_SIZE(this->name)))
		return false;
	if (other->end >= 0)
		return (other->start <= this->current && this->current <= other->end);
	else
		return (other->start <= this->current);
}

struct changeitem *create_changeitem(const char *name,
		int start, int end, int step)
{
	struct changeitem *ci = kmalloc(sizeof(struct changeitem));

	assert(ci);

	memset(ci->name, 0, ARRAY_SIZE(ci->name));
	strncpy(ci->name, name, ARRAY_SIZE(ci->name));
	ci->start = start;
	ci->end = end;
	ci->step = step;

	ci->p_next = changeitem_next;
	ci->m_name = changeitem_name;
	ci->m_current = changeitem_current;
	ci->m_init = changeitem_init;
	ci->m_change = changeitem_change;
	ci->m_print = changeitem_print;
	ci->m_is_included = changeitem_is_included;

	ci->m_init(ci);

	return ci;
}

void destroy_changeitem(struct changeitem *this)
{
	kfree(this);
}
