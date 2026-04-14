/*  Copyright (c) 2010, shalma.
 *  Portions Copyright (c) 2002, Pete Bernert.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1307 USA
 */

#include "psxhw.h"
#include "gpu.h"
#include "psxdma.h"

extern unsigned int hSyncCount;
/////////////////////////////////////////////////
#define TW_RING_MAX_COUNT (128*1024)
#define tw_ring_count(str) ((u32)labs((str[1]%TW_RING_MAX_COUNT)-(str[0]%TW_RING_MAX_COUNT)))
#define tw_read_idx tw_idx[0]
#define tw_write_idx tw_idx[1]
/////////////////////////////////////////////////


#define GPUSTATUS_ODDLINES            0x80000000
#define GPUSTATUS_DMABITS             0x60000000 // Two bits
#define GPUSTATUS_READYFORCOMMANDS    0x10000000
#define GPUSTATUS_READYFORVRAM        0x08000000
#define GPUSTATUS_IDLE                0x04000000

#define GPUSTATUS_DISPLAYDISABLED     0x00800000
#define GPUSTATUS_INTERLACED          0x00400000
#define GPUSTATUS_RGB24               0x00200000
#define GPUSTATUS_PAL                 0x00100000
#define GPUSTATUS_DOUBLEHEIGHT        0x00080000
#define GPUSTATUS_WIDTHBITS           0x00070000 // Three bits
#define GPUSTATUS_MASKENABLED         0x00001000
#define GPUSTATUS_MASKDRAWN           0x00000800
#define GPUSTATUS_DRAWINGALLOWED      0x00000400
#define GPUSTATUS_DITHER              0x00000200

static volatile	uint32_t gpu_thread_running = 0;
static volatile uint32_t gpu_thread_exit = 1;
static volatile uint32_t dma_addr;
static __declspec(align(128)) uint32_t tw_ring[TW_RING_MAX_COUNT];
static volatile  __declspec(align(128))  uint64_t tw_idx[2] = {0,0};

// Taken from PEOPS SOFTGPU
u32 lUsedAddr[3];

#define GPUDMA_INT(eCycle) { \
	psxRegs.interrupt |= (1 << PSXINT_GPUDMA); \
	psxRegs.intCycle[PSXINT_GPUDMA].cycle = eCycle; \
	psxRegs.intCycle[PSXINT_GPUDMA].sCycle = psxRegs.cycle; \
}

__inline boolean CheckForEndlessLoop(u32 laddr) {
	if (laddr == lUsedAddr[1]) return TRUE;
	if (laddr == lUsedAddr[2]) return TRUE;

	if (laddr < lUsedAddr[0]) lUsedAddr[1] = laddr;
	else lUsedAddr[2] = laddr;

	lUsedAddr[0] = laddr;

	return FALSE;
}

static u32 gpuDmaChainSize(u32 addr) {
	u32 size;
	u32 DMACommandCounter = 0;
	lUsedAddr[0] = lUsedAddr[1] = lUsedAddr[2] = 0xffffff;

	// initial linked list ptr (word)
	size = 1;

	do {
		addr &= 0x1ffffc;

		if (DMACommandCounter++ > 2000000) break;
		if (CheckForEndlessLoop(addr)) break;


		// # 32-bit blocks to transfer
		size += psxMu8_2( addr + 3 );

		
		// next 32-bit pointer
		addr = psxMu32_2( addr & ~0x3 ) & 0xffffff;
		size += 1;
	} while (addr != 0xffffff);

	
	return size;
}

__inline static void WaitForGpuThread() {
    while(gpu_thread_running||tw_ring_count(tw_idx)>0) {
		YieldProcessor(); // or r31, r31, r31
	}

	// High priority
	__asm{
		or r3, r3, r3
	};
}

