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

#ifndef __COMPILER_H__
#define __COMPILER_H__

/* GCC */
#ifndef __GNUC__
#error "Sorry only gcc is supported yet"
#endif

#define __weak __attribute__((weak))
#define __alias(symbol) __attribute__((alias(#symbol)))
#define __maybe_unused __attribute__((unused))
#define __always_unused __attribute__((unused))

#ifndef __section
#define __section(x) __attribute__((section(x)))
#endif

#define __retain 	    __section(".retention")
#define __xip_boot__    __section(".xip.boot")
#define __xip__ 	    __section(".xip.text")
#define __iram__ 	    __section(".iram.text")
#ifdef CONFIG_USE_KERNEL_BSS_DEFAULT_SECTION
#define __kernel__      __section(".kernel")
#else
#define __kernel__      __section(".bss")
#endif
#ifdef CONFIG_BUILD_ROM
#define __dram__ 	    __section(".iram.data")
#define __dconst__
#define __dnode__		__section(".iram.data")
#else
#define __dram__
#define __dconst__ const
#define __dnode__		__section(".node.data")
#endif

#if defined(CONFIG_XIP) || defined(CONFIG_HOSTBOOT)
#ifdef CONFIG_ILM_O1
#define __ilm__			__OPT_O1__ __section(".ilm.text")
#define __ilm_wlan_tx__		__OPT_O1__ __section(".ilm.wlan.tx.text")
#define __ilm_wlan__		__OPT_O1__ __section(".ilm.wlan.text")
#define __ilm_ble__		__OPT_O1__ __section(".ilm.ble.text")

#else
#define __ilm__			__section(".ilm.text")
#define __ilm_wlan_tx__		__section(".ilm.wlan.tx.text")
#define __ilm_wlan__		__section(".ilm.wlan.text")
#define __ilm_ble__		__section(".ilm.ble.text")
#endif /* CONFIG_ILM_O1 */
#else
#define __ilm__			 __iram__
#define __ilm_wlan_tx__		__iram__
#define __ilm_wlan__		__iram__
#define __ilm_ble__		__iram__
#endif

#define __pmstore__ 	__section(".pmstore")
#ifdef CONFIG_BUILD_ROM
#define __rom_text__    __section(".rom.text")
#else
#define __rom_text__
#endif
#define __func_tab__    __section(".func_tab")
#define __romfunc__ 	__section(".rom.patch_func")
#define __ram_dma_desc__	__section(".dma_desc")
#ifdef CONFIG_SDIO
#define __sdio_dma_desc__	__section(".sdio_dma_desc")
#endif

#define __OPT_O1__		__attribute__((optimize("O1")))

#ifndef __packed
#define __packed __attribute__((packed))
#endif

#ifndef __unused
#define __unused __attribute__((unused))
#endif

#ifndef __aligned
#define __aligned(x) __attribute__((aligned(x)))
#endif

#ifndef __inline__
#define __inline__ inline __maybe_unused
#endif



#endif
