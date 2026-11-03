// Exact transparent-source bounds for cached EPIC12 atlas pages.
// Metadata is generated once per page upload; sprite queries never read VRAM.
#ifndef FBNEO_EPIC12_GPU_ALPHA_H
#define FBNEO_EPIC12_GPU_ALPHA_H
#include <stdint.h>
#include "epic12_gpu_alpha_vmx.h"

#if defined(_XBOX)
#include <ppcintrinsics.h>
#endif

#if defined(EPIC12_GPU_ALPHA_PROFILE)
struct Epic12GpuAlphaStats {
    uint64_t builtPixels, summaryReads, rowReads, queryPages, trimCommands, emptyCommands;
    uint64_t cacheHits, cacheMisses;
};
static Epic12GpuAlphaStats epic12_gpu_alpha_stats;
#define EPIC12_ALPHA_COUNT(field, count) (epic12_gpu_alpha_stats.field += (count))
#else
#define EPIC12_ALPHA_COUNT(field, count) ((void)0)
#endif

// Call only for a nonzero word. Bit 31 names the leftmost source pixel.
static unsigned epic12_alpha_leading_zeros(uint32_t value)
{
#if defined(_XBOX)
    return _CountLeadingZeros((long)value);
#elif defined(__GNUC__) || defined(__clang__)
    return (unsigned)__builtin_clz(value);
#else
    unsigned count=0;
    if (!(value&0xffff0000U)) {count+=16;value<<=16;}
    if (!(value&0xff000000U)) {count+=8;value<<=8;}
    if (!(value&0xf0000000U)) {count+=4;value<<=4;}
    if (!(value&0xc0000000U)) {count+=2;value<<=2;}
    if (!(value&0x80000000U)) ++count;
    return count;
#endif
}

struct Epic12GpuAlphaPage {
    enum { SIZE=128, WORDS=4, GROUP_ROWS=8 };
    uint32_t rows[SIZE][WORDS];
    uint32_t groups[SIZE/GROUP_ROWS][WORDS];

    // Four sets of four FIFO entries. Cache hits never move entries. The 128
    // bytes per page add 32 KiB for a 256-slot atlas. Keys use only 28 bits;
    // ~0U in entry zero marks a stale table, while 0x80000000 is an invalid
    // individual entry. Lazy clearing keeps a row upload's invalidation O(1).
    mutable uint32_t cacheKeys[16], cacheBounds[16];
    Epic12GpuAlphaPage() { invalidate_cache(); }
    void invalidate_cache() const { cacheKeys[0]=~0U; }

    static unsigned cache_set(uint32_t key) {
        return ((key*0x9e3779b1U)>>30)*4;
    }

    // Build y=0..127 in order when uploading the corresponding atlas slot.
    // Calling this immediately after that row's memcpy reuses its cached data.
    void build_row(int y,const uint32_t* source)
    {
        invalidate_cache();
#if defined(EPIC12_ALPHA_VECTOR)
        if(epic12_alpha_vector_enabled && !((size_t)source&15)) {
            Epic12GpuAlphaVector vector;
            build_vector_row(y,source,vector);
            return;
        }
#endif
        for(int word=0;word<WORDS;++word) {
            uint32_t mask=0;
            for(int x=0;x<32;++x)
                mask=(mask<<1)|((source[word*32+x]>>29)&1U);
            rows[y][word]=mask;
            if (!(y&(GROUP_ROWS-1))) groups[y/GROUP_ROWS][word]=mask;
            else groups[y/GROUP_ROWS][word]|=mask;
        }
        EPIC12_ALPHA_COUNT(builtPixels,SIZE);
    }

    // source is the page's upper-left pixel; pitchWords is at least 128.
    void build(const uint32_t* source,int pitchWords)
    {
#if defined(EPIC12_ALPHA_VECTOR)
        if(epic12_alpha_vector_enabled && !((size_t)source&15) && !(pitchWords&3)) {
            Epic12GpuAlphaVector vector;
            invalidate_cache();
            for(int y=0;y<SIZE;++y)build_vector_row(y,source+y*pitchWords,vector);
            return;
        }
#endif
        for(int y=0;y<SIZE;++y) build_row(y,source+y*pitchWords);
    }

#if defined(EPIC12_ALPHA_VECTOR)
    void build_vector_row(int y,const uint32_t* source,const Epic12GpuAlphaVector& vector)
    {
        vector.row(source,rows[y]);
        for(int word=0;word<WORDS;++word) {
            if(!(y&(GROUP_ROWS-1)))groups[y/GROUP_ROWS][word]=rows[y][word];
            else groups[y/GROUP_ROWS][word]|=rows[y][word];
        }
        EPIC12_ALPHA_COUNT(builtPixels,SIZE);
    }
#endif

