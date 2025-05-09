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
#ifndef _RECEIVER_H
#define _RECEIVER_H

#include <hal/types.h>
#include <u-boot/list.h>
#include <changeset.h>
#include <assessor.h>

struct receiver {
	int (*m_add_changeitem)(struct receiver *this, const char *name, int start, int end, int step);
	int (*m_configure)(struct receiver *this, const char *cat, const char *item, void *val);
	int (*m_clear_cs)(struct receiver *this);
	int (*m_init_cs)(struct receiver *this);
	int (*m_start)(struct receiver *this, int repeat);
	int (*m_stop)(struct receiver *this);
	void (*m_checksum)(struct receiver *this, bool checksum);
	bool (*m_is_checksum_enabled)(struct receiver *this);
	s64 (*m_expected_total)(struct receiver *this);
	s64 (*m_received)(struct receiver *this);
	s64 (*m_missed)(struct receiver *this);
	s64 (*m_error)(struct receiver *this);
	struct changeset *(*m_query)(struct receiver *this, bool ok, s64 index);

	int (*p_setup)(struct receiver *this);
	void (*p_setup_assessor)(struct receiver *this);
	void (*p_start_monitor)(struct receiver *this);
	void (*p_stop_monitor)(struct receiver *this);

	struct changeset *cs;
	struct assessor *as;

	bool checksum;

	u8 *scoreboard;
	int scoreboard_size;
	u64 chksum_error;
	int repeat;
};

struct receiver *create_receiver(struct receiver *base);
void destroy_receiver(struct receiver *this);

#endif /* _RECEIVER_H */
