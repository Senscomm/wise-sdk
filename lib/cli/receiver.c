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
#include <stdlib.h>
#include <string.h>
#include <hal/kernel.h>
#include <hal/kmem.h>
#include <compat_param.h>
#include <receiver.h>

static
int receiver_add_changeitem(struct receiver *this, const char *name, int start, int end, int step)
{
	if (this->cs)
		this->cs->m_add(this->cs, name, start, end, step);

	return 0;
}

static
int receiver_clear_cs(struct receiver *this)
{
	destroy_changeset(this->cs);
	this->cs = create_changeset();
	this->cs->m_init(this->cs);

	return 0;
}

static
int receiver_init_cs(struct receiver *this)
{
	if (this->cs)
		this->cs->m_init(this->cs);

	return 0;
}

static
int receiver_start(struct receiver *this, int repeat)
{
	this->repeat = repeat;

	if (this->scoreboard)
		kfree(this->scoreboard);

	this->scoreboard_size = howmany(this->m_expected_total(this), NBBY);
	this->scoreboard = kmalloc(this->scoreboard_size);
	if (!this->scoreboard) {
		printf("[%s, %d] failed to alloc scoreboard in %d bytes\n",
				__func__, __LINE__, this->scoreboard_size);
		return -1;
	}

	memset(this->scoreboard, 0, this->scoreboard_size);
	this->chksum_error = 0;

	if (this->p_setup(this))
		return -1;

	this->p_start_monitor(this);

	return 0;
}

static
int receiver_stop(struct receiver *this)
{
	this->p_stop_monitor(this);

	return 0;
}

static
void receiver_checksum(struct receiver *this, bool enable)
{
	this->checksum = enable;
}

static
bool receiver_is_checksum_enabled(struct receiver *this)
{
	return this->checksum;
}

static
s64 receiver_expected_total(struct receiver *this)
{
	bool more, passed;
	s64 n = 0;

	more = this->cs->m_init(this->cs);
	while (more) {
		passed = this->as ? this->as->m_assess(this->as, this->cs) : true;
		if (passed)
			n += this->repeat;
		more = this->cs->m_iter(this->cs);
	}

	return n;
}

/*
 * if target < 0, returns the index in scoreboard of item corresponding (ok, target) tuple
 * otherwise, returns the number of items in scoreboard corresponding (ok) condition
 */
static
s64 receiver_lookup(struct receiver *this, bool ok, s64 target)
{
	s64 n = 0, i, total;

	assert(this->scoreboard);

	total = this->m_expected_total(this);
	for (i = 0; i < total; i++) {
		bool check = ok ? isset(this->scoreboard, i) : isclr(this->scoreboard, i);
		if (check)
			n++;
		if (target >= 0 && n == target + 1)
			return i; /* return index of the target item */
	}

	return n; /* return total number */
}

static
s64 receiver_received(struct receiver *this)
{
	if (!this->scoreboard)
		return 0;

	return receiver_lookup(this, true, -1);
}

static
s64 receiver_missed(struct receiver *this)
{
	if (!this->scoreboard)
		return 0;

	return receiver_lookup(this, false, -1);
}

static
s64 receiver_error(struct receiver *this)
{
	return this->chksum_error;
}

static
struct changeset *receiver_query(struct receiver *this, bool ok, s64 index)
{
	s64 total, target;
	bool more, passed;

	if (!this->scoreboard)
		return NULL;

	total = receiver_lookup(this, ok, -1);
	if (index < 0 || index > total - 1)
		return NULL;

	target = receiver_lookup(this, ok, index);
	more = this->cs->m_init(this->cs);
	while (more) {
		passed = this->as ? this->as->m_assess(this->as, this->cs) : true;
		if (passed)
			target -= this->repeat;
		if (target < 0) {
			break;
		}
		more = this->cs->m_iter(this->cs);
	}

	return target < 0 ? this->cs : NULL;
}

struct receiver *create_receiver(struct receiver *base)
{
	struct receiver *r = base;

	assert(r);
	assert(r->m_configure);
	assert(r->p_setup);
	assert(r->p_setup_assessor);
	assert(r->p_start_monitor);
	assert(r->p_stop_monitor);

	r->m_add_changeitem = receiver_add_changeitem;
	r->m_clear_cs = receiver_clear_cs;
	r->m_init_cs = receiver_init_cs;
	r->m_start = receiver_start;
	r->m_stop = receiver_stop;
	r->m_checksum = receiver_checksum;
	r->m_is_checksum_enabled = receiver_is_checksum_enabled;
	r->m_expected_total = receiver_expected_total;
	r->m_received = receiver_received;
	r->m_missed = receiver_missed;
	r->m_error = receiver_error;
	r->m_query = receiver_query;

	r->cs = create_changeset();
	r->as = create_assessor();
	r->p_setup_assessor(r);

	r->scoreboard = NULL;
	r->repeat = 0;
	r->checksum = false;

	return r;
}

void destroy_receiver(struct receiver *this)
{
	if (!this)
		return;

	destroy_assessor(this->as);
	destroy_changeset(this->cs);

	kfree(this);
}
