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

#include <hal/kernel.h>
#include <hal/device.h>
#include <hal/spi-flash.h>
#include <hal/timer.h>
#include <hal/console.h>
#include <hal/kmem.h>
#include <hal/init.h>
#ifdef CONFIG_WDT
#include <hal/wdt.h>
#endif

#include "spi-flash-internal.h"

#include <string.h>
#include <stdlib.h>

#include <cmsis_os.h>
#include <proc.h>

/*
#define DEBUG
*/

#ifdef DEBUG
#define dbg(arg ...) printk(arg);
#else
static void dbg(const char *fmt, ...)
{
	return;
}
#endif
#define err(arg ...) printk(arg);

#if CONFIG_FLASH_DEBUG_VERBOSITY == 0 /* no debug, no error */
#define debug(arg ...)
#define error(arg ...)
#elif CONFIG_FLASH_DEBUG_VERBOSITY == 1 /* no debug, simple error */
#define debug(arg ...)
#define error(arg ...) printf("Error(%s, %d)\n", __func__, __LINE__);
#else /* full debug, full error */
#define debug(arg ...) printf(arg)
#define error(arg ...) printf(arg)
#endif

int nr_spi_flash = 0;
LIST_HEAD_DEF(spi_flash_list);

/*
 * Note:
 * Routines here should be self-contained and all be running on SRAM.
 * No __div or __mod library.
 *
 */
#define ERASE_BLK_ALIGN(p, blksz) (((p) + (blksz) - 1) & ~((blksz) - 1))
#define is_aligned(p, blksz) (ERASE_BLK_ALIGN(p, blksz) == (p))

/**
 * spi_flash_cmd_read() - read from the flash
 *
 * @flash: spi flash memory to read from
 * @cmd: read command bytes
 * @cmd_len: command cycle
 * @dummy_len: dummy cycle
 * @rx: buffer
 * @rx_len: bytes to read
 *
 */
int spi_flash_cmd_read(struct spi_flash *flash, const u8 *cmd,
		       size_t cmd_len, size_t dummy_len, void *rx, size_t rx_len)
{
	struct device *master = flash->master;
	struct spi_flash_master_ops *ops = spi_flash_master_ops(master);

	return ops == NULL ? -EINVAL :
		ops->cmd_read(master, cmd, cmd_len, dummy_len, rx, rx_len);
}

/**
 * spi_flash_cmd_mio_read() - read from the flash in MIO mode
 *
 * @flash: spi flash memory to read from
 * @cmd: multi I/O read command
 * @addr: address
 * @rx: buffer
 * @rx_len: bytes to read
 *
 */
int spi_flash_cmd_mio_read(struct spi_flash *flash,
                           struct fast_read_cmd *cmd, u32 addr,
                           void *rx, size_t rx_len)
{
	struct device *master = flash->master;
	struct spi_flash_master_ops *ops = spi_flash_master_ops(master);

	return ops == NULL ? -EINVAL :
		ops->cmd_mio_read(master, cmd, addr, rx, rx_len);
}


/**
 * spi_flash_cmd_write() - write to the flash
 *
 * @flash: spi flash memory to read from
 * @cmd: write command bytes
 * @cmd_len: command cycle
 * @tx: tx buffer
 * @tx_len: bytes to write
 *
 */
int spi_flash_cmd_write(struct spi_flash *flash, const u8 *cmd,
			size_t cmd_len, void *tx, size_t tx_len)
{
	struct device *master = flash->master;
	struct spi_flash_master_ops *ops = spi_flash_master_ops(master);

	return ops == NULL ? -EINVAL :
		ops->cmd_write(master, cmd, cmd_len, tx, tx_len);
}

/*
 * SPI flash access primitive routines
 */

int read_status(struct spi_flash *flash)
{
	u8 cmd = SPI_FLASH_OP_RDSR, status;
	int ret;

	ret = spi_flash_cmd_read(flash, &cmd, 1, 0, &status, 1);
	if (ret < 0)
		return ret;
	return status;
}

int read_config(struct spi_flash *flash)
{
	u8 cmd = SPI_FLASH_OP_RDCR, config;
	int ret;

	ret = spi_flash_cmd_read(flash, &cmd, 1, 0, &config, 1);
	if (ret < 0)
		return ret;
	return config;
}

/* XXX: SST26WF080B and XT2508B writes status and configuration together */
int write_status(struct spi_flash *flash, u8 status)
{
	u8 cmd = SPI_FLASH_OP_WRSR;
	int ret;

	ret = spi_flash_cmd_write(flash, &cmd, 1, &status, 1);
	if (ret < 0)
		return ret;
	return status;
}

int spi_flash_write_enable(struct spi_flash *flash)
{
#ifdef CONFIG_ATCSPI200_COMPACT
	return 0;
#else
	u8 cmd = SPI_FLASH_OP_WREN;

	return spi_flash_cmd_write(flash, &cmd, 1, NULL, 0);
#endif
}

int spi_flash_write_disable(struct spi_flash *flash)
{
#ifdef CONFIG_ATCSPI200_COMPACT
	return 0;
#else
	u8 cmd = SPI_FLASH_OP_WRDI;

	return spi_flash_cmd_write(flash, &cmd, 1, NULL, 0);
#endif
}

int spi_flash_unlock(struct spi_flash *flash)
{
	if (flash->unlock)
		return flash->unlock(flash);

	return 0;
}

/**
 * spi_flash_sr_ready() - check flash SR busy status
 *
 * Returns: 0/1 if busy/ready; negative on error
 *
 */
int spi_flash_sr_ready(struct spi_flash *flash)
{
	int sr = read_status(flash);

	if (sr < 0)
		return sr;

	return !(sr & SR_WIP);
}

/**
 * spi_flash_wait_till_ready() - wait until flash becomes not busy
 * @flash: flash
 * @timeout: timeout in microseconds
 */
int spi_flash_wait_till_ready(struct spi_flash *flash,
			      unsigned long timeout)
{
	int ret;
	unsigned long ts, now = hal_timer_value(), to;

   	to = now + us_to_tick(timeout);
	do {
		ret = spi_flash_sr_ready(flash);
		if (ret < 0)
			goto out;
		if (ret) {
			ret = 0;
			goto out;
		}
		ts = hal_timer_value();
	} while (ts < to);
	dbg("%s: timeout (to=%ld, ts=%ld, now=%ld)\n", __func__, timeout, ts, to);
	ret = -ETIME;
 out:
	return ret;
}


void spi_flash_addr(u8 *cmd, u32 addr)
{
	cmd[1] = addr >> 16;
	cmd[2] = addr >> 8;
	cmd[3] = addr >> 0;
}

/*
 * SFDP (Serial Flash Discoverable Parameters) per JESD216(B)
 */

struct sfdp_hdr {
	u8 signature[4];
	u8 minor;
	u8 major;
	u8 nph;
	u8 unused;
} __attribute__((aligned(4)));

struct sfdp_param_hdr {
	u8 id_lsb;
	u8 minor;
	u8 major;
	u8 wlen;
	u8 ptp[3];
	u8 id_msb;
} __attribute__((aligned(4)));

#define bfield_w(v, pos, width) (((v) >> (pos)) & ((1 << (width)) - 1))
#define bfield_r(v, end, start) bfield_w(v, start, (end-start+1))

/**
 * spi_flash_read_sfdp() - read sfdp data
 * @flash: spi flash
 * @addr: byte address within SFDP
 * @size: bytes to read
 * @buf: buffer
 */
static ssize_t
spi_flash_read_sfdp(struct spi_flash *flash, off_t addr, size_t size,
		    void *buf)
{
	u8 cmd[SPI_FLASH_CMD_LEN];
	int ret;

	cmd[0] = SPI_FLASH_OP_RDSFDP;
	spi_flash_addr(cmd, addr);

	ret = spi_flash_cmd_read(flash, cmd, 4, 1, buf, size);
	if (ret < 0)
		return ret;
	else
		return size;
}

