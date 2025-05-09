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
#include <hal/compiler.h>

#include "mmap.h"
#include "linker.h"
#include "version.h"

#ifdef CONFIG_BUILD_ROM
const img_ver_t rom_version __section(".version") = {
	.major = CONFIG_BUILD_ROM_VER_MAJOR,
	.minor = CONFIG_BUILD_ROM_VER_MINOR,
};
#else
const img_ver_t fw_version __section(".version") = {
	.major = CONFIG_IMAGE_VER_MAJOR,
	.minor = CONFIG_IMAGE_VER_MINOR,
};
#endif

#ifdef CONFIG_LINK_TO_ROM
int verify_rom_version(void)
{
    extern const img_ver_t rom_version;
    if (CONFIG_LINK_TO_ROM_VER_MAJOR != rom_version.major ||
        CONFIG_LINK_TO_ROM_VER_MINOR != rom_version.minor) {
        return -1;
    }
    return 0;
}
#endif
