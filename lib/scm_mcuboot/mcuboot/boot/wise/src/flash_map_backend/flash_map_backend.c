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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <bootutil/bootutil_log.h>

#include "flash_map_backend/flash_map_backend.h"
#include "sysflash/sysflash.h"

#include "hal/compiler.h"

#include "flash_priv.h"

#define min(a, b)                   (a > b ? b : a)

#define ARRAYSIZE(x)                (sizeof((x)) / sizeof((x)[0]))

#define OK      0
#define ERROR   -1

struct flash_device_s {
    /* Reference to the flash area configuration parameters */
    struct flash_area *fa_cfg;
};


static struct flash_area g_primary_img0 = {
    .fa_id = FLASH_AREA_IMAGE_PRIMARY(0),
    .fa_device_id = 0,
    .fa_off = CONFIG_SCM2010_OTA_PRIMARY_SLOT_OFFSET,
    .fa_size = CONFIG_SCM2010_OTA_SLOT_SIZE,
};

static struct flash_device_s g_primary_priv = {
    .fa_cfg = &g_primary_img0,
};

static struct flash_area g_secondary_img0 = {
    .fa_id = FLASH_AREA_IMAGE_SECONDARY(0),
    .fa_device_id = 0,
    .fa_off =CONFIG_SCM2010_OTA_SECONDARY_SLOT_OFFSET,
    .fa_size = CONFIG_SCM2010_OTA_SLOT_SIZE,
};

static struct flash_device_s g_secondary_priv = {
    .fa_cfg = &g_secondary_img0,
};

static struct flash_area g_scratch_img0 = {
    .fa_id = FLASH_AREA_IMAGE_SCRATCH,
    .fa_device_id = 0,
    .fa_off = CONFIG_SCM2010_OTA_SCRATCH_OFFSET,
    .fa_size = CONFIG_SCM2010_OTA_SCRATCH_SIZE,
};

static struct flash_device_s g_scratch_priv = {
    .fa_cfg = &g_scratch_img0,
};

static struct flash_device_s *g_flash_devices[] = {
    &g_primary_priv,
    &g_secondary_priv,
    &g_scratch_priv,
};

static int aligned_write(const uint8_t *buf, uint32_t start_addr, int len)
{
    uint32_t offset;
    uint32_t aligned_addr;
    uint32_t mask;
    uint32_t remain;
    uint32_t esize;
    uint8_t *eblock;
    int ret = len;

    esize = 4096;
    eblock = NULL;

    offset = start_addr;
    remain = len;

    mask = esize - 1;
    aligned_addr = (offset + mask) & ~mask;
    if (aligned_addr > offset) {
        uint32_t rw_addr;
        uint32_t unaligned_size;
        uint32_t unaligned_offset;

        rw_addr = offset & ~mask;

        eblock = malloc(esize);
        if (!eblock) {
            ret = -ENOMEM;
            goto out;
        }

        flash_backend_read(rw_addr, eblock, esize);

        if (flash_backend_erase(rw_addr, esize) < 0) {
            ret = -1;
            goto out;
        }

        unaligned_offset = offset & mask;

        unaligned_size = min(remain, esize - unaligned_offset);

        memcpy(eblock + unaligned_offset, buf, unaligned_size);

        if (flash_backend_write(rw_addr, eblock, esize) < 0) {
            ret = -1;
            goto out;
        }

        offset += unaligned_size;
        buf += unaligned_size;
        remain -= unaligned_size;
    }

    while (remain >= esize) {
        if (flash_backend_erase(offset, esize) < 0) {
            ret = -1;
            goto out;
        }

        if (flash_backend_write(offset, (void *)buf, esize) < 0) {
            ret = -1;
            goto out;
        }

        offset += esize;
        buf += esize;
        remain -= esize;
    }

    if (remain > 0) {

        if (!eblock) {
            eblock = malloc(esize);
            if (!eblock) {
                ret = -ENOMEM;
                goto out;
            }
        }

        flash_backend_read(offset, eblock, esize);

        if (flash_backend_erase(offset, esize) < 0) {
            ret = -1;
            goto out;
        }

        memcpy(eblock, buf, remain);

        if (flash_backend_write(offset, eblock, esize) < 0) {
            ret = -1;
            goto out;
        }
    }

out:
    if (eblock) {
        free(eblock);
    }

    return ret;
}