int spi_create_default_map(struct spi_flash *flash)
{
	int i;

	if (!flash->size)
		return -ENODEV;

	flash->region = malloc(sizeof(flash->region[0]));
	if (flash->region == NULL)
		return -ENOMEM;

	/* NB: a single uniform erase region is assumed */
	flash->num_region = 1;
	flash->region->id = 0;
	flash->region->start = 0;
	flash->region->size = flash->size;

	flash->region->method = 0;
	for (i = 0; i < 4; i++) {
		if (flash->erase_info[i].size > 0
				&& flash->erase_info[i].opcode)
			flash->region->method |= (1 << i);
	}

	return 0;
}

int sfdp_parse_basic(struct spi_flash *flash, struct sfdp_param_hdr *hdr,
		     void *buf)
{
	unsigned *dword = buf;
	u32 v, max_block_size = 0;
	int i, j;
    bool supp_1_1_4, supp_1_4_4, supp_1_1_2, supp_1_2_2;

	dword--; /* for indexing starting from 1 */

	flash->erase_4KB_op = bfield_r(dword[1], 15, 8);

    /* Support Fast Read: dword 1 */
    supp_1_1_4 = (bfield_r(dword[1], 22, 22) == 1);
    supp_1_4_4 = (bfield_r(dword[1], 21, 21) == 1);
    supp_1_2_2 = (bfield_r(dword[1], 20, 20) == 1);
    supp_1_1_2 = (bfield_r(dword[1], 16, 16) == 1);

	/* Density: dword 2*/
	if (bfield_r(dword[2], 31, 31) == 0)
		flash->size = (dword[2] + 1) / 8; /* in bytes */
	else
		flash->size = (1 << (dword[2] & 0x7ffffffff)) / 8;

	dbg("- Density: %d KiB\n", (int) flash->size/1024);

    j = 0;
    /* in the order of decreasing speed, i.e., fast_read[0] is the fastest. */
	/* Fast Read (1-4-4), (1-1-4): dword 3*/
    for (i = 0; i < 2; i++) {
        if ((i == 0 && !supp_1_4_4) || (i == 1 && !supp_1_1_4))
            continue;
        v = bfield_w(dword[3], 16 * i, 16);
        if (bfield_r(v, 15, 8)) {
            flash->fast_read[j].opcode = bfield_r(v, 15, 8);
            flash->fast_read[j].address = (i == 0 ? 4 : 1);
            flash->fast_read[j].data = 4;
            flash->fast_read[j].mode = bfield_r(v, 7, 5);
            flash->fast_read[j].dummy = bfield_r(v, 4, 0);
	        dbg("- Fast Read: (1-%d-%d), opcode 0x%x, mode %d, dummy %d\n",
                    flash->fast_read[j].address,
                    flash->fast_read[j].data,
                    flash->fast_read[j].opcode,
                    flash->fast_read[j].mode,
                    flash->fast_read[j].dummy);
            j++;
        }
    }

	/* Fast Read (1-1-2), (1-2-2): dword 4*/
    for (i = 1; i >= 0; i--) {
        if ((i == 0 && !supp_1_1_2) || (i == 1 && !supp_1_2_2))
            continue;
        v = bfield_w(dword[4], 16 * i, 16);
        if (bfield_r(v, 15, 8)) {
            flash->fast_read[j].opcode = bfield_r(v, 15, 8);
            flash->fast_read[j].address = (i == 0 ? 1 : 2);
            flash->fast_read[j].data = 2;
            flash->fast_read[j].mode = bfield_r(v, 7, 5);
            flash->fast_read[j].dummy = bfield_r(v, 4, 0);
	        dbg("- Fast Read: (1-%d-%d), opcode 0x%x, mode %d, dummy %d\n",
                    flash->fast_read[j].address,
                    flash->fast_read[j].data,
                    flash->fast_read[j].opcode,
                    flash->fast_read[j].mode,
                    flash->fast_read[j].dummy);
            j++;
        }
    }

    /* NB: (4-4-4), (2-2-2) should be added */

	/*
	 * Erase type:
	 * dword 8: type 1 and 2
	 * dword 9: type 3 and 4
	 */
	for (i = 0; i < 4; i++) {
		v = bfield_w(dword[i > 1 ? 9 : 8], i & 1 ? 16: 0, 16);
		if (!bfield_r(v, 7, 0)) {
			/* XXX: size 0 means this erase type doesn't exist */
			continue;
		}
		flash->erase_info[i].size = 1 << bfield_r(v, 7, 0);
		flash->erase_info[i].opcode = bfield_r(v, 15, 8);
		/* NB: timeout is not retrieved by 1.0 basic param table */
		/* NB: excessive 1 sec should be better than nothing */
		flash->erase_info[i].timeout = 1000 * 1000; /* 1sec */

		if (flash->erase_info[i].size > max_block_size)
			max_block_size = flash->erase_info[i].size;

		dbg("- Erase type %d: size=%3d (KiB), opcode=%02x, "
		    "timeout=%d us\n", i,
		    flash->erase_info[i].size/1024,
		    flash->erase_info[i].opcode,
		    flash->erase_info[i].timeout);
	}

	return 0;
}

int sfdp_parse_basic_216B(struct spi_flash *flash, struct sfdp_param_hdr *hdr,
		     void *buf)
{
	unsigned *dword = buf;
	u32 v, timeout, max_block_size = 0;
	int i, j, unit[] = {1, 16, 256, 1000};
    bool supp_1_1_4, supp_1_4_4, supp_1_1_2, supp_1_2_2;

	dword--; /* for indexing starting from 1 */

	flash->erase_4KB_op = bfield_r(dword[1], 15, 8);

    /* Support Fast Read: dword 1 */
    supp_1_1_4 = (bfield_r(dword[1], 22, 22) == 1);
    supp_1_4_4 = (bfield_r(dword[1], 21, 21) == 1);
    supp_1_2_2 = (bfield_r(dword[1], 20, 20) == 1);
    supp_1_1_2 = (bfield_r(dword[1], 16, 16) == 1);

	/* Density: dword 2*/
	if (bfield_r(dword[2], 31, 31) == 0)
		flash->size = (dword[2] + 1) / 8; /* in bytes */
	else
		flash->size = (1 << (dword[2] & 0x7ffffffff)) / 8;

	dbg("- Density: %d KiB\n", (int) flash->size/1024);

    j = 0;
    /* in the order of decreasing speed, i.e., fast_read[0] is the fastest. */
	/* Fast Read (1-4-4), (1-1-4): dword 3*/
    for (i = 0; i < 2; i++) {
        if ((i == 0 && !supp_1_4_4) || (i == 1 && !supp_1_1_4))
            continue;
        v = bfield_w(dword[3], 16 * i, 16);
        if (bfield_r(v, 15, 8)) {
            flash->fast_read[j].opcode = bfield_r(v, 15, 8);
            flash->fast_read[j].address = (i == 0 ? 4 : 1);
            flash->fast_read[j].data = 4;
            flash->fast_read[j].mode = bfield_r(v, 7, 5);
            flash->fast_read[j].dummy = bfield_r(v, 4, 0);
	        dbg("- Fast Read: (1-%d-%d), opcode 0x%x, mode %d, dummy %d\n",
                    flash->fast_read[j].address,
                    flash->fast_read[j].data,
                    flash->fast_read[j].opcode,
                    flash->fast_read[j].mode,
                    flash->fast_read[j].dummy);
            j++;
        }
    }

	/* Fast Read (1-1-2), (1-2-2): dword 4*/
    for (i = 1; i >= 0; i--) {
        if ((i == 0 && !supp_1_1_2) || (i == 1 && !supp_1_2_2))
            continue;
        v = bfield_w(dword[3], 16 * i, 16);
        if (bfield_r(v, 15, 8)) {
            flash->fast_read[j].opcode = bfield_r(v, 15, 8);
            flash->fast_read[j].address = (i == 0 ? 1 : 2);
            flash->fast_read[j].data = 2;
            flash->fast_read[j].mode = bfield_r(v, 7, 5);
            flash->fast_read[j].dummy = bfield_r(v, 4, 0);
	        dbg("- Fast Read: (1-%d-%d), opcode 0x%x, mode %d, dummy %d\n",
                    flash->fast_read[j].address,
                    flash->fast_read[j].data,
                    flash->fast_read[j].opcode,
                    flash->fast_read[j].mode,
                    flash->fast_read[j].dummy);
            j++;
        }
    }

    /* NB: (4-4-4), (2-2-2) should be added */

	/*
	 * Erase type:
	 * dword 8: type 1 and 2
	 * dword 9: type 3 and 4
	 * dword 10: time
	 */
	for (i = 0; i < 4; i++) {
		v = bfield_w(dword[i > 1 ? 9 : 8], i & 1 ? 16: 0, 16);
		if (!bfield_r(v, 7, 0)) {
			/* XXX: size 0 means this erase type doesn't exist */
			continue;
		}
		flash->erase_info[i].size = 1 << bfield_r(v, 7, 0);
		flash->erase_info[i].opcode = bfield_r(v, 15, 8);

		v = bfield_w(dword[10], (4 + i * 7), 7);
		timeout = (bfield_w(v, 0, 5) + 1) * unit[bfield_w(v, 5, 2)];
		timeout *= (1 + bfield_w(dword[10], 0, 4)) * 2; /* multiplier */
		flash->erase_info[i].timeout = timeout * 1000; /* us */

		if (flash->erase_info[i].size > max_block_size)
			max_block_size = flash->erase_info[i].size;

		dbg("- Erase type %d: size=%3d (KiB), opcode=%02x, "
		    "timeout=%d us\n", i,
		    flash->erase_info[i].size/1024,
		    flash->erase_info[i].opcode,
		    flash->erase_info[i].timeout);
	}

	/* Page program: dword 11,  */
	v = dword[11];
	flash->page_size = 1 << bfield_r(v, 7, 4);
	timeout = bfield_r(v, 12, 8) + 1;
	timeout *= (bfield_r(v, 13, 13) == 0 ? 8: 64);
	timeout *= (2 * (bfield_r(v, 3, 0) + 1));
	flash->pp_timeout = timeout;
	dbg("- PP: page size=%d B, timeout=%d us\n", flash->page_size,
	       flash->pp_timeout);

	return 0;
}

