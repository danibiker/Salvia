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

#define GPUDMA_INT(eCycle) set_event(PSXINT_GPUDMA, eCycle)

/*
 * Historical note on two upstream fixes merged into this file:
 *
 *  1. Removed the legacy PEOPS-SOFTGPU `CheckForEndlessLoop` /
 *     `lUsedAddr[3]` heuristic that used to guard the DMA-chain parser.
 *     It tracked only two effective recent addresses and false-positived
 *     on legitimate OT access patterns — notably Soul Reaver, which
 *     splices particle-effect chunks into the ordering table post-sort,
 *     producing non-monotonic revisits that tripped the heuristic and
 *     caused every subsequent chain node to be silently dropped. The
 *     plain DMACommandCounter safety net (also used by PCSX-ReARMed) is
 *     sufficient.
 *
 *  2. End-of-linked-list terminator corrected from `addr == 0xffffff` to
 *     `addr & 0x800000`. Contrary to some documentation, the PSX GPU
 *     DMA-chain terminator is ANY pointer with bit 23 set, not the
 *     specific value 0xFF'FFFF. Soul Reaver emits terminators like
 *     0x800000 / 0x810000 / etc., which the old equality check walked
 *     straight past — reading 32-bit words of RAM past the real end
 *     into the GPU FIFO as if they were GP0 commands. Stray control
 *     commands (E3 set-drawing-area, E4, E5 set-draw-offset, E1
 *     texpage) landed there and corrupted GPU state for the rest of
 *     the frame, silently scissoring out the next chain's soul
 *     primitives. Matches PCSX-ReARMed behaviour.
 */

