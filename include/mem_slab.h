/*
 * Copyright 2024-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __MEM_SLAB_H__
#define __MEM_SLAB_H__

#include <sys/types.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <stddef.h>
#include <stdarg.h>
#include <fcntl.h>
#include <dirent.h>

#include <FreeRTOS/FreeRTOS.h>

#include <cmsis_os.h>

#include <hal/kernel.h>
#include <hal/compiler.h>

/**
 * struct mem_slab_info
 *
 * @num_blocks: total number of blocks
 * @block_size: size in bytes of each block
 * @num_used: number of blocks in use
 * @max_used: maximum number of blocks ever used
 */
struct mem_slab_info {
	uint32_t num_blocks;
	size_t   block_size;
	uint32_t num_used;
	uint32_t max_used;
};

/**
 * struct mem_slab
 *
 * @wait_q: queue on which clients wait for a free block
 * @free_block: a free block just returned to an empty memory slab
 * @buffer: buffer of memory slab
 * @free_list: list of free memory blocks
 * @info: configuration and statistics
 * @list: list of memory slabs
 */
struct mem_slab {
	osMessageQueueId_t wait_q;
	StaticQueue_t cb;
	void *free_block;
	char *buffer;
	char *free_list;
	struct mem_slab_info info;
	struct list_head list;
};


#define ROUND_UP(x, align)                                   \
	((((unsigned long)(x) + ((unsigned long)(align) - 1)) / \
	  (unsigned long)(align)) * (unsigned long)(align))

#define WB_UP(x) ROUND_UP(x, sizeof(void *))

#define MEM_SLAB_INITIALIZER(_slab_buffer, _slab_block_size,        \
                                   _slab_num_blocks)                \
	{                                                               \
	.free_block = NULL,                                             \
	.buffer = _slab_buffer,                                         \
	.free_list = NULL,                                              \
	.info = {_slab_num_blocks, _slab_block_size, 0, 0}              \
	}

/**
 * @brief Statically define and initialize a memory slab.
 *
 * The memory slab's buffer contains @a slab_num_blocks memory blocks
 * that are @a slab_block_size bytes long. The buffer is aligned to a
 * @a slab_align -byte boundary. To ensure that each memory block is similarly
 * aligned to this boundary, @a slab_block_size must also be a multiple of
 * @a slab_align.
 *
 * @code extern struct mem_slab <name>; @endcode
 *
 * @param name Name of the memory slab.
 * @param slab_block_size Size of each memory block (in bytes).
 * @param slab_num_blocks Number memory blocks.
 * @param slab_align Alignment of the memory slab's buffer (power of 2).
 */
#define MEM_SLAB_DEFINE(name, slab_block_size, slab_num_blocks, slab_align, s) \
	char mem_slab_buf_##name[(slab_num_blocks) * WB_UP(slab_block_size)] \
         __aligned(WB_UP(slab_align)) __attribute__((__section__(#s))); \
	ll_entry_declare(struct mem_slab, name, memslab) = \
		MEM_SLAB_INITIALIZER(mem_slab_buf_##name, WB_UP(slab_block_size), \
                slab_num_blocks);

#define MEMSLAB(_name_) \
	llsym(struct mem_slab, _name_, memslab)

#define memslab_start() ll_entry_start(struct mem_slab, memslab)
#define memslab_end()   ll_entry_end(struct mem_slab, memslab)

int mem_slab_init(struct mem_slab *slab, void *buffer, size_t block_size,
        uint32_t num_blocks);
int mem_slab_deinit(struct mem_slab *slab);
int mem_slab_alloc(struct mem_slab *slab, void **mem, uint32_t timeout);
void mem_slab_free(struct mem_slab *slab, void *mem);

#endif
