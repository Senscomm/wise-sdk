/*
 * Copyright 2018-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef __MACHINE_H__
#define __MACHINE_H__

#include <hal/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

struct hal_soc {
	const char *name;

	int (*init)(void);
	void (*timer_init)(void);
};

struct hal_board {
	const char *name;
	struct hal_soc *soc;

	int (*init)(void);
	int (*soc_fixup)(struct hal_soc *soc);
};

void soc_preinit(void);
void soc_init(void);
void board_init(void);

#define SOC(_name_) \
	ll_entry_declare(struct hal_soc, _name_, soc)

#define BOARD(_name_) \
	ll_entry_declare(struct hal_board, _name_, board)

#ifdef __cplusplus
}
#endif

#endif