int sfdp_parse_sector_map(struct spi_flash *flash, struct sfdp_param_hdr *hdr,
			  void *buf)
{
	struct erase_region *r;
	u32 *dword = buf;
	u32 index = 0, i, addr = 0, config = 0;

	/*
	 * Process config detection command descriptor if any
	 * word #0
	 * - [31:24]: mask
	 * - [23:22]: address length (0: no, 1: 3byte, 2: 4byte: 3: variable)
	 * - [19:16]: read latency
	 * - [15: 8]: instruction
	 * - [ 1: 1]: type
	 * - [ 0: 0]; last
	 * word #1
	 *  - [31:0]: address
	 */
	while ((dword[index] & 0x2) == 0x0) {
		u8 cmd[1 + 4 + 14], data = 0, dummylen = 0;
		u32 cmdlen = 0, addr = dword[index+1];

		cmd[cmdlen++] = bfield_r(dword[index], 15, 8);

		switch (bfield_r(dword[index], 23, 22)) {
		case 2:
			cmd[cmdlen++] = bfield_r(addr, 31, 24);
		default:
			/* XXX: we only consider 3-byte address */
		case 1:
			cmd[cmdlen++] = bfield_r(addr, 23, 16);
			cmd[cmdlen++] = bfield_r(addr, 15,  8);
			cmd[cmdlen++] = bfield_r(addr,  7,  0);
		case 0:
			break;
		}

        /* XXX: This is actually dummy 'cycles', not dummy 'bytes'. */

        /* We will get into trouble when the instruction is multi I/O. */

		dummylen = bfield_r(dword[index], 19, 16); /* ignore 0xff */

		spi_flash_cmd_read(flash, cmd, cmdlen, dummylen, &data, 1);

		config <<= 1;
		config |= (data & bfield_r(dword[index], 31, 24)) ? 1 : 0;

		index += 2;
	}

	/* Skip map descriptor belonging to other configs */
	while (bfield_r(dword[index], 15, 8) != config) {
		if ((dword[index] & 0x1) == 0x1)
			return -ENODEV;
		dbg("- Map descriptor: config=%d (skipped)\n",
		       bfield_r(dword[index], 15, 8));
		index += 1; /* current word */
		index += bfield_r(dword[index], 23, 16) + 1; /* region dwords */
	}

	flash->num_region = bfield_r(dword[index], 23, 16) + 1;
	flash->region = malloc(sizeof(flash->region[0]) * flash->num_region);
	if ((r = flash->region) == NULL)
		return -ENOMEM;

	dbg("- Map descriptor: config=%d, #region=%d\n",
	       config, flash->num_region);

	index++;
	for (i = 0; i < flash->num_region; i++, r++, index++) {
		unsigned mask, m;
		char punc __maybe_unused = ':';

		r->id = i;
		r->start = addr;
		r->size = 256 * (bfield_r(dword[index], 31, 8) + 1);
		/* A region cannot be larger than the whole. */
		if (r->size > flash->size)
			r->size = flash->size;
		r->method = bfield_r(dword[index], 3, 0);
		addr += r->size;

		dbg("- #%d [%08x-%08x] (%3d KiB), method", i,
		       r->start, r->start + r->size, r->size / 1024);

		for (mask = r->method, m = 0; mask != 0; mask >>= 1, m++) {
			if (!(mask & 1))
				continue;
			dbg("%c %dK", punc, flash->erase_info[m].size/1024);
			punc = ',';
		}
		dbg("\n");
	}
	return 0;
}

#define PARSER(_id, _version, _parse) { .id = _id, .version = _version, .parse = _parse }
#define VERSION(major, minor)	(major << 8 | minor)
#define MAJOR(ver) ((ver & 0xff00) >> 8)

typedef struct {
	u16 id;
	u16 version;
	int (*parse)(struct spi_flash *, struct sfdp_param_hdr *, void *);
} sfdp_parser_t;

sfdp_parser_t param_parser[] = {
	PARSER(0xff00, VERSION(0x1, 0x0), sfdp_parse_basic), 		/* basic */
	PARSER(0xff00, VERSION(0x1, 0x6), sfdp_parse_basic_216B),	/* basic */
	PARSER(0xff81, VERSION(0x1, 0x0), sfdp_parse_sector_map),	/* sector map */
};

#define call(f) {			\
		ret = f; 		\
		if (ret < 0)		\
			goto out;	\
	}

static inline off_t sfdp_param_address(struct sfdp_param_hdr *h)
{
	return (h->ptp[2] << 16 | h->ptp[1] << 8 | h->ptp[0]);
}

static inline u16 sfdp_param_id(struct sfdp_param_hdr *h)
{
	return (h->id_msb << 8 | h->id_lsb);
}

static inline int sfdp_param_size(struct sfdp_param_hdr *h)
{
	return h->wlen * 4;
}

static inline u16 sfdp_param_version(struct sfdp_param_hdr *h)
{
	return VERSION(h->major, h->minor);
}

static int
sfdp_parse_param(struct spi_flash *flash, struct sfdp_param_hdr *hdr)
{
	void *buf;
	u16 id = sfdp_param_id(hdr);
	u16 ver = sfdp_param_version(hdr);
	off_t addr = sfdp_param_address(hdr);
	size_t size = sfdp_param_size(hdr);
	int i, ret;
    sfdp_parser_t *best_parser;

	dbg("SF: SFDP param (id=%04x, ver=%d.%d, ptp=%08x, len=%d)\n",
	       id, hdr->major, hdr->minor, (unsigned) addr, (unsigned) size);

	for (i = 0, best_parser = NULL; i < ARRAY_SIZE(param_parser); i++) {
        sfdp_parser_t *parser = &param_parser[i];
		if (parser->id == id) {
            if ((MAJOR(parser->version) != MAJOR(ver))
                    || (parser->version > ver)) {
                /* If major revision is different, they are totally incompatible.
                 * And the SFDP table is also not forward compatible. */
                continue;
            }
            /* Find the parser that supports the highest minor revision
             * below the actual table.
             * Ex.) If table is 1.7, best parser should be 1.6, not 1.0.
             */
            if (!best_parser || parser->version > best_parser->version)
                best_parser = parser;
        }
	}
    /* NB: once we see any unsupported table, then the rest will be ignored. */
    if (!best_parser)
	    return -1;

    /* There IS the best parser. Let it work then. */

	if (!(buf = malloc(size)))
		return -ENOMEM;

	call(spi_flash_read_sfdp(flash, addr, size, buf));
	call(best_parser->parse(flash, hdr, buf));
	ret = 0;
 out:
	free(buf);
	return ret;
}

