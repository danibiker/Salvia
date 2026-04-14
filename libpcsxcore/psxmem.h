/***************************************************************************
 *   Copyright (C) 2007 Ryan Schultz, PCSX-df Team, PCSX team              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02111-1307 USA.           *
 ***************************************************************************/
#ifndef __PSXMEMORY_H__
#define __PSXMEMORY_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "psxcommon.h"

extern __declspec(align(32))u8* psxMemWLUT[0x10000];
extern __declspec(align(32))u8* psxMemRLUT[0x10000];
extern __declspec(align(32))s8 psxM_2[0x00220000];
extern __declspec(align(32))s8 psxR_2[0x00080000];
extern s8 *psxH_2;


#ifdef _XBOX

#define SWAP16(x) _byteswap_ushort(x)
#define SWAP32(x) _byteswap_ulong(x)

#define SWAPu16(v) SWAP16((u16)(v))
#define SWAPu32(v) SWAP32((u32)(v))

#endif


#if !defined(PSXREC) && (defined(__x86_64__) || defined(__i386__) || defined(__ppc__)) && !defined(NOPSXREC)
#define PSXREC
#endif

#define psxMu8_2(mem)		(*(u8 *)&psxM_2[(mem) & 0x1fffff])
#define psxMu16_2(mem)	(SWAP16(*(u16 *)&psxM_2[(mem) & 0x1fffff]))
#define psxMu32_2(mem)	(SWAP32(*(u32 *)&psxM_2[(mem) & 0x1fffff]))
#define psxMu8ref_2(mem)	(*(u8 *)&psxM_2[(mem) & 0x1fffff])
#define psxMu16ref_2(mem)	(*(u16 *)&psxM_2[(mem) & 0x1fffff])
#define psxMu32ref_2(mem)	(*(u32 *)&psxM_2[(mem) & 0x1fffff])

#define psxRu32_2(mem)	(SWAP32(*(u32 *)&psxR_2[(mem) & 0x7ffff]))
#define psxRu32ref_2(mem)	(*(u32*)&psxR_2[(mem) & 0x7ffff])

#define psxHu8_2(mem)		(*(u8 *)&psxH_2[(mem) & 0xffff])
#define psxHu16_2(mem)	(SWAP16(*(u16 *)&psxH_2[(mem) & 0xffff]))
#define psxHu32_2(mem)	(SWAP32(*(u32 *)&psxH_2[(mem) & 0xffff]))
#define psxHu8ref_2(mem)	(*(u8 *)&psxH_2[(mem) & 0xffff])
#define psxHu16ref_2(mem)	(*(u16 *)&psxH_2[(mem) & 0xffff])
#define psxHu32ref_2(mem)	(*(u32 *)&psxH_2[(mem) & 0xffff])

#define PSXM_2(mem)		(psxMemRLUT[(mem) >> 16] == 0 ? NULL : (u8*)(psxMemRLUT[(mem) >> 16] + ((mem) & 0xffff)))
#define PSXMs8_2(mem)		(*(s8 *)PSXM_2(mem))
#define PSXMs16_2(mem)	(SWAP16(*(s16 *)PSXM_2(mem)))
#define PSXMs32_2(mem)	(SWAP32(*(s32 *)PSXM_2(mem)))
#define PSXMu8_2(mem)		(*(u8 *)PSXM_2(mem))
#define PSXMu16_2(mem)	(SWAP16(*(u16 *)PSXM_2(mem)))
#define PSXMu32_2(mem)	(SWAP32(*(u32 *)PSXM_2(mem)))

