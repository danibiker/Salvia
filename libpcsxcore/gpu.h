#ifndef __GPU_H__
#define __GPU_H__

#ifdef __cplusplus
extern "C"
{
#endif
	// New !
	void gpuDmaThreadInit();
	void gpuDmaThreadShutdown();
	void gpuThreadEnable(int enable);

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