int spi_flash_probe_sfdp(struct spi_flash *flash)
{
	struct sfdp_hdr top;
	struct sfdp_param_hdr pdir;
	int i, ret;

	spi_flash_read_sfdp(flash, 0x0, 8, &top);
	if (memcmp(top.signature, "SFDP", 4))
		return -ENXIO;

	for (i = 0; i < top.nph + 1; i++) {
		spi_flash_read_sfdp(flash, 8 + 8 * i, sizeof(pdir), &pdir);
		call(sfdp_parse_param(flash, &pdir));
	}
 out:
	return ret;
}

#define SPI_FLASH_PP_TIMEOUT 		100*1000 /* us */
#define SPI_FLASH_ERASE_TIMEOUT		100*1000 /* us */

#define spi_flash_drv_start()						\
	ll_entry_start(struct spi_flash_driver, spi_flash_driver)

#define spi_flash_drv_end()						\
	ll_entry_end(struct spi_flash_driver, spi_flash_driver)

int spi_flash_probe_id(struct spi_flash *flash)
{
	struct spi_flash_driver *drv;
	int ret;
	u8 cmd, *id = flash->id;

	cmd = SPI_FLASH_OP_RDID;
	ret = spi_flash_cmd_read(flash, &cmd, 1, 0, id, SPI_FLASH_MAX_ID_LEN);
	if (ret < 0)
		return ret;

	dbg("SF: id=%02x%02x%02x\n", id[0], id[1], id[2]);

	for (drv = spi_flash_drv_start(); drv < spi_flash_drv_end(); drv++) {
		int i;
		for (i = 0; i < ARRAY_SIZE(drv->vid); i++)
			if (drv->vid[i] == id[0] && !drv->probe(flash))
				return 0;
	}
	err("SF: unknown flash memory %02x%02x%02x\n", id[0], id[1], id[2]);
	return -EINVAL;
}

static void spi_flash_print(struct spi_flash *flash,
			    const char *prefix,
			    int (*print)(const char *fmt, ...)) __maybe_unused;
static void spi_flash_print(struct spi_flash *flash,
			    const char *prefix,
			    int (*print)(const char *fmt, ...))
{
	struct erase_region *r;
	char punc = ':';
	int i;

	print("%sid=%02x%02x%02x @[%x-%x] (%d KiB) on %s\n",
	      prefix,
	      flash->id[0], flash->id[1], flash->id[2],
	      flash->mem_base, flash->mem_base + flash->size,
	      flash->size/1024,
	      dev_name(flash->master));
	print("- Page program: size=%dB, timeout=%dus\n",
	      flash->page_size, flash->pp_timeout);

	print("- Erase region:\n");
	for (i = 0, r = flash->region; i < flash->num_region; i++, r++) {
		unsigned mask, m;

		print("- #%d [%08x-%08x] (%3d KiB), method", i,
		       r->start, r->start + r->size, r->size / 1024);

		punc = ':';
		for (mask = r->method, m = 0; mask != 0; mask >>= 1, m++) {
			if (!(mask & 1))
				continue;
			print("%c %dK", punc,
			       flash->erase_info[m].size/1024);
			punc = ',';
		}
		print("\n");
	}
}

struct spi_flash *spi_flash_alloc_device(void)
{
	struct spi_flash *flash = kzalloc(sizeof(*flash));

	INIT_LIST_HEAD(&flash->list);

	return flash;
}

void spi_flash_free_device(struct spi_flash *flash)
{
	if (!list_empty(&flash->list))
		list_del_init(&flash->list);

	kfree(flash);
}

extern int spiffs_init(struct spi_flash *flash, off_t offset);
static int __flash_probe_partition_table(struct spi_flash *flash);

void spi_flash_add_device(struct spi_flash *flash)
{
	flash->index = nr_spi_flash++;

	list_add_tail(&flash->list, &spi_flash_list);
#if 0
	spi_flash_print(flash, "SF: ", printk);
	spiffs_init(flash, flash->size * 3 / 4);
#endif
	__flash_probe_partition_table(flash);
}

void spi_flash_remove_devie(struct spi_flash *flash)
{
	if (!list_empty(&flash->list)) {
		list_del_init(&flash->list);
		nr_spi_flash--;
	}
}

struct spi_flash *spi_flash_find_device(int index)
{
	struct spi_flash *flash;

	list_for_each_entry(flash, &spi_flash_list, list) {
		if (flash->index == index)
			return flash;
	}
	return NULL;
}

struct spi_flash *spi_flash_find_by_addr(off_t addr)
{
	struct spi_flash *flash;

	list_for_each_entry(flash, &spi_flash_list, list) {
		if ((off_t) flash->mem_base <= addr &&
		    (off_t) flash->mem_base + flash->size > addr)
			return flash;
	}
	return NULL;
}

struct spi_flash *spi_flash_probe(struct device *master)
{
	struct spi_flash *flash;
	struct spi_flash_master_ops *ops = spi_flash_master_ops(master);
    int i;

	flash = spi_flash_alloc_device();
	if (!flash)
		return NULL;

	flash->master = master;
	flash->mem_base = master->base[1];

	spi_flash_probe_sfdp(flash);
	spi_flash_probe_id(flash);

	if (flash->num_region == 0) {
#ifdef CONFIG_FLASH_DEFAULT_MAP
		/* NB: create and use a uniform map as a last resort */
		/* XXX: Is it is dangerous? */

		if (spi_create_default_map(flash) < 0)
			return NULL;

		/* also set other default params */
		if (flash->page_size == 0)
			flash->page_size = 256;
		if (flash->pp_timeout == 0)
			flash->pp_timeout = 100 * 1000; /* 100 ms */
#else
		spi_flash_free_device(flash);
		return NULL;
#endif
	}

    /* Select the fastest Multi I/O read mode, if any. */
    /* Again, flash->fast_read[0] is the fastest. */
    for (i = 0; i < ARRAY_SIZE(flash->fast_read); i++) {
        struct fast_read_cmd *rcmd = &flash->fast_read[i];
        if (rcmd->opcode && ops->set_mm_rcmd(master, rcmd, true)) {
            bool qio = ops->is_quad_feasible ? ops->is_quad_feasible(master) : false;
            /* SPI controller supports it.
             * If it is a Quad I/O, the device should be configured
             * to accept it.
             */
            if (rcmd->data == 4 && flash->quadio && qio) {
                dbg("Selecting fast read mode: opcode 0x%x\n", rcmd->opcode);
                flash->quadio(flash, true);
                ops->set_mm_rcmd(master, rcmd, false);
                flash->cur_fast_read = rcmd;
                break;
            } else if (rcmd->data != 4) {/* Dual I/O can always be selected. */
                dbg("Selecting fast read mode: opcode 0x%x\n", rcmd->opcode);
                ops->set_mm_rcmd(master, rcmd, false);
                flash->cur_fast_read = rcmd;
                break;
            }
        }
    }

	return flash;
}

/**
 * spi_flash_lookup_region() - find the erase region
 * @flash: spi flash
 * @offset: the offset to lookup
 *
 * Returns:
 * the pointer to struct erase_region that contains @offset, or
 * NULL if no sutiable region could be found.
 */
struct erase_region *
spi_flash_lookup_region(struct spi_flash *flash, off_t offset)
{
	struct erase_region *r;
	int i;

	for (i = 0, r = flash->region; i < flash->num_region; i++, r++)
		if (r->start <= offset && offset < r->start + r->size)
			return r;
	return NULL;
}

static int erase_block_superset(struct erase_block *b1, struct erase_block *b2)
{
	return (b1->start <= b2->start &&
		b1->end >= b2->end);
}

static int erase_block_subset(struct erase_block *b1, struct erase_block *b2)
{
	return (b1->start >= b2->start &&
		b1->end <= b2->end);
}

