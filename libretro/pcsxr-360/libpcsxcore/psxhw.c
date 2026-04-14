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

/*
* Functions for PSX hardware control.
*/

#include "psxhw.h"
#include "mdec.h"
#include "cdrom.h"
#include "gpu.h"

#define _RF(type, func) \
	static type _##func##(u32 add) {	\
	return func();				\
}

#define _WF(type, func) \
	static void _##func##(u32 add, type value) {	\
	func(value);				\
}



#define DmaExec(n) \
	static void DmaExec##n##(u32 add, u32 value) { \
		HW_DMA##n##_CHCR = SWAPu32(value); \
		\
		if (SWAPu32(HW_DMA##n##_CHCR) & 0x01000000 && SWAPu32(HW_DMA_PCR) & (8 << (n * 4))) { \
		psxDma##n(SWAPu32(HW_DMA##n##_MADR), SWAPu32(HW_DMA##n##_BCR), SWAPu32(HW_DMA##n##_CHCR)); \
		} \
	} \

#define DmaExec_2(n) \
	static void DmaExec_2##n##(u32 add, u32 value) { \
		HW_DMA##n##_CHCR_2 = SWAPu32(value); \
		\
		if (SWAPu32(HW_DMA##n##_CHCR_2) & 0x01000000 && SWAPu32(HW_DMA_PCR_2) & (8 << (n * 4))) { \
		psxDma##n(SWAPu32(HW_DMA##n##_MADR_2), SWAPu32(HW_DMA##n##_BCR_2), SWAPu32(HW_DMA##n##_CHCR_2)); \
		} \
	} \


hw_read8_t *	hw_read8_handler;
hw_read16_t *	hw_read16_handler;
hw_read32_t *	hw_read32_handler;

hw_write8_t *	hw_write8_handler;
hw_write16_t *	hw_write16_handler;
hw_write32_t *	hw_write32_handler;

/******************************************************************** 
*							IO MAPPING
********************************************************************/

//DmaExec(0);
//DmaExec(1);
//DmaExec(2);
//DmaExec(3);
//DmaExec(4);
//DmaExec(6);

DmaExec_2(0);
DmaExec_2(1);
DmaExec_2(2);
DmaExec_2(3);
DmaExec_2(4);
DmaExec_2(6);


static void DmaIcr(u32 add, u32 value) {
//if(use_vm){
//	u32 tmp = (~value) & SWAPu32(HW_DMA_ICR);
//	HW_DMA_ICR = SWAPu32(((tmp ^ value) & 0xffffff) ^ tmp);}
//else{
	u32 tmp = (~value) & SWAPu32(HW_DMA_ICR_2);
	HW_DMA_ICR_2 = SWAPu32(((tmp ^ value) & 0xffffff) ^ tmp);//}//teste
}

/**
* Sio
*/
static u32 _sioRead32(u32 add) {
	u32 hard;
	hard = sioRead8();
	hard |= sioRead8() << 8;
	hard |= sioRead8() << 16;
	hard |= sioRead8() << 24;
	return hard;
}

static u16 _sioRead16(u32 add) {
	u16 hard;
	hard = sioRead8();
	hard |= sioRead8() << 8;
	return hard;
}

static u8 _sioRead8(u32 add) {
	return sioRead8();
}

static void _sioWrite32(u32 add, u32 value) {
	sioWrite8((u8)value);
	sioWrite8((u8)(value>>8));
	sioWrite8((u8)(value>>16));
	sioWrite8((u8)(value>>24));
}

static void _sioWrite16(u32 add, u16 value) {
	sioWrite8((u8)value);
	sioWrite8((u8)(value>>8));
}

static void _sioWrite8(u32 add, u8 value) {
	sioWrite8((u8)value);
}

_RF(u16, sioReadStat16);
_RF(u16, sioReadMode16);
_RF(u16, sioReadCtrl16);
_RF(u16, sioReadBaud16);

