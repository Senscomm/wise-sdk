/*
 * Copyright 2023-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

#include "hal/kernel.h"
#include "hal/console.h"
#include <hal/kmem.h>
#include <hal/timer.h>
#include <hal/spi-flash.h> /* XXX: use API */
#include "mem.h"
#include "cli.h"
#include "scm_i2s.h"

static struct scm_i2s_cfg i2s_cfg;

static int scm_cli_i2s_init(int argc, char *argv[])
{
    int err;

    err = scm_i2s_init();
    if (err) {
        printf("i2s init error %x\n", err);
        return CMD_RET_FAILURE;
    }

    return CMD_RET_SUCCESS;
}

static int scm_cli_i2s_deinit(int argc, char *argv[])
{
    int err;

    err = scm_i2s_deinit();
    if (err) {
        printf("i2s deinit error %x\n", err);
        return CMD_RET_FAILURE;
    }

    return CMD_RET_SUCCESS;
}

static int scm_cli_i2s_config(int argc, char *argv[])
{
    int err;

    argc--;
    argv++;

    if (argc < 5) {
        return CMD_RET_USAGE;
    }

    if (!strcmp(argv[0], "rx")) {
        i2s_cfg.dir = SCM_I2S_RX;
    } else if (!strcmp(argv[0], "tx")) {
        i2s_cfg.dir = SCM_I2S_TX;
    } else {
        return CMD_RET_USAGE;
    }

    switch (atoi(argv[1])) {
        case 16:
            i2s_cfg.word_length = SCM_I2S_WL_16;
            break;
        case 20:
            i2s_cfg.word_length = SCM_I2S_WL_20;
            break;
        case 24:
            i2s_cfg.word_length = SCM_I2S_WL_24;
            break;
        default:
            return CMD_RET_USAGE;
    }

    switch (atoi(argv[2])) {
        case 0:
            i2s_cfg.format = SCM_I2S_FMT_I2S;
            break;
        case 1:
            i2s_cfg.format = SCM_I2S_FMT_LJ;
            break;
        case 2:
            i2s_cfg.format = SCM_I2S_FMT_RJ;
            break;
        default:
            return CMD_RET_USAGE;
    }

    i2s_cfg.role = (atoi(argv[3]) ? SCM_I2S_ROLE_MASTER : SCM_I2S_ROLE_SLAVE);
    i2s_cfg.fs = atoi(argv[4]);
    if (argc > 5) {
        if (argc < 7) {
            return CMD_RET_USAGE;
        }
        i2s_cfg.duration_per_block = atoi(argv[5]);
        i2s_cfg.number_of_blocks = atoi(argv[6]);
    } else if (i2s_cfg.word_length == SCM_I2S_WL_16) {
        i2s_cfg.duration_per_block = 100;
        i2s_cfg.number_of_blocks = 5;
    } else {
        i2s_cfg.duration_per_block = 50;
        i2s_cfg.number_of_blocks = 5;
    }

    i2s_cfg.timeout = 3000; /* 3 seconds */

    if ((err = scm_i2s_configure(&i2s_cfg)) != WISE_OK) {
        if (err == WISE_ERR_NO_MEM) {
            printf("Insufficient memory.\n");
        }
        return CMD_RET_FAILURE;
    }

    return CMD_RET_SUCCESS;
}

/* Dumps only non-zero portion of data in words.
*/
static void hexdump_nz(void *buf, size_t buflen, size_t len)
{
    int i;
    uint32_t *p;

    for (i = 0, p = (uint32_t *)buf; i < buflen / 4; i++, p++) {
        if (p[0]) {
            int rem, rdlen;
            rem = buflen - i * 4;
            rdlen = min(len, rem);
            hexdump(p, rdlen);
            break;
        }
    }
}

