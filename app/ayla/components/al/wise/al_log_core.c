/*
 * Copyright 2021 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */

#ifdef LOG_CORE_USER
typedef unsigned char	u8;
typedef unsigned short	u16;
typedef unsigned int	u32;
typedef unsigned long long	u64;

typedef signed char	s8;
typedef short		s16;
typedef int		s32;
typedef long long	s64;
#define HAVE_UTYPES

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <elf.h>
#include <ayla/utypes.h>
#include <ayla/compiler_gcc.h>
#include <ayla/base64.h>
#else
#include <string.h>
#include <elf.h>
#include <sdkconfig.h>
#include <esp_idf_version.h>
#include <esp_partition.h>
#ifdef CONFIG_IDF_TARGET_ARCH_XTENSA
#include <xtensa/config/specreg.h>
#include <freertos/xtensa_context.h>
#include <xtensa/specreg.h>
#endif
#include <soc/soc_memory_layout.h>
#include <ayla/utypes.h>
#include <ayla/log.h>
#include <ayla/compiler_gcc.h>
#include <ayla/base64.h>
#include <esp_spi_flash.h>
#include <esp_core_dump.h>
#include <freertos/FreeRTOS.h>
#include <freertos/list.h>
#include <ayla/crc.h>
#endif
/* the following are in components/espcoredump/include_core_dump */
#include <esp_core_dump_types.h>
/* end of espcoredump include files */
#include "pfm_log_core_int.h"

#if !defined(CONFIG_IDF_TARGET_ARCH_XTENSA) && \
    !defined(CONFIG_IDF_TARGET_ARCH_RISCV)
#error define target architecture
#endif

/*
 * For now, use standard snprintf() instead of ayla_snprintf().
 */
#undef snprintf

#ifdef LOG_CORE_USER
#define LOG_WARN "warn: "
#define LOG_ERR "err: "
#define LOG_INFO "info: "
#define LOG_DEBUG "debug: "
#define log_put(...) \
	do { \
		printf(__VA_ARGS__); \
		printf("\n"); \
	} while (0)
#define printcli(fmt, ...) \
	do { \
		printf(__VA_ARGS__); \
		printf("\n"); \
	}
#endif

struct log_core_context {
#ifdef LOG_CORE_USER
	int		fd;
#else
	const esp_partition_t *part;
#endif
	size_t		core_len;
	Elf32_Phdr	*phead;
	unsigned int	phead_count;
};

#define CORE_MAX_REG_PAIRS	20	/* number of extra regs to handle */

static void log_core(const char *fmt, ...)
{
	ADA_VA_LIST args;

	ADA_VA_START(args, fmt);
#ifdef LOG_CORE_USER
	vprintf(fmt, args);
	printf("\n");
#else
	log_put_va(LOG_MOD_DEFAULT, fmt, args);
#endif
	ADA_VA_END(args);
}

static ssize_t log_core_read(const struct log_core_context *ctxt,
	size_t off, void *buf, size_t len)
{
#ifdef LOG_CORE_USER
	int rc;

	if (lseek(ctxt->fd, off, SEEK_SET) == -1) {
		perror("seek");
		log_core(LOG_ERR, "core: seek failed to offset %#zx", off);
		return -1;
	}
	rc = read(ctxt->fd, buf, len);
	if (rc < 0) {
		perror("read");
		log_core(LOG_ERR "core: read failed at offset %#zx len %#zx",
		    off, len);
		return -1;
	}
	if (rc != len) {
		log_core(LOG_ERR "core: read - short at off %#zx len %#zx",
		    off, len);
		return -1;
	}
#else
	esp_err_t err;

	err = esp_partition_read(ctxt->part, off, buf, len);
	if (err) {
		log_core(LOG_ERR "core: read failed %d at offset %#zx len %#zx",
		    err, off, len);
		return -1;
	}
#endif
	return 0;
}

/*
 * Find section containing address.
 */
static Elf32_Phdr *log_core_mem_find(const struct log_core_context *ctxt,
			size_t addr)
{
	Elf32_Phdr *ph;
	unsigned int i;
	size_t off;

	for (i = 0; i < ctxt->phead_count; i++) {
		ph = &ctxt->phead[i];
		if (ph->p_type != PT_LOAD) {
			continue;
		}
		if (ph->p_vaddr > addr) {
			continue;
		}
		off = addr - ph->p_vaddr;
		if (off < ph->p_filesz && off < ph->p_memsz) {
			return ph;
		}
	}
	return NULL;
}