_WF(u16, sioWriteStat16);
_WF(u16, sioWriteMode16);
_WF(u16, sioWriteCtrl16);
_WF(u16, sioWriteBaud16);

/**
* Cdrom
*/
_RF(u8, cdrRead0);
_RF(u8, cdrRead1);
_RF(u8, cdrRead2);
_RF(u8, cdrRead3);

_WF(u8, cdrWrite0);
_WF(u8, cdrWrite1);
_WF(u8, cdrWrite2);
_WF(u8, cdrWrite3);

/**
* Counter
*/
static u32 psxRcnt0(u32 add) {
	return psxRcntRcount(0);
}
static u32 psxRcnt1(u32 add) {
	return psxRcntRcount(1);
}
static u32 psxRcnt2(u32 add) {
	return psxRcntRcount(2);
}

static u32 psxRmod0(u32 add) {
	return psxRcntRmode(0);
}
static u32 psxRmod1(u32 add) {
	return psxRcntRmode(1);
}
static u32 psxRmod2(u32 add) {
	return psxRcntRmode(2);
}

static u32 psxRtgt0(u32 add) {
	return psxRcntRtarget(0);
}
static u32 psxRtgt1(u32 add) {
	return psxRcntRtarget(1);
}
static u32 psxRtgt2(u32 add) {
	return psxRcntRtarget(2);
}


static void psxWcnt0(u32 add, u32 v) {
	psxRcntWcount(0, v);
}
static void psxWcnt1(u32 add, u32 v) {
	psxRcntWcount(1, v);
}
static void psxWcnt2(u32 add, u32 v) {
	psxRcntWcount(2, v);
}

static void psxWmod0(u32 add, u32 v) {
	psxRcntWmode(0, v);
}
static void psxWmod1(u32 add, u32 v) {
	psxRcntWmode(1, v);
}
static void psxWmod2(u32 add, u32 v) {
	psxRcntWmode(2, v);
}

static void psxWtgt0(u32 add, u32 v) {
	psxRcntWtarget(0, v);
}
static void psxWtgt1(u32 add, u32 v) {
	psxRcntWtarget(1, v);
}
static void psxWtgt2(u32 add, u32 v) {
	psxRcntWtarget(2, v);
}

static void psxWiReg16(u32 add, u16 value) {
//if(use_vm){
//	if (Config.Sio) psxHu16ref(0x1070) |= SWAPu16(0x80);
//	if (Config.SpuIrq) psxHu16ref(0x1070) |= SWAPu16(0x200);
//	psxHu16ref(0x1070) &= SWAPu16(value);}
//else{
	if (Config.Sio) psxHu16ref_2(0x1070) |= SWAPu16(0x80);
	if (Config.SpuIrq) psxHu16ref_2(0x1070) |= SWAPu16(0x200);
	psxHu16ref_2(0x1070) &= SWAPu16(value);//}//teste
}

static void psxWiReg32(u32 add, u32 value) {
//if(use_vm){
//	if (Config.Sio) psxHu32ref(0x1070) |= SWAPu32(0x80);
//	if (Config.SpuIrq) psxHu32ref(0x1070) |= SWAPu32(0x200);
//	psxHu32ref(0x1070) &= SWAPu32(value);}
//else{
	if (Config.Sio) psxHu32ref_2(0x1070) |= SWAPu32(0x80);
	if (Config.SpuIrq) psxHu32ref_2(0x1070) |= SWAPu32(0x200);
	psxHu32ref_2(0x1070) &= SWAPu32(value);//}//teste
}

/**
* Gpu
**/
_RF(u32, gpuReadData);
_RF(u32, GPU_readStatus);

_WF(u32, gpuWriteData);
_WF(u32, gpuWriteStatus);

/**
* Mdec
**/
_RF(u32, mdecRead0);
_RF(u32, mdecRead1);

_WF(u32, mdecWrite0);
_WF(u32, mdecWrite1);