static int scm_cli_i2s_read(int argc, char *argv[])
{
    int bufsz;
    uint8_t *buf = NULL;
    int c = -1;
    int err, ret = CMD_RET_FAILURE;

    bufsz = scm_i2s_get_block_buffer_size(&i2s_cfg);
    if (bufsz <= 0) {
        printf("scm_i2s_get_block_buffer_size failed.\n");
        goto done;
    }

    buf = zalloc(bufsz);
    if (!buf) {
        printf("malloc %d bytes failed.\n", bufsz);
        goto done;
    }

    err = scm_i2s_start(SCM_I2S_RX);
    if (err) {
        printf("scm_i2s_start failed.\n");
        goto done;
    }

    while ((c = getchar_timeout(0)) <= 0) {
        size_t len = bufsz;
        err = scm_i2s_read_block(buf, &len);
        if (err) {
            if (err == WISE_ERR_NO_MEM) {
                printf("I2S is not running.\nRun I2S master clock first.\n");
            }
            break;
        }
        hexdump_nz(buf, len, 64);
    }

    err = scm_i2s_stop(SCM_I2S_RX);
    if (err) {
        printf("scm_i2s_stop failed.\n");
        goto done;
    }

    ret = CMD_RET_SUCCESS;

done:
    if (buf) {
        free(buf);
    }

    return ret;
}

#define SAMPLE_NO 64

/* The data represent a sine wave */
static int16_t beep[SAMPLE_NO] = {
    3211,   6392,   9511,  12539,  15446,  18204,  20787,  23169,
    25329,  27244,  28897,  30272,  31356,  32137,  32609,  32767,
    32609,  32137,  31356,  30272,  28897,  27244,  25329,  23169,
    20787,  18204,  15446,  12539,   9511,   6392,   3211,      0,
    -3212,  -6393,  -9512, -12540, -15447, -18205, -20788, -23170,
    -25330, -27245, -28898, -30273, -31357, -32138, -32610, -32767,
    -32610, -32138, -31357, -30273, -28898, -27245, -25330, -23170,
    -20788, -18205, -15447, -12540,  -9512,  -6393,  -3212,     -1,
};

/* Fill buffer with sine wave on left channel, and sine wave shifted by
 * 90 degrees on right channel. "att" represents a power of two to attenuate
 * the samples by
 */
static void fill_buf16(int16_t *tx_block, int att)
{
    int r_idx;

    for (int i = 0; i < SAMPLE_NO; i++) {
        /* Left channel is sine wave */
        tx_block[2 * i] = beep[i] / (1 << att);
        /* Right channel is same sine wave, shifted by 90 degrees */
        r_idx = (i + (ARRAY_SIZE(beep) / 4)) % ARRAY_SIZE(beep);
        tx_block[2 * i + 1] = beep[r_idx] / (1 << att);
    }
}

static void fill_buf32(int32_t *tx_block, int att)
{
    int r_idx;

    for (int i = 0; i < SAMPLE_NO; i++) {
        int32_t lword, rword;
        /* Left channel is sine wave, and scaled up to 32bit range. */
        lword = ((int32_t)beep[i] << 16);
        tx_block[2 * i] = lword / (1 << att);
        /* Right channel is same sine wave, shifted by 90 degrees,
         * and scaled up to 32bit range.*/
        r_idx = (i + (ARRAY_SIZE(beep) / 4)) % ARRAY_SIZE(beep);
        rword = ((int32_t)beep[r_idx] << 16);
        tx_block[2 * i + 1] = rword / (1 << att);
    }
}

static void fill_block_data(void *mem_block, uint32_t size, bool half_word)
{
    u8 *subblk;
    int i, subblk_sz;

    if (half_word) {
        subblk_sz = (ARRAY_SIZE(beep) * 2 * 2);
        for (i = 0, subblk = mem_block; (subblk + subblk_sz) < (u8 *)mem_block + size;
                subblk += subblk_sz, i++) {
            fill_buf16((int16_t *)subblk, i % 3);
        }
    } else {
        subblk_sz = (ARRAY_SIZE(beep) * 4 * 2);
        for (i = 0, subblk = mem_block; (subblk + subblk_sz) < (u8 *)mem_block + size;
                subblk += subblk_sz, i++) {
            fill_buf32((int32_t *)subblk, i % 3);
        }

    }
}

static int scm_cli_i2s_beep(int argc, char *argv[])
{
    int bufsz;
    uint8_t *buf = NULL;
    int err, ret = CMD_RET_FAILURE;

    bufsz = scm_i2s_get_block_buffer_size(&i2s_cfg);
    if (bufsz <= 0) {
        printf("scm_i2s_get_block_buffer_size failed.\n");
        goto done;
    }

    buf = zalloc(bufsz);
    if (!buf) {
        printf("malloc %d bytes failed.\n", bufsz);
        goto done;
    }

    fill_block_data(buf, bufsz, (i2s_cfg.word_length == SCM_I2S_WL_16));

    err = scm_i2s_start(SCM_I2S_TX);
    if (err) {
        printf("scm_i2s_start failed.\n");
        goto done;
    }

    for (int i = 0; i < i2s_cfg.number_of_blocks; i++) {
#if 0
        hexdump(buf, 32);
#endif
        err = scm_i2s_write_block(buf, bufsz);
        if (err) {
            printf("scm_i2s_write_block failed.\n");
            break;
        }
    }

    err = scm_i2s_stop(SCM_I2S_TX);
    if (err) {
        printf("scm_i2s_stop failed.\n");
        goto done;
    }

    ret = CMD_RET_SUCCESS;

done:
    if (buf) {
        free(buf);
    }

    return ret;
}

