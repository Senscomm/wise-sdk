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
#ifndef _TRANSMITACTION_H
#define _TRANSMITACTION_H

#include <hal/types.h>
#include <u-boot/list.h>
#include <action.h>

struct tx_stats {
	u64 ok;
	u64 fail_reason[16];
	u64 sent;
	u64 acked;
};

struct transmitaction {
	struct action base;

	void (*m_set_repeat)(struct transmitaction *this, int repeat);
	void (*m_set_addr)(struct transmitaction *this, u8 addr[]);
	void (*m_set_peer)(struct transmitaction *this, u8 peer[]);
	void (*m_set_ciph)(struct transmitaction *this, u8 ciph);
	void (*m_set_keyi)(struct transmitaction *this, u8 keyi);
	void (*m_set_gtk)(struct transmitaction *this, u8 gtk);
	void (*m_set_noack)(struct transmitaction *this, u8 noack);
	void (*m_clear_stat)(struct transmitaction *this);

	int repeat;
	u8 addr[6];
	u8 peer[6];
	u8 ciph;
	u8 keyi;
	u8 gtk;
	u8 noack;

	struct tx_stats stats;
	bool checksum;
};

struct transmitaction *create_transmitaction(struct transmitaction *base);
void destroy_transmitaction(struct transmitaction *this);

#endif /* _TRANSMITACTION_H */