/**
* Spu
**/
static void SpuWriteRegister16(u32 add, u16 value) {
	SPU_writeRegister(add, value);
}

static void SpuWriteRegister32(u32 add, u32 value) {
	SPU_writeRegister(add, value&0xffff);

	// next 16bit
	SPU_writeRegister(add+2, (value>>16)&0xffff);
}



/**
* Default ...
*/

static u8 _psxHwMemRead8(u32 add) {
//if(use_vm){
//	return PSXMu8(add);}
//else{
    return psxHu8_2(add);//} //teste
}

static u16 _psxHwMemRead16(u32 add) {
//if(use_vm){
//	return PSXMu16(add);}
//else{
	return psxHu16_2(add);//} //teste
}

static u32 _psxHwMemRead32(u32 add) {
//if(use_vm){
//	return PSXMu32(add);}
//else{
    return psxHu32_2(add);//}//teste
}

static void _psxHwMemWrite8(u32 add, u8 value) {
//if(use_vm){
//	psxVMu8ref(add) = value;}
//else{
    psxHu8ref_2(add) = value;//}//teste
}

static void _psxHwMemWrite16(u32 add, u16 value) {
//if(use_vm){
//	psxVMu16ref(add) = SWAPu16(value);}
//else{
    psxHu16ref_2(add) =  SWAPu16(value);//}//teste
}

static void _psxHwMemWrite32(u32 add, u32 value) {
//if(use_vm){
//	psxVMu32ref(add) = SWAPu32(value);}
//else{
    psxHu32ref_2(add) =  SWAPu32(value);//}//teste
}