static int scm_cli_i2s_write(int argc, char *argv[])
{
    int bufsz;
    uint8_t *buf = NULL;
    uint32_t pattern;
    int i;
    int err, ret = CMD_RET_FAILURE;

    argc--;
    argv++;

    if (argc < 1) {
        return CMD_RET_USAGE;
    }

    pattern = strtoul(argv[0], NULL, 16);

    bufsz = scm_i2s_get_block_buffer_size(&i2s_cfg);
    if (bufsz <= 0) {
        printf("scm_i2s_get_block_buffer_size failed.\n");
        goto done;
    }

    buf = zalloc(bufsz);
    if (!buf) {
        printf("malloc %d bytes failed.\n", bufsz);
        goto done;
    }

    for (i = 0; i < bufsz / 4; i = i + 4) {
        memcpy(buf + i, &pattern, sizeof(uint32_t));
    }

    err = scm_i2s_start(SCM_I2S_TX);
    if (err) {
        printf("scm_i2s_start failed.\n");
        goto done;
    }

    for (i = 0; i < i2s_cfg.number_of_blocks; i++) {
#if 0
        hexdump(buf, 32);
#endif
        err = scm_i2s_write_block(buf, bufsz);
        if (err) {
            printf("scm_i2s_write_block failed.\n");
            break;
        }
    }

    err = scm_i2s_stop(SCM_I2S_TX);
    if (err) {
        printf("scm_i2s_stop failed.\n");
        goto done;
    }

    ret = CMD_RET_SUCCESS;

done:
    if (buf) {
        free(buf);
    }

    return ret;
}

static void process_block_data(void *mem_block, void *echo_block, uint32_t size,
        bool half_word)
{
    uint32_t num_samples = half_word ? (size / 2) : (size / 4);
    if (half_word) {
        int16_t *echo = (int16_t *)echo_block;
        for (int i = 0; i < num_samples; ++i) {
            int16_t *sample = &((int16_t *)mem_block)[i];
            *sample += echo[i];
            echo[i] = (*sample) / 2;
        }
    } else {
        int32_t *echo = (int32_t *)echo_block;
        for (int i = 0; i < num_samples; ++i) {
            int32_t *sample = &((int32_t *)mem_block)[i];
            *sample += echo[i];
            echo[i] = (*sample) / 2;
        }
    }
}

static int scm_cli_i2s_echo(int argc, char *argv[])
{
    int bufsz;
    uint8_t *buf = NULL, *echo = NULL;
    int c = -1;
    int err, ret = CMD_RET_FAILURE;

    bufsz = scm_i2s_get_block_buffer_size(&i2s_cfg);
    if (bufsz <= 0) {
        printf("scm_i2s_get_block_buffer_size failed.\n");
        goto done;
    }

    buf = zalloc(bufsz);
    if (!buf) {
        printf("malloc %d bytes failed.\n", bufsz);
        goto done;
    }

    echo = zalloc(bufsz);
    if (echo == NULL) {
        printf("malloc %d bytes failed.\n", bufsz);
        goto done;
    }

    err = scm_i2s_start(SCM_I2S_BOTH);
    if (err) {
        printf("scm_i2s_start failed.\n");
        goto done;
    }

    while ((c = getchar_timeout(0)) <= 0) {
        size_t len = bufsz;
        err = scm_i2s_read_block(buf, &len);
        if (err) {
            if (err == WISE_ERR_NO_MEM) {
                printf("I2S is not running.\nRun I2S master clock first.\n");
            }
            break;
        }
        process_block_data(buf, echo, len, i2s_cfg.word_length == SCM_I2S_WL_16);
#if 0
        hexdump(buf, min(32, len));
#endif
        err = scm_i2s_write_block(buf, len);
        if (err) {
            printf("scm_i2s_write_block failed.\n");
            break;
        }
    }

    err = scm_i2s_stop(SCM_I2S_BOTH);
    if (err) {
        printf("scm_i2s_stop failed.\n");
        goto done;
    }

    ret = CMD_RET_SUCCESS;

done:
    if (buf) {
        free(buf);
    }

    if (echo) {
        free(echo);
    }

    return ret;
}