    uint32_t row_bits(int y,int firstWord,int lastWord,const uint32_t* masks) const
    {
        uint32_t found=0;
        for(int word=firstWord;word<=lastWord;++word) {
            found|=rows[y][word]&masks[word];
            EPIC12_ALPHA_COUNT(rowReads,1);
        }
        return found;
    }

    // Exact repeated queries reuse both nonempty and empty answers. Bounds
    // remain unchanged on an empty hit, matching the uncached query contract.
    bool bounds(int x0,int y0,int x1,int y1,int& left,int& top,int& right,int& bottom) const
    {
        EPIC12_ALPHA_COUNT(queryPages,1);
        if(cacheKeys[0]==~0U)
            for(int i=0;i<16;++i) {
                cacheKeys[i]=0x80000000U;cacheBounds[i]=0;
            }
        uint32_t key=(uint32_t)x0|((uint32_t)y0<<7)|((uint32_t)x1<<14)|((uint32_t)y1<<21);
        unsigned base=cache_set(key),entry=base;
        for(;entry<base+4;++entry) if(cacheKeys[entry]==key) break;
        if(entry<base+4) {
            EPIC12_ALPHA_COUNT(cacheHits,1);
            uint32_t packed=cacheBounds[entry];
            if(packed&0x80000000U) return false;
            left=packed&127;top=(packed>>7)&127;
            right=(packed>>14)&127;bottom=(packed>>21)&127;
            return true;
        }
        EPIC12_ALPHA_COUNT(cacheMisses,1);
        bool found=scan_bounds(x0,y0,x1,y1,left,top,right,bottom);
        uint32_t packed=found?((uint32_t)left|((uint32_t)top<<7)|
            ((uint32_t)right<<14)|((uint32_t)bottom<<21)):0x80000000U;
        for(unsigned i=base+3;i>base;--i) {
            cacheKeys[i]=cacheKeys[i-1];
            cacheBounds[i]=cacheBounds[i-1];
        }
        cacheKeys[base]=key;cacheBounds[base]=packed;
        return found;
    }

    // Inclusive page-local coordinates, guaranteed within 0..127 by caller.
    // Outputs are untouched for an empty rectangle. Complete eight-row groups
    // use four OR summaries; only boundary rows need individual row masks.
    bool scan_bounds(int x0,int y0,int x1,int y1,int& left,int& top,int& right,int& bottom) const
    {
        int firstWord=x0>>5,lastWord=x1>>5;
        uint32_t masks[WORDS]={0,0,0,0},columns[WORDS]={0,0,0,0};
        for(int word=firstWord;word<=lastWord;++word) masks[word]=~0U;
        masks[firstWord]&=~0U>>(x0&31);
        masks[lastWord]&=~0U<<(31-(x1&31));
        int firstGroup=-1,lastGroup=-1;
        for(int group=y0/GROUP_ROWS;group<=y1/GROUP_ROWS;++group) {
            int begin=group*GROUP_ROWS,end=begin+GROUP_ROWS-1;
            if(begin<y0) begin=y0;
            if(end>y1) end=y1;
            uint32_t found=0;
            if(begin==group*GROUP_ROWS && end==begin+GROUP_ROWS-1) {
                for(int word=firstWord;word<=lastWord;++word) {
                    uint32_t bits=groups[group][word]&masks[word];
                    columns[word]|=bits;found|=bits;
                    EPIC12_ALPHA_COUNT(summaryReads,1);
                }
            } else {
                for(int y=begin;y<=end;++y) for(int word=firstWord;word<=lastWord;++word) {
                    uint32_t bits=rows[y][word]&masks[word];
                    columns[word]|=bits;found|=bits;
                    EPIC12_ALPHA_COUNT(rowReads,1);
                }
            }
            if(found) {
                if(firstGroup<0) firstGroup=group;
                lastGroup=group;
            }
        }
        if(firstGroup<0) return false;
        for(int word=firstWord;word<=lastWord;++word) if(columns[word]) {
            left=word*32+(int)epic12_alpha_leading_zeros(columns[word]);break;
        }
        for(int word=lastWord;word>=firstWord;--word) if(columns[word]) {
            uint32_t lowest=columns[word]&(0U-columns[word]);
            right=word*32+(int)epic12_alpha_leading_zeros(lowest);break;
        }
        int begin=firstGroup*GROUP_ROWS,end=begin+GROUP_ROWS-1;
        if(begin<y0) begin=y0;
        if(end>y1) end=y1;
        for(int y=begin;y<=end;++y) if(row_bits(y,firstWord,lastWord,masks)) {top=y;break;}
        begin=lastGroup*GROUP_ROWS;end=begin+GROUP_ROWS-1;
        if(begin<y0) begin=y0;
        if(end>y1) end=y1;
        for(int y=end;y>=begin;--y) if(row_bits(y,firstWord,lastWord,masks)) {bottom=y;break;}
        return true;
    }
};