/**
* Init ...
*/
void psxHwInit() {
	int i = 0;
	int size = 0xFFFF * sizeof(void*);

	hw_read8_handler = (hw_read8_t*)malloc(size);
	hw_read16_handler = (hw_read16_t*)malloc(size);
	hw_read32_handler = (hw_read32_t*)malloc(size);

	hw_write8_handler = (hw_write8_t*)malloc(size);
	hw_write16_handler = (hw_write16_t*)malloc(size);
	hw_write32_handler = (hw_write32_t*)malloc(size);

	memset(hw_read8_handler, 0, size);
	memset(hw_read16_handler, 0, size);
	memset(hw_read32_handler, 0, size);

	memset(hw_write8_handler, 0, size);
	memset(hw_write16_handler, 0, size);
	memset(hw_write32_handler, 0, size);

	// default handler
	for(i = 0; i < 0xFFFF; i++) {
		hw_read8_handler[i] = _psxHwMemRead8;
		hw_read16_handler[i] = _psxHwMemRead16;
		hw_read32_handler[i] = _psxHwMemRead32;

		hw_write8_handler[i] = _psxHwMemWrite8;
		hw_write16_handler[i] = _psxHwMemWrite16;
		hw_write32_handler[i] = _psxHwMemWrite32;
	}

	// Spu
	for(i = 0x1c00; i < 0x1e00; i++) {
		hw_read16_handler[i] = (hw_read16_t)SPU_readRegister;
		hw_write16_handler[i] = (hw_write16_t)SpuWriteRegister16;
		hw_write32_handler[i] = (hw_write32_t)SpuWriteRegister32;
	}

	// read8 handler
	hw_read8_handler[0x1040] = _sioRead8;
	hw_read8_handler[0x1800] = _cdrRead0;
	hw_read8_handler[0x1801] = _cdrRead1;
	hw_read8_handler[0x1802] = _cdrRead2;
	hw_read8_handler[0x1803] = _cdrRead3;

	// read16 handler
	hw_read16_handler[0x1040] = _sioRead16;
	hw_read16_handler[0x1044] = _sioReadStat16;
	hw_read16_handler[0x1048] = _sioReadMode16;
	hw_read16_handler[0x104a] = _sioReadCtrl16;
	hw_read16_handler[0x104e] = _sioReadBaud16;

	hw_read16_handler[0x1100] = (hw_read16_t)psxRcnt0;
	hw_read16_handler[0x1104] = (hw_read16_t)psxRmod0;
	hw_read16_handler[0x1108] = (hw_read16_t)psxRtgt0;

	hw_read16_handler[0x1110] = (hw_read16_t)psxRcnt1;
	hw_read16_handler[0x1114] = (hw_read16_t)psxRmod1;
	hw_read16_handler[0x1118] = (hw_read16_t)psxRtgt1;

	hw_read16_handler[0x1120] = (hw_read16_t)psxRcnt2;
	hw_read16_handler[0x1124] = (hw_read16_t)psxRmod2;
	hw_read16_handler[0x1128] = (hw_read16_t)psxRtgt2;

	// read32 handler
	hw_read32_handler[0x1040] = _sioRead32;
	hw_read32_handler[0x1810] = (hw_read32_t)_gpuReadData;
	hw_read32_handler[0x1814] = (hw_read32_t)_GPU_readStatus;
	hw_read32_handler[0x1820] = _mdecRead0;
	hw_read32_handler[0x1824] = _mdecRead1;

	hw_read32_handler[0x1100] = (hw_read32_t)psxRcnt0;
	hw_read32_handler[0x1104] = (hw_read32_t)psxRmod0;
	hw_read32_handler[0x1108] = (hw_read32_t)psxRtgt0;

	hw_read32_handler[0x1110] = (hw_read32_t)psxRcnt1;
	hw_read32_handler[0x1114] = (hw_read32_t)psxRmod1;
	hw_read32_handler[0x1118] = (hw_read32_t)psxRtgt1;

	hw_read32_handler[0x1120] = (hw_read32_t)psxRcnt2;
	hw_read32_handler[0x1124] = (hw_read32_t)psxRmod2;
	hw_read32_handler[0x1128] = (hw_read32_t)psxRtgt2;

	// write 8
	hw_write8_handler[0x1040] = _sioWrite8;
	hw_write8_handler[0x1800] = _cdrWrite0;
	hw_write8_handler[0x1801] = _cdrWrite1;
	hw_write8_handler[0x1802] = _cdrWrite2;
	hw_write8_handler[0x1803] = _cdrWrite3;

	// write 16
	hw_write16_handler[0x1040] = _sioWrite16;
	hw_write16_handler[0x1044] = _sioWriteStat16;
	hw_write16_handler[0x1048] = _sioWriteMode16;
	hw_write16_handler[0x104a] = _sioWriteCtrl16;
	hw_write16_handler[0x104e] = _sioWriteBaud16;

	// IREG
	hw_write16_handler[0x1070] = psxWiReg16;

	hw_write16_handler[0x1100] = (hw_write16_t)psxWcnt0;
	hw_write16_handler[0x1104] = (hw_write16_t)psxWmod0;
	hw_write16_handler[0x1108] = (hw_write16_t)psxWtgt0;

	hw_write16_handler[0x1110] = (hw_write16_t)psxWcnt1;
	hw_write16_handler[0x1114] = (hw_write16_t)psxWmod1;
	hw_write16_handler[0x1118] = (hw_write16_t)psxWtgt1;

	hw_write16_handler[0x1120] = (hw_write16_t)psxWcnt2;
	hw_write16_handler[0x1124] = (hw_write16_t)psxWmod2;
	hw_write16_handler[0x1128] = (hw_write16_t)psxWtgt2;

	// write 32
	hw_write32_handler[0x1040] = _sioWrite32;

	// IREG
	hw_write32_handler[0x1070] = psxWiReg32;
//if(use_vm){
//	hw_write32_handler[0x1088] = DmaExec0;
//	hw_write32_handler[0x1098] = DmaExec1;
//	hw_write32_handler[0x10a8] = DmaExec2;
//	hw_write32_handler[0x10b8] = DmaExec3;
//	hw_write32_handler[0x10c8] = DmaExec4;
//	hw_write32_handler[0x10e8] = DmaExec6;}
//else{
	hw_write32_handler[0x1088] = DmaExec_20;
	hw_write32_handler[0x1098] = DmaExec_21;
	hw_write32_handler[0x10a8] = DmaExec_22;
	hw_write32_handler[0x10b8] = DmaExec_23;
	hw_write32_handler[0x10c8] = DmaExec_24;
	hw_write32_handler[0x10e8] = DmaExec_26;//}//teste

	hw_write32_handler[0x10f4] = DmaIcr;

	hw_write32_handler[0x1100] = psxWcnt0;
	hw_write32_handler[0x1104] = psxWmod0;
	hw_write32_handler[0x1108] = psxWtgt0;

	hw_write32_handler[0x1110] = psxWcnt1;
	hw_write32_handler[0x1114] = psxWmod1;
	hw_write32_handler[0x1118] = psxWtgt1;

	hw_write32_handler[0x1120] = psxWcnt2;
	hw_write32_handler[0x1124] = psxWmod2;
	hw_write32_handler[0x1128] = psxWtgt2;

	hw_write32_handler[0x1810] = _gpuWriteData;
	hw_write32_handler[0x1814] = _gpuWriteStatus;
	hw_write32_handler[0x1820] = _mdecWrite0;
	hw_write32_handler[0x1824] = _mdecWrite1;
}