/*
#define DEBUG_TIMING
*/

static int scm_cli_i2s_record(int argc, char *argv[])
{
    int bufsz;
    uint8_t *buf = NULL;
    off_t addr, wp;
    size_t size, written = 0;
    int c = -1;
    int err, ret = CMD_RET_FAILURE;
    u32 start __maybe_unused;

    argc--;
    argv++;

    if (argc < 2) {
        return CMD_RET_USAGE;
    }

    addr = (off_t)strtoul(argv[0], NULL, 16);
    size = (size_t)strtoul(argv[1], NULL, 16);

#ifdef DEBUG_TIMING
    start = ktime();
#endif

    err = flash_erase(addr, size, 0);
    if (err) {
        printf("flash_erase failed.\n");
        goto done;
    }

#ifdef DEBUG_TIMING
        printk("%d us to erase %d bytes\n", tick_to_us(ktime() - start), size);
#endif

    bufsz = scm_i2s_get_block_buffer_size(&i2s_cfg);
    if (bufsz <= 0) {
        printf("scm_i2s_get_block_buffer_size failed.\n");
        goto done;
    }

    buf = zalloc(bufsz);
    if (!buf) {
        printf("malloc %d bytes failed.\n", bufsz);
        goto done;
    }

    err = scm_i2s_start(SCM_I2S_RX);
    if (err) {
        printf("scm_i2s_start failed.\n");
        goto done;
    }

    wp = addr;
    while ((c = getchar_timeout(0)) <= 0) {
        size_t len = bufsz, wr;
        err = scm_i2s_read_block(buf, &len);
        if (err) {
            if (err == WISE_ERR_NO_MEM) {
                printf("I2S is not running.\nRun I2S master clock first.\n");
            }
#ifdef DEBUG_TIMING
            printk("Giving up...\n");
#endif
            break;
        }
        /* hexdump_nz(buf, len, 64); */
        if (written + len > size) {
            break;
        }
#ifdef DEBUG_TIMING
        start = ktime();
#endif
        if ((wr = flash_write(wp, buf, len)) != len) {
            printf("write failed(%d).\n", wr);
            break;
        }
#ifdef DEBUG_TIMING
        printk("%d us, (%d / %d)\n", tick_to_us(ktime() - start), written, size);
#endif
        wp += wr;
        written += wr;
    }

    err = scm_i2s_stop(SCM_I2S_RX);
    if (err) {
        printf("scm_i2s_stop failed.\n");
        goto done;
    }

    ret = CMD_RET_SUCCESS;

done:
    if (buf) {
        free(buf);
    }

    return ret;
}

static int scm_cli_i2s_record_fs(int argc, char *argv[])
{
    int bufsz;
    uint8_t *buf = NULL;
    int c = -1;
    const char *filename;
    int fd = -1;
    int err, ret = CMD_RET_FAILURE;

    argc--;
    argv++;

    if (argc < 1) {
        return CMD_RET_USAGE;
    }

    filename = argv[0];

    fd = open(filename, O_RDWR | O_CREAT | O_TRUNC);
    if (fd < 0) {
        printf("open fail: %s\n", filename);
        goto done;
    }

    bufsz = scm_i2s_get_block_buffer_size(&i2s_cfg);
    if (bufsz <= 0) {
        printf("scm_i2s_get_block_buffer_size failed.\n");
        goto done;
    }

    buf = zalloc(bufsz);
    if (!buf) {
        printf("malloc %d bytes failed.\n", bufsz);
        goto done;
    }

    err = scm_i2s_start(SCM_I2S_RX);
    if (err) {
        printf("scm_i2s_start failed.\n");
        goto done;
    }

    while ((c = getchar_timeout(0)) <= 0) {
        size_t len = bufsz, wr;
        err = scm_i2s_read_block(buf, &len);
        if (err) {
            if (err == WISE_ERR_NO_MEM) {
                printf("I2S is not running.\nRun I2S master clock first.\n");
            }
            break;
        }
#if 0
        hexdump_nz(buf, len, 64);
#endif
        if ((wr = write(fd, buf, len)) != len) {
            printf("write failed(%d).\n", wr);
            break;
        }
    }

    err = scm_i2s_stop(SCM_I2S_RX);
    if (err) {
        printf("scm_i2s_stop failed.\n");
        goto done;
    }

    ret = CMD_RET_SUCCESS;

done:
    if (buf) {
        free(buf);
    }
    if (fd >= 0) {
        close(fd);
    }

    return ret;
}