/*
/////////////////////////////////////////////////////////////////////////////////////////////////////vm
                                                                                                   //
extern s8 *psxVM;                                                                                  //
extern s8 *psxM;                                                                                   //
extern s8 *psxR;                                                                                   //
extern s8 *psxH;                                                                                   //
                                                                                                   //
#define VM_SIZE	0x20000000                                                                         //
#define VM_MASK (VM_SIZE - 1)                                                                      //
#define VM_PSX_MEM_MASK			(0x00200000 - 1)                                                   //
#define PSX_MEM_MIRROR			0x00800000                                                         //
#define CHECK_ADR(adr) (adr>=0x00000000 && adr<(0x00200000)) || (adr>=0x1f000000 && adr<0x1fc80000)//
                                                                                                   //
#define psxVMu8(mem)	(*(u8 *)&psxVM[(mem) & VM_MASK])                                           //
#define psxVMu16(mem)	(SWAP16(*(u16 *)&psxVM[(mem) & VM_MASK]))                                  //
#define psxVMu32(mem)	(SWAP32(*(u32 *)&psxVM[(mem) & VM_MASK]))                                  //
#define psxVMu8ref(mem)		(*(u8 *)&psxVM[(mem) & VM_MASK])                                       //
#define psxVMu16ref(mem)	(*(u16 *)&psxVM[(mem) & VM_MASK])                                      //
#define psxVMu32ref(mem)	(*(u32 *)&psxVM[(mem) & VM_MASK])                                      //
                                                                                                   //
#define psxMu8(mem)		(*(u8 *)&psxM[(mem) & 0x1fffff])                                           //
#define psxMu16(mem)	(SWAP16(*(u16 *)&psxM[(mem) & 0x1fffff]))                                  //
#define psxMu32(mem)	(SWAP32(*(u32 *)&psxM[(mem) & 0x1fffff]))                                  //
#define psxMu8ref(mem)	(*(u8 *)&psxM[(mem) & 0x1fffff])                                           //
#define psxMu16ref(mem)	(*(u16 *)&psxM[(mem) & 0x1fffff])                                          //
#define psxMu32ref(mem)	(*(u32 *)&psxM[(mem) & 0x1fffff])                                          //
                                                                                                   //
#define psxRu32(mem)	(SWAP32(*(u32 *)&psxR[(mem) & 0x7ffff]))                                   //
#define psxRu32ref(mem)	(*(u32*)&psxR[(mem) & 0x7ffff])                                            //
                                                                                                   //
#define psxHu8(mem)		(*(u8 *)&psxH[(mem) & 0xffff])                                             //
#define psxHu32(mem)	(SWAP32(*(u32 *)&psxH[(mem) & 0xffff]))                                    //
#define psxHu16ref(mem)	(*(u16 *)&psxH[(mem) & 0xffff])                                            //
#define psxHu32ref(mem)	(*(u32 *)&psxH[(mem) & 0xffff])                                            //
                                                                                                   //
#define PSXM(mem)		(u8*)(&psxVM[(mem) & VM_MASK])                                             //
#define PSXMu8(mem)		(*(u8 *)PSXM(mem))                                                         //
#define PSXMu16(mem)	(SWAP16(*(u16 *)PSXM(mem)))                                                //
#define PSXMu32(mem)	(SWAP32(*(u32 *)PSXM(mem)))                                                //
                                                                                                   //
int psxMemInit();                                                                                  //
void psxMemReset();                                                                                //
void psxMemShutdown();                                                                             //
                                                                                                   //
u8 psxMemRead8  (u32 mem);                                                                         //
u16 psxMemRead16(u32 mem);                                                                         //
u32 psxMemRead32(u32 mem);                                                                         // 
                                                                                                   //
void psxMemWrite8 (u32 mem, u8  value);                                                            //
void psxMemWrite16(u32 mem, u16 value);                                                            //
void psxMemWrite32(u32 mem, u32 value);                                                            //
                                                                                                   //
/////////////////////////////////////////////////////////////////////////////////////////////////////
*/

int psxMemInit_2();
void psxMemReset_2();
void psxMemShutdown_2();

u8 psxMemRead8_2  (u32 mem);
u16 psxMemRead16_2(u32 mem);
u32 psxMemRead32_2(u32 mem);

void psxMemWrite8_2 (u32 mem, u8  value);
void psxMemWrite16_2(u32 mem, u16 value);
void psxMemWrite32_2(u32 mem, u32 value);

#ifdef __cplusplus
}
#endif
#endif