/*
 * Read memory from core file as mapped by program headers.
 */
static int log_core_mem_read(const struct log_core_context *ctxt,
	size_t addr, void *buf, size_t len)
{
	Elf32_Phdr *ph;
	size_t off;

	ph = log_core_mem_find(ctxt, addr);
	if (!ph) {
		return -1;
	}
	off = addr - ph->p_vaddr;
	if (off + len > ph->p_filesz) {
		log_core(LOG_ERR "core: mem read past end of segment "
		   "addr %#zx len %#zx",
		    addr, len);
		return -1;
	}
	if (log_core_read(ctxt,
	     sizeof(core_dump_header_t) + off + ph->p_offset, buf, len)) {
		log_core(LOG_ERR "core: ELF mem read addr %#zx len %#zx",
		    addr, len);
		return -1;
	}
	return 0;
}

/*
 * Add a register and its name to a buffer.
 */
static size_t log_core_reg(char *buf, size_t len, const char *name, u32 val)
{
	return snprintf(buf, len, " %8s: %#10.8x", name, (unsigned int)val);
}

#ifdef CONFIG_IDF_TARGET_ARCH_XTENSA
/*
 * Process return address for Tensillica ISA.
 */
static u32 log_core_ra(u32 pc, u32 ra)
{
	if (ra & 0x80000000U) {
		ra = (ra & ~0xc0000000U) | (pc & 0xc0000000U);
		ra -= 3;	/* show address of call instruction */
	}
	return ra;
}
#endif /* CONFIG_IDF_TARGET_ARCH_XTENSA */

/*
 * Log words of traceback.
 */
static void log_core_trace(u32 *trace, unsigned int count)
{
	unsigned int i;
	char buf[count * 12];
	int len = 0;

	for (i = 0; i < count; i++) {
		len += snprintf(buf + len, sizeof(buf) - len,
		    " %#x", (unsigned int)trace[i]);
	}
	log_core(LOG_WARN "core: trace:%s", buf);
}

#ifdef CONFIG_IDF_TARGET_ARCH_XTENSA
static void log_core_task(struct log_core_context *ctxt, size_t off, size_t len)
{
	struct xtensa_elf_reg_dump *reg_dump;
	struct xtensa_pr_status *ps;
	struct xtensa_greg_set *gp;
	u32 tcb;
	struct freertos_tcb task;
	u32 stack[4];
	u32 trace[6];
	unsigned int i;
	u32 pc;
	u32 sp;
	int blen = 0;
	char buf[120];
	char name[8];

	if (len < sizeof(*reg_dump)) {
		log_core(LOG_ERR "core: ELF regs short off %zx len %zx",
		    off, len);
		return;
	}
	if (len > sizeof(*reg_dump)) {
		len = sizeof(*reg_dump);
	}
	reg_dump = calloc(1, sizeof(*reg_dump));
	if (!reg_dump) {
		log_core(LOG_ERR "core: regs malloc failure");
		return;
	}

	if (log_core_read(ctxt, off, reg_dump, sizeof(*reg_dump))) {
		log_core(LOG_ERR "core: ELF regs read error");
		free(reg_dump);
		return;
	}
	ps = &reg_dump->pr_status;
	tcb = ps->pr_pid;

	/*
	 * Dump thread info.
	 */
	if (log_core_mem_read(ctxt, tcb, &task, sizeof(task))) {
		free(reg_dump);
		return;
	}
	log_core(LOG_WARN "core: task %#x name \"%s\"",
	    (unsigned int)tcb, task.pcTaskName);

	/*
	 * Dump registers.
	 */
	gp = &reg_dump->regs;
	blen = log_core_reg(buf, sizeof(buf), "PC", gp->pc);
	blen += log_core_reg(buf + blen, sizeof(buf) + blen, "PS", gp->ps);
	for (i = 0; i < 16; i++) {
		snprintf(name, sizeof(name), "A%u", i);
		blen += log_core_reg(buf + blen, sizeof(buf) - blen,
		    name, gp->ar[i]);
		if ((i + 2) % 4 == 3) {
			log_core(LOG_WARN "core %s", buf);
			blen = 0;
		}
	}
	blen += log_core_reg(buf + blen, sizeof(buf) - blen, "SAR", gp->sar);
	log_core(LOG_WARN "core:%s", buf);
	blen = 0;
	blen += log_core_reg(buf + blen, sizeof(buf) - blen, "LBEG", gp->lbeg);
	blen += log_core_reg(buf + blen, sizeof(buf) - blen, "LEND", gp->lend);
	blen += log_core_reg(buf + blen, sizeof(buf) - blen,
	    "LCOUNT", gp->lcount);
	log_core(LOG_WARN "core:%s", buf);

	/*
	 * Traceback.
	 */
	pc = gp->pc;
	trace[0] = pc;
	pc = log_core_ra(pc, gp->ar[0]);
	trace[1] = pc;
	i = 2;
	sp = gp->ar[1];
	while (sp) {
		sp -= 4 * sizeof(u32);
		if (log_core_mem_read(ctxt, sp, stack, sizeof(stack))) {
			log_core(LOG_WARN "invalid sp %#x", (unsigned int)sp);
			break;
		}
		if (!stack[0]) {
			break;
		}
		pc = log_core_ra(pc, stack[0]);
		trace[i++] = pc;
		if (i >= ARRAY_LEN(trace)) {
			log_core_trace(trace, i);
			i = 0;
		}
		sp = stack[1];
	}
	if (i) {
		log_core_trace(trace, i);
	}
	free(reg_dump);
}