/**
 *
 * This function finds an erase section/block and record it in @out,
 * which is the fully included by @in.
 */
static int spi_flash_lookup_erase_info(struct spi_flash *flash,
				       struct erase_block *in,
				       struct erase_block *out, unsigned how)
{
	struct erase_region *r;
	struct erase_method *m, *best = NULL;
	struct erase_block b;
	unsigned mask;
	int ok;

	r = spi_flash_lookup_region(flash, in->start);
        if (r == NULL)
                return -EINVAL;

	m = flash->erase_info;
    for (mask = r->method; mask != 0; mask >>= 1, m++) {
		if (!(mask & 1))
			continue;

		b.start = in->start & ~(m->size - 1);
		b.end = b.start + m->size;

		ok = 1;
		ok &= ((how & EB_SUPERSET) ? erase_block_superset(&b, in) :
		       erase_block_subset(&b, in));
		ok &= (how & EB_UNALIGNED) ? 1 : (b.start == in->start);
		if (!ok)
			continue;

        if (!best || (!(how & EB_MIN_BLOCK) && m->size > best->size) ||
		    ((how & EB_MIN_BLOCK) && m->size < best->size))
                        best = m;
    }
	if (!best)
		return -EINVAL;

	out->region = r;
	out->method = best;
	out->start = in->start & ~(best->size - 1);
	out->end = out->start + best->size ;

	return 0;
}

int spi_flash_lookup_erase_block(struct spi_flash *flash, off_t *start,
				 off_t *end, unsigned how)
{
	struct erase_block out, in = {
		.start = *start,
		.end = *start + 1,
	};

	if (!spi_flash_lookup_erase_info(flash, &in, &out, how)) {
		*start = out.start;
		if (end)
			*end = out.end;
		return 0;
	}
	return -ENXIO;
}

int spi_flash_write_sequence(struct spi_flash *flash,
			     const u8 *cmd, size_t cmd_len,
			     void *tx, size_t tx_len,
			     unsigned long timeout)
{
	int ret;

#ifdef CONFIG_ATCSPI200_COMPACT
	ret = spi_flash_cmd_write(flash, cmd, cmd_len, tx, tx_len);
#else
	unsigned long flags;

	local_irq_save(flags);

	call(spi_flash_write_enable(flash));
	call(spi_flash_cmd_write(flash, cmd, cmd_len, tx, tx_len));
	call(spi_flash_wait_till_ready(flash, timeout));
	call(spi_flash_write_disable(flash));
 out:
	local_irq_restore(flags);

#endif

	return ret;
}

u32 spi_flash_get_erase_min_size(struct spi_flash *flash, struct erase_block *in)
{
	struct erase_region *r;
	struct erase_method *m;
	unsigned mask;
	u32 erase_size = UINT32_MAX;

	r = spi_flash_lookup_region(flash, in->start);
	if (r == NULL) {
		printk("Invalid flash region 0x%08x\n", in->start);
		return 0;
	}

	m = flash->erase_info;
    for (mask = r->method; mask != 0; mask >>= 1, m++) {
		if (!(mask & 1)) {
			continue;
		}

		if (erase_size > m->size) {
			erase_size = m->size;
		}
	}

	if (erase_size == UINT32_MAX) {
		return 0;
	}

	return erase_size;
}

/**
 * spi_flash_erase() - erase flash range
 * @flash: spi flash
 * @start: start offset within @flash
 * @size: erase size in bytes
 *
 * Returns:
 * 0 	    successful completion
 * -EINVAL  specified region is not aligned with erase sector/block
 * -ETIME   timeout occurred during erase
 */
int spi_flash_erase(struct spi_flash *flash, off_t start, size_t size,
		unsigned how)
{
	struct erase_block out, in = {
		.start = start,
		.end = start + size,
	};
	u8 cmd[SPI_FLASH_CMD_LEN];
	int ret = -1, dryrun = 1;

	dbg("SF: erase 0x%08x-0x%08x, (%d K)\n",
	    start, start + size, size/1024);
 loop:
	while (in.start < in.end) {
		struct erase_method *op;

		if (spi_flash_lookup_erase_info(flash, &in, &out, how) < 0) {
			u32 erase_size_min;

			/* could not find an aligned, maximum, subset */
			if ((erase_size_min = spi_flash_get_erase_min_size(flash, &in))) {
				if ((start % erase_size_min) || (size % erase_size_min)) {
					printk("Flash erase address or size must be aligned %d", erase_size_min);
				}
			}

			errno = EINVAL;
			return -1;
		}
		op = out.method;
		if (!dryrun) {
			cmd[0] = op->opcode;
			spi_flash_addr(cmd, (u32) out.start);
			ret = spi_flash_write_sequence(flash, cmd, 4, NULL, 0,
						       op->timeout);
			if (ret < 0)
				return -ret;
		}
		dbg("SF(%s): erase 0x%08x-0x%08x\n", dryrun?"dryrun":"real", out.start, out.end);
		in.start += op->size;
	}
	if (!how && in.start != in.end) {
		errno = EINVAL;
		return -1;
	} else if (dryrun) {
		in.start = start;
		dryrun = false;
		goto loop;
	}

	return 0;
}

/**
 * spi_flash_write() - generic serial flash write
 *
 * @flash: spi flash memory
 * @offset: flash memory address
 * @len: size in byte
 * @buf: buffer to write
 *
 * Returns: actual bytes on success, negative error number otherwise
 *
 */
int spi_flash_write(struct spi_flash *flash, u32 offset, size_t size,
		    void *buf)
{
	u8 cmd[SPI_FLASH_CMD_LEN];
	size_t page_size, actual, len;
	int ret;

	if (((u32)buf >= (u32)flash->mem_base) &&
	    ((u32)buf <= (u32)flash->mem_base + flash->size)) {
		printk("Invalid buf address : 0x%08x\n", (u32)buf);
		errno = EINVAL;
		return -1;
	}

	if (flash->write)
		return flash->write(flash, offset, size, buf);

	dbg("SF: write 0x%08x-0x%08x, (%d K)\n",
	    offset, offset + size, size/1024);

	actual = 0;
	cmd[0] = SPI_FLASH_OP_PP;
	page_size = flash->page_size;

	while (size > 0) {
		spi_flash_addr(cmd, offset);

		len = min(size, page_size);
		ret = spi_flash_write_sequence(flash, cmd, 4, buf, len,
					       flash->pp_timeout);
		if (ret < 0) {
			err("%s: failed\n", __func__);
			return ret;
		}

		offset += len;
		buf += len;
		size -= len;
		actual += len;
	}

	return actual;
}

/**
 * spi_flash_read() - generic serial flash read
 *
 * @flash: spi flash memory
 * @offset: flash memory address
 * @len: size in byte
 * @buf: buffer to save read data
 *
 * Returns: actual bytes on success, negative error number otherwise
 */
int spi_flash_read(struct spi_flash *flash, u32 offset, size_t size, void *buf)
{
	struct spi_flash_master_ops *ops = spi_flash_master_ops(flash->master);
	size_t rlen, xlen;
	int ret;

#if 1
	xlen =  ops->max_xfer_size;
	if (xlen <= 0)
		xlen = size;

	while (size > 0) {
		rlen = min(size, xlen);
        if (flash->cur_fast_read) {
            if ((ret = spi_flash_cmd_mio_read(flash, flash->cur_fast_read,
                                              offset, buf, rlen) < 0)) {
                break;
            }
        } else {
	        u8 cmd[SPI_FLASH_CMD_LEN];
	        cmd[0] = SPI_FLASH_OP_READ;
            spi_flash_addr(cmd, offset);
            if ((ret = spi_flash_cmd_read(flash, cmd, 4, 0, buf, rlen) < 0))
                break;
        }
		buf += rlen;
		offset += rlen;
		size -= rlen;
	}
#else
	cmd[0] = SPI_FLASH_OP_READ;
	spi_flash_addr(cmd, offset);
	ret = spi_flash_cmd_read(flash, cmd, 4, 0, buf, size);
#endif
	if (ret < 0)
		return ret;

	return ret;
}


