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
#ifndef _TRANSMITTER_H
#define _TRANSMITTER_H

#include <hal/types.h>
#include <u-boot/list.h>
#include <changeset.h>
#include <assessor.h>
#include <sweeper.h>
#include <transmitaction.h>

struct transmitter {
	int (*m_add_changeitem)(struct transmitter *this, const char *name, int start, int end, int step);
	int (*m_configure)(struct transmitter *this, const char *cat, const char *item, void *val);
	int (*m_clear_cs)(struct transmitter *this);
	int (*m_init_cs)(struct transmitter *this);
	int (*m_start)(struct transmitter *this, int repeat);
	int (*m_stop)(struct transmitter *this);
	void (*m_get_stats)(struct transmitter *this, struct tx_stats *stats);
	void (*m_checksum)(struct transmitter *this, bool enable);

	int (*p_setup)(struct transmitter *this);
	void (*p_cleanup)(struct transmitter *this);
	struct transmitaction *(*p_create_transmitaction)(struct transmitter *this);
	void (*p_destroy_transmitaction)(struct transmitter *this, struct transmitaction *ta);
	void (*p_setup_assessor)(struct transmitter *this);

	struct changeset *cs;
	struct assessor *as;
	struct sweeper *sw;
	struct transmitaction *ta;
};

struct transmitter *create_transmitter(struct transmitter *base);
void destroy_transmitter(struct transmitter *this);

#endif /* _TRANSMITTER_H */
