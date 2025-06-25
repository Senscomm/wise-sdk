/*
 * FreeRTOS Kernel V10.2.1
 * Copyright (C) 2019 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://www.FreeRTOS.org
 * http://aws.amazon.com/freertos
 *
 * 1 tab == 4 spaces!
 */

/*
 * A sample implementation of pvPortMalloc() that allows the heap to be defined
 * across multiple non-contigous blocks and combines (coalescences) adjacent
 * memory blocks as they are freed.
 *
 * See heap_1.c, heap_2.c, heap_3.c and heap_4.c for alternative
 * implementations, and the memory management pages of http://www.FreeRTOS.org
 * for more information.
 *
 * Usage notes:
 *
 * vPortDefineHeapRegions() ***must*** be called before pvPortMalloc().
 * pvPortMalloc() will be called if any task objects (tasks, queues, event
 * groups, etc.) are created, therefore vPortDefineHeapRegions() ***must*** be
 * called before any other objects are defined.
 *
 * vPortDefineHeapRegions() takes a single parameter.  The parameter is an array
 * of HeapRegion_t structures.  HeapRegion_t is defined in portable.h as
 *
 * typedef struct HeapRegion
 * {
 *	uint8_t *pucStartAddress; << Start address of a block of memory that will be part of the heap.
 *	size_t xSizeInBytes;	  << Size of the block of memory.
 * } HeapRegion_t;
 *
 * The array is terminated using a NULL zero sized region definition, and the
 * memory regions defined in the array ***must*** appear in address order from
 * low address to high address.  So the following is a valid example of how
 * to use the function.
 *
 * HeapRegion_t xHeapRegions[] =
 * {
 * 	{ ( uint8_t * ) 0x80000000UL, 0x10000 }, << Defines a block of 0x10000 bytes starting at address 0x80000000
 * 	{ ( uint8_t * ) 0x90000000UL, 0xa0000 }, << Defines a block of 0xa0000 bytes starting at address of 0x90000000
 * 	{ NULL, 0 }                << Terminates the array.
 * };
 *
 * vPortDefineHeapRegions( xHeapRegions ); << Pass the array into vPortDefineHeapRegions().
 *
 * Note 0x80000000 is the lower address so appears in the array first.
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <hal/types.h>
#include <hal/compiler.h>
#include <hal/rom.h>

/* Defining MPU_WRAPPERS_INCLUDED_FROM_API_FILE prevents task.h from redefining
all the API functions to use the MPU wrappers.  That should only be done when
task.h is included from an application file. */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE

#include "FreeRTOS.h"
#include "task.h"

#ifndef CONFIG_LINK_TO_ROM
	#error "Not support HEAP memory debug"
#endif

#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE

#if( configSUPPORT_DYNAMIC_ALLOCATION == 0 )
	#error This file must not be used if configSUPPORT_DYNAMIC_ALLOCATION is 0
#endif

/* Block sizes must not get too small. */
#define heapMINIMUM_BLOCK_SIZE	( ( size_t ) ( xHeapStructSize << 1 ) )

/* Assumes 8bit bytes! */
#define heapBITS_PER_BYTE		( ( size_t ) 8 )

/* Define the linked list structure.  This is used to link free blocks in order
of their memory address. */
typedef struct A_BLOCK_LINK
{
#if( configUSE_MALLOC_DEBUG == 1 )
    uint32_t ulHeadCanary;                  /*<< The head canary. */
#endif
	struct A_BLOCK_LINK *pxNextFreeBlock;	/*<< The next free block in the list. */
	size_t xBlockSize;						/*<< The size of the free block. */
#if( configUSE_MALLOC_DEBUG == 1 )
    TaskHandle_t xOwner;                    /*<< The buffer owner. */
    size_t xWantedSize;
#ifdef CONFIG_MEM_HEAP_DEBUG
	char xFuncName[CONFIG_MEM_HEAP_DEBUG_FUNCNAMELEN];
#endif /* CONFIG_MEM_HEAP_DEBUG */
#endif /* configUSE_MALLOC_DEBUG */
} BlockLink_t;

#if( configUSE_MALLOC_DEBUG == 1 )

#ifndef ARRAY_SIZE
#define ARRAY_SIZE( x ) ( sizeof( x ) / sizeof( x[ 0 ] ) )
#endif

#define xstr(s) str(s)
#define str(s) #s
#define FMT "%-" xstr(configMAX_TASK_NAME_LEN) "s"
#ifdef CONFIG_MEM_HEAP_DEBUG
#define FMTF "%-" xstr(CONFIG_MEM_HEAP_DEBUG_FUNCNAMELEN) "s"
#endif

#define OPTIMIZE_FAST __attribute__((optimize("O3")))

/* Canary patterns. */
#define HEAD_CANARY_PATTERN     ( 0xCAFE1234 )
#define TAIL_CANARY_PATTERN     ( 0xDEAF5678 )

void vPortAddToAllocList( BlockLink_t *pxLink );
BaseType_t xPortRemoveFromAllocList( BlockLink_t *pxLink );
void vPortUpdateFreeBlockList( void );

BlockLink_t *pxAllocList[ configSIZE_ALLOC_LIST ] = {0};
BlockLink_t *pxAllocListCopy[ configSIZE_ALLOC_LIST ] = {0};

typedef struct
{
    char *vma;
    size_t size;
} tMemoryRegion;

extern char __data_start[], __data_end[];
extern char __bss_start[], __bss_end[];
extern char __heap_start[], __heap_end[];
#ifdef CONFIG_N22_ONLY
extern char __heapext1_start[], __heapext1_end[];
extern char __heapext2_start[], __heapext2_end[];
#endif

tMemoryRegion ram[5];

#endif

#ifdef CONFIG_SUPPORT_DMA_DYNAMIC_ALLOC
BlockLink_t xStartDMA;
BlockLink_t *pxEndDMA;

#define DMA_BUFFER_MARK		( 0xFFFFFFFF )
#endif

/*-----------------------------------------------------------*/

/*
 * Inserts a block of memory that is being freed into the correct position in
 * the list of free memory blocks.  The block being freed will be merged with
 * the block in front it and/or the block behind it if the memory blocks are
 * adjacent to each other.
 */
void p_prvInsertBlockIntoFreeList( BlockLink_t *pxBlockToInsert );
extern void (*prvInsertBlockIntoFreeList)( BlockLink_t *pxBlockToInsert );
PATCH(prvInsertBlockIntoFreeList, &prvInsertBlockIntoFreeList, &p_prvInsertBlockIntoFreeList);

/*-----------------------------------------------------------*/

/* The size of the structure placed at the beginning of each allocated memory
block must by correctly byte aligned. */
static const size_t xHeapStructSize	= ( sizeof( BlockLink_t ) + ( ( size_t ) ( portBYTE_ALIGNMENT - 1 ) ) ) & ~( ( size_t ) portBYTE_ALIGNMENT_MASK );

/* Create a couple of list links to mark the start and end of the list. */
static BlockLink_t _xStart, *_pxEnd = NULL;

/* Keeps track of the number of free bytes remaining, but says nothing about
fragmentation. */
extern size_t xFreeBytesRemaining;
extern size_t xMinimumEverFreeBytesRemaining;

/* Gets set to the top bit of an size_t type.  When this bit in the xBlockSize
member of an BlockLink_t structure is set then the block belongs to the
application.  When the bit is free the block is still part of the free heap
space. */
static size_t _xBlockAllocatedBit = 0;