int flash_erase(off_t addr, size_t size, unsigned how)
{
	struct spi_flash *flash = spi_flash_find_by_addr(addr);

	if (!flash)
		return -ENODEV;

	addr -= (off_t) flash->mem_base;
	return spi_flash_erase(flash, addr, size, how);
}

int flash_write(off_t addr, void *buf, size_t size)
{
	struct spi_flash *flash = spi_flash_find_by_addr(addr);

	if (!flash)
		return -ENODEV;

	addr -= (off_t) flash->mem_base;
	return spi_flash_write(flash, addr, size, buf);
}

int flash_read(off_t addr, void *buf, size_t size)
{
	struct spi_flash *flash = spi_flash_find_by_addr(addr);

	if (!flash)
		return -ENODEV;

	addr -= (off_t) flash->mem_base;
	return spi_flash_read(flash, addr, size, buf);
}

int flash_unlock_all(void)
{
	int ret;
	struct spi_flash *flash;
	int index = 0;

	while(1) {
		flash = spi_flash_find_device(index++);
		if (flash == NULL)
			break;
		call(spi_flash_write_enable(flash));
		call(spi_flash_unlock(flash));
		call(spi_flash_write_disable(flash));
	};
 out:
	return 0;
}


/*
 * Flash partition management
 *
 * - There is a one partition table at the top erase block of the last flash
 *   memory (the one with the highest system address).
 *
 */

static inline struct spi_flash *flash_last_device(void)
{
	if (list_empty(&spi_flash_list))
		return NULL;

	return list_last_entry(&spi_flash_list, struct spi_flash, list);
}

off_t spi_flash_top_block(struct spi_flash *flash)
{
	off_t offset = flash->size - 1;
	int ret;

	ret = spi_flash_lookup_erase_block(flash, &offset, NULL,
					   EB_SUPERSET | EB_UNALIGNED |
					   EB_MIN_BLOCK);
	if (ret < 0)
		return (off_t) -1;

	return offset;
}

static int __flash_probe_partition_table(struct spi_flash *flash)
{
	off_t offset;
	size_t len = sizeof(flash->partition);
	int ret;

	offset = spi_flash_top_block(flash);
	ret = spi_flash_read(flash, offset, len, flash->partition);
	if (ret < 0)
		return -1;

	return 0;
}

static inline int flash_init_partition_table(void)
{
	struct spi_flash *flash = flash_last_device();

	if (flash)
		return __flash_probe_partition_table(flash);
	return -ENODEV;
}

/*__initcall__(1, flash_init_partition_table);*/

static int flash_sync_partition_table(struct spi_flash *flash)
{
	size_t len = sizeof(flash->partition);
	int ret;
	off_t offset = spi_flash_top_block(flash);

	ret = spi_flash_erase(flash, offset, flash->size - offset, 0);
	if (ret < 0)
		return ret;

	return spi_flash_write(flash, offset, len, flash->partition);
}

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define flash_end(f) ((off_t) (f)->mem_base + (f)->size)

#ifdef CONFIG_BOOTLOADER
static size_t estimate_partition_size(struct spi_flash *flash,
					   flash_part_t *p) __maybe_unused;
static size_t estimate_partition_size(struct spi_flash *flash,
					   flash_part_t *p)
{
	off_t start = p->start - (off_t) flash->mem_base;
	off_t end;
	unsigned buf, flags = EB_MIN_BLOCK | EB_SUPERSET;

	/* Find the first erased-and-not-written block */

	/* Read the first word of each erase block and see if it is -1 */
	while (start < flash->size) {
		int i, ret;

		spi_flash_lookup_erase_block(flash, &start, &end, flags);

		for (i = start; i < end; i += 4) {
			ret = spi_flash_read(flash, i, sizeof(buf), &buf);
			if (ret < 0) {
				err("Read error at %08x\n", i);
				return 0;
			}
			if (buf != -1U)
				break;
		}
		if (i  ==  end)
			/* the entire block has 0xff */
			return start;
		start = end;
	}
 	return 0;
}
#endif

/* XXX use less efficient sorting function than qsort to reduce code size */
static void flash_sort_partition_table(struct spi_flash *flash)
{
	flash_part_t temp[ARRAY_SIZE(flash->partition)], *part, *minp;
	uint32_t min;
	int i, j;

#define MAX32	(0xFFFFFFFF)
	memset(temp, 0xFF, sizeof(temp));
	for (i = 0; i < ARRAY_SIZE(temp); i++) {
		min = MAX32;
		minp = NULL;
		for (j = 0; j < ARRAY_SIZE(flash->partition); j++) {
			part = &flash->partition[j];
			if (part->start < min) {
				minp = part;
				min = part->start;
			}
		}
		if (minp) {
			temp[i] = *minp;
			minp->start = MAX32;
		}
	}

	memcpy(flash->partition, temp, sizeof(flash->partition));
#undef MAX32
}

#ifdef CONFIG_BOOTLOADER
static void
flash_build_initial_partition_table(struct spi_flash *flash)
{
	flash_part_t *pt = flash->partition;
	struct spi_flash *first = spi_flash_find_device(0);

	if (!first)
		return;

	memset(pt, '\xff', sizeof(flash->partition));

	/* Partition 0: WISE-BOOT */
	strcpy(pt[0].name, "wise-boot");
	pt[0].start = (unsigned long) first->mem_base;
	pt[0].size = 0x40000;
	pt[0].data_length = pt[0].size;
	pt[0].flags = P_SYSTEM;

	/* Partition 1: NUTTX */
	strcpy(pt[1].name, "nuttx");
	pt[1].start = (unsigned long) (pt[0].start + pt[0].size);
	pt[1].size = CONFIG_PT_NUTTX_SIZE * 1024;
	pt[1].data_length = pt[1].size;
	pt[1].flags = P_SYSTEM;

	/* Partition 2: WISE */
	strcpy(pt[2].name, "wise");
	pt[2].start = (unsigned long) (pt[1].start + pt[1].size);
	pt[2].size = CONFIG_PT_WISE_SIZE * 1024;
	pt[2].data_length = pt[2].size;
	pt[2].flags = P_SYSTEM;

#ifdef CONFIG_EFUSE_SIMULATION

    /* Partition 4: eFuse simulation */
	strcpy(pt[4].name, "efuse-sim");
	pt[4].start = (unsigned long) (pt[2].start + pt[2].size);
	pt[4].size = 0x40000;
	pt[4].data_length = pt[4].size;
	pt[4].flags = P_SYSTEM;

#endif

	/* Partition 3: flash partition table */
	strcpy(pt[3].name, "config");
	pt[3].start = ((unsigned long) flash->mem_base +
		       spi_flash_top_block(flash));
	pt[3].size = flash->size - spi_flash_top_block(flash);
	pt[3].data_length = sizeof(flash->partition);
	pt[3].flags = P_SYSTEM;
}
#endif

/**
 * flash_lookup_partition() - look up the partition
 * @name: the name of the partition to look up
 * @flash: the placeholder of the flash device
 * @num: the placeholder of the partition number
 *
 * This function looks up the partition with given @name.
 */
flash_part_t *flash_lookup_partition(char *name, int *num)
{
	struct spi_flash *f = flash_last_device();
	flash_part_t *p;
	int n;

	if (f == NULL)
		return NULL;

	p =  f->partition;
	for (n = 0; n < ARRAY_SIZE(f->partition); n++, p++) {
		if (!strncmp(name, p->name, sizeof(p->name))) {
			if (num)
				*num = n;
			return p;
		}
	}
	return NULL;
}


/**
 * flash_find_device_by_part() - reverse lookup flash device from partition
 */
static inline struct spi_flash *flash_find_device_by_part(flash_part_t *p)
{
	return spi_flash_find_by_addr(p->start);
}