static int doubler(uint8_t *src, int srcsz, uint8_t *dst, int dstsz, bool half_word)
{
    if (src == dst)
        return srcsz;

    assert(srcsz <= dstsz / 2);

    if (half_word) {
        uint16_t *s = (uint16_t *)src;
        uint16_t *d = (uint16_t *)dst;
        for (int i = 0; i < srcsz / 2; i++) {
            *d++ = s[i];
            *d++ = s[i];
        }
    } else {
        uint32_t *s = (uint32_t *)src;
        uint32_t *d = (uint32_t *)dst;
        for (int i = 0; i < srcsz / 4; i++) {
            *d++ = s[i];
            *d++ = s[i];
        }
    }

    return srcsz * 2;
}

static int scm_cli_i2s_playback(int argc, char *argv[])
{
    int bufsz, rdsz;
    uint8_t *buf = NULL, *rdbuf = NULL;
    off_t addr, rp;
    size_t size, played = 0;
    int c = -1;
    int err, ret = CMD_RET_SUCCESS;
    bool mono;

    argc--;
    argv++;

    if (argc < 3) {
        return CMD_RET_USAGE;
    }

    addr = (off_t)strtoul(argv[0], NULL, 16);
    size = (size_t)strtoul(argv[1], NULL, 16);

    if (!strcmp(argv[2], "m") || !strcmp(argv[2], "mono")) {
        mono = true;
    } else if (!strcmp(argv[2], "s") || !strcmp(argv[2], "stereo")) {
        mono = false;
    } else {
        return CMD_RET_USAGE;
    }

    bufsz = scm_i2s_get_block_buffer_size(&i2s_cfg);
    if (bufsz <= 0) {
        printf("scm_i2s_get_block_buffer_size failed.\n");
        goto done;
    }

    buf = zalloc(bufsz);
    if (!buf) {
        printf("malloc %d bytes failed.\n", bufsz);
        goto done;
    }

    if (mono) {
        rdsz = bufsz / 2;
        rdbuf = zalloc(rdsz);
        if (!rdbuf) {
            printf("malloc %d bytes failed.\n", rdsz);
            goto done;
        }
    } else {
        rdsz = bufsz;
        rdbuf = buf;
    }

    err = scm_i2s_start(SCM_I2S_TX);
    if (err) {
        printf("scm_i2s_start failed.\n");
        goto done;
    }

    rp = addr;
    while ((c = getchar_timeout(0)) <= 0 && played < size) {
        rdsz = min(rdsz, size - played);
        memset(rdbuf, 0, rdsz);
        if ((err = flash_read(rp, rdbuf, rdsz)) < 0) {
           printf("flash_read failed(%d).\n", err);
           break;
        } else {
            int n = rdsz;
            n = doubler(rdbuf, n, buf, bufsz, i2s_cfg.word_length == SCM_I2S_WL_16);
#if 0
            hexdump_nz(buf, bufsz, 64);
#endif
            err = scm_i2s_write_block(buf, n);
            if (err) {
                printf("scm_i2s_write_block failed.\n");
                break;
            }
            rp += rdsz;
            played += rdsz;
        }
    }

    err = scm_i2s_stop(SCM_I2S_TX);
    if (err) {
        printf("scm_i2s_stop failed.\n");
        goto done;
    }

    ret = CMD_RET_SUCCESS;

done:
    if (rdbuf && rdbuf != buf) {
        free(rdbuf);
    }
    if (buf) {
        free(buf);
    }

    return ret;
}

