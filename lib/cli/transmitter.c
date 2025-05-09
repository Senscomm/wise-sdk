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
#include <transmitter.h>

static
int transmitter_add_changeitem(struct transmitter *this, const char *name, int start, int end, int step)
{
	if (this->sw && this->sw->m_is_busy(this->sw))
		return -1;

	if (this->cs)
		this->cs->m_add(this->cs, name, start, end, step);

	return 0;
}

static
int transmitter_clear_cs(struct transmitter *this)
{
	if (this->sw && this->sw->m_is_busy(this->sw))
		return -1;

	destroy_changeset(this->cs);
	this->cs = create_changeset();
	this->cs->m_init(this->cs);
	this->sw->m_set_changeset(this->sw, this->cs);

	return 0;
}

static
int transmitter_init_cs(struct transmitter *this)
{
	if (this->sw && this->sw->m_is_busy(this->sw))
		return -1;

	if (this->cs)
		this->cs->m_init(this->cs);

	return 0;
}

static
int transmitter_start(struct transmitter *this, int repeat)
{
	if (this->sw && this->sw->m_is_busy(this->sw))
		return -1;

	if (this->p_setup(this))
		return -1;
	if (this->ta) {
		this->ta->m_set_repeat(this->ta, repeat);
		this->ta->m_clear_stat(this->ta);
	}
	if (this->sw)
		this->sw->m_sweep(this->sw);

	return 0;
}

static
int transmitter_stop(struct transmitter *this)
{
	int retry = 10;

	if (this->sw && !this->sw->m_is_busy(this->sw))
		return -1;

	if (this->ta)
		this->ta->m_set_repeat(this->ta, 0);

	while (this->sw->m_is_busy(this->sw)
			&& (retry--) > 0)
		osDelay(1);

	if (!retry)
		return -1;

	this->p_cleanup(this);

	return 0;
}

static
void transmitter_get_stats(struct transmitter *this, struct tx_stats *stats)
{
	if (this->ta)
		memcpy(stats, &this->ta->stats, sizeof(*stats));
	else
		memset(stats, 0, sizeof(*stats));
}

static
void transmitter_checksum(struct transmitter *this, bool enable)
{
	if (this->ta)
		this->ta->checksum = enable;
}

struct transmitter *create_transmitter(struct transmitter *base)
{
	struct transmitter *t = base;

	assert(t);
	assert(t->m_configure);
	assert(t->p_setup);
	assert(t->p_cleanup);
	assert(t->p_create_transmitaction);
	assert(t->p_destroy_transmitaction);
	assert(t->p_setup_assessor);

	t->m_add_changeitem = transmitter_add_changeitem;
	t->m_clear_cs = transmitter_clear_cs;
	t->m_init_cs = transmitter_init_cs;
	t->m_start = transmitter_start;
	t->m_stop = transmitter_stop;
	t->m_get_stats = transmitter_get_stats;
	t->m_checksum = transmitter_checksum;

	t->cs = create_changeset();
	t->as = create_assessor();
	t->p_setup_assessor(t);
	t->ta = t->p_create_transmitaction(t);
	t->sw = create_sweeper();
	t->sw->m_set_changeset(t->sw, t->cs);
	t->sw->m_set_assessor(t->sw, t->as);
	t->sw->m_set_action(t->sw, (struct action *)t->ta);

	return t;
}

void destroy_transmitter(struct transmitter *this)
{
	if (!this)
		return;

	destroy_sweeper(this->sw);
	this->p_destroy_transmitaction(this, this->ta);
	destroy_assessor(this->as);
	destroy_changeset(this->cs);

	kfree(this);
}
