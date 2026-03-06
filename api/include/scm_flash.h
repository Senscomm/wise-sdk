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

#ifndef __SCM_FLASH_H__
#define __SCM_FLASH_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Partition types */
typedef enum {
    FLASH_PARTITION_BOOTLOADER,
    FLASH_PARTITION_FACTORY,
    FLASH_PARTITION_APPLICATION,
    FLASH_PARTITION_FS,
    FLASH_PARTITION_LOG,
#ifdef PATCH_HYD_EXTRA_FLASH_PARTITION
    FLASH_PARTITION_TMP,
#endif
    FLASH_PARTITION_MAX
} flash_partition_type_t;

/* Function declarations */
int scm_partition_erase(flash_partition_type_t partition, off_t offset, size_t size);
int scm_partition_write(flash_partition_type_t partition, off_t offset, const void *buf, size_t size);
int scm_partition_read(flash_partition_type_t partition, off_t offset, void *buf, size_t size);

#ifdef __cplusplus
}
#endif

#endif //__SCM_FLASH_H__
