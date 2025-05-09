#ifndef __FREERTOS_TICK_CONFIG_H__
#define __FREERTOS_TICK_CONFIG_H__

/* Platform includes. */
#include "platform.h"

/* Macros for enable/disable global interrupt (mstatus.mie) */
#define prvMIE_DISABLE()    __asm volatile( "csrc mstatus, 0x8" )
#define prvMIE_ENABLE()     __asm volatile( "csrs mstatus, 0x8" )
#define prvMIE_SAVE()       prvMieSave()
#define prvMIE_RESTORE( x ) prvMieRestore( x )

/* Macros to access high and low part of 64-bit mtime registers in RV32 */
#if __riscv_xlen == 32
	#define prvREG64_HI(reg_addr) ( ( (volatile uint32_t *)reg_addr )[1] )
	#define prvREG64_LO(reg_addr) ( ( (volatile uint32_t *)reg_addr )[0] )
#endif

/*-----------------------------------------------------------*/

extern uint64_t ullNextTime;
extern const size_t uxTimerIncrementsForOneTick;

extern volatile uint64_t * const pullMtimeRegister;
extern volatile uint64_t * pullMtimecmpRegister;

extern volatile BaseType_t xTickFlag;

/*-----------------------------------------------------------*/
static portFORCE_INLINE void prvWriteMtime( uint64_t ullNewMtime )
{
	#if __riscv_xlen == 32
		/* To avoid triggering spurious interrupts when writing to mtimecmp,
		keeping the high part to the maximum value in the writing progress.

		We assume mtime overflow doesn't occur, because 64-bit mtime overflow
		period is longer than 500 years for the CPUs whose clock rate is lower
		than 1GHz. */

		prvREG64_HI( pullMtimeRegister ) = UINT32_MAX;
		prvREG64_LO( pullMtimeRegister ) = ullNewMtime & 0xFFFFFFFF;
		prvREG64_HI( pullMtimeRegister ) = ( ullNewMtime >> 32 ) & 0xFFFFFFFF;
	#else
		*pullMtimeRegister = ullNewMtime;
	#endif
}

/*-----------------------------------------------------------*/

/* prvReadMtime(): Read machine timer register.
Note: Always use this API to access mtime */

static portFORCE_INLINE uint64_t prvReadMtime( void )
{
	#if __riscv_xlen == 32
		uint32_t ulCurrentTimeHigh, ulCurrentTimeLow;
		do
		{
			ulCurrentTimeHigh = prvREG64_HI( pullMtimeRegister );
			ulCurrentTimeLow = prvREG64_LO( pullMtimeRegister );
		} while ( ulCurrentTimeHigh != prvREG64_HI( pullMtimeRegister ) );

		return ( ( ( uint64_t ) ulCurrentTimeHigh ) << 32 ) | ulCurrentTimeLow;
	#else
		return *pullMtimeRegister;
	#endif
}

/*-----------------------------------------------------------*/

/* prvWriteMtimecmp(ullNewMtimecmp): Write machine timer compare register.
Note: Use this API to access register if timer interrupt is enabled. */
static portFORCE_INLINE void prvWriteMtimecmp( uint64_t ullNewMtimecmp )
{
	#if __riscv_xlen == 32
		/* To avoid triggering spurious interrupts when writing to mtimecmp,
		keeping the high part to the maximum value in the writing progress.

		We assume mtime overflow doesn't occur, because 64-bit mtime overflow
		period is longer than 500 years for the CPUs whose clock rate is lower
		than 1GHz. */

		prvREG64_HI( pullMtimecmpRegister ) = UINT32_MAX;
		prvREG64_LO( pullMtimecmpRegister ) = ullNewMtimecmp & 0xFFFFFFFF;
		prvREG64_HI( pullMtimecmpRegister ) = ( ullNewMtimecmp >> 32 ) & 0xFFFFFFFF;
	#else
		*pullMtimecmpRegister = ullNewMtimecmp;
	#endif
}

/*-----------------------------------------------------------*/

static portFORCE_INLINE uint64_t prvReadMtimecmp( void )
{
	#if __riscv_xlen == 32
		uint32_t ulCompareTimeHigh, ulCompareTimeLow;
		do
		{
			ulCompareTimeHigh = prvREG64_HI( pullMtimecmpRegister );
			ulCompareTimeLow = prvREG64_LO( pullMtimecmpRegister );
		} while ( ulCompareTimeHigh != prvREG64_HI( pullMtimecmpRegister ) );

		return ( ( ( uint64_t ) ulCompareTimeHigh ) << 32 ) | ulCompareTimeLow;
	#else
		return *pullMtimecmpRegister;
	#endif
}

/*-----------------------------------------------------------*/

static portFORCE_INLINE UBaseType_t prvMieSave()
{
UBaseType_t uxSavedStatusValue;

	__asm volatile( "csrrc %0, mstatus, 0x8":"=r"( uxSavedStatusValue ) );
	return uxSavedStatusValue;
}
/*-----------------------------------------------------------*/

static portFORCE_INLINE void prvMieRestore( UBaseType_t uxSavedStatusValue )
{
	__asm volatile( "csrw mstatus, %0"::"r"( uxSavedStatusValue ) );
}
/*-----------------------------------------------------------*/

/* Stop mtime interrupt by setting timer compare register to maximum value.
Again, We assume mtime overflow doesn't occur, like prvWriteMtimecmp().

This function backups previous value of timer compare register and
returning it. */
static portFORCE_INLINE uint64_t prvStopMtimeIrq( void )
{
uint64_t ullSavedMtimecmp;
UBaseType_t uxSavedStatus;

	uxSavedStatus = prvMIE_SAVE();
	{
		ullSavedMtimecmp = *pullMtimecmpRegister;
		*pullMtimecmpRegister = UINT64_MAX;
	}
	prvMIE_RESTORE( uxSavedStatus );
	return ullSavedMtimecmp;
}
/*-----------------------------------------------------------*/

#endif /* __FREERTOS_TICK_CONFIG_H__ */
