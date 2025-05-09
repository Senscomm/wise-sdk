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

#ifndef __SPI_FLASH_H__
#define __SPI_FLASH_H__

#include <hal/kernel.h>
#include <sys/types.h>

#define SPI_FLASH_MAX_ID_LEN	6

#ifndef CONFIG_NUM_FLASH_PARTITION
#define CONFIG_NUM_FLASH_PARTITION 	8
#endif

/**
 * the smallest erase section/block containing offset
 * EB_UNALIGNED|EB_MIN_BLOCK
 */
#define EB_MIN_BLOCK 	BIT(0)
#define EB_SUPERSET 	BIT(1)
#define EB_UNALIGNED	BIT(2)

/**
 * struct fast_read_cmd - multi I/O read command
 *
 * @opcode: instruction to use
 * @address: number of I/O lines used in address
 * @data: number of I/O lines used in data
 * @mode: number of clocks for mode bits
 * @dummy: number of wait states, i.e., dummy clocks before data out
 */
struct fast_read_cmd {
	u8 opcode; /* nonzero : supported, 0 : not supported */
	u8 address;
	u8 data;
	u8 mode;
	u8 dummy;
};

/**
 * struct erase_method - erase method
 *
 * @size: erase granularity (4096, 8192, ...)
 * @opcode: SPI flash instruction (opcode) to use
 * @timeout: erase time in ms
 */
struct erase_method {
	u32 size;
	u8 opcode;
	unsigned timeout; /* in ms */
};

/**
 * struct erase_region - erase region
 *
 * @id: erase region id
 * @start: relative offset from the start of the flash
 * @size: size of the region in bytes
 * @method: bit vector of erase methods this region supports (a
 *          region can support more than one erase methods).
 *
 * An erase region is a set of contiguous blocks having the same size
 * and the same erase method (i.e., granularity, instruction, and so
 * on).
 */
struct erase_region {
	int id;
	unsigned start;
	unsigned size;
	unsigned method;
};

/**
 * struct erase_block - erase block
 *
 * @start: start offset of an erase block
 * @end: end offset of an erase block
 * @region: the region this erase block belongs to
 * @method: erase method to be applied to erase this block
 */
struct erase_block {
	unsigned start;
	unsigned end;
	struct erase_region *region;
	struct erase_method *method;
};

/* flash partition flags */
#define P_SYSTEM	BIT(0)

typedef	union {
	char buffer[32];
	struct {
		char name[16];
		uint32_t start; /* absolute flash address */
		uint32_t size;
		uint32_t data_length;
		uint32_t flags;
	};
} flash_part_t;

/**
 * struct spi_flash - SPI flash memory
 *
 * @master: SPI controller this memory is attached to.
 * @list: list of SPI flash memories
 * @index: #
 * @mem_base: system address this flash is mapped to
 * @size: total size of the flash in bytes
 * @min_erase_size: effective erase region size
 * @partition: partition table
 */
struct spi_flash {
	struct device *master;
	struct list_head list;
	int index;
	void *mem_base;

	/* Flash information */
	const char *name;
	u8 id[SPI_FLASH_MAX_ID_LEN];
	int id_len;
	size_t size;

    struct fast_read_cmd fast_read[4];
    struct fast_read_cmd *cur_fast_read;

	struct erase_method erase_info[4];
	struct erase_region *region;
	int num_region;
#ifdef __remove__
	int block_size;
#endif
	int erase_4KB_op; /* uniform 4KB erase supported */

	int page_size;
	unsigned pp_timeout;

	flash_part_t partition[CONFIG_NUM_FLASH_PARTITION];

	int (*unlock)(struct spi_flash *);
	int (*lock)(struct spi_flash *);
	int (*read)(struct spi_flash *, u32, size_t, void *);
	int (*erase)(struct spi_flash *, u32, size_t);
	int (*write)(struct spi_flash *, u32, size_t, void *);
    int (*quadio) (struct spi_flash *, bool);
};

struct spi_flash *spi_flash_alloc_device(void);
void spi_flash_free_device(struct spi_flash *flash);
void spi_flash_add_device(struct spi_flash *flash);
void spi_flash_remove_devie(struct spi_flash *flash);

struct spi_flash *spi_flash_find_device(int index);
struct spi_flash *spi_flash_find_by_addr(off_t addr);

extern struct list_head spi_flash_list;

#define for_each_spi_flash(flash)				\
	list_for_each_entry(flash, &spi_flash_list, list)

off_t spi_flash_top_block(struct spi_flash *flash);

int spi_flash_probe_sfdp(struct spi_flash *flash);
struct spi_flash *spi_flash_probe(struct device *master);
flash_part_t *flash_lookup_partition(char *name, int *num);
int flash_partition_erase_size(flash_part_t *);

#define for_each_spi_flash_partition(flash, part)		\
	for (part = flash->partition; part->name[0] != '\xff'; part++)

int spi_flash_unlock(struct spi_flash *flash);
int spi_flash_erase(struct spi_flash *flash, off_t start, size_t size, unsigned how);
int spi_flash_write(struct spi_flash *flash, u32 offset, size_t size, void *buf);
int spi_flash_read(struct spi_flash *flash, u32 offset, size_t size, void *buf);


int flash_erase(off_t addr, size_t size, unsigned how);
int flash_write(off_t addr, void *buf, size_t size);
int flash_read(off_t addr, void *buf, size_t size);
int flash_unlock_all(void);



#endif
