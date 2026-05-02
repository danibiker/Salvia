#ifndef __GPU_H__
#define __GPU_H__

#include "../plugins/xbox_soft/peops_prof.h"

#ifdef __cplusplus
extern "C"
{
#endif
	/* GPU helper thread lifecycle.  Llamados desde libretro_core.cpp
	 * en emu_setup / emu_teardown.  Idempotentes y safe contra calls
	 * repetidos.  Implementacion en gpu.c, sub-seccion "GPU THREADING
	 * SUBSYSTEM". */
	void gpuDmaThreadInit(void);
	void gpuDmaThreadShutdown(void);

	/* Deprecated: el lifecycle del thread esta gestionado SOLO por
	 * Init/Shutdown.  Esta funcion era fuente del bug que reseteaba
	 * el flag de salida tras Init.  Se mantiene como no-op para no
	 * romper compatibilidad de fuente (libretro_core.cpp aun la llama). */
	void gpuThreadEnable(int enable);

	/* Per-frame counter de QueryPerformanceCounter ticks que la CPU
	 * emulada paso bloqueada en ring_drain esperando al GPU helper
	 * thread.  retro_run lo resetea a 0 antes de psxCpu->Execute() y
	 * lo lee despues para:
	 *   (a) Auto-frameskip: skipear solo si gpu_wait domina el exceso.
	 *   (b) Dump [PERF]: desglose CPU vs GPU del exec time. */
	extern volatile uint64_t gpu_wait_ticks;

    void gpuWriteData(u32 data);
	u32  gpuReadData(void);	

/////////////////////////////////////////////updatelace
	void gpuUpdateLace();
/////////////////////////////////////////////

////////////////////////////////////////////try again
void gpuWriteStatus(u32 data);
//u32 gpuReadStatus(void);
/////////////////////////////////////////
	void gpuWriteDataMem(uint32_t *, int);
	void gpuReadDataMem(uint32_t *, int);

	void psxDma2(u32 madr, u32 bcr, u32 chcr);

#define gpuInterrupt() \
HW_DMA2_CHCR_2 &= SWAP32(~0x01000000); \
DMA_INTERRUPT_2(2);

void CALLBACK GPUbusy( int ticks );

#ifdef __cplusplus
}
#endif

#endif
