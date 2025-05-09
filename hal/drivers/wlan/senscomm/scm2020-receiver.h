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
#ifndef _SCM2020_RECEIVER_H
#define _SCM2020_RECEIVER_H

#include <hal/types.h>
#include <u-boot/list.h>
#include <receiver.h>

struct scm2020_receiver {
	struct receiver base;
	struct list_head blacklists;
	void *sc;
	u8 addr[6];
	u8 peer[6];
	u8 ciph;
	u8 keyi;
	u8 gtk;
	u8 noack;
	int freq;
	int opbw;
	int prch;
	int mimo;
	struct ieee80211_key key;
	u32 rx_filter;
};

struct scm2020_receiver *create_scm2020_receiver(void *sc);
void destroy_scm2020_receiver(struct scm2020_receiver *this);

#endif /* _SCM2020_RECEIVER_H */