/****************************************************************************
 * Name: lookup_flash_device_by_id
 *
 * Description:
 *   Retrieve flash device from a given flash area ID.
 *
 * Input Parameters:
 *   fa_id - ID of the flash area.
 *
 * Returned Value:
 *   Reference to the found flash device, or NULL in case it does not exist.
 *
 ****************************************************************************/
__maybe_unused
static struct flash_device_s *lookup_flash_device_by_id(uint8_t fa_id)
{
    size_t i;

    for (i = 0; i < ARRAYSIZE(g_flash_devices); i++) {
        struct flash_device_s *dev = g_flash_devices[i];

        if (fa_id == dev->fa_cfg->fa_id) {
            return dev;
        }
    }

    return NULL;
}

/****************************************************************************
 * Name: lookup_flash_device_by_offset
 *
 * Description:
 *   Retrieve flash device from a given flash area offset.
 *
 * Input Parameters:
 *   offset - Offset of the flash area.
 *
 * Returned Value:
 *   Reference to the found flash device, or NULL in case it does not exist.
 *
 ****************************************************************************/
__maybe_unused
static struct flash_device_s *lookup_flash_device_by_offset(uint32_t offset)
{
    size_t i;

    for (i = 0; i < ARRAYSIZE(g_flash_devices); i++) {
        struct flash_device_s *dev = g_flash_devices[i];

        if (offset == dev->fa_cfg->fa_off) {
            return dev;
        }
    }

    return NULL;
}

/****************************************************************************
 * Name: flash_area_open
 *
 * Description:
 *   Retrieve flash area from the flash map for a given ID.
 *
 * Input Parameters:
 *   id - ID of the flash area.
 *
 * Output Parameters:
 *   fa - Pointer which will contain the reference to flash_area.
 *        If ID is unknown, it will be NULL on output.
 *
 * Returned Value:
 *   Zero on success, or negative value in case of error.
 *
 ****************************************************************************/

int flash_area_open(uint8_t id, const struct flash_area **fa)
{
    struct flash_device_s *dev;

    dev = lookup_flash_device_by_id(id);
    if (dev == NULL) {
        BOOT_LOG_ERR("Undefined flash area: %d", id);
        return ERROR;
    }

    *fa = dev->fa_cfg;

    BOOT_LOG_INF("ID:%d opened", dev->fa_cfg->fa_id);
    BOOT_LOG_INF("Flash area offset: 0x%x", dev->fa_cfg->fa_off);
    BOOT_LOG_INF("Flash area size: 0x%x", dev->fa_cfg->fa_size);

    return OK;
}

/****************************************************************************
 * Name: flash_area_close
 *
 * Description:
 *   Close a given flash area.
 *
 * Input Parameters:
 *   fa - Flash area to be closed.
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

void flash_area_close(const struct flash_area *fa)
{
    BOOT_LOG_INF("ID:%d closed", fa->fa_id);
}

/****************************************************************************
 * Name: flash_area_read
 *
 * Description:
 *   Read data from flash area.
 *   Area readout boundaries are asserted before read request. API has the
 *   same limitation regarding read-block alignment and size as the
 *   underlying flash driver.
 *
 * Input Parameters:
 *   fa  - Flash area to be read.
 *   off - Offset relative from beginning of flash area to be read.
 *   len - Number of bytes to read.
 *
 * Output Parameters:
 *   dst - Buffer to store read data.
 *
 * Returned Value:
 *   Zero on success, or negative value in case of error.
 *
 ****************************************************************************/

int flash_area_read(const struct flash_area *fa, uint32_t off,
        void *dst, uint32_t len)
{
    uint32_t addr;

    addr = fa->fa_off + off;

    BOOT_LOG_INF("ID:%d offset:0x%08x length:%d",
            fa->fa_id, addr, len);

    if (off + len > fa->fa_size) {
        BOOT_LOG_ERR("Attempt to read out of flash area bounds");
        return ERROR;
    }

    flash_backend_read(addr, dst, len);

    return OK;
}

