/*
 * Copyright 2023-2025 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "hal/spi-flash.h"
#include "scm_flash.h"

/* Partition information structure */
typedef struct {
    flash_partition_type_t type;
    off_t start_addr;
    size_t size;
    bool read_only;
} flash_partition_info_t;

/* Partition table - defined at compile time */
static const flash_partition_info_t partition_table[FLASH_PARTITION_MAX] = {
    [FLASH_PARTITION_BOOTLOADER] = {
        .type = FLASH_PARTITION_BOOTLOADER,
        .start_addr = CONFIG_FLASH_BASE_OFFSET,
        .size = CONFIG_BOOTLOADER_IMAGE_SIZE,
        .read_only = true
    },
    [FLASH_PARTITION_FACTORY] = {
        .type = FLASH_PARTITION_FACTORY,
        .start_addr = CONFIG_FACTORY_PARTITION_OFFSET,
        .size = CONFIG_FACTORY_PARTITION_SIZE,
        .read_only = false
    },
    [FLASH_PARTITION_APPLICATION] = {
        .type = FLASH_PARTITION_APPLICATION,
        .start_addr = CONFIG_FLASH_IMAGE_OFFSET,
        .size = CONFIG_FLASH_IMAGE_SIZE,
        .read_only = true
    },
    [FLASH_PARTITION_FS] = {
        .type = FLASH_PARTITION_FS,
        .start_addr = CONFIG_SPIFFS_SYSTEM_PART_ADDR,
        .size = CONFIG_SPIFFS_SYSTEM_PART_SIZE,
        .read_only = true
    },
    [FLASH_PARTITION_LOG] = {
        .type = FLASH_PARTITION_LOG,
        .start_addr = CONFIG_LOG_PARTITION_OFFSET,
        .size = CONFIG_LOG_PARTITION_SIZE,
        .read_only = false
    },
};

/* Get partition information by partition type */
static const flash_partition_info_t* get_partition_info(flash_partition_type_t partition)
{
    if (partition >= FLASH_PARTITION_MAX) {
        return NULL;
    }
    return &partition_table[partition];
}

/* Validate partition access parameters */
static int validate_partition_access(flash_partition_type_t partition, off_t offset, size_t size, bool is_write)
{
    const flash_partition_info_t* info = get_partition_info(partition);

    if (info == NULL) {
        return -EINVAL;
    }

    /* Check if write operation on read-only partition */
    if (is_write && info->read_only) {
        return -EACCES;
    }

    /* Check bounds */
    if (offset < 0 || size == 0) {
        return -EINVAL;
    }

    /* Check if operation exceeds partition boundaries */
    if ((size_t)offset + size > info->size) {
        return -ERANGE; /* Out of range */
    }

    return 0;
}

/* Erase flash partition */
int scm_partition_erase(flash_partition_type_t partition, off_t offset, size_t size)
{
    int ret = validate_partition_access(partition, offset, size, true);
    if (ret != 0) {
        return ret;
    }

    const flash_partition_info_t* info = get_partition_info(partition);
    off_t absolute_addr = info->start_addr + offset;

    return flash_erase(absolute_addr, size, 0);
}

/* Write to flash partition */
int scm_partition_write(flash_partition_type_t partition, off_t offset, const void *buf, size_t size)
{
    int ret = validate_partition_access(partition, offset, size, true);
    if (ret != 0) {
        return ret;
    }

    const flash_partition_info_t* info = get_partition_info(partition);
    off_t absolute_addr = info->start_addr + offset;

    /* Cast away const qualifier as flash_write requires non-const pointer */
    return flash_write(absolute_addr, (void *)buf, size);
}

/* Read from flash partition */
int scm_partition_read(flash_partition_type_t partition, off_t offset, void *buf, size_t size)
{
    int ret = validate_partition_access(partition, offset, size, false);
    if (ret != 0) {
        return ret;
    }

    const flash_partition_info_t* info = get_partition_info(partition);
    off_t absolute_addr = info->start_addr + offset;

    return flash_read(absolute_addr, buf, size);
}
