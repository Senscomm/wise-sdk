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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdio.h>

#include "flash_map_backend/flash_map_backend.h"

#include "hal/types.h"
#include "hal/spi-flash.h"

#include <bootutil/bootutil.h>
#include <bootutil/image.h>

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * do_boot
 ****************************************************************************/

extern int driver_deinit(void);

static void do_boot(struct boot_rsp *rsp)
{
    const struct flash_area *flash_area;
    int area_id;
    void (*entry)(void);
    unsigned long flags;

    area_id = flash_area_id_from_image_offset(rsp->br_image_off);

    flash_area_open(area_id, &flash_area);

    entry = (void *)(flash_area->fa_off + rsp->br_hdr->ih_hdr_size);

    flash_area_close(flash_area);

	local_irq_save(flags);

    driver_deinit();

    /*
     * Bootloader being executed by N22
     * Boot by jumping to the entry point
     */
    (*entry)();

	/* will never be reached */
	local_irq_restore(flags);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * main
 ****************************************************************************/

void mcuboot_main(void)
{
    struct boot_rsp rsp;
    fih_int fih_rc = FIH_FAILURE;

    printf("*** Booting MCUboot ***\n");

    FIH_CALL(boot_go, fih_rc, &rsp);

    if (fih_not_eq(fih_rc, FIH_SUCCESS)) {
        printf("Unable to find bootable image\n");
        FIH_PANIC;
    }

    do_boot(&rsp);

    while (1);
}
