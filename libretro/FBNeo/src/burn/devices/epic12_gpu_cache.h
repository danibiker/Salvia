// Persistent source-page atlas bookkeeping shared by Xenos and its CPU model.
// A batch pins every resident page before allocating any misses, so choosing a
// new source page never evicts data needed later in the same command list.
#ifndef FBNEO_EPIC12_GPU_CACHE_H
#define FBNEO_EPIC12_GPU_CACHE_H
#include <limits.h>
#include <string.h>

#ifndef EPIC12_VRAM_PAGES
#define EPIC12_VRAM_PAGES 2176
#endif

struct Epic12GpuPageCache {
	enum { MAX_SLOTS=256, VRAM_PAGES=EPIC12_VRAM_PAGES };
	int capacity;
	int pages[MAX_SLOTS];
	unsigned lastUse[MAX_SLOTS],clock;
	bool pinned[MAX_SLOTS];
	short pageToSlot[VRAM_PAGES];

	Epic12GpuPageCache() : capacity(128) { clear(); }

	void clear()
	{
		for(int i=0;i<MAX_SLOTS;++i) {
			pages[i]=-1; lastUse[i]=0; pinned[i]=false;
		}
		for(int p=0;p<VRAM_PAGES;++p) pageToSlot[p]=-1;
		clock=0;
	}

	void reset(int slots)
	{
		// Native capacities are 128/256. Smaller sizes also allow exhaustive
		// tests of replacement and pinning without large texture allocations.
		capacity=slots>0 && slots<=MAX_SLOTS?slots:128;
		clear();
	}

	void advance_clock()
	{
		if(clock==UINT_MAX) {
			// Preserve age order, including ties, before an unsigned wrap. This
			// slow path occurs only after over four billion submitted batches.
			unsigned ranks[MAX_SLOTS];
			for(int i=0;i<capacity;++i) {
				ranks[i]=0;
				if(pages[i]<0) continue;
				ranks[i]=1;
				for(int j=0;j<capacity;++j)
					if(pages[j]>=0 && lastUse[j]<lastUse[i]) ++ranks[i];
			}
			for(int i=0;i<capacity;++i) lastUse[i]=ranks[i];
			clock=(unsigned)capacity;
		}
		++clock;
	}

	unsigned begin_batch(const bool needed[VRAM_PAGES],int slots[VRAM_PAGES])
	{
		advance_clock();
		for(int p=0;p<VRAM_PAGES;++p) slots[p]=-1;
		unsigned hits=0;
		for(int i=0;i<capacity;++i) {
			int page=pages[i];
			pinned[i]=page>=0 && needed[page];
			if(pinned[i]) { slots[page]=i; lastUse[i]=clock; ++hits; }
		}
		return hits;
	}

	// The caller uploads pixels into the returned slot before rendering.
	// If that upload fails and the GPU remains usable, clear() must discard
	// these new tags before another batch can use the atlas.
	int allocate(int page)
	{
		if(page<0 || page>=VRAM_PAGES) return -1;
		if(pageToSlot[page]>=0) {
			int s=pageToSlot[page];
			pinned[s]=true; lastUse[s]=clock; return s;
		}
		int empty=-1,oldest=-1;
		for(int i=0;i<capacity;++i) {
			if(pinned[i]) continue;
			if(pages[i]<0) { if(empty<0) empty=i; }
			else if(oldest<0 || lastUse[i]<lastUse[oldest]) oldest=i;
		}
		int slot=empty>=0?empty:oldest;
		if(slot<0) return -1;
		if(pages[slot]>=0) pageToSlot[pages[slot]]=-1;
		pages[slot]=page; pinned[slot]=true; lastUse[slot]=clock;
		pageToSlot[page]=(short)slot;
		return slot;
	}

	// Inclusive VRAM pixel bounds. Empty and wholly outside rectangles do
	// not evict pages; the caller handles wrapped writes with clear().
	void invalidate(int x0,int y0,int x1,int y1)
	{
		if(x1<x0 || y1<y0) return;
		int px0=x0>>7, px1=x1>>7;
		int py0=y0>>7, py1=y1>>7;
		if(px0<0) px0=0; if(px1>63) px1=63;
		if(py0<0) py0=0; if(py1>33) py1=33;
		int count=(px1-px0+1)*(py1-py0+1);
		if(count<=16) {
			for(int py=py0;py<=py1;++py) {
				int row=py<<6;
				for(int px=px0;px<=px1;++px) {
					int p=row|px;
					if(p<VRAM_PAGES) {
						int s=pageToSlot[p];
						if(s>=0) {
							pageToSlot[p]=-1;
							pages[s]=-1; lastUse[s]=0; pinned[s]=false;
						}
					}
				}
			}
		} else {
			for(int i=0;i<capacity;++i) if(pages[i]>=0) {
				int p = pages[i];
				int px = p & 63, py = p >> 6;
				if(px >= px0 && px <= px1 && py >= py0 && py <= py1) {
					pageToSlot[p]=-1; pages[i]=-1; lastUse[i]=0; pinned[i]=false;
				}
			}
		}
	}
};
#endif