#ifdef CONFIG_MEM_HEAP_PERIOD_TIME_DEBUG

static size_t _xFreeBytesStart = 0;
static size_t _xFreeBytes = 0;
static size_t _xMinimumEverFreeBytes = 0;

void xPortCheckMemStart( void )
{
	_xFreeBytesStart = xFreeBytesRemaining;
	_xFreeBytes = xFreeBytesRemaining;
	_xMinimumEverFreeBytes = xFreeBytesRemaining;
}

void xPortCheckMemEnd( void )
{
	printf("Increase   %d bytes during the period \n", _xFreeBytesStart - _xMinimumEverFreeBytes);
}

#endif /* CONFIG_MEM_HEAP_PERIOD_TIME_DEBUG */

/*-----------------------------------------------------------*/

/*-----------------------------------------------------------*/

#ifdef CONFIG_SUPPORT_DMA_DYNAMIC_ALLOC

__ilm__ static void prvInsertBlockIntoFreeListWithDMA( BlockLink_t *pxBlockToInsert, BaseType_t xDMABuffer )
{
BlockLink_t *pxBlockStart = &_xStart;
BlockLink_t *pxBlockEnd = _pxEnd;
BlockLink_t *pxIterator;
uint8_t *puc;

	if( xDMABuffer == pdTRUE )
	{
		pxBlockStart = &xStartDMA;
		pxBlockEnd = pxEndDMA;
	}

	/* Iterate through the list until a block is found that has a higher address
	than the block being inserted. */
	for( pxIterator = pxBlockStart; pxIterator->pxNextFreeBlock < pxBlockToInsert; pxIterator = pxIterator->pxNextFreeBlock )
	{
		/* Nothing to do here, just iterate to the right position. */
	}

	/* Do the block being inserted, and the block it is being inserted after
	make a contiguous block of memory? */
	puc = ( uint8_t * ) pxIterator;
	if( ( puc + pxIterator->xBlockSize ) == ( uint8_t * ) pxBlockToInsert )
	{
		pxIterator->xBlockSize += pxBlockToInsert->xBlockSize;
		pxBlockToInsert = pxIterator;
	}
	else
	{
		mtCOVERAGE_TEST_MARKER();
	}

	/* Do the block being inserted, and the block it is being inserted before
	make a contiguous block of memory? */
	puc = ( uint8_t * ) pxBlockToInsert;
	if( ( puc + pxBlockToInsert->xBlockSize ) == ( uint8_t * ) pxIterator->pxNextFreeBlock )
	{
		if( pxIterator->pxNextFreeBlock != pxBlockEnd )
		{
			/* Form one big block from the two blocks. */
			pxBlockToInsert->xBlockSize += pxIterator->pxNextFreeBlock->xBlockSize;
			pxBlockToInsert->pxNextFreeBlock = pxIterator->pxNextFreeBlock->pxNextFreeBlock;
		}
		else
		{
			pxBlockToInsert->pxNextFreeBlock = pxBlockEnd;
		}
	}
	else
	{
		pxBlockToInsert->pxNextFreeBlock = pxIterator->pxNextFreeBlock;
	}

	/* If the block being inserted plugged a gab, so was merged with the block
	before and the block after, then it's pxNextFreeBlock pointer will have
	already been set, and should not be set here as that would make it point
	to itself. */
	if( pxIterator != pxBlockToInsert )
	{
		pxIterator->pxNextFreeBlock = pxBlockToInsert;
	}
	else
	{
		mtCOVERAGE_TEST_MARKER();
	}
}