#elif defined(CONFIG_IDF_TARGET_ARCH_RISCV)

static void log_core_task(struct log_core_context *ctxt, size_t off, size_t len)
{
	struct riscv_prstatus *reg_dump;
	struct riscv_prstatus *ps;
	union riscv_regs *gp;
	u32 tcb;
	struct freertos_tcb task;
	u32 stack[1];
	u32 trace[6];
	u32 pc;
	u32 sp;
	unsigned int i;
	int blen = 0;
	char buf[120];
	static const char reg_names[] = RISCV_REG_NAMES;
	const char *cp;
	char name[RISCV_REG_NAME_LEN + 1];

	if (len < sizeof(*reg_dump)) {
		log_core(LOG_ERR "core: ELF regs short off %zx len %zx",
		    off, len);
		return;
	}
	if (len > sizeof(*reg_dump)) {
		len = sizeof(*reg_dump);
	}
	reg_dump = calloc(1, sizeof(*reg_dump));
	if (!reg_dump) {
		log_core(LOG_ERR "core: regs malloc failure");
		return;
	}

	if (log_core_read(ctxt, off, reg_dump, sizeof(*reg_dump))) {
		log_core(LOG_ERR "core: ELF regs read error");
		free(reg_dump);
		return;
	}
	ps = reg_dump;
	tcb = ps->pid;

	/*
	 * Dump thread info.
	 */
	if (log_core_mem_read(ctxt, tcb, &task, sizeof(task))) {
		free(reg_dump);
		return;
	}
	log_core(LOG_WARN "core: task %#x name \"%s\"",
	    (unsigned int)tcb, task.pcTaskName);

	/*
	 * Dump registers.
	 */
	gp = &reg_dump->regs;
	blen = 0;
	for (i = 0, cp = reg_names; cp < ARRAY_END(reg_names) - 1;
	    cp += RISCV_REG_NAME_LEN, i++) {
		memcpy(name, cp, RISCV_REG_NAME_LEN);
		name[RISCV_REG_NAME_LEN] = '\0';
		blen += log_core_reg(buf + blen, sizeof(buf) - blen,
		    name, gp->as_array[i]);
		if (i % 4 == 3) {
			log_core(LOG_WARN "core %s", buf);
			blen = 0;
		}
	}
	blen += log_core_reg(buf + blen, sizeof(buf) - blen,
	    "signal", ps->signal);
	log_core(LOG_WARN "core:%s", buf);
	blen = 0;

	/*
	 * Traceback.
	 */
	log_core(LOG_WARN "core: note: Traceback may include unrelated "
	    "addresses from stack.");
	pc = gp->pc;
	trace[0] = pc;
	trace[1] = gp->ra;
	i = 2;
	sp = gp->sp;
	while (sp) {
		if (sp >= tcb) {
			break;		/* end of stack is start of TCB */
		}
		if (log_core_mem_read(ctxt, sp, stack, sizeof(stack))) {
			break;
		}
		sp += sizeof(u32);
		pc = stack[0];
		if (!esp_ptr_executable((void *)pc)) {
			continue;
		}
		trace[i++] = pc;
		if (i >= ARRAY_LEN(trace)) {
			log_core_trace(trace, i);
			i = 0;
		}
	}
	if (i) {
		log_core_trace(trace, i);
	}
	free(reg_dump);
}
#endif /* CONFIG_IDF_TARGET_ARCH_RISCV */