static void gpuThread() {
	uint64_t  __declspec(align(128)) lidx[2];
	__vector4 vt;

	while(!gpu_thread_exit) {

		// atomic ...
		vt = __lvx((void*)tw_idx, 0);
		__stvx(vt, lidx, 0);

		if(tw_ring_count(lidx)!=0)
        {
            uint32_t ri=lidx[0]%TW_RING_MAX_COUNT;
            uint32_t rc=tw_ring_count(lidx);
            
            uint32_t chunk=min(rc,(TW_RING_MAX_COUNT-ri));
            uint32_t * chunk_start=&tw_ring[ri];

            gpu_thread_running = 1;

			GPU_writeDataMem(chunk_start, chunk);
            
            tw_read_idx+=chunk;
            
            gpu_thread_running = 0;
        }
	}

	// Exit thread
	ExitThread(0);
}


void threadedgpuWriteData(uint32_t * pMem, int size) {
	u32 * lda=pMem;
    u32 wi=tw_write_idx;

	if (gpu_thread_exit) {
		GPU_writeDataMem(pMem, size);
		return;
	}


    while(size>TW_RING_MAX_COUNT-tw_ring_count(tw_idx)) 
		YieldProcessor(); // or r31, r31, r31
    
    while((lda-pMem)<size)
    {
		// Copy data ...
        u32 * d =&tw_ring[wi%TW_RING_MAX_COUNT];

		*d=*lda;

        ++wi;
        ++lda;
    }

    tw_write_idx+=size;
}

////////////////////////////////////////////////////////////gpu.c

// Classic call !
void gpuDmaChain(uint32_t addr)
{
	uint32_t dmaMem;
	uint32_t * baseAddrL;
	unsigned char * baseAddrB;
	short count;
	unsigned int DMACommandCounter = 0;

	 baseAddrL = (u32 *)psxM_2;


	lUsedAddr[0]=lUsedAddr[1]=lUsedAddr[2]=0xffffff;

	baseAddrB = (unsigned char*) baseAddrL;

	do
	{
		addr&=0x1FFFFC;
		if(DMACommandCounter++ > 2000000) break;
		if(CheckForEndlessLoop(addr)) break;

		count = baseAddrB[addr+3];

		dmaMem=addr+4;

		if(count>0) {
			// Call threaded gpu func
			threadedgpuWriteData(&baseAddrL[dmaMem>>2],count);
		}

		addr = psxMu32_2( addr & ~0x3 ) & 0xffffff;
	}
	while (addr != 0xffffff);
}

void gpuReadDataMem(uint32_t * addr, int size) {

	if(!gpu_thread_exit) {
		WaitForGpuThread();
	} 

	GPU_readDataMem(addr, size);
}

void gpuWriteDataMem(uint32_t * pMem, int size) {

	if(!gpu_thread_exit) {
		WaitForGpuThread();
	} 
	GPU_writeDataMem(pMem, size);

}

//////////////////////////////////////////////////////////psxhw.c
u32 gpuReadData(void) {

	if(!gpu_thread_exit) {
		WaitForGpuThread();
	} 

	return GPU_readData();
}

void gpuWriteData(u32 data) {

	if(!gpu_thread_exit) {
		WaitForGpuThread();
	} 

	GPU_writeData(data);
}
//////////////////////////////////////////////////////////////////////try again

/*
u32 gpuReadStatus(void) {
	u32 hard;

//	if(!gpu_thread_exit) {
//       WaitForGpuThread();
//	} 
	
	// GPU plugin
	hard = GPU_readStatus();

	return hard;
}
*/

void gpuWriteStatus(u32 data) {

	if(!gpu_thread_exit) {
		WaitForGpuThread();
	} 

	GPU_writeStatus(data);
}
//////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////updatelace
void gpuUpdateLace() {

	if(!gpu_thread_exit) {
		WaitForGpuThread();
	} 

	GPU_updateLace();
}
//////////////////////////////////////////////////////////////

static HANDLE gpuHandle = NULL;