void psxHwShutdown() {
	
	if (hw_read8_handler)
		free(hw_read8_handler);

	if (hw_read16_handler)
		free(hw_read16_handler);

	if (hw_read32_handler)
		free(hw_read32_handler);

	if (hw_write8_handler)
		free(hw_write8_handler);

	if (hw_write16_handler)
		free(hw_write16_handler);

	if (hw_write32_handler)
		free(hw_write32_handler);
}

void psxHwReset() {
//if(use_vm){
//	if (Config.Sio) psxHu32ref(0x1070) |= SWAP32(0x80);
//	if (Config.SpuIrq) psxHu32ref(0x1070) |= SWAP32(0x200);}
//else{
	if (Config.Sio) psxHu32ref_2(0x1070) |= SWAP32(0x80);
	if (Config.SpuIrq) psxHu32ref_2(0x1070) |= SWAP32(0x200);//}//teste
//if(use_vm)
//	memset(psxH, 0, 0x10000);
//else
	memset(psxH_2, 0, 0x10000);//teste

	mdecInit(); // initialize mdec decoder
	cdrReset();
	psxRcntInit();

	psxHwShutdown();
	psxHwInit();
}


u8 psxHwRead8(u32 add) {
	u32 p = add & 0xFFFF;
	hw_read8_t func = NULL;

	func = hw_read8_handler[p];
	return func(add);
}

u16 psxHwRead16(u32 add) {
	u32 p = add & 0xFFFF;
	hw_read16_t func = NULL;

	func = hw_read16_handler[p];
	return func(add);
}

u32 psxHwRead32(u32 add) {
	u32 p = add & 0xFFFF;
	hw_read32_t func = NULL;

	func = hw_read32_handler[p];
	return func(add);
}

void psxHwWrite8(u32 add, u8 value) {
	u32 p = add & 0xFFFF;
	hw_write8_t func = NULL;

	func = hw_write8_handler[p];
	func(add, value);
}

void psxHwWrite16(u32 add, u16 value) {
	u32 p = add & 0xFFFF;
	hw_write16_t func = NULL;

	func = hw_write16_handler[p];
	func(add, value);
}

void psxHwWrite32(u32 add, u32 value) {
	u32 p = add & 0xFFFF;
	hw_write32_t func = NULL;

	func = hw_write32_handler[p];
	func(add, value);
}

int psxHwFreeze(gzFile f, int Mode) {
	return 0;
}