static u32 gpuDmaChainSize(u32 addr) {
	u32 size;
	u32 DMACommandCounter = 0;

	// initial linked list ptr (word)
	size = 1;

	do {
		addr &= 0x1ffffc;

		if (DMACommandCounter++ > 2000000) break;


		// # 32-bit blocks to transfer
		size += psxMu8_2( addr + 3 );


		// next 32-bit pointer
		addr = psxMu32_2( addr & ~0x3 ) & 0xffffff;
		size += 1;
	} while (!(addr & 0x800000));

	
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


/* ===== Soul-Reaver collapsed-quad workaround =====
 *
 * Some game(s), most visibly Soul Reaver, render small textured sprites as
 * 0x2E primitives (QuadFlatTexBlend SemiTrans, 9 words: cmd+color, V0, UV0,
 * V1, UV1, V2, UV2, V3, UV3). pcsxr-360's CPU/GTE emulation produces these
 * primitives with all four vertex words equal to the same 32-bit packed XY,
 * so the quad has zero area and nothing is drawn. The cmd, color and UVs are
 * correct — only the 4 vertices collapse.
 *
 * Workaround: scan every GP0 chunk on its way to the GPU plugin, find any
 * 0x2E primitive whose 4 vertex words are identical, and expand the corners
 * around that (correct) center using a fixed half-size of (11, 8). This is
 * the sprite size observed in no$psx for the soul effect. Other games that
 * happen to use 0x2E correctly are unaffected because their 4 vertices
 * differ.
 *
 * Toggled at runtime by `soul_reaver_quad_fix` (set from the libretro core
 * variable `pcsxr360_fix_soul_reaver_quads`). Default off so other games
 * are not perturbed.
 */
int soul_reaver_quad_fix = 0;

/* Per-call scratch — chunks come from gpuDmaChain in nodes of <=255 words,
 * but the GP0 port path can pass larger blocks. 4096 words is the same cap
 * PEOPS uses for prim assembly. */
#define SOUL_FIX_BUF_WORDS 4096
static u32 soul_fix_buf[SOUL_FIX_BUF_WORDS];

/* PSX primitive length lookup. Returns 0 for cmds we don't recognise so the
 * caller can fall back to advancing one word at a time. */
static int soul_fix_prim_len(u8 cmd)
{
	switch (cmd >> 5) {
	case 1: { /* polygon 0x20-0x3F */
		int len = (cmd & 0x08) ? ((cmd & 0x04) ? 9 : 5)
		                       : ((cmd & 0x04) ? 7 : 4);
		if (cmd & 0x10) len += (cmd & 0x08) ? 3 : 2;
		return len;
	}
	case 2: return (cmd & 0x10) ? 4 : 3;     /* line */
	case 3: return (cmd & 0x18) ? ((cmd & 0x04) ? 3 : 2)
	                            : ((cmd & 0x04) ? 4 : 3); /* sprite/rect */
	default: return 0;
	}
}

/* Walk `buf` (host-native u32s, i.e. PSX-LE bytes byte-swapped on BE) and
 * rewrite collapsed 0x2E quads. Returns the number of primitives fixed. */
static int soul_fix_chunk(u32 *buf, int size)
{
	int fixed = 0;
	int i = 0;
	while (i < size) {
		u8 cmd = ((u8 *)&buf[i])[3];   /* PSX cmd byte = byte+3 of native u32 */
		int len = soul_fix_prim_len(cmd);
		if (len == 0) { i++; continue; }
		if (i + len > size) break;

		if (cmd == 0x2E
		    && buf[i+1] == buf[i+3]
		    && buf[i+3] == buf[i+5]
		    && buf[i+5] == buf[i+7]) {
			u32 psx_v = SWAP32(buf[i+1]);
			s16 cx = (s16)(psx_v & 0xFFFF);
			s16 cy = (s16)((psx_v >> 16) & 0xFFFF);
			const s16 hw = 11;
			const s16 hh = 8;
			/* V0=BR, V1=BL, V2=TR, V3=TL — matches the UV layout no$psx
			 * shows for the soul sprite (UV0 right-top, UV3 left-bottom). */
			#define PACK_XY(x, y) \
				(((u32)((u16)((s16)(y))) << 16) | ((u32)((u16)((s16)(x)))))
			buf[i+1] = SWAP32(PACK_XY(cx + hw, cy + hh));
			buf[i+3] = SWAP32(PACK_XY(cx - hw, cy + hh));
			buf[i+5] = SWAP32(PACK_XY(cx + hw, cy - hh));
			buf[i+7] = SWAP32(PACK_XY(cx - hw, cy - hh));
			#undef PACK_XY
			fixed++;
		}
		i += len;
	}
	return fixed;
}

void threadedgpuWriteData(uint32_t * pMem, int size) {
	u32 * lda=pMem;
    u32 wi=tw_write_idx;

	/* Soul-Reaver collapsed-quad workaround: scan the chunk for 0x2E quads
	 * with all 4 vertices identical, and expand them into a sized rectangle.
	 * Only enabled when the libretro option requests it. We copy the chunk
	 * to a scratch buffer first so we never mutate PSX RAM. */
	if (soul_reaver_quad_fix && size > 0 && size <= SOUL_FIX_BUF_WORDS) {
		int j;
		for (j = 0; j < size; j++) soul_fix_buf[j] = pMem[j];
		if (soul_fix_chunk(soul_fix_buf, size) > 0) {
			pMem = soul_fix_buf;
			lda  = pMem;
		}
	}

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

	baseAddrB = (unsigned char*) baseAddrL;

	do
	{
		addr&=0x1FFFFC;
		if(DMACommandCounter++ > 2000000) break;

		count = baseAddrB[addr+3];

		dmaMem=addr+4;

		if(count>0) {
			// Call threaded gpu func
			threadedgpuWriteData(&baseAddrL[dmaMem>>2],count);
		}

		addr = psxMu32_2( addr & ~0x3 ) & 0xffffff;
	}
	while (!(addr & 0x800000));
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
	SetThreadPriority(gpuHandle, THREAD_PRIORITY_ABOVE_NORMAL);
	XSetThreadProcessor(gpuHandle, 4);

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
			/* Use the *synchronised* wrapper (gpuReadDataMem, lowercase),
			 * not the raw plugin pointer GPU_readDataMem. The wrapper
			 * waits for the GPU thread to drain its in-flight chunk
			 * before entering the plugin; otherwise both threads can be
			 * inside PEOPS_GPUreadDataMem at once, racing on its
			 * internal state (DataReadMode, VRAMRead.*, gpuDataC, ...).
			 * Tekken 3 hits this during the post-fight cinematic, where
			 * a vram2mem read fires while the thread is still processing
			 * polygon-batch commands, producing the random solid-colour
			 * block artefacts in the HUD overlay. */
			gpuReadDataMem(ptr, size);

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
			/* Use the *synchronised* wrapper (gpuWriteDataMem, lowercase),
			 * not the raw plugin pointer GPU_writeDataMem. The bypass
			 * commented out below is the actual cause of Tekken 3's
			 * random solid-colour HUD blocks: PEOPS_GPUwriteDataMem keeps
			 * a per-call multi-word command parser (gpuDataC, gpuCommand,
			 * gpuDataM[], DataWriteMode), and lets the GPU thread (core 4,
			 * draining tw_ring) and the main thread (this DMA path) both
			 * enter it concurrently. The mem2vram payload words land
			 * inside a half-parsed textured-sprite command, scrambling its
			 * UV/palette and producing the solid-colour rectangles seen in
			 * the post-fight cinematic. The wrapper drains the queue
			 * first, restoring exclusive access. */
			gpuWriteDataMem(ptr, size);


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