void gpuDmaThreadShutdown() {

	// ask to shutdown thread
	gpu_thread_exit = 1;

	// wait for thread exit ...
	WaitForSingleObject(gpuHandle, INFINITE);
	
	// close thread handle
	CloseHandle(gpuHandle);
	gpuHandle = NULL;

}

void gpuDmaThreadInit() {

	// if thread running Shutdown it ...
	if (gpuHandle) {
		gpuDmaThreadShutdown();
	}

	// Reset thread variables
	gpu_thread_exit = 0;
	tw_write_idx = 0;
	tw_read_idx = 0;

	memset(tw_ring, 0, TW_RING_MAX_COUNT * sizeof(int));

	// Create gpu thread on cpu 2
	gpuHandle = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)gpuThread, NULL, CREATE_SUSPENDED, NULL);

	XSetThreadProcessor(gpuHandle, 2);

	ResumeThread(gpuHandle);

}

void gpuThreadEnable(int enable) {
	gpu_thread_exit = !enable;
}

void psxDma2(u32 madr, u32 bcr, u32 chcr) { // GPU
	u32 *ptr;
	u32 size;

	switch (chcr) {
		case 0x01000200: // vram2mem
#ifdef PSXDMA_LOG
			PSXDMA_LOG("*** DMA2 GPU - vram2mem *** %lx addr = %lx size = %lx\n", chcr, madr, bcr);
#endif
	ptr = (u32 *)PSXM_2(madr);
			if (ptr == NULL) {
#ifdef CPU_LOG
				CPU_LOG("*** DMA2 GPU - vram2mem *** NULL Pointer!!!\n");
#endif
				break;
			}
			// BA blocks * BS words (word = 32-bits)
			size = (bcr >> 16) * (bcr & 0xffff);
//			gpuReadDataMem(ptr, size);
			GPU_readDataMem(ptr, size);////////////////////////////////
						
			psxCpu->Clear(madr, size);

			// already 32-bit word size ((size * 4) / 4)
			GPUDMA_INT(size);
			return;

		case 0x01000201: // mem2vram
#ifdef PSXDMA_LOG
			PSXDMA_LOG("*** DMA 2 - GPU mem2vram *** %lx addr = %lx size = %lx\n", chcr, madr, bcr);
#endif

	ptr = (u32 *)PSXM_2(madr);
			if (ptr == NULL) {
#ifdef CPU_LOG
				CPU_LOG("*** DMA2 GPU - mem2vram *** NULL Pointer!!!\n");
#endif
				break;
			}
			// BA blocks * BS words (word = 32-bits)
			size = (bcr >> 16) * (bcr & 0xffff);
//			gpuWriteDataMem(ptr, size);
			GPU_writeDataMem(ptr, size);////////////////////////////////


			// already 32-bit word size ((size * 4) / 4)
			GPUDMA_INT(size);
			return;

		case 0x01000401: // dma chain
#ifdef PSXDMA_LOG
			PSXDMA_LOG("*** DMA 2 - GPU dma chain *** %lx addr = %lx size = %lx\n", chcr, madr, bcr);
#endif

			size = gpuDmaChainSize(madr);

	        gpuDmaChain(madr & 0x1fffff);
			
			// Tekken 3 = use 1.0 only (not 1.5x)

			// Einhander = parse linked list in pieces (todo)
			// Final Fantasy 4 = internal vram time (todo)
			// Rebel Assault 2 = parse linked list in pieces (todo)
			// Vampire Hunter D = allow edits to linked list (todo)
			GPUDMA_INT(size);
			return;

#ifdef PSXDMA_LOG
		default:
			PSXDMA_LOG("*** DMA 2 - GPU unknown *** %lx addr = %lx size = %lx\n", chcr, madr, bcr);
			break;
#endif
	}
	HW_DMA2_CHCR_2 &= SWAP32(~0x01000000);
	DMA_INTERRUPT_2(2);
}