static int scm_cli_i2s_playback_fs(int argc, char *argv[])
{
    int bufsz, rdsz;
    uint8_t *buf = NULL, *rdbuf = NULL;
    const char *filename;
    int fd = -1, n;
    int c = -1;
    int err, ret = CMD_RET_SUCCESS;
    bool mono;

    argc--;
    argv++;

    if (argc < 2) {
        return CMD_RET_USAGE;
    }

    filename = argv[0];
    if (!strcmp(argv[1], "m") || !strcmp(argv[1], "mono")) {
        mono = true;
    } else if (!strcmp(argv[1], "s") || !strcmp(argv[1], "stereo")) {
        mono = false;
    } else {
        return CMD_RET_USAGE;
    }

    fd = open(filename, O_RDONLY, 0);
    if (fd < 0) {
        printf("open fail: %s\n", filename);
        goto done;
    }

    bufsz = scm_i2s_get_block_buffer_size(&i2s_cfg);
    if (bufsz <= 0) {
        printf("scm_i2s_get_block_buffer_size failed.\n");
        goto done;
    }

    buf = zalloc(bufsz);
    if (!buf) {
        printf("malloc %d bytes failed.\n", bufsz);
        goto done;
    }

    if (mono) {
        rdsz = bufsz / 2;
        rdbuf = zalloc(rdsz);
        if (!rdbuf) {
            printf("malloc %d bytes failed.\n", rdsz);
            goto done;
        }
    } else {
        rdsz = bufsz;
        rdbuf = buf;
    }

    err = scm_i2s_start(SCM_I2S_TX);
    if (err) {
        printf("scm_i2s_start failed.\n");
        goto done;
    }

    while ((c = getchar_timeout(0)) <= 0) {
        memset(rdbuf, 0, rdsz);
        if ((n = read(fd, rdbuf, rdsz)) > 0) {
           n = doubler(rdbuf, n, buf, bufsz, i2s_cfg.word_length == SCM_I2S_WL_16);
#if 0
           hexdump_nz(buf, bufsz, 64);
#endif
           err = scm_i2s_write_block(buf, n);
           if (err) {
               printf("scm_i2s_write_block failed.\n");
               break;
           }
       } else {
           break;
       }
    }

    err = scm_i2s_stop(SCM_I2S_TX);
    if (err) {
        printf("scm_i2s_stop failed.\n");
        goto done;
    }

    ret = CMD_RET_SUCCESS;

done:
    if (rdbuf && rdbuf != buf) {
        free(rdbuf);
    }
    if (buf) {
        free(buf);
    }
    if (fd >= 0) {
        close(fd);
    }

    return ret;
}


static const struct cli_cmd scm_cli_i2s_cmd[] = {
    CMDENTRY(init, scm_cli_i2s_init, "", ""),
    CMDENTRY(deinit, scm_cli_i2s_deinit, "", ""),
    CMDENTRY(config, scm_cli_i2s_config, "", ""),
    CMDENTRY(read, scm_cli_i2s_read, "", ""),
    CMDENTRY(beep, scm_cli_i2s_beep, "", ""),
    CMDENTRY(write, scm_cli_i2s_write, "", ""),
    CMDENTRY(echo, scm_cli_i2s_echo, "", ""),
    CMDENTRY(record, scm_cli_i2s_record, "", ""),
    CMDENTRY(recordfs, scm_cli_i2s_record_fs, "", ""),
    CMDENTRY(play, scm_cli_i2s_playback, "", ""),
    CMDENTRY(playfs, scm_cli_i2s_playback_fs, "", ""),
};

static int do_scm_cli_i2s(int argc, char *argv[])
{
    const struct cli_cmd *cmd;

    argc--;
    argv++;

    cmd = cli_find_cmd(argv[0], scm_cli_i2s_cmd, ARRAY_SIZE(scm_cli_i2s_cmd));
    if (cmd == NULL)
        return CMD_RET_USAGE;

    return cmd->handler(argc, argv);
}

CMD(i2s, do_scm_cli_i2s,
        "CLI commands for I2S API",
        "i2s init" OR
        "i2s deinit" OR
        "i2s config [dir:rx|tx] [wl:16|20|24] [fmt:0(I2S)|1(LJ)|2(RJ)]\n\t\
        [master:0|1] [sample freq.]\n\t<[duration per block(ms): less than 1000]\
        [number of blocks]>" OR
        "i2s read" OR
        "i2s beep" OR
        "i2s write <word pattern>" OR
        "i2s echo" OR
        "i2s record [address:hex] [length:hex]" OR
        "i2s recordfs [filename: full path]" OR
        "i2s play [address:hex] [length:hex] [m(ono)|s(tereo)]" OR
        "i2s playfs [filename: full path] [m(ono)|s(tereo)]"
   );