#ifdef CONFIG_MEM_HEAP_DEBUG
__ilm__ void *pvPortMallocDMA( size_t xWantedSize, const char *xFuncName )
#else
__ilm__ void *pvPortMallocDMA( size_t xWantedSize )
#endif
{
BlockLink_t *pxBlock, *pxPreviousBlock, *pxNewBlockLink;
size_t xOrgWantedSize __maybe_unused;
void *pvReturn = NULL;

	/* The heap must be initialised before the first call to
	prvPortMalloc(). */
	configASSERT( _pxEnd );

	vTaskSuspendAll();
	{
		/* Check the requested block size is not so large that the top bit is
		set.  The top bit of the block size member of the BlockLink_t structure
		is used to determine who owns the block - the application or the
		kernel, so it must be free. */
		if( ( xWantedSize & _xBlockAllocatedBit ) == 0 )
		{
            /* Record the original wanted size. */
            xOrgWantedSize = xWantedSize;

			/* The wanted size is increased so it can contain a BlockLink_t
			structure in addition to the requested amount of bytes. */
			if( xWantedSize > 0 )
			{
				xWantedSize += xHeapStructSize;

#if( configUSE_MALLOC_DEBUG == 1 )
                xWantedSize += sizeof(uint32_t); /* Room for the tail canary. */
#endif

				/* Ensure that blocks are always aligned to the required number
				of bytes. */
				if( ( xWantedSize & portBYTE_ALIGNMENT_MASK ) != 0x00 )
				{
					/* Byte alignment required. */
					xWantedSize += ( portBYTE_ALIGNMENT - ( xWantedSize & portBYTE_ALIGNMENT_MASK ) );
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}

			if( ( xWantedSize > 0 ) && ( xWantedSize <= xFreeBytesRemaining ) )
			{
				/* Traverse the list from the start	(lowest address) block until
				one	of adequate size is found. */
				pxPreviousBlock = &xStartDMA;
				pxBlock = xStartDMA.pxNextFreeBlock;

				while( ( pxBlock->xBlockSize < xWantedSize ) && ( pxBlock->pxNextFreeBlock != NULL ) )
				{
					pxPreviousBlock = pxBlock;
					pxBlock = pxBlock->pxNextFreeBlock;
				}

				/* If the end marker was reached then a block of adequate size
				was	not found. */
				if( pxBlock != pxEndDMA )
				{
					/* Return the memory space pointed to - jumping over the
					BlockLink_t structure at its start. */
					pvReturn = ( void * ) ( ( ( uint8_t * ) pxPreviousBlock->pxNextFreeBlock ) + xHeapStructSize );

#if( configUSE_MALLOC_DEBUG == 1 )
                    /* Record the current task handle. */
                    if( xTaskGetCurrentTaskHandle() && ( xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED ) )
                    {
                        pxPreviousBlock->pxNextFreeBlock->xOwner = xTaskGetCurrentTaskHandle();
                    }
					else
					{
                        pxPreviousBlock->pxNextFreeBlock->xOwner = NULL;
					}

                    /* Record the wanted size. */
                    pxPreviousBlock->pxNextFreeBlock->xWantedSize = xOrgWantedSize;
#endif
					/* This block is being returned for use so must be taken out
					of the list of free blocks. */
					pxPreviousBlock->pxNextFreeBlock = pxBlock->pxNextFreeBlock;

					/* If the block is larger than required it can be split into
					two. */
					if( ( pxBlock->xBlockSize - xWantedSize ) > heapMINIMUM_BLOCK_SIZE )
					{
						/* This block is to be split into two.  Create a new
						block following the number of bytes requested. The void
						cast is used to prevent byte alignment warnings from the
						compiler. */
						pxNewBlockLink = ( void * ) ( ( ( uint8_t * ) pxBlock ) + xWantedSize );

						/* Calculate the sizes of two blocks split from the
						single block. */
						pxNewBlockLink->xBlockSize = pxBlock->xBlockSize - xWantedSize;
						pxBlock->xBlockSize = xWantedSize;

						prvInsertBlockIntoFreeListWithDMA( ( pxNewBlockLink ), pdTRUE );
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}

#ifdef CONFIG_MEM_HEAP_DEBUG
					memset(pxBlock->xFuncName, '\0', CONFIG_MEM_HEAP_DEBUG_FUNCNAMELEN);
					strncpy(pxBlock->xFuncName, xFuncName, CONFIG_MEM_HEAP_DEBUG_FUNCNAMELEN);
#endif

#ifdef CONFIG_MEM_HEAP_PERIOD_TIME_DEBUG
					_xFreeBytes -= pxBlock->xBlockSize;

					if (_xFreeBytes < _xMinimumEverFreeBytes)
						_xMinimumEverFreeBytes = _xFreeBytes;
#endif /* CONFIG_MEM_HEAP_PERIOD_TIME_DEBUG */

					xFreeBytesRemaining -= pxBlock->xBlockSize;

					if( xFreeBytesRemaining < xMinimumEverFreeBytesRemaining )
					{
						xMinimumEverFreeBytesRemaining = xFreeBytesRemaining;
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}

					/* The block is being returned - it is allocated and owned
					by the application and has no "next" block. */
					pxBlock->xBlockSize |= _xBlockAllocatedBit;
					pxBlock->pxNextFreeBlock = ( void * ) DMA_BUFFER_MARK;
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}

		traceMALLOC( pvReturn, xWantedSize );
	}

#if( configUSE_MALLOC_DEBUG == 1 )
    if( pvReturn != NULL )
    {
        vPortAddToAllocList( ( BlockLink_t * ) ( (uint32_t) pvReturn - xHeapStructSize ) );
    }
#endif

	( void ) xTaskResumeAll();

	#if( configUSE_MALLOC_FAILED_HOOK == 1 )
	{
		if( pvReturn == NULL )
		{
			extern void vApplicationMallocFailedHook( void );
			vApplicationMallocFailedHook();
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}
	#endif

	return pvReturn;
}


static BaseType_t xPortHeapRegionsIsSame( const HeapRegion_t *pxHeapRegion, BlockLink_t *pxPreviousBlock )
{
size_t xAddress;

	xAddress = ( size_t ) pxHeapRegion->pucStartAddress;

	if( ( xAddress & portBYTE_ALIGNMENT_MASK ) != 0 )
	{
		xAddress += ( portBYTE_ALIGNMENT - 1 );
		xAddress &= ~portBYTE_ALIGNMENT_MASK;
	}

	if( xAddress == ( size_t ) pxPreviousBlock->pxNextFreeBlock )
	{
		return pdTRUE;
	}

	return pdFALSE;
}

void vPortDefineHeapRegionsDMA( const HeapRegion_t * const pxHeapRegions )
{
BlockLink_t *pxFirstFreeBlockInRegion = NULL, *pxPreviousFreeBlock;
size_t xAlignedHeap;
size_t xTotalRegionSize, xTotalHeapSize = 0;
BaseType_t xDefinedRegions = 0;
size_t xAddress;
const HeapRegion_t *pxHeapRegion;

	/* Can only call after vPortDefineHeapRegions */
	configASSERT( _pxEnd != NULL );

	/* Can only call once! */
	configASSERT( pxEndDMA == NULL );

	pxHeapRegion = &( pxHeapRegions[ xDefinedRegions ] );

	/* Check if the nonDMA area and DMA region are the same */
	if ( xPortHeapRegionsIsSame( pxHeapRegion, &_xStart ) )
	{
		memcpy( &xStartDMA, &_xStart, sizeof( BlockLink_t ) );
		pxEndDMA = _pxEnd;

		return;
	}

	while( pxHeapRegion->xSizeInBytes > 0 )
	{
		xTotalRegionSize = pxHeapRegion->xSizeInBytes;

		/* Ensure the heap region starts on a correctly aligned boundary. */
		xAddress = ( size_t ) pxHeapRegion->pucStartAddress;
		if( ( xAddress & portBYTE_ALIGNMENT_MASK ) != 0 )
		{
			xAddress += ( portBYTE_ALIGNMENT - 1 );
			xAddress &= ~portBYTE_ALIGNMENT_MASK;

			/* Adjust the size for the bytes lost to alignment. */
			xTotalRegionSize -= xAddress - ( size_t ) pxHeapRegion->pucStartAddress;
		}

		xAlignedHeap = xAddress;

		/* Set xStartDMA if it has not already been set. */
		if( xDefinedRegions == 0 )
		{
			/* xStartDMA is used to hold a pointer to the first item in the list of
			free blocks.  The void cast is used to prevent compiler warnings. */
			xStartDMA.pxNextFreeBlock = ( BlockLink_t * ) xAlignedHeap;
			xStartDMA.xBlockSize = ( size_t ) 0;
		}
		else
		{
			/* Should only get here if one region has already been added to the
			heap. */
			configASSERT( pxEndDMA != NULL );

			/* Check blocks are passed in with increasing start addresses. */
			configASSERT( xAddress > ( size_t ) pxEndDMA );
		}

		/* Remember the location of the end marker in the previous region, if
		any. */
		pxPreviousFreeBlock = pxEndDMA;

		/* pxEndDMA is used to mark the end of the list of free blocks and is
		inserted at the end of the region space. */
		xAddress = xAlignedHeap + xTotalRegionSize;
		xAddress -= xHeapStructSize;
		xAddress &= ~portBYTE_ALIGNMENT_MASK;
		pxEndDMA = ( BlockLink_t * ) xAddress;
		pxEndDMA->xBlockSize = 0;
		pxEndDMA->pxNextFreeBlock = NULL;


		/* To start with there is a single free block in this region that is
		sized to take up the entire heap region minus the space taken by the
		free block structure. */
		pxFirstFreeBlockInRegion = ( BlockLink_t * ) xAlignedHeap;
		pxFirstFreeBlockInRegion->xBlockSize = xAddress - ( size_t ) pxFirstFreeBlockInRegion;
		pxFirstFreeBlockInRegion->pxNextFreeBlock = pxEndDMA;

		/* If this is not the first region that makes up the entire heap space
		then link the previous region to this region. */
		if( pxPreviousFreeBlock != NULL )
		{
			pxPreviousFreeBlock->pxNextFreeBlock = pxFirstFreeBlockInRegion;
		}

		xTotalHeapSize += pxFirstFreeBlockInRegion->xBlockSize;

		/* Move onto the next HeapRegion_t structure. */
		xDefinedRegions++;
		pxHeapRegion = &( pxHeapRegions[ xDefinedRegions ] );
	}

	xMinimumEverFreeBytesRemaining += xTotalHeapSize;
	xFreeBytesRemaining += xTotalHeapSize;

	/* Check something was actually defined before it is accessed. */
	configASSERT( xTotalHeapSize );
}

#endif /* CONFIG_SUPPORT_DMA_DYNAMIC_ALLOC */

void *p_pvPortMalloc( size_t xWantedSize );
extern void *(*pvPortMalloc)( size_t xWantedSize );
PATCH(pvPortMalloc, &pvPortMalloc, &p_pvPortMalloc);

static void conv_to_str(char *out, uint32_t in) __maybe_unused;
static void conv_to_str(char *out, uint32_t in)
{
	int i;
	uint8_t b;
	for (i = 0; i < 8; i++) {
		b = (in >> (4 * (7 - i))) & 0xf;
		*(out + i) = b < 10 ? '0' + b : 'a' + (b - 10);
	}
}

__ilm__ void *p_pvPortMalloc( size_t xWantedSize )
{
BlockLink_t *pxBlock, *pxPreviousBlock, *pxNewBlockLink;
BlockLink_t *pxBlockEnd = _pxEnd;
size_t xOrgWantedSize __maybe_unused;
void *pvReturn = NULL;
#ifdef CONFIG_SUPPORT_DMA_DYNAMIC_ALLOC
BaseType_t xDMABuffer = pdFALSE;
#endif

	/* The heap must be initialised before the first call to
	prvPortMalloc(). */
	configASSERT( _pxEnd );

	vTaskSuspendAll();
	{
		/* Check the requested block size is not so large that the top bit is
		set.  The top bit of the block size member of the BlockLink_t structure
		is used to determine who owns the block - the application or the
		kernel, so it must be free. */
		if( ( xWantedSize & _xBlockAllocatedBit ) == 0 )
		{
            /* Record the original wanted size. */
            xOrgWantedSize = xWantedSize;

			/* The wanted size is increased so it can contain a BlockLink_t
			structure in addition to the requested amount of bytes. */
			if( xWantedSize > 0 )
			{
				xWantedSize += xHeapStructSize;

#if( configUSE_MALLOC_DEBUG == 1 )
                xWantedSize += sizeof(uint32_t); /* Room for the tail canary. */
#endif

				/* Ensure that blocks are always aligned to the required number
				of bytes. */
				if( ( xWantedSize & portBYTE_ALIGNMENT_MASK ) != 0x00 )
				{
					/* Byte alignment required. */
					xWantedSize += ( portBYTE_ALIGNMENT - ( xWantedSize & portBYTE_ALIGNMENT_MASK ) );
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}

			if( ( xWantedSize > 0 ) && ( xWantedSize <= xFreeBytesRemaining ) )
			{
				/* Traverse the list from the start	(lowest address) block until
				one	of adequate size is found. */
				pxPreviousBlock = &_xStart;
				pxBlock = _xStart.pxNextFreeBlock;
				while( ( pxBlock->xBlockSize < xWantedSize ) && ( pxBlock->pxNextFreeBlock != NULL ) )
				{
					pxPreviousBlock = pxBlock;
					pxBlock = pxBlock->pxNextFreeBlock;
				}

#ifdef CONFIG_SUPPORT_DMA_DYNAMIC_ALLOC
				if ( pxBlock == _pxEnd && pxEndDMA != _pxEnd )
				{
					pxPreviousBlock = &xStartDMA;
					pxBlock = xStartDMA.pxNextFreeBlock;
					pxBlockEnd = pxEndDMA;
					xDMABuffer = pdTRUE;

					while( ( pxBlock->xBlockSize < xWantedSize ) && ( pxBlock->pxNextFreeBlock != NULL ) )
					{
						pxPreviousBlock = pxBlock;
						pxBlock = pxBlock->pxNextFreeBlock;
					}
				}
#endif

				/* If the end marker was reached then a block of adequate size
				was	not found. */
				if( pxBlock != pxBlockEnd )
				{
					/* Return the memory space pointed to - jumping over the
					BlockLink_t structure at its start. */
					pvReturn = ( void * ) ( ( ( uint8_t * ) pxPreviousBlock->pxNextFreeBlock ) + xHeapStructSize );

#if( configUSE_MALLOC_DEBUG == 1 )
                    /* Record the current task handle. */
                    if( xTaskGetCurrentTaskHandle() && ( xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED ) )
                    {
                        pxPreviousBlock->pxNextFreeBlock->xOwner = xTaskGetCurrentTaskHandle();
                    }
                    else
                    {
                        pxPreviousBlock->pxNextFreeBlock->xOwner = NULL;
                    }

                    /* Record the wanted size. */
                    pxPreviousBlock->pxNextFreeBlock->xWantedSize = xOrgWantedSize;
#endif
					/* This block is being returned for use so must be taken out
					of the list of free blocks. */
					pxPreviousBlock->pxNextFreeBlock = pxBlock->pxNextFreeBlock;

					/* If the block is larger than required it can be split into
					two. */
					if( ( pxBlock->xBlockSize - xWantedSize ) > heapMINIMUM_BLOCK_SIZE )
					{
						/* This block is to be split into two.  Create a new
						block following the number of bytes requested. The void
						cast is used to prevent byte alignment warnings from the
						compiler. */
						pxNewBlockLink = ( void * ) ( ( ( uint8_t * ) pxBlock ) + xWantedSize );

						/* Calculate the sizes of two blocks split from the
						single block. */
						pxNewBlockLink->xBlockSize = pxBlock->xBlockSize - xWantedSize;
						pxBlock->xBlockSize = xWantedSize;

#ifdef CONFIG_SUPPORT_DMA_DYNAMIC_ALLOC
						prvInsertBlockIntoFreeListWithDMA( ( pxNewBlockLink ), xDMABuffer );
#else
						/* Insert the new block into the list of free blocks. */
						prvInsertBlockIntoFreeList( ( pxNewBlockLink ) );
#endif
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}

#ifdef CONFIG_MEM_HEAP_DEBUG
#if CONFIG_MEM_HEAP_DEBUG_FUNCNAMELEN < 14
#error "FUNCNAMELEN is too small."
#endif
					/* There is nothing to record.
					 */
					memset(pxBlock->xFuncName, '\0', CONFIG_MEM_HEAP_DEBUG_FUNCNAMELEN);
                    {
					   uint32_t ra = (uint32_t)__builtin_return_address(0);

                       pxBlock->xFuncName[0] = 'r';
                       pxBlock->xFuncName[1] = 'o';
                       pxBlock->xFuncName[2] = '-';
                       conv_to_str(pxBlock->xFuncName + 3, ra);
                    }
#endif

#ifdef CONFIG_MEM_HEAP_PERIOD_TIME_DEBUG

					_xFreeBytes -= pxBlock->xBlockSize;

					if (_xFreeBytes < _xMinimumEverFreeBytes)
						_xMinimumEverFreeBytes = _xFreeBytes;
#endif /* CONFIG_MEM_HEAP_PERIOD_TIME_DEBUG */

					xFreeBytesRemaining -= pxBlock->xBlockSize;

					if( xFreeBytesRemaining < xMinimumEverFreeBytesRemaining )
					{
						xMinimumEverFreeBytesRemaining = xFreeBytesRemaining;
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}

					/* The block is being returned - it is allocated and owned
					by the application and has no "next" block. */
					pxBlock->xBlockSize |= _xBlockAllocatedBit;
#ifdef CONFIG_SUPPORT_DMA_DYNAMIC_ALLOC
					if ( xDMABuffer )
					{
						pxBlock->pxNextFreeBlock = ( void * ) DMA_BUFFER_MARK;
					}
					else
					{
						pxBlock->pxNextFreeBlock = NULL;
					}
#else
					pxBlock->pxNextFreeBlock = NULL;
#endif
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}

		traceMALLOC( pvReturn, xWantedSize );
	}

#if( configUSE_MALLOC_DEBUG == 1 )
    if( pvReturn != NULL )
    {
        vPortAddToAllocList( ( BlockLink_t * ) ( (uint32_t) pvReturn - xHeapStructSize ) );
    }
#endif

	( void ) xTaskResumeAll();

	#if( configUSE_MALLOC_FAILED_HOOK == 1 )
	{
		if( pvReturn == NULL )
		{
			extern void vApplicationMallocFailedHook( void );
			vApplicationMallocFailedHook();
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}
	#endif

	return pvReturn;
}

#ifdef CONFIG_MEM_HEAP_DEBUG

/* This ix almost the same with pvPortMalloc except for recording caller's name.
 * Ugly, but there is no other choice to consider callers in our ROM library.
 */

__ilm__ void *pvPortMallocDbg( size_t xWantedSize, const char *xFuncName )
{
BlockLink_t *pxBlock, *pxPreviousBlock, *pxNewBlockLink;
BlockLink_t *pxBlockEnd = _pxEnd;
size_t xOrgWantedSize;
void *pvReturn = NULL;
#ifdef CONFIG_SUPPORT_DMA_DYNAMIC_ALLOC
BaseType_t xDMABuffer = pdFALSE;
#endif

	/* The heap must be initialised before the first call to
	prvPortMalloc(). */
	configASSERT( _pxEnd );

	vTaskSuspendAll();
	{
		/* Check the requested block size is not so large that the top bit is
		set.  The top bit of the block size member of the BlockLink_t structure
		is used to determine who owns the block - the application or the
		kernel, so it must be free. */
		if( ( xWantedSize & _xBlockAllocatedBit ) == 0 )
		{
            /* Record the original wanted size. */
            xOrgWantedSize = xWantedSize;

			/* The wanted size is increased so it can contain a BlockLink_t
			structure in addition to the requested amount of bytes. */
			if( xWantedSize > 0 )
			{
				xWantedSize += xHeapStructSize;

#if( configUSE_MALLOC_DEBUG == 1 )
                xWantedSize += sizeof(uint32_t); /* Room for the tail canary. */
#endif

				/* Ensure that blocks are always aligned to the required number
				of bytes. */
				if( ( xWantedSize & portBYTE_ALIGNMENT_MASK ) != 0x00 )
				{
					/* Byte alignment required. */
					xWantedSize += ( portBYTE_ALIGNMENT - ( xWantedSize & portBYTE_ALIGNMENT_MASK ) );
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}

			if( ( xWantedSize > 0 ) && ( xWantedSize <= xFreeBytesRemaining ) )
			{
				/* Traverse the list from the start	(lowest address) block until
				one	of adequate size is found. */
				pxPreviousBlock = &_xStart;
				pxBlock = _xStart.pxNextFreeBlock;
				while( ( pxBlock->xBlockSize < xWantedSize ) && ( pxBlock->pxNextFreeBlock != NULL ) )
				{
					pxPreviousBlock = pxBlock;
					pxBlock = pxBlock->pxNextFreeBlock;
				}

#ifdef CONFIG_SUPPORT_DMA_DYNAMIC_ALLOC
				if ( pxBlock == _pxEnd )
				{
					pxPreviousBlock = &xStartDMA;
					pxBlock = xStartDMA.pxNextFreeBlock;
					pxBlockEnd = pxEndDMA;
					xDMABuffer = pdTRUE;

					while( ( pxBlock->xBlockSize < xWantedSize ) && ( pxBlock->pxNextFreeBlock != NULL ) )
					{
						pxPreviousBlock = pxBlock;
						pxBlock = pxBlock->pxNextFreeBlock;
					}
				}
#endif

				/* If the end marker was reached then a block of adequate size
				was	not found. */
				if( pxBlock != pxBlockEnd )
				{
					/* Return the memory space pointed to - jumping over the
					BlockLink_t structure at its start. */
					pvReturn = ( void * ) ( ( ( uint8_t * ) pxPreviousBlock->pxNextFreeBlock ) + xHeapStructSize );

#if( configUSE_MALLOC_DEBUG == 1 )
                    /* Record the current task handle. */
                    if( xTaskGetCurrentTaskHandle() && ( xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED ) )
                    {
                        pxPreviousBlock->pxNextFreeBlock->xOwner = xTaskGetCurrentTaskHandle();
                    }
					else
					{
                        pxPreviousBlock->pxNextFreeBlock->xOwner = NULL;
					}

                    /* Record the wanted size. */
                    pxPreviousBlock->pxNextFreeBlock->xWantedSize = xOrgWantedSize;
#endif
					/* This block is being returned for use so must be taken out
					of the list of free blocks. */
					pxPreviousBlock->pxNextFreeBlock = pxBlock->pxNextFreeBlock;

					/* If the block is larger than required it can be split into
					two. */
					if( ( pxBlock->xBlockSize - xWantedSize ) > heapMINIMUM_BLOCK_SIZE )
					{
						/* This block is to be split into two.  Create a new
						block following the number of bytes requested. The void
						cast is used to prevent byte alignment warnings from the
						compiler. */
						pxNewBlockLink = ( void * ) ( ( ( uint8_t * ) pxBlock ) + xWantedSize );

						/* Calculate the sizes of two blocks split from the
						single block. */
						pxNewBlockLink->xBlockSize = pxBlock->xBlockSize - xWantedSize;
						pxBlock->xBlockSize = xWantedSize;

#ifdef CONFIG_SUPPORT_DMA_DYNAMIC_ALLOC
						prvInsertBlockIntoFreeListWithDMA( ( pxNewBlockLink ), xDMABuffer );
#else
						/* Insert the new block into the list of free blocks. */
						prvInsertBlockIntoFreeList( ( pxNewBlockLink ) );
#endif
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}

					memset(pxBlock->xFuncName, '\0', CONFIG_MEM_HEAP_DEBUG_FUNCNAMELEN);
					strncpy(pxBlock->xFuncName, xFuncName, CONFIG_MEM_HEAP_DEBUG_FUNCNAMELEN);

#ifdef CONFIG_MEM_HEAP_PERIOD_TIME_DEBUG
					_xFreeBytes -= pxBlock->xBlockSize;

					if (_xFreeBytes < _xMinimumEverFreeBytes)
						_xMinimumEverFreeBytes = _xFreeBytes;
#endif /* CONFIG_MEM_HEAP_PERIOD_TIME_DEBUG */

					xFreeBytesRemaining -= pxBlock->xBlockSize;

					if( xFreeBytesRemaining < xMinimumEverFreeBytesRemaining )
					{
						xMinimumEverFreeBytesRemaining = xFreeBytesRemaining;
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}

					/* The block is being returned - it is allocated and owned
					by the application and has no "next" block. */
					pxBlock->xBlockSize |= _xBlockAllocatedBit;
#ifdef CONFIG_SUPPORT_DMA_DYNAMIC_ALLOC
					if ( xDMABuffer )
					{
						pxBlock->pxNextFreeBlock = ( void * ) DMA_BUFFER_MARK;
					}
					else
					{
						pxBlock->pxNextFreeBlock = NULL;
					}
#else
					pxBlock->pxNextFreeBlock = NULL;
#endif
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}

		traceMALLOC( pvReturn, xWantedSize );
	}

#if( configUSE_MALLOC_DEBUG == 1 )
    if( pvReturn != NULL )
    {
        vPortAddToAllocList( ( BlockLink_t * ) ( (uint32_t) pvReturn - xHeapStructSize ) );
    }
#endif

	( void ) xTaskResumeAll();

	#if( configUSE_MALLOC_FAILED_HOOK == 1 )
	{
		if( pvReturn == NULL )
		{
			extern void vApplicationMallocFailedHook( void );
			vApplicationMallocFailedHook();
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}
	#endif

	return pvReturn;
}

#endif

/*-----------------------------------------------------------*/

void p_vPortFree( void *pv );
extern void (*vPortFree)( void *pv );
PATCH(vPortFree, &vPortFree, &p_vPortFree);

__ilm__ void p_vPortFree( void *pv )
{
uint8_t *puc = ( uint8_t * ) pv;
BlockLink_t *pxLink;
#ifdef CONFIG_SUPPORT_DMA_DYNAMIC_ALLOC
BaseType_t xDMABuffer = pdFALSE;
#endif

	if( pv != NULL )
	{
		/* The memory being freed will have an BlockLink_t structure immediately
		before it. */
		puc -= xHeapStructSize;

		/* This casting is to keep the compiler from issuing warnings. */
		pxLink = ( void * ) puc;

		/* Check the block is actually allocated. */
		configASSERT( ( pxLink->xBlockSize & _xBlockAllocatedBit ) != 0 );
#ifdef CONFIG_SUPPORT_DMA_DYNAMIC_ALLOC
		configASSERT( ( pxLink->pxNextFreeBlock == NULL ) ||
				      ( (uint32_t)pxLink->pxNextFreeBlock == DMA_BUFFER_MARK ) );
#else
		configASSERT( pxLink->pxNextFreeBlock == NULL );
#endif

		if( ( pxLink->xBlockSize & _xBlockAllocatedBit ) != 0 )
		{
#ifdef CONFIG_SUPPORT_DMA_DYNAMIC_ALLOC
			if( ( pxLink->pxNextFreeBlock == NULL ) ||
				( (uint32_t) pxLink->pxNextFreeBlock == DMA_BUFFER_MARK) )
#else
			if( pxLink->pxNextFreeBlock == NULL )
#endif
			{
				/* The block is being returned to the heap - it is no longer
				allocated. */
				pxLink->xBlockSize &= ~_xBlockAllocatedBit;

				vTaskSuspendAll();
				{
					/* Add this block to the list of free blocks. */
					xFreeBytesRemaining += pxLink->xBlockSize;

#ifdef CONFIG_MEM_HEAP_PERIOD_TIME_DEBUG
					_xFreeBytes += pxLink->xBlockSize;
#endif

					traceFREE( pv, pxLink->xBlockSize );
#if( configUSE_MALLOC_DEBUG == 1 )
                    pxLink->xOwner = NULL;
                    (void ) xPortRemoveFromAllocList( pxLink );
#endif

#ifdef CONFIG_SUPPORT_DMA_DYNAMIC_ALLOC
					if ( ((uint32_t)pxLink->pxNextFreeBlock == DMA_BUFFER_MARK ) )
					{
						xDMABuffer = pdTRUE;
						pxLink->pxNextFreeBlock = NULL;
					}

					prvInsertBlockIntoFreeListWithDMA( ( ( BlockLink_t * ) pxLink ), xDMABuffer );
#else
					prvInsertBlockIntoFreeList( ( ( BlockLink_t * ) pxLink ) );
#endif
				}
#if( configUSE_MALLOC_DEBUG == 1 )
                vPortUpdateFreeBlockList();
#endif
				( void ) xTaskResumeAll();
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}
}
/*-----------------------------------------------------------*/

void p_prvInsertBlockIntoFreeList( BlockLink_t *pxBlockToInsert )
{
BlockLink_t *pxIterator;
uint8_t *puc;

	/* Iterate through the list until a block is found that has a higher address
	than the block being inserted. */
	for( pxIterator = &_xStart; pxIterator->pxNextFreeBlock < pxBlockToInsert; pxIterator = pxIterator->pxNextFreeBlock )
	{
		/* Nothing to do here, just iterate to the right position. */
	}

	/* Do the block being inserted, and the block it is being inserted after
	make a contiguous block of memory? */
	puc = ( uint8_t * ) pxIterator;
	if( ( puc + pxIterator->xBlockSize ) == ( uint8_t * ) pxBlockToInsert )
	{
		pxIterator->xBlockSize += pxBlockToInsert->xBlockSize;
		pxBlockToInsert = pxIterator;
	}
	else
	{
		mtCOVERAGE_TEST_MARKER();
	}

	/* Do the block being inserted, and the block it is being inserted before
	make a contiguous block of memory? */
	puc = ( uint8_t * ) pxBlockToInsert;
	if( ( puc + pxBlockToInsert->xBlockSize ) == ( uint8_t * ) pxIterator->pxNextFreeBlock )
	{
		if( pxIterator->pxNextFreeBlock != _pxEnd )
		{
			/* Form one big block from the two blocks. */
			pxBlockToInsert->xBlockSize += pxIterator->pxNextFreeBlock->xBlockSize;
			pxBlockToInsert->pxNextFreeBlock = pxIterator->pxNextFreeBlock->pxNextFreeBlock;
		}
		else
		{
			pxBlockToInsert->pxNextFreeBlock = _pxEnd;
		}
	}
	else
	{
		pxBlockToInsert->pxNextFreeBlock = pxIterator->pxNextFreeBlock;
	}

	/* If the block being inserted plugged a gab, so was merged with the block
	before and the block after, then it's pxNextFreeBlock pointer will have
	already been set, and should not be set here as that would make it point
	to itself. */
	if( pxIterator != pxBlockToInsert )
	{
		pxIterator->pxNextFreeBlock = pxBlockToInsert;
	}
	else
	{
		mtCOVERAGE_TEST_MARKER();
	}
}
/*-----------------------------------------------------------*/

void p_vPortDefineHeapRegions( const HeapRegion_t * const pxHeapRegions );
extern void (*vPortDefineHeapRegions)( const HeapRegion_t * const pxHeapRegions );
PATCH(vPortDefineHeapRegions, &vPortDefineHeapRegions, &p_vPortDefineHeapRegions);

void p_vPortDefineHeapRegions( const HeapRegion_t * const pxHeapRegions )
{
BlockLink_t *pxFirstFreeBlockInRegion = NULL, *pxPreviousFreeBlock;
size_t xAlignedHeap;
size_t xTotalRegionSize, xTotalHeapSize = 0;
BaseType_t xDefinedRegions = 0;
size_t xAddress;
const HeapRegion_t *pxHeapRegion;

	/* Can only call once! */
	configASSERT( _pxEnd == NULL );

	pxHeapRegion = &( pxHeapRegions[ xDefinedRegions ] );

	while( pxHeapRegion->xSizeInBytes > 0 )
	{
		xTotalRegionSize = pxHeapRegion->xSizeInBytes;

		/* Ensure the heap region starts on a correctly aligned boundary. */
		xAddress = ( size_t ) pxHeapRegion->pucStartAddress;
		if( ( xAddress & portBYTE_ALIGNMENT_MASK ) != 0 )
		{
			xAddress += ( portBYTE_ALIGNMENT - 1 );
			xAddress &= ~portBYTE_ALIGNMENT_MASK;

			/* Adjust the size for the bytes lost to alignment. */
			xTotalRegionSize -= xAddress - ( size_t ) pxHeapRegion->pucStartAddress;
		}

		xAlignedHeap = xAddress;

		/* Set _xStart if it has not already been set. */
		if( xDefinedRegions == 0 )
		{
			/* _xStart is used to hold a pointer to the first item in the list of
			free blocks.  The void cast is used to prevent compiler warnings. */
			_xStart.pxNextFreeBlock = ( BlockLink_t * ) xAlignedHeap;
			_xStart.xBlockSize = ( size_t ) 0;
		}
		else
		{
			/* Should only get here if one region has already been added to the
			heap. */
			configASSERT( _pxEnd != NULL );

			/* Check blocks are passed in with increasing start addresses. */
			configASSERT( xAddress > ( size_t ) _pxEnd );
		}

		/* Remember the location of the end marker in the previous region, if
		any. */
		pxPreviousFreeBlock = _pxEnd;

		/* _pxEnd is used to mark the end of the list of free blocks and is
		inserted at the end of the region space. */
		xAddress = xAlignedHeap + xTotalRegionSize;
		xAddress -= xHeapStructSize;
		xAddress &= ~portBYTE_ALIGNMENT_MASK;
		_pxEnd = ( BlockLink_t * ) xAddress;
		_pxEnd->xBlockSize = 0;
		_pxEnd->pxNextFreeBlock = NULL;

		/* To start with there is a single free block in this region that is
		sized to take up the entire heap region minus the space taken by the
		free block structure. */
		pxFirstFreeBlockInRegion = ( BlockLink_t * ) xAlignedHeap;
		pxFirstFreeBlockInRegion->xBlockSize = xAddress - ( size_t ) pxFirstFreeBlockInRegion;
		pxFirstFreeBlockInRegion->pxNextFreeBlock = _pxEnd;

		/* If this is not the first region that makes up the entire heap space
		then link the previous region to this region. */
		if( pxPreviousFreeBlock != NULL )
		{
			pxPreviousFreeBlock->pxNextFreeBlock = pxFirstFreeBlockInRegion;
		}

		xTotalHeapSize += pxFirstFreeBlockInRegion->xBlockSize;

		/* Move onto the next HeapRegion_t structure. */
		xDefinedRegions++;
		pxHeapRegion = &( pxHeapRegions[ xDefinedRegions ] );
	}

	xMinimumEverFreeBytesRemaining = xTotalHeapSize;
	xFreeBytesRemaining = xTotalHeapSize;

	/* Check something was actually defined before it is accessed. */
	configASSERT( xTotalHeapSize );

	/* Work out the position of the top bit in a size_t variable. */
	_xBlockAllocatedBit = ( ( size_t ) 1 ) << ( ( sizeof( size_t ) * heapBITS_PER_BYTE ) - 1 );

#if( configUSE_MALLOC_DEBUG == 1 )

    ram[0].vma = __data_start;
    ram[0].size = ( __data_end - __data_start );
    ram[1].vma = __bss_start;
    ram[1].size = ( __bss_end - __bss_start );
    ram[2].vma = __heap_start;
    ram[2].size = ( __heap_end - __heap_start );
#ifdef CONFIG_N22_ONLY
    ram[3].vma = __heapext1_start;
    ram[3].size = ( __heapext1_end - __heapext1_start );
    ram[4].vma = __heapext2_start;
    ram[4].size = ( __heapext2_end - __heapext2_start );
#endif

#endif
}

#if( configUSE_MALLOC_DEBUG == 1 )
/* Additional functions to scan memory (buffer overflow, memory leaks). */

#define GET_BUFFER_ADDRESS(x) ( uint32_t * ) ( ( uint32_t ) x + xHeapStructSize )
#define HEAD_CANARY(x)        (x->ulHeadCanary)
#define TAIL_CANARY(x)      * ( uint32_t * ) ( ( uint32_t ) x + ( x->xBlockSize & ~_xBlockAllocatedBit ) - 4 )

void OPTIMIZE_FAST vPortAddToAllocList( BlockLink_t *pxLink )
{
    uint32_t ulPos;

    HEAD_CANARY(pxLink) = HEAD_CANARY_PATTERN;
    TAIL_CANARY(pxLink) = TAIL_CANARY_PATTERN;

	for( ulPos = 0; ulPos < ARRAY_SIZE( pxAllocList ); ulPos++ )
    {
        /* Find the first empty slot and take it. */
        if( pxAllocList[ ulPos ] == NULL )
        {
            pxAllocList[ ulPos ] = pxLink;
            break;
        }
    }

    vPortUpdateFreeBlockList();
}

BaseType_t OPTIMIZE_FAST xPortRemoveFromAllocList( BlockLink_t *pxLink )
{
    uint32_t ulPos;

    for( ulPos = 0; ulPos < ARRAY_SIZE( pxAllocList ); ulPos++ )
    {
        if( pxAllocList[ ulPos ] == pxLink )
        {
            pxAllocList[ ulPos ] = NULL;
            return pdPASS;
        }
    }

    return pdFAIL;
}

void OPTIMIZE_FAST vPortUpdateFreeBlockList( void )
{
    BlockLink_t *pxIterator;

    for( pxIterator = &_xStart; pxIterator != _pxEnd; pxIterator = pxIterator->pxNextFreeBlock )
    {
        HEAD_CANARY( pxIterator ) = HEAD_CANARY_PATTERN;
    }

#ifdef CONFIG_SUPPORT_DMA_DYNAMIC_ALLOC

	if ( _pxEnd != pxEndDMA )
	{
		for( pxIterator = &xStartDMA; pxIterator != pxEndDMA; pxIterator = pxIterator->pxNextFreeBlock )
		{
			HEAD_CANARY( pxIterator ) = HEAD_CANARY_PATTERN;
		}
	}

#endif
}

bool OPTIMIZE_FAST vPortIsAlive( TaskHandle_t task )
{
	uint32_t i, count;
	TaskStatus_t *tasks;
	bool alive = false;

    count = uxTaskGetNumberOfTasks();
#ifdef CONFIG_MEM_HEAP_DEBUG
	tasks = pvPortMallocDbg( count * sizeof(TaskStatus_t), __func__ );
#else
	tasks = pvPortMalloc( count * sizeof(TaskStatus_t) );
#endif
    if( tasks != NULL )
	{
      count = uxTaskGetSystemState( tasks, count, NULL );

      for( i = 0U; i < count; i++ )
	  {
		  if( tasks[i].xHandle == task )
		  {
			  alive = true;
			  break;
		  }
      }
    }

	vPortFree(tasks);

	return alive;
}

void OPTIMIZE_FAST vPortCheckIntegrity( void )
{
    BlockLink_t *pxIterator;
    uint32_t ulPos;
    TaskStatus_t xTaskStatus;
    uint32_t ulBufAddr;
    size_t xBufSize;
    uint8_t ucCheckHead;
    uint8_t ucCheckTail;
    BlockLink_t *pxBlockLink;
	bool alive = true;

    /* Copy into the local backup. */
    vTaskEnterCritical();
    memcpy( pxAllocListCopy, pxAllocList, ARRAY_SIZE( pxAllocListCopy ) * sizeof(BlockLink_t *) );
    vTaskExitCritical();

    /* Check free blocks. */
    for( pxIterator = &_xStart; pxIterator != _pxEnd; pxIterator = pxIterator->pxNextFreeBlock )
    {
        configASSERT( HEAD_CANARY( pxIterator ) == HEAD_CANARY_PATTERN );
    }

#ifdef CONFIG_SUPPORT_DMA_DYNAMIC_ALLOC

	if ( _pxEnd != pxEndDMA )
	{
		for( pxIterator = &xStartDMA; pxIterator != pxEndDMA; pxIterator = pxIterator->pxNextFreeBlock )
		{
			configASSERT( HEAD_CANARY( pxIterator ) == HEAD_CANARY_PATTERN );
		}
	}
#endif

    /* Check allocated blocks. */
    for( ulPos = 0; ulPos < ARRAY_SIZE( pxAllocListCopy ); ulPos++ )
    {
        pxBlockLink = pxAllocListCopy[ulPos];

        if( pxBlockLink == NULL )
        {
            continue;
        }

        ucCheckHead = ( HEAD_CANARY( pxBlockLink ) == HEAD_CANARY_PATTERN ? 1 : 0 );
        ucCheckTail = ( TAIL_CANARY( pxBlockLink ) == TAIL_CANARY_PATTERN ? 1 : 0 );
        if( ucCheckHead == 0 || ucCheckTail == 0 )
        {
            printf( "Detected buffer overflow at %s.\n", ucCheckHead ? "tail" : "head" );
            if( pxBlockLink->xOwner )
            {
				alive = vPortIsAlive( pxBlockLink->xOwner );
				if( alive )
                {
                	vTaskGetInfo( pxBlockLink->xOwner, &xTaskStatus, 0, 0 );
                }
                ulBufAddr = ( uint32_t ) pxBlockLink + xHeapStructSize;
                xBufSize =  pxBlockLink->xWantedSize;
                printf( "Block owner %s address %lx size %lu\n",
						alive == false ? "(deceased)" : xTaskStatus.pcTaskName,
						ulBufAddr, xBufSize );
            }
            configASSERT( 0 );
        }
    }
    printf("Okay.\n");
}

static uint32_t ulAllocAddress;

StackType_t *pxSearchInStack( TaskHandle_t xTask)
{
    TaskStatus_t xTaskStatus;
    StackType_t *pxStack;
    uint8_t ucFound = 0;

    /* Only support a stack growing up for now.
     * Refer to xTaskCreate.
     */
    configASSERT( portSTACK_GROWTH < 0 );

    /* Get the task information. */
    vTaskGetInfo(xTask, &xTaskStatus, 0, 0);

    for( pxStack = ( StackType_t *) xTask; pxStack > xTaskStatus.pxStackBase; pxStack-- )
    {
        if( *pxStack == ulAllocAddress)
        {
            ucFound = 1;
            break;
        }
    }

    return ( ucFound ? pxStack : NULL );
}

#ifdef CONFIG_MEM_HEAP_DEBUG

static void vCopyName(char *pcDest, const char *pcSrc, uint32_t ulDestSz)
{
	uint32_t ulPos;
	const char *pc;

	memset(pcDest, 0, ulDestSz);

	for ( ulPos = 0, pc = pcSrc; ulPos < ulDestSz; ulPos++, pc++ )
	{
		if (isalnum( ( int ) *pc ) || ( *pc == '_' ) || ( *pc == ' ' ) || ( *pc == '-' ) )
		{
			pcDest[ulPos] = *pc;
		}
		else
		{
			break;
		}
	}
}

#endif

void OPTIMIZE_FAST vPortMemoryScan( void )
{
    uint32_t ulPos;
    uint32_t ulIndex;
    TaskHandle_t xTask;
    TaskStatus_t xTaskStatus;
    size_t xBufSize;
    BlockLink_t *pxBlockLink;
	char xTaskName[configMAX_TASK_NAME_LEN];
#ifdef CONFIG_MEM_HEAP_DEBUG
	char xFuncName[CONFIG_MEM_HEAP_DEBUG_FUNCNAMELEN];
#endif
#ifdef CONFIG_MEM_HEAP_SEARCH_REF
    uint8_t ucFound; /* 0 : not found, 1 : in stack, 2 : data, 3 : bss, 4-6 : heap  */
    const char *location[] = { "nowhere", "stack", "data", "bss", "heap", "heapext1", "heapext2" };
	uint32_t *pulAddr, *pulEnd;
#endif
	bool alive = true;

    vTaskEnterCritical();

    /* Copy into the local backup. */
    memcpy( pxAllocListCopy, pxAllocList, ARRAY_SIZE( pxAllocListCopy ) * sizeof(BlockLink_t *) );

    /* Cycle through pointer array until null pointer is found. */
    for( ulPos = 0, ulIndex = 0; ulPos < ARRAY_SIZE( pxAllocListCopy ); ulPos++ )
    {
        pxBlockLink = pxAllocListCopy[ulPos];

        if( pxBlockLink == NULL )
        {
            continue;
        }

        /* Get address of allocated buffer that will be searched in the memory. */
        ulAllocAddress = xHeapStructSize + ( uint32_t ) pxBlockLink;
        /* Get buffer owner. */
        xTask = ( TaskHandle_t ) pxBlockLink->xOwner;
        if( xTask )
        {
            alive = vPortIsAlive( xTask );
            if( alive )
            {
                /* Get the task information. */
                vTaskGetInfo(xTask, &xTaskStatus, 0, 0);
                strncpy( xTaskName, xTaskStatus.pcTaskName, ( sizeof(xTaskName) - 1 ) );
            }
            else
            {
               strncpy( xTaskName, "(deceased)", ( sizeof(xTaskName) - 1 ) );
            }
        }

#ifdef CONFIG_MEM_HEAP_SEARCH_REF
        ucFound = 0;
        /* Only check if there is buffer owner who is alive. */
        if( xTask != NULL && alive )
        {
            if( ( pulAddr = pxSearchInStack( xTask ) ) != NULL )
            {
				ucFound = 1;
            }
			else
            {
                /* Scan defined memory regions if we still don't have reference. */
                for (int i = 0; i < 5; i++) {
                    /* Calculate start address. */
                    pulAddr = ( uint32_t * ) ram[i].vma;
                    /* Calculate end address. */
                    pulEnd = ( uint32_t * ) ( ( uint32_t ) ram[i].vma + ram[i].size );
                    /* Scan memory. */
                    while( pulAddr < pulEnd )
                    {
						/* Let's skip this obvious reference.
						 */
						if( ( i == 1 ) && ( pulAddr == &ulAllocAddress ) )
						{
							pulAddr++;
							continue;
						}
                        /* Check if we have reference pointers. */
                        if( ( *pulAddr == ulAllocAddress ) )
                        {
                            ucFound = 2 + i;
                            break;
                        }
                        pulAddr++;
                    }
                    if( pulAddr < pulEnd )
                    {
                        break;
                    }
                }
            }
        }
#endif

        xBufSize = pxBlockLink->xWantedSize;

#ifdef CONFIG_MEM_HEAP_DEBUG

		vCopyName( xFuncName, pxBlockLink->xFuncName, ( sizeof(xFuncName) - 1 ) );

#ifdef CONFIG_MEM_HEAP_SEARCH_REF
        printf("[%4lu] "FMT""FMTF" address %8lx size %5lu at %8s %p\n",
                ulIndex, xTask ? xTaskName : "(none)", xFuncName, ulAllocAddress,
				xBufSize, location[ ucFound ], ucFound ? pulAddr : NULL);
#else /* CONFIG_MEM_HEAP_SEARCH_REF */
        printf("[%4lu] "FMT""FMTF" address %8lx size %5lu\n", ulIndex,
				xTask ? xTaskName : "(none)", xFuncName, ulAllocAddress, xBufSize);
#endif

#else
#ifdef CONFIG_MEM_HEAP_SEARCH_REF
        printf("[%4lu] "FMT" address %8lx size %5lu at %8s %p\n",
                ulIndex, xTask ? xTaskName : "(none)", ulAllocAddress,
                xBufSize, location[ ucFound ], ucFound ? pulAddr : NULL);
#else
        printf("[%4lu] "FMT" address %8lx size %5lu\n", ulIndex,
				xTask ? xTaskName : "(none)", ulAllocAddress, xBufSize);
#endif
#endif
		ulIndex++;
    }

    vTaskExitCritical();
}
#endif