static inline int flash_add_partition(flash_part_t *p)
{
	struct spi_flash *flash = flash_last_device();
	flash_part_t *q;

	if (!flash) {
		err("No flash memory is probed\n");
		return -ENODEV;
	}
	if (flash_find_device_by_part(p) == NULL) {
		err("No suitable flash for partition %s (%08x-%08x)\n",
		       p->name, (unsigned long) p->start,
		       (unsigned long) p->start + p->size);
		return -EINVAL;
	}
	if (p->name[0] == '\0' || p->name[0] == '\xff')
		return -EINVAL;
	if (flash_lookup_partition(p->name, NULL))
		return -EEXIST;

	if (p->size == -1)
		return -EINVAL;

	for (q = flash->partition; q->name[0] != '\xff'; q++) {
		/* Check if @part overlaps with existing partitions */
		if ((p->start < q->start + q->size) &&
		    (p->start + p->size > q->start)) {
			err("Overlaps with an existing partition %s\n",
			       q->name);
			return -EINVAL;
		}
	}

	/* q points to the first free partition table entry */
	memcpy(q, p, sizeof(*p));
	flash_sort_partition_table(flash);
	flash_sync_partition_table(flash);
	return 0;
}

static inline void flash_delete_partition(flash_part_t *part)
{
	struct spi_flash *flash = flash_last_device();

	if (flash == NULL) {
		err("Failed to probe a flash device\n");
		return;
	}

	/* Erase the partition */
	if (flash_erase(part->start, part->size, 0) != 0) {
		err("Failed to erase the partition %s@%08x-%08x\n",
		       part->name, (unsigned long) part->start,
		       (unsigned long) part->start + part->size);
		return;
	}

	/* Invalidate @partition and update the partition table */
	memset(part, 0xff, sizeof(*part));
	flash_sort_partition_table(flash); /* To keep the table compact */
	flash_sync_partition_table(flash);
}

/**
 * flash_partition_compute_erase_size()
 *
 * Compute the least common multiple of the smallest erase block size
 * of erase regions consitituting @part.
 *
 */
int flash_partition_erase_size(flash_part_t *part)
{
	struct spi_flash *flash = flash_find_device_by_part(part);
	struct erase_region *r;
	struct erase_method *m;
	off_t start, end;
	int size = 4 * 1024;
	u32 mask;

	start = part->start - (off_t) flash->mem_base;
	end = start + part->size;

	while (start < end) {
		int min_erase_size = 64*1024;

		r = spi_flash_lookup_region(flash, start);
		if (r == NULL) {
			return -EINVAL;
		}
		m = flash->erase_info;
		for (mask = r->method; mask != 0; mask >>= 1, m++) {
			if (!(mask & 1))
				continue;
			min_erase_size = min(m->size, min_erase_size);
		}

		/* size: LCM of size and min_erase_size */
		size = max(size, min_erase_size);
		start += r->size;
	}

	return size;
}

#ifdef CONFIG_CMD_FLASH

/**
 * Flash CLI commands
 */
#include <cli.h>

/*
 * "4096"   --> 4096
 * "0x1000" --> 4096
 * "4K"     --> 4096
 * "4M"     --> 4*1024*1024
 */
static int str2size(const char *p, unsigned long *sz)
{
	unsigned long size;
	char *endp, c;

	if (sz == NULL || p == NULL || *p == '\0')
		return -1;

	size = strtoul(p, &endp, 0);
	if ((c = *endp) != '\0') {
		*endp = '\0';
		size = strtoul(p, NULL, 0);
		*endp = c;

		switch (c) {
		case 'K':
			size *= 1024;
			break;
		case 'M':
			size *= 1024 * 1024;
			break;
		case 'k':
			size *= 1000;
			break;
		case 'm':
			size *= 1000 * 1000;
			break;
		default:
			return -1;
		}
	}
	*sz = size;
	return 0;
}

#ifdef CONFIG_BOOTLOADER
static int do_flash_init(int argc, char *argv[])
{
	struct spi_flash *flash;

	if ((flash = flash_last_device()) == NULL) {
		error("Could not identify the flash memory\n");
		return CMD_RET_FAILURE;
	}

	flash_build_initial_partition_table(flash);
	flash_sync_partition_table(flash);
	return 0;
}
#endif

static int do_flash_list(int argc, char *argv[])
{
	struct spi_flash *flash;
	flash_part_t *p;
	int i = 0;

	list_for_each_entry(flash, &spi_flash_list, list) {
		printf("#%d: ", flash->index);
		spi_flash_print(flash, "", os_printf);
	}

	flash = flash_last_device();
	if (flash == NULL ||
		__flash_probe_partition_table(flash) < 0) {
		error("Failed to locate partition table\n");
	} else {
	    printf("\n");
        p = flash->partition;
        printf("%-12s %-21s %-10s\n", "Name", "Address", "Size (KiB)");
        for (i = 0; i < ARRAY_SIZE(flash->partition); i++, p++) {
            if (p->name[0] == '\xff')
                break;

            printf("%-12s 0x%08x-0x%08x %9d\n", p->name,
                   (unsigned long) p->start,
                   (unsigned long) p->start + p->size, (int) p->size/1024);
        }
        if (i == 0) {
            error("Partition table invalid. Please run 'flash init'\n");
        }
    }
	return CMD_RET_SUCCESS;
}

/*
 * flash mkpart <name> <flash address> <size>
 */
static int do_flash_mkpart(int argc, char *argv[])
{
	flash_part_t p;
	unsigned long addr, size;
	int err = -EINVAL;
	char *endp;

	memset(&p, '\xff', sizeof(p));

	if (argc != 4)
		return CMD_RET_USAGE;

	strncpy(p.name, argv[1], sizeof(p.name));

	addr = strtoul(argv[2], &endp, 0);
	if (*endp != '\0') {
		error("Invalid flash address %08x\n", addr);
		return CMD_RET_USAGE;
	}
	p.start = addr;

	if (str2size(argv[3], &size) != 0) {
		error("invalid partition size %s\n", argv[3]);
		return CMD_RET_FAILURE;
	}
	p.size = size;
	p.flags = 0;

	err = flash_add_partition(&p);
	if (err != 0) {
		error("Failed to create partition %s @0x%08x-0x%08x: %d\n",
		       p.name,
		       (unsigned long) p.start,
		       (unsigned long) p.start + p.size,
		       -err);
		return CMD_RET_FAILURE;

	}
	return CMD_RET_SUCCESS;
}

static int do_flash_rmpart(int argc, char *argv[])
{
	flash_part_t *p;

	if (argc != 2)
		return CMD_RET_USAGE;

	p = flash_lookup_partition(argv[1], NULL);
	if (p == NULL) {
		error("No such partition: %s\n", argv[1]);
		return CMD_RET_FAILURE;
	}

#ifndef CONFIG_BOOTLOADER
	if (p->flags & P_SYSTEM) {
		error("Cannot delete system partition: %s\n", argv[1]);
		return CMD_RET_FAILURE;
	}
#endif

	debug("Deleting partition %s\n", argv[1]);

	flash_delete_partition(p);

	return CMD_RET_SUCCESS;
}

static int do_flash_erase_part(int argc, char *argv[])
{
	flash_part_t *p;

	if (argc != 2)
		return CMD_RET_USAGE;

	p = flash_lookup_partition(argv[1], NULL);
	if (p == NULL) {
		error("No such partition: %s\n", argv[1]);
		return CMD_RET_FAILURE;
	}

#ifndef CONFIG_BOOTLOADER
	if (p->flags & P_SYSTEM) {
		error("Cannot erase system partition: %s\n", argv[1]);
		return CMD_RET_FAILURE;
	}
#endif
	debug("Wiping partition %s 0x%08x-0x%08x\n", p->name,
	       (unsigned long) p->start,
	       (unsigned long) p->start + p->size);

	if (flash_erase(p->start, p->size, 0) != 0)
		return CMD_RET_FAILURE;

	return CMD_RET_SUCCESS;
}

#if defined(CONFIG_TINYUSB) \
        && defined(CONFIG_TUSB_TUD_DFU) \
        && defined(CONFIG_TUSB_TUD_DFU_RUNTIME)
#include "tusb_main.h"

static struct _dfu_ctx {
	u32 addr;
	osSemaphoreId_t dfu_sem_id;
} dfu_ctx;

static int dfu_prog(void *ctx, void const *buf, size_t size)
{
	struct _dfu_ctx *dctx = ctx;
	u32 written;

	written = flash_write(dctx->addr, (void *)buf, size);
	dctx->addr += written;

    return written;
}

