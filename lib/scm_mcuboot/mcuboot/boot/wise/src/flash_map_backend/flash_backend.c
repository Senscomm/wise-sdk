/*
 * Copyright 2021-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "hal/io.h"
#include "hal/init.h"
#include "hal/spi-flash.h"
#include "flash_crypto_priv.h"

#if 0
#define fdbg            printf
#else
#define fdbg(...)
#endif

struct flash_crypto g_crypto;

int flash_backend_write(off_t addr, uint8_t *buf, size_t size)
{
    uint8_t *enc_buf = NULL;
    uint8_t *write_buf = NULL;
    int ret;

    if (g_crypto.enable && !g_crypto.user_disable) {
        enc_buf = malloc(size);
        if (!enc_buf) {
            return -ENOMEM;
        }

        flash_crypto_data_encrypt(&g_crypto, addr, buf, enc_buf, size);
    }



    if (enc_buf) {
        write_buf = enc_buf;
    } else {
        write_buf = buf;
    }

    ret = flash_write(addr, write_buf, size);

    if (enc_buf) {
        free(enc_buf);
    }

    return ret;
}


int flash_backend_erase(off_t addr, size_t size)
{
    int ret;

    ret = flash_erase(addr, size, 0);

    return ret;
}

int flash_backend_read(off_t addr, uint8_t *buf, size_t size)
{
    if (g_crypto.enable && g_crypto.user_disable) {
        flash_read(addr, buf, size);
    } else {
        memcpy(buf, (void *)addr, size);
    }

    return 0;
}

int flash_crypto_enable(uint8_t enable)
{
    if (g_crypto.enable) {
        if (enable) {
            g_crypto.user_disable = 0;
        } else {
            g_crypto.user_disable = 1;
        }

        return 0;
    }

    return -1;
}

int flash_init(void)
{
    flash_cyprot_init(&g_crypto);

    return 0;
}
__initcall__(subsystem, flash_init);

#ifdef CONFIG_SCM_MCUBOOT_BOOTLOADER

#define BOOTLOADER_BASE_ADDRESS         CONFIG_FLASH_IMAGE_OFFSET
#define BOOTLOADER_SIZE                 CONFIG_FLASH_IMAGE_SIZE
#define WISE_BASE_ADDRESS               CONFIG_SCM2010_OTA_PRIMARY_SLOT_OFFSET
#define WISE_OTA_ADDRESS                CONFIG_SCM2010_OTA_SECONDARY_SLOT_OFFSET
#define WISE_SIZE                       CONFIG_SCM2010_OTA_SLOT_SIZE
#define XYZM_BUFSZ                      1024

#include <cli.h>
#include <u-boot/xyzModem.h>

static int getcxmodem(void) {
    int ret = getchar_timeout(0);
    if (ret >= 0)
        return (char) ret;

    return -1;
}

static int do_flash_update(int argc, char *argv[])
{
    connection_info_t info = {
        .mode = xyzModem_ymodem,
    };
    char *name, *buf = NULL;
    uint32_t start, offset;
    int size = 0, err, res;
    uint32_t slot_size;
    bool erased = false;


    if (argc < 2) {
        return CMD_RET_USAGE;
    }

    name = argv[1];

    if (!strcmp(name, "boot")) {
#ifdef CONFIG_XIP
        return CMD_RET_USAGE;
#else
        start = BOOTLOADER_BASE_ADDRESS;
        slot_size = BOOTLOADER_SIZE;
#endif
    } else if (!strcmp(name, "wise")) {
        start = WISE_BASE_ADDRESS;
        slot_size = WISE_SIZE;
    } else if (!strcmp(name, "ota")) {
        start = WISE_OTA_ADDRESS;
        slot_size = WISE_SIZE;
    } else {
        return CMD_RET_USAGE;
    }

    buf = malloc(XYZM_BUFSZ);
    if (buf == NULL) {
        fdbg("No memory available for receiving\n");
        return CMD_RET_FAILURE;
    }

    flash_crypto_enable(0);

    if ((res = xyzModem_stream_open(&info, &err)) != 0) {
        fdbg("%s\n", xyzModem_error(err));
        goto out;
    }

    offset = 0;
    while ((res = xyzModem_stream_read(buf, XYZM_BUFSZ, &err)) > 0) {
        int ret;

        if (!erased) {
            erased = true;
            if (flash_backend_erase(start, slot_size) < 0) {
                fdbg("slot erase failed\n");
                goto out;
            }
        }

        ret = flash_backend_write(start + offset, (uint8_t *)buf, res);
        if (ret < 0) {
            fdbg("flash program failed at %08x\n", start + offset);
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

    printf("## Total Size = 0x%08x = %d Bytes\n", size, size);

out:
    flash_crypto_enable(1);

    xyzModem_stream_close(&err);
    xyzModem_stream_terminate(false, getcxmodem);
    free(buf);

    return (err == 0) ? 0: CMD_RET_FAILURE;
}

static int do_flash_erase(int argc, char *argv[])
{
    char *name;
    int err;
    uint32_t start;
    uint32_t slot_size;

    if (argc < 2) {
        return CMD_RET_USAGE;
    }

    name = argv[1];

    if (!strcmp(name, "boot")) {
#ifdef CONFIG_XIP
        return CMD_RET_USAGE;
#else
        start = BOOTLOADER_BASE_ADDRESS;
        slot_size = BOOTLOADER_SIZE;
#endif
    } else if (!strcmp(name, "wise")) {
        start = WISE_BASE_ADDRESS;
        slot_size = WISE_SIZE;
    } else if (!strcmp(name, "ota")) {
        start = WISE_OTA_ADDRESS;
        slot_size = WISE_SIZE;
    } else {
        return CMD_RET_USAGE;
    }

    flash_crypto_enable(0);

	err = flash_backend_erase(start, slot_size);
	if (err < 0) {
		fdbg("slot erase failed\n");
		goto out;
	}

    printf("## Erased 0x%x (%d) bytes from 0x%08x\n", slot_size, slot_size, start);

out:
    flash_crypto_enable(1);

    return (err == 0) ? 0: CMD_RET_FAILURE;
}

static const struct cli_cmd flash_cmd[] = {
    CMDENTRY(update, do_flash_update, "", ""),
    CMDENTRY(erase, do_flash_erase, "", ""),
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
        "flash command",
        "flash update name" OR
        "flash erase name"
        "\nname is one of\n"
        "\tboot (RAM build only)\n"
        "\twise\n"
        "\tota\n"
   );
#endif
