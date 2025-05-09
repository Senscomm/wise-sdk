
/*
 * Auto generated Run-Time-Environment Component Configuration File
 *      *** Do not modify ! ***
 *
 * Project: 'CMSIS_CV' 
 * Target:  'FVP' 
 */

#ifndef RTE_COMPONENTS_H
#define RTE_COMPONENTS_H


/*
 * Define the Device Header File: 
 */
#if __ARM_ARCH == 6 && __ARM_ARCH_6M__ == 1 && __ARM_ARCH_ISA_THUMB == 1
#define CMSIS_device_header "hal/cmsis/ARMCM0.h"
#elif __ARM_ARCH == 7 && __ARM_ARCH_7M__ == 1 && __ARM_ARCH_ISA_THUMB == 2
#define CMSIS_device_header "hal/cmsis/ARMCM3.h"
#else
#error Unknown Arm processor
#endif


#endif /* RTE_COMPONENTS_H */
