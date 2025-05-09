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
#ifndef __CMSIS_H__
#define __CMSIS_H__

/*
 *  Only Cortex-M0 and Cortex-M3 are considered.
 */

#ifdef __arm__

#if __ARM_ARCH == 6 && __ARM_ARCH_6M__ == 1 && __ARM_ARCH_ISA_THUMB == 1
#include "cmsis/core_cm0.h"
#elif __ARM_ARCH == 7 && __ARM_ARCH_7M__ == 1 && __ARM_ARCH_ISA_THUMB == 2
#include "cmsis/core_cm3.h"
#elif __ARM_ARCH == 7
#else
#error Unknown Arm processor
#endif

#endif /* __ARM_EABI */

#endif