#ifdef CONFIG_IDF_TARGET_ARCH_XTENSA
static void log_core_notes_extra_info(struct log_core_context *ctxt,
	 size_t off, size_t len)
{
	struct {
		struct xtensa_extra_info info;
		struct xtensa_core_dump_reg_pair pair[CORE_MAX_REG_PAIRS];
	} buf;
	unsigned int i;
	unsigned int reg;
	u32 exccause = 0;
	u32 excvaddr = 0;

	if (len < sizeof(buf.info)) {
		log_core(LOG_ERR "core: extra info short off %zx len %zx",
		    off, len);
		return;
	}
	if (len > sizeof(buf)) {
		len = sizeof(buf);
	}
	if (log_core_read(ctxt, off, &buf, sizeof(buf))) {
		log_core(LOG_ERR "core: ELF regs read error");
		return;
	}
	len = (len - sizeof(buf.info)) / sizeof(buf.pair[0]);
	for (i = 0; i < len; i++) {
		reg = buf.pair[i].reg_val;
		switch (buf.pair[i].reg_index) {
		case EXCCAUSE:
			exccause = reg;
			break;
		case EXCVADDR:
			excvaddr = reg;
			break;
		default:
			continue;		/* skip other regs */
		}
	}
	log_core(LOG_WARN "core: %8s %#x %8s %#x %8s %#x",
	    "task", buf.info.crashed_task_tcb,
	    "EXCCAUSE", exccause, "EXCVADDR", excvaddr);
}

#elif defined(CONFIG_IDF_TARGET_ARCH_RISCV)

static void log_core_notes_extra_info(struct log_core_context *ctxt,
	 size_t off, size_t len)
{
	struct {
		struct riscv_extra_info info;
	} buf;

	if (len < sizeof(buf.info)) {
		log_core(LOG_ERR "core: extra info short off %zx len %zx",
		    off, len);
		return;
	}
	if (len > sizeof(buf)) {
		len = sizeof(buf);
	}
	if (log_core_read(ctxt, off, &buf, sizeof(buf))) {
		log_core(LOG_ERR "core: extra info read error");
		return;
	}
	log_core(LOG_WARN "core: %8s %#x", "task", buf.info.crashed_task_tcb);
}
#endif /* CONFIG_IDF_TARGET_ARCH_RISCV */

/*
 * Show dump info.
 */
static void log_core_version(struct log_core_context *ctxt,
	size_t off, size_t len)
{
	struct core_dump_elf_version ver = {0};

	if (len > sizeof(ver)) {
		len = sizeof(ver);
	}
	if (log_core_read(ctxt, off, &ver, len)) {
		return;
	}
#if CONFIG_ESP_COREDUMP_CHECKSUM_SHA256
	log_core(LOG_WARN "core: ELF SHA256 \"%s\"", ver.sha256);
#elif CONFIG_ESP_COREDUMP_CHECKSUM_CRC32
	/* Note: on RISCV ESP32-C3, this is actually a SHA256 hash */
	log_core(LOG_WARN "core: ELF SHA256 \"%s\"", ver.sha256);
#else
#error espcoredump verification type not configured
#endif
}

/*
 * Iterate through each note in a notes section.
 */