static int dfu_done(void *ctx)
{
    osStatus_t res;
	struct _dfu_ctx *dctx = ctx;

    res = osSemaphoreRelease(dctx->dfu_sem_id);

    return (res == osOK ? 0 : -1);
}

static int flash_update_by_dfu(int argc, char *argv[])
{
	flash_part_t *p;
	char *name = argv[1];
	struct _dfu_ctx *ctx = &dfu_ctx;

	p = flash_lookup_partition(name, NULL);
	if (p == NULL) {
		error("No such partition: %s\n", name);
		return CMD_RET_FAILURE;
	}

#ifndef CONFIG_BOOTLOADER
	if (p->flags & P_SYSTEM) {
		error("Cannot update system partition: %s\n", argv[1]);
		return CMD_RET_FAILURE;
	}
#endif

    if (flash_erase(p->start, p->size, 0) != 0) {
		error("Failed to erase the partition\n");
		return CMD_RET_FAILURE;
	}

	memset(ctx, 0, sizeof(*ctx));

    ctx->dfu_sem_id = osSemaphoreNew(1, 0, NULL);
	ctx->addr = p->start;

    tusb_dfu_start(ctx, dfu_prog, dfu_done);

    osSemaphoreAcquire(ctx->dfu_sem_id, osWaitForever);

    osSemaphoreDelete(ctx->dfu_sem_id);

    return CMD_RET_SUCCESS;
}

#endif

#include <u-boot/xyzModem.h>

static int getcxmodem(void) {
	int ret = getchar_timeout(0);
	if (ret >= 0)
		return (char) ret;

	return -1;
}

#define XYZM_BUFSZ	(1024)

static int flash_update_by_xyzmodem(int argc, char *argv[])
{
	connection_info_t info = {
		.mode = xyzModem_ymodem,
	};
	flash_part_t *p;
	int size = 0, err, res;
	char *name, *buf = NULL;
	off_t offset = 0;

	if (argc == 4) {
		if (!strcmp(argv[3], "x")) {
			info.mode = xyzModem_xmodem;
		} else if (strcmp(argv[3], "y")) {
			return CMD_RET_USAGE;
		}
	}

	name = argv[1];

	p = flash_lookup_partition(name, NULL);
	if (p == NULL) {
		error("No such partition: %s\n", name);
		return CMD_RET_FAILURE;
	}

#ifndef CONFIG_BOOTLOADER
	if (p->flags & P_SYSTEM) {
		error("Cannot update system partition: %s\n", argv[1]);
		return CMD_RET_FAILURE;
	}
#endif

	buf = malloc(XYZM_BUFSZ);
	if (buf == NULL) {
		error("No memory available for receiving\n");
		return CMD_RET_FAILURE;
	}

	if (flash_erase(p->start, p->size, 0) != 0) {
		error("Failed to erase the partition\n");
		goto out;
	}

	if ((res = xyzModem_stream_open(&info, &err)) != 0) {
		error("%s\n", xyzModem_error(err));
		goto out;
	}

	while ((res = xyzModem_stream_read(buf, XYZM_BUFSZ, &err)) > 0) {
		int rc;

		rc = flash_write(p->start + offset, buf, res);
		if (rc < 0) {
			error("Flash program failed for partition %s @0x%08x\n",
			       name, (unsigned long) p->start + offset);
			err = -1;
			goto out;
		}

		size += res;
		offset += res;
        /* Reading less than 1024 bytes is not necessarily an error. */
        err = 0;
	}

	if (err == xyzModem_timeout && !(size % XYZM_BUFSZ)) {
		/* False timeout at the end. */
		err = 0;
	}

	debug("## Total Size = 0x%08x = %d Bytes\n", size, size);
 out:
	xyzModem_stream_close(&err);
	xyzModem_stream_terminate(false, getcxmodem);
	free(buf);

	return (err == 0) ? 0: CMD_RET_FAILURE;
}

/* flash update <name> [-r <x|y|u> ] */
static int do_flash_update(int argc, char *argv[])
{
	if (argc == 4) {
		if (strcmp(argv[2], "-r"))
			return CMD_RET_USAGE;
#if defined(CONFIG_TINYUSB) \
        && defined(CONFIG_TUSB_TUD_DFU) \
        && defined(CONFIG_TUSB_TUD_DFU_RUNTIME)
   		else if (!strcmp(argv[3], "u"))
			return flash_update_by_dfu(argc, argv);
#endif
	} else if (argc != 2)
		return CMD_RET_USAGE;

    return flash_update_by_xyzmodem(argc, argv);
}

int do_flash_erase(int argc, char *argv[])
{
	unsigned long addr;
	size_t size;
	struct spi_flash *flash;
	int ret;
	unsigned how = 0;

	if (argc < 2)
		return CMD_RET_USAGE;
	else if (argc == 2)
		return do_flash_erase_part(argc, argv);

	addr = strtoul(argv[1], NULL, 16);
	size = strtoul(argv[2], NULL, 0);

	if (argc > 3) {
		how = strtoul(argv[3], NULL, 0);
		if (how)
			how = (EB_MIN_BLOCK | EB_SUPERSET | EB_UNALIGNED);
	}

	flash = spi_flash_find_by_addr(addr);
	if (!flash) {
		error("No flash memory is mapped to address 0x%08x\n", addr);
		return CMD_RET_FAILURE;
	}

	debug("Erase flash memory region: 0x%08x - 0x%08x\n",
	       addr, addr + size);

	ret = flash_erase(addr, size, how);
	if (ret) {
		error("Erase failed: %s\n", strerror(errno));
	} else {
		debug("Erase done\n");
	}

	return ret;
}

int do_flash_write(int argc, char *argv[])
{
	unsigned long flash_addr, addr;
	size_t size;
	struct spi_flash *flash;
	int ret;

	if (argc < 4)
		return CMD_RET_USAGE;

	flash_addr = strtoul(argv[1], NULL, 16);
	addr = strtoul(argv[2], NULL, 16);
	size = strtoul(argv[3], NULL, 0);

	flash = spi_flash_find_by_addr(flash_addr);
	if (!flash) {
		error("No flash memory is mapped to address 0x%08x\n", flash_addr);
		return CMD_RET_FAILURE;
	}

	debug("Write to flash memory region: 0x%08x - 0x%08x\n",
	       flash_addr, flash_addr + size);

	ret = flash_write(flash_addr, (void *)addr, size);
	if (ret != size) {
		error("Flash write failed\n");
	} else {
		debug("Flash write finished\n");
	}

	return ret;
}

int do_flash_unlock(int argc, char *argv[])
{
	flash_unlock_all();
	return CMD_RET_SUCCESS;
}

static const struct cli_cmd flash_cmd[] = {
	CMDENTRY(erase, do_flash_erase, "", ""),
	CMDENTRY(write, do_flash_write, "", ""),
	CMDENTRY(unlock, do_flash_unlock, "", ""),
#ifdef CONFIG_BOOTLOADER
	CMDENTRY(init, do_flash_init, "init", ""),
#endif
	CMDENTRY(list, do_flash_list, "list", ""),
	CMDENTRY(mkpart, do_flash_mkpart, "add a partition", ""),
	CMDENTRY(rmpart, do_flash_rmpart, "delete a partition", ""),
	CMDENTRY(update, do_flash_update, "update a partition", ""),
};

static int do_flash(int argc, char *argv[])
{
	const struct cli_cmd *cmd;

	argc--;
	argv++;

	if (argc == 0)
		return CMD_RET_USAGE;

	cmd = cli_find_cmd(argv[0], flash_cmd, ARRAY_SIZE(flash_cmd));
	if (cmd == NULL)
		return CMD_RET_USAGE;

	return cmd->handler(argc, argv);
}

CMD(flash, do_flash,
    "manipulate spi flash memory",
#ifdef CONFIG_BOOTLOADER
    "flash init" OR
#endif
    "flash list" OR
    "flash mkpart <name> <addr> <size>" OR
    "flash rmpart <name>" OR
    "flash update <name> [-r <x|y|u> ]" OR
    "flash erase <name>" OR
    "flash erase <addr> <length> [min. superset:1]" OR
    "flash write <dst addr> <src addr> <length>" OR
    "flash unlock"
    );
#endif
