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
#ifndef _SCM2020_TRANSMITTER_H
#define _SCM2020_TRANSMITTER_H

#include <hal/types.h>
#include <u-boot/list.h>
#include <transmitter.h>

struct scm2020_transmitter {
	struct transmitter base;
	struct list_head blacklists;
	void *sc;
	int freq;
	int opbw;
	int prch;
	int mimo;
};

struct scm2020_transmitter *create_scm2020_transmitter(void *sc);
void destroy_scm2020_transmitter(struct scm2020_transmitter *this);

#endif /* _SCM2020_TRANSMITTER_H */