static void log_core_dump_info_note(struct log_core_context *ctxt,
	size_t off, size_t len)
{
	size_t tlen;
	size_t nlen;
	size_t desc_off;
	struct {
		Elf32_Nhdr elf_nhdr;
		char name[32];
	} buf;
	Elf32_Nhdr *note;
	int task_done = 0;

	for (; len > 0; off += nlen, len -= nlen) {
		tlen = len;
		if (tlen < sizeof(*note)) {
			log_core(LOG_ERR "core: "
			    "short note section off %#x len %#x",
			    (unsigned int)off, (unsigned int)len);
			break;
		}
		if (tlen > sizeof(buf)) {
			tlen = sizeof(buf);
		}
		if (log_core_read(ctxt, off, &buf, tlen)) {
			log_core(LOG_ERR "core: note read err");
			break;
		}
		note = &buf.elf_nhdr;
		nlen = sizeof(*note) + note->n_namesz + note->n_descsz;
		if (nlen > len) {
			log_core(LOG_ERR "core: note too long. "
			    "off %#x nlen %zx rem %zx",
			    (unsigned int)off, nlen, len - off);
			break;
		}
		if (note->n_namesz > sizeof(buf.name)) {
			continue;
		}
		desc_off = off + sizeof(*note) + note->n_namesz;
		switch (note->n_type) {
		case ELF_CORE_SEC_TYPE:			/* type 1 */
			/*
			 * Show the first task only. It took the core dump.
			 */
			if (task_done) {
				break;
			}
			task_done = 1;

			/*
			 * task registers
			 */
			log_core_task(ctxt, desc_off, note->n_descsz);
			break;
		case ELF_ESP_CORE_DUMP_INFO_TYPE:	/* 8266 = 0x204a */
			/*
			 * SHA256 hash of ELF file.
			 */
			log_core_version(ctxt, desc_off, note->n_descsz);
			break;
		case ELF_ESP_CORE_DUMP_EXTRA_INFO_TYPE:	/* 677 = 0x2a5 */
			/*
			 * Extra info: special registers.
			 */
			log_core_notes_extra_info(ctxt,
			    desc_off, note->n_descsz);
		}
	}
}

/*
 * Print info about core dump, if any.
 * This may be put into a log snapshot.
 */
static void log_core_dump_info(struct log_core_context *ctxt)
{
	size_t addr;
	core_dump_header_t head;
	Elf32_Ehdr elf_head;
	Elf32_Phdr *ph;
	unsigned int i;

	/*
	 * Read header.
	 */
	if (log_core_read(ctxt, 0, &head, sizeof(head))) {
		log_core(LOG_ERR "core: head read error");
		return;
	}

	/*
	 * Require the core dump use the same type of verification
	 * (CRC32 or SHA256) for which the running system is configured.
	 */
	if (head.version != esp_core_dump_elf_version()) {
		log_core(LOG_ERR "core: unsupported core dump version %x",
		    (unsigned int)head.version);
		return;
	}
	if (head.data_len != ctxt->core_len) {
		log_core(LOG_ERR "core: bad len %u",
		    (unsigned int) head.data_len);
		return;
	}

	/*
	 * Read ELF header.
	 */
	addr = sizeof(head);
	if (log_core_read(ctxt, addr, &elf_head, sizeof(elf_head))) {
		log_core(LOG_ERR "core: ELF head read error");
		return;
	}
	if (memcmp(elf_head.e_ident, ELFMAG, SELFMAG) ||
	    elf_head.e_ident[EI_CLASS] != ELFCLASS32 ||
	    elf_head.e_ident[EI_DATA] != ELFDATA2LSB ||
	    elf_head.e_ident[EI_VERSION] != EV_CURRENT) {
		log_core(LOG_ERR "core: bad ELF type");
		return;
	}
	if (elf_head.e_phoff + elf_head.e_phentsize > ctxt->core_len ||
	    elf_head.e_phentsize != sizeof(*ph)) {
		log_core(LOG_ERR "core: bad program header size/len");
		return;
	}

	log_core(LOG_WARN "core: a core dump was taken earlier");

	/*
	 * Read program headers.
	 */
	ctxt->phead = calloc(elf_head.e_phnum, sizeof(*ctxt->phead));
	if (!ctxt->phead) {
		log_core(LOG_ERR "core: phead malloc failed");
		return;
	}
	addr = sizeof(head) + sizeof(elf_head);
	if (log_core_read(ctxt, addr, ctxt->phead,
	     elf_head.e_phnum * sizeof(*ctxt->phead))) {
		log_core(LOG_ERR "core: ELF phdr read error");
		goto out;
	}
	ctxt->phead_count = elf_head.e_phnum;

	/*
	 * Handle each section in turn.
	 */
	for (i = 0; i < elf_head.e_phnum; i++) {
		ph = &ctxt->phead[i];
		switch (ph->p_type) {
		case PT_NOTE:
			log_core_dump_info_note(ctxt,
			    sizeof(head) + ph->p_offset,
			    ph->p_filesz);
			break;
		default:
			break;
		}
	}
out:
	free(ctxt->phead);
	ctxt->phead = NULL;
	ctxt->phead_count = 0;
}

#ifndef LOG_CORE_USER

