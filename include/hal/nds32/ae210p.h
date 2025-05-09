/*****************************************************************************
 *
 *            Copyright Andes Technology Corporation 2014
 *                         All Rights Reserved.
 *
 ****************************************************************************/

#ifndef __AE210P_H__
#define __AE210P_H__

#ifndef __ASSEMBLER__
#include <inttypes.h>
#include <nds32_intrinsic.h>
#endif

/*****************************************************************************
 * System clock
 ****************************************************************************/

#define KHZ                     1000
#define MHZ                     1000000

#define MB_CPUCLK               (160 * MHZ)
#define MB_OSCCLK               (80 * MHZ)
#define MB_HCLK                 (MB_CPUCLK)
#define MB_PCLK                 (MB_CPUCLK)
#define MB_UCLK                 (MB_PCLK)

/*****************************************************************************
 * IRQ Vector
 ****************************************************************************/
#define IRQ_PIT_VECTOR          4

/* Include ae210p memory mapping and register definition */
#include "ae210p_defs.h"
#include "ae210p_regs.h"

#endif	/* __AE210P_H__ */