/****************************************************************************
 * Name: flash_area_write
 *
 * Description:
 *   Write data to flash area.
 *   Area write boundaries are asserted before write request. API has the
 *   same limitation regarding write-block alignment and size as the
 *   underlying flash driver.
 *
 * Input Parameters:
 *   fa  - Flash area to be written.
 *   off - Offset relative from beginning of flash area to be written.
 *   src - Buffer with data to be written.
 *   len - Number of bytes to write.
 *
 * Returned Value:
 *   Zero on success, or negative value in case of error.
 *
 ****************************************************************************/

int flash_area_write(const struct flash_area *fa, uint32_t off,
        const void *src, uint32_t len)
{
    uint32_t addr;
    int ret;

    addr = fa->fa_off + off;

    BOOT_LOG_INF("ID:%d offset:0x%08x length:0x%x",
            fa->fa_id, addr, len);

    if (off + len > fa->fa_size) {
        BOOT_LOG_ERR("Attempt to write out of flash area bounds");
        return ERROR;
    }

    ret = aligned_write((const uint8_t *)src, addr, (int)len);
    if (ret < 0) {
        return ERROR;
    }

    return OK;
}

/****************************************************************************
 * Name: flash_area_erase
 *
 * Description:
 *   Erase a given flash area range.
 *   Area boundaries are asserted before erase request. API has the same
 *   limitation regarding erase-block alignment and size as the underlying
 *   flash driver.
 *
 * Input Parameters:
 *   fa  - Flash area to be erased.
 *   off - Offset relative from beginning of flash area to be erased.
 *   len - Number of bytes to be erase.
 *
 * Returned Value:
 *   Zero on success, or negative value in case of error.
 *
 ****************************************************************************/

int flash_area_erase(const struct flash_area *fa, uint32_t off, uint32_t len)
{
    void *buffer;
    size_t i;
    const size_t sector_size = 4096;
    const uint8_t erase_val = 0xff;
    uint32_t addr;
    int ret;

    addr = fa->fa_off + off;

    BOOT_LOG_INF("ID:%d offset:0x%08x length:0x%x",
            fa->fa_id, addr, len);

    buffer = malloc(sector_size);
    if (buffer == NULL) {
        BOOT_LOG_ERR("Failed to allocate erase buffer");
        return ERROR;
    }

    memset(buffer, erase_val, sector_size);


    if (len < sector_size) {
        ret = flash_area_write(fa, off, buffer, len);
        return ret;
    }

    i = 0;

    do {
        ret = flash_area_write(fa, off + i, buffer, sector_size);
        i += sector_size;
    } while (ret == OK && i < (len - sector_size));

    if (ret == OK) {
        if (len - i) {
            ret = flash_area_write(fa, off + i, buffer, len - i);
        }
    }

    free(buffer);

    return ret;
}

/****************************************************************************
 * Name: flash_area_align
 *
 * Description:
 *   Get write block size of the flash area.
 *   Write block size might be treated as read block size, although most
 *   drivers support unaligned readout.
 *
 * Input Parameters:
 *   fa - Flash area.
 *
 * Returned Value:
 *   Alignment restriction for flash writes in the given flash area.
 *
 ****************************************************************************/

uint8_t flash_area_align(const struct flash_area *fa)
{
    const uint8_t minimum_write_length = 1;

    BOOT_LOG_INF("ID:%d align:%d",
            fa->fa_id, minimum_write_length);

    return minimum_write_length;
}

/****************************************************************************
 * Name: flash_area_erased_val
 *
 * Description:
 *   Get the value expected to be read when accessing any erased flash byte.
 *   This API is compatible with the MCUboot's porting layer.
 *
 * Input Parameters:
 *   fa - Flash area.
 *
 * Returned Value:
 *   Byte value of erased memory.
 *
 ****************************************************************************/

uint8_t flash_area_erased_val(const struct flash_area *fa)
{
    uint8_t erased_val;

    erased_val = 0xff;

    BOOT_LOG_INF("ID:%d erased_val:0x%x", fa->fa_id, erased_val);

    return erased_val;
}

/****************************************************************************
 * Name: flash_area_get_sectors
 *
 * Description:
 *   Retrieve info about sectors within the area.
 *
 * Input Parameters:
 *   fa_id   - ID of the flash area whose info will be retrieved.
 *   count   - On input, represents the capacity of the sectors buffer.
 *
 * Output Parameters:
 *   count   - On output, it shall contain the number of retrieved sectors.
 *   sectors - Buffer for sectors data.
 *
 * Returned Value:
 *   Zero on success, or negative value in case of error.
 *
 ****************************************************************************/

