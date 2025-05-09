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
#include <transmitaction.h>

static
void transmitaction_set_repeat(struct transmitaction *this, int repeat)
{
	this->repeat = repeat;
}

static
void transmitaction_set_addr(struct transmitaction *this, u8 addr[])
{
	memcpy(this->addr, addr, ARRAY_SIZE(this->addr));
}

static
void transmitaction_set_peer(struct transmitaction *this, u8 peer[])
{
	memcpy(this->peer, peer, ARRAY_SIZE(this->peer));
}

static
void transmitaction_set_ciph(struct transmitaction *this, u8 ciph)
{
	this->ciph = ciph;
}

static
void transmitaction_set_keyi(struct transmitaction *this, u8 keyi)
{
	this->keyi = keyi;
}

static
void transmitaction_set_gtk(struct transmitaction *this, u8 gtk)
{
	this->gtk = gtk;
}

static
void transmitaction_set_noack(struct transmitaction *this, u8 noack)
{
	this->noack = noack;
}

static
void transmitaction_clear_stat(struct transmitaction *this)
{
	memset(&this->stats, 0, sizeof(this->stats));
}

struct transmitaction *create_transmitaction(struct transmitaction *base)
{
	struct transmitaction *ta = base;

	assert(ta);
	assert(ta->base.m_do);

	ta->m_set_repeat = transmitaction_set_repeat;
	ta->m_set_addr = transmitaction_set_addr;
	ta->m_set_peer = transmitaction_set_peer;
	ta->m_set_ciph = transmitaction_set_ciph;
	ta->m_set_keyi = transmitaction_set_keyi;
	ta->m_set_gtk = transmitaction_set_gtk;
	ta->m_set_noack = transmitaction_set_noack;
	ta->m_clear_stat = transmitaction_clear_stat;

	ta->checksum = false;

	return ta;
}

void destroy_transmitaction(struct transmitaction *this)
{
	if (!this)
		return;

	kfree(this);
}
