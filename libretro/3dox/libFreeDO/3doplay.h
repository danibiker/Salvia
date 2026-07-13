/*
	3DOplay sources v1.7.3 based on FreeDOcore
	3doplay.do.am
	Developer: Viktor Ivanov
	Any uses of the 3DOplay sources or any other material published by Viktor Ivanov have to be accompanied with full credits.
All rights reserved.
*/
/*
  www.freedo.org
The first and only working 3DO multiplayer emulator.

The FreeDO licensed under modified GNU LGPL, with following notes:

*   The owners and original authors of the FreeDO have full right to develop closed source derivative work.
*   Any non-commercial uses of the FreeDO sources or any knowledge obtained by studying or reverse engineering
    of the sources, or any other material published by FreeDO have to be accompanied with full credits.
*   Any commercial uses of FreeDO sources or any knowledge obtained by studying or reverse engineering of the sources,
    or any other material published by FreeDO is strictly forbidden without owners approval.

The above notes are taking precedence over GNU LGPL in conflicting situations.

Project authors:

Alexander Troosh
Maxim Grishin
Allen Wright
John Sammons
Felix Lazarev
*/


#ifndef __3DO_SYSTEM_HEADER_DEFINITION
#define __3DO_SYSTEM_HEADER_DEFINITION

// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#ifdef _XBOX
#include <xtl.h>
#include <process.h>
#else
#include <windows.h>
#endif

// TODO: reference additional headers your program requires here
#include "freedoconfig.h"
 

#include "arm.h"
#include "vdlp.h"
#include "DSP.h"
#include "Clio.h"
#include "Madam.h"
#include "SPORT.h"
#include "XBUS.h"
#include "DiagPort.h"
#include "quarz.h"

//------------------------------------------------------------------------------
 
struct VDLLine
//VDLP Line - ����� ��������� �� ���� ������������ ������� �������� ������� VDLP
{
        unsigned short line[320*4];//,line2[320*2*16];
        unsigned char xCLUTB[32];
	unsigned char xCLUTG[32];
	unsigned char xCLUTR[32];
        unsigned int xOUTCONTROLL;
        unsigned int xCLUTDMA;
	unsigned int xBACKGROUND;
};
struct VDLFrame
{
        VDLLine lines[240*4];
        unsigned int srcw,srch;
};

 
 
#define EXT_READ_ROMS           1
#define EXT_READ_NVRAM          2
#define EXT_WRITE_NVRAM         3
#define EXT_SWAPFRAME           5       //������������ ������ ��� ������ �����, ���� � ����� ������
#define EXT_PUSH_SAMPLE         6       //�������� ������ �� �������� �����
#define EXT_GET_PBUSLEN         7
#define EXT_GETP_PBUSDATA       8
//#define EXT_KPRINT              9
#define EXT_DEBUG_PRINT         10
#define EXT_FRAMETRIGGER_MT     12      //multytasking
#define EXT_READ2048            14      //for XBUS Plugin
#define EXT_GET_DISC_SIZE       15
#define EXT_ON_SECTOR           16
typedef void* (*_ext_Interface)(int, void*);

#define FDP_FREEDOCORE_VERSION  0
#define FDP_INIT                1    //set ext_interface
#define FDP_DESTROY             2
#define FDP_DO_EXECFRAME        3       //��������� 1/60 ������� ������ �������
#define FDP_DO_FRAME_MT         4      //multytasking
#define FDP_DO_EXECFRAME_MT     5      //multytasking
#define FDP_DO_LOAD             6       //��������� �� ����������� ������ ��������� ���������, ������� !NULL ���� ��� ��
#define FDP_GET_SAVE_SIZE       7       //return size of savestatemachine
#define FDP_DO_SAVE             8       //�������� � ���������� ����� ��������� ���������
#define FDP_GETP_NVRAM          9       //���������� ��������� �� NVRAM 32K
#define FDP_GETP_RAMS           10       //���������� ��������� �� RAM 3M
#define FDP_GETP_ROMS           11       //���������� ��������� �� ROM 2M
#define FDP_GETP_PROFILE        12       //���������� ��������� �� ������ �������� - ������ 3�/4 ���������
#define FDP_BUGTEMPORALFIX      13
#define FDP_SET_ARMCLOCK        14
#define FDP_SET_TEXQUALITY      15
#define FDP_GETP_WRCOUNT        16
#define FDP_SET_FIX_MODE        17
#define FDP_SET_SWI_HLE         18       //activa/desactiva el HLE de los SWI de matematica (datum!=0 = ON)
#define FDP_SET_CEL_HOIST       19       //activa/desactiva el hoist de PPROC del blitter CEL (datum!=0 = ON)

#define FIX_BIT_TIMING_1        (0x00000001)
#define FIX_BIT_TIMING_2        (0x00000002)
#define FIX_BIT_TIMING_3        (0x00000004)
#define FIX_BIT_TIMING_4        (0x00000008)
#define FIX_BIT_TIMING_5        (0x00000010)
#define FIX_BIT_TIMING_6        (0x00000020)
#define FIX_BIT_TIMING_7        (0x00000040)
#define FIX_BIT_GRAPHICS_STEP_Y (0x00080000) // Preserve Y coordinate rather than X between CELs.

#ifdef THREEDOCORE_EXPORTS
#define THREEDOCORE_API __declspec(dllexport)
#else
#define THREEDOCORE_API
#endif
 
       extern void* _freedo_Interface(int procedure, void *datum=0);
 

//------------------------------------------------------------------------------


#endif