// Command needs normalized sx/sy/x/y/w/h, flipx/flipy and transparent fields.
// 1: draw (possibly cropped); 0: wholly transparent; -1: invalid source/slot.
// On 0/-1 the command is unchanged. A nontransparent draw is never cropped,
// even if every stored alpha bit is zero: those pixels still overwrite VRAM.
template<class Command>
static int epic12_gpu_alpha_trim(Command& command,const Epic12GpuAlphaPage* alpha,
    const int* slots,int slotCount)
{
    if(!command.transparent) return 1;
    EPIC12_ALPHA_COUNT(trimCommands,1);
    if(!alpha || !slots || slotCount<1 || slotCount>256) return -10;
    if(command.sx<0 || command.sx>=8192) return -11;
    if(command.sy<0 || command.sy>=4352) return -12;
    if(command.w<1 || command.w>8192-command.sx) return -13;
    if(command.h<1 || command.h>4352-command.sy) return -14;
    if(command.x<0 || command.x>=8192 || command.y<0 || command.y>=4096) return -15;
    if(command.w>8192-command.x || command.h>4096-command.y) return -16;
    int endX=command.sx+command.w-1,endY=command.sy+command.h-1;
    int left=endX+1,top=endY+1,right=-1,bottom=-1;
    for(int py=command.sy>>7;py<=(endY>>7);++py) for(int px=command.sx>>7;px<=(endX>>7);++px) {
        int slot=slots[py*64+px];
        if(slot<0 || slot>=slotCount) return -20;
        int pageX=px*128,pageY=py*128;
        int x0=command.sx>pageX?command.sx-pageX:0;
        int y0=command.sy>pageY?command.sy-pageY:0;
        int x1=endX-pageX<127?endX-pageX:127;
        int y1=endY-pageY<127?endY-pageY:127;
        int l=0,t=0,r=0,b=0;
        if(alpha[slot].bounds(x0,y0,x1,y1,l,t,r,b)) {
            l+=pageX;t+=pageY;r+=pageX;b+=pageY;
            if(l<left) left=l;
            if(t<top) top=t;
            if(r>right) right=r;
            if(b>bottom) bottom=b;
        }
    }
    if(right<left) {EPIC12_ALPHA_COUNT(emptyCommands,1);return 0;}
    command.x+=command.flipx?endX-right:left-command.sx;
    command.y+=command.flipy?endY-bottom:top-command.sy;
    command.sx=left;command.sy=top;
    command.w=right-left+1;command.h=bottom-top+1;
    return 1;
}

// output has room for count commands; valid input also permits in-place use.
// Return survivor count, or -1 on error (output may then be partly written).
// bounds is written only on a successful nonempty batch. Empty batches keep
// its old value and need no GPU input upload, draw or readback.
template<class Command,class Bounds>
static int epic12_gpu_alpha_crop_batch(const Command* input,int count,Command* output,
    const Epic12GpuAlphaPage* alpha,const int* slots,int slotCount,Bounds& bounds)
{
    if(count<0 || (count>0 && (!input || !output))) return -1;
    int kept=0,left=8192,top=4096,right=-1,bottom=-1;
    for(int i=0;i<count;++i) {
        Command c=input[i];
        int result=epic12_gpu_alpha_trim(c,alpha,slots,slotCount);
        if(result<0) return result;
        if(!result) continue;
        if(c.x<0 || c.y<0 || c.w<1 || c.h<1 || c.x>=8192 || c.y>=4096 ||
            c.w>8192-c.x || c.h>4096-c.y) return -30;
        output[kept++]=c;
        if(c.x<left) left=c.x;
        if(c.y<top) top=c.y;
        int x1=c.x+c.w-1,y1=c.y+c.h-1;
        if(x1>right) right=x1;
        if(y1>bottom) bottom=y1;
    }
    if(kept) {
        bounds.min_x=left;bounds.max_x=right;
        bounds.min_y=top;bounds.max_y=bottom;
    }
    return kept;
}
#undef EPIC12_ALPHA_COUNT
#endif