int flash_area_get_sectors(int fa_id, uint32_t *count,
        struct flash_sector *sectors)
{
    size_t off;
    uint32_t total_count = 0;
    struct flash_device_s *dev = lookup_flash_device_by_id(fa_id);
    const size_t sector_size = 4096;
    const struct flash_area *fa = dev->fa_cfg;

    for (off = 0; off < fa->fa_size; off += sector_size) {
        /* Note: Offset here is relative to flash area, not device */
        sectors[total_count].fs_off = off;
        sectors[total_count].fs_size = sector_size;
        total_count++;
    }

    *count = total_count;

    BOOT_LOG_INF("ID:%d count:%d", fa_id, *count);

    return OK;
}

/****************************************************************************
 * Name: flash_area_id_from_multi_image_slot
 *
 * Description:
 *   Return the flash area ID for a given slot and a given image index
 *   (in case of a multi-image setup).
 *
 * Input Parameters:
 *   image_index - Index of the image.
 *   slot        - Image slot, which may be 0 (primary) or 1 (secondary).
 *
 * Returned Value:
 *   Flash area ID (0 or 1), or negative value in case the requested slot
 *   is invalid.
 *
 ****************************************************************************/

int flash_area_id_from_multi_image_slot(int image_index, int slot)
{
    BOOT_LOG_INF("image_index:%d slot:%d", image_index, slot);

    switch (slot) {
        case 0:
            return FLASH_AREA_IMAGE_PRIMARY(image_index);
        case 1:
            return FLASH_AREA_IMAGE_SECONDARY(image_index);
    }

    BOOT_LOG_ERR("Unexpected Request: image_index:%d, slot:%d",
            image_index, slot);

    return ERROR; /* flash_area_open will fail on that */
}

/****************************************************************************
 * Name: flash_area_id_from_image_slot
 *
 * Description:
 *   Return the flash area ID for a given slot.
 *
 * Input Parameters:
 *   slot - Image slot, which may be 0 (primary) or 1 (secondary).
 *
 * Returned Value:
 *   Flash area ID (0 or 1), or negative value in case the requested slot
 *   is invalid.
 *
 ****************************************************************************/

int flash_area_id_from_image_slot(int slot)
{
    BOOT_LOG_INF("slot:%d", slot);

    return flash_area_id_from_multi_image_slot(0, slot);
}

/****************************************************************************
 * Name: flash_area_id_to_multi_image_slot
 *
 * Description:
 *   Convert the specified flash area ID and image index (in case of a
 *   multi-image setup) to an image slot index.
 *
 * Input Parameters:
 *   image_index - Index of the image.
 *   area_id     - Unique identifier that is represented by fa_id in the
 *                 flash_area struct.
 * Returned Value:
 *   Image slot index (0 or 1), or negative value in case ID doesn't
 *   correspond to an image slot.
 *
 ****************************************************************************/

int flash_area_id_to_multi_image_slot(int image_index, int area_id)
{
    BOOT_LOG_INF("image_index:%d area_id:%d", image_index, area_id);

    if (area_id == FLASH_AREA_IMAGE_PRIMARY(image_index)) {
        return 0;
    }

    if (area_id == FLASH_AREA_IMAGE_SECONDARY(image_index)) {
        return 1;
    }

    BOOT_LOG_ERR("Unexpected Request: image_index:%d, area_id:%d",
            image_index, area_id);

    return ERROR; /* flash_area_open will fail on that */
}

/****************************************************************************
 * Name: flash_area_id_from_image_offset
 *
 * Description:
 *   Return the flash area ID for a given image offset.
 *
 * Input Parameters:
 *   offset - Image offset.
 *
 * Returned Value:
 *   Flash area ID (0 or 1), or negative value in case the requested offset
 *   is invalid.
 *
 ****************************************************************************/

int flash_area_id_from_image_offset(uint32_t offset)
{
    struct flash_device_s *dev = lookup_flash_device_by_offset(offset);

    BOOT_LOG_INF("offset:0x%08x", offset);

    if (dev != NULL) {
        return dev->fa_cfg->fa_id;
    }

    BOOT_LOG_ERR("Unexpected Request: offset:0x%08x", offset);

    return ERROR; /* flash_area_open will fail on that */
}