/*
 * Return non-zero if a core dump has been saved.
 */
int pfm_log_core_is_present(void)
{
	size_t addr;
	size_t len;
	esp_err_t err;

	err = esp_core_dump_image_get(&addr, &len);
	if (err) {
		return 0;
	}
	return 1;
}

/*
 * If a core dump is present, log information about it.
 * Returns 0 on success, -1 on error, 1 if no core dump present.
 */
int al_log_core_dump_info(void)
{
	struct log_core_context log_core_context = { 0 };
	struct log_core_context *ctxt = &log_core_context;
	size_t addr;
	esp_err_t err;

	/*
	 * Find the flash partition offset and overall length of the dump.
	 */
	err = esp_core_dump_image_get(&addr, &ctxt->core_len);
	if (err) {
		return 1;
	}

	/*
	 * Find the partition.
	 */
	ctxt->part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
	    ESP_PARTITION_SUBTYPE_DATA_COREDUMP, NULL);
	if (!ctxt->part) {
		log_core(LOG_ERR "core: partition not found");
		return -1;
	}
	log_core_dump_info(ctxt);
	return 0;
}

/*
 * If a core dump is present, dump it to the log in base-64.
 * Returns 0 on success, less than zero on error, 1 if core dump is not present.
 */
int al_log_core_dump(void)
{
	const esp_partition_t *part;
	size_t addr = 0;
	size_t len = 0;
	char buf[48];	/* Read buffer.  Size must be a multiple of 3 and 16. */
	char lbuf[BASE64_LEN_EXPAND(sizeof(buf)) + 4]; /* line buffer */
	size_t tlen;
	size_t llen;
	size_t off;
	esp_err_t err;

	err = esp_core_dump_image_get(&addr, &len);
	if (err) {
		return 1;
	}
	part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
	    ESP_PARTITION_SUBTYPE_DATA_COREDUMP, NULL);
	if (!part) {
		return -1;
	}
	for (off = 0; off < len; off += tlen) {
		tlen = len - off;
		if (tlen > sizeof(buf)) {
			tlen = sizeof(buf);
		}
		err = esp_partition_read(part, off, buf, tlen);
		if (err) {
			return -1;
		}
		llen = sizeof(lbuf);
		if (ayla_base64_encode(buf, tlen, lbuf, &llen) < 0) {
			break;
		}
		printcli("%s", lbuf);
	}
	return 0;
}

/*
 * If a core dump is present, erase it.
 * This is useful during problem analysis when crashes don't always create
 * a new dump.
 */
void al_log_core_dump_erase(void)
{
	esp_err_t err;

	err = esp_core_dump_image_erase();
	if (err) {
		log_core(LOG_ERR "coredump erase err %#x", err);
	}
}

/*
 * Return a hash of the core dump.
 * Returns 0 on success, -1 on error, 1 if no core dump present.
 * The hash is the last word of the coredump, which may be the CRC-32 or
 * part of the SHA-256 hash.
 * This is just used to see if the core is the same as seen before or is new.
 */
int pfm_log_core_hash(u32 *hashp)
{
	const esp_partition_t *part;
	size_t addr = 0;
	size_t len = 0;
	u32 hash;
	esp_err_t err;

	err = esp_core_dump_image_get(&addr, &len);
	if (err) {
		return 1;
	}
	part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
	    ESP_PARTITION_SUBTYPE_DATA_COREDUMP, NULL);
	if (!part) {
		return -1;
	}
	err = esp_partition_read(part, len - sizeof(hash), &hash, sizeof(hash));
	if (err) {
		return -1;
	}
	*hashp = hash;
	return 0;
}

#else /* LOG_CORE_USER */

/*
 * User program to test coredump from binary image.
 */
int main(int argc, char **argv)
{
	struct log_core_context log_core_context = { 0 };
	struct log_core_context *ctxt = &log_core_context;
	int fd;
	const char *cmdname = argv[0];

	if (argc != 2) {
		fprintf(stderr, "usage: %s <file>\n", cmdname);
		return 1;
	}

	fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		perror("open");
		return -1;
	}

	ctxt->core_len = lseek(fd, 0, SEEK_END);
	if (ctxt->core_len == (off_t)-1) {
		perror("lseek");
		return -1;
	}

	ctxt->fd = fd;
	log_core_dump_info(ctxt);
	return 0;
}
#endif /* LOG_CORE_USER */
