/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus-rsp-hle - plugin.c                                        *
 *   Mupen64Plus homepage: https://mupen64plus.org/                        *
 *   Copyright (C) 2014 Bobby Smiles                                       *
 *   Copyright (C) 2009 Richard Goedeken                                   *
 *   Copyright (C) 2002 Hacktarux                                          *
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
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <stdarg.h>
#include <stdio.h>

#include "wintypes.h"
#include "common.h"
#include "hle.h"
#include "hle_internal.h"
#include "hle_external.h"

#include "RSPPlugin.h"

/* [Xbox360] ★ ENLACE C PARA LOS CUATRO SIMBOLOS QUE SALEN DE rsp_hle.
 *
 * Este directorio se compila como C++ (CompileAs=CompileAsCpp en el
 * vcxproj) porque es C99 moderno -- 52 declaraciones a mitad de bloque y
 * 14 `for (int i...)` -- y el compilador de C del XDK es C89 sin
 * interruptor.  Se midio antes de decidirlo: cero malloc/calloc sin cast,
 * cero identificadores reservados de C++, cero definiciones tentativas.
 *
 * Pero eso DECORA los nombres, y estos cuatro los llama codigo C:
 *
 *     doRspCycles     gc_memory/memory.c:629, :698, :707
 *     initiateRSP     xbox/plataforma.c   (rsp_info_init)
 *     romClosed_RSP   xbox/plataforma.c   (xbox64_unload_rom)
 *     closeDLL_RSP    xbox/plataforma.c   (xbox64_unload_rom)
 *
 * Todo lo demas que cruza esta frontera son PUNTEROS a funcion que el
 * nucleo pasa en RSP_INFO (l_ProcessDlistList y companyia, mas abajo), y
 * un puntero no lleva decoracion.
 *
 * ★ SE ENVUELVE EL INCLUDE, NO SE REDECLARAN LAS FUNCIONES.  La primera
 * version puso un bloque `extern "C"` con las cuatro firmas DESPUES de
 * este include, y no vale por dos razones:
 *
 *   1. `Rsp_#1.1.h` ya las declara (:104, :144, :178, :198).  Una segunda
 *      declaracion con enlace distinto es C2732, y si ademas la firma no
 *      coincide al milimetro el compilador la toma por una SOBRECARGA:
 *      `C2733: second C linkage of overloaded function`.
 *   2. Escribir las firmas a mano es duplicar una fuente de verdad que ya
 *      existe tres lineas mas abajo.
 *
 * Envolviendo el include, las cuatro quedan con enlace C desde su unica
 * declaracion, y las definiciones de mas abajo lo heredan.
 *
 * `Audio_#1.1.h` se queda fuera a proposito: plugin.c no usa nada suyo
 * (comprobado) y no define ninguna de sus funciones. */
#ifdef __cplusplus
extern "C" {
#endif
#include "Rsp_#1.1.h"
#ifdef __cplusplus
}
#endif

#include "Audio_#1.1.h"

/* local variables */
static struct hle_t g_hle;
static void (*l_CheckInterrupts)(void) = NULL;
static void (*l_ProcessDlistList)(void) = NULL;
static void (*l_ProcessAlistList)(void) = NULL;
static void (*l_ProcessRdpList)(void) = NULL;
static void (*l_ShowCFB)(void) = NULL;

/* Global functions needed by HLE core */
void HleVerboseMessage(void* UNUSED(user_defined), const char *message, ...)
{
    /* discard verbose message */
}

void HleInfoMessage(void* UNUSED(user_defined), const char *message, ...)
{
    /* discard informational message */
}

void HleErrorMessage(void* UNUSED(user_defined), const char *message, ...)
{
    /* discard error message */
}

void HleWarnMessage(void* UNUSED(user_defined), const char *message, ...)
{
    /* discard warning message */
}

void HleCheckInterrupts(void* UNUSED(user_defined))
{
    if (l_CheckInterrupts == NULL)
        return;

    (*l_CheckInterrupts)();
}

void HleProcessDlistList(void* UNUSED(user_defined))
{
    if (l_ProcessDlistList == NULL)
        return;

    (*l_ProcessDlistList)();
}

void HleProcessAlistList(void* UNUSED(user_defined))
{
    if (l_ProcessAlistList == NULL)
        return;

    (*l_ProcessAlistList)();
}

void HleProcessRdpList(void* UNUSED(user_defined))
{
    if (l_ProcessRdpList == NULL)
        return;

    (*l_ProcessRdpList)();
}

void HleShowCFB(void* UNUSED(user_defined))
{
    if (l_ShowCFB == NULL)
        return;

    (*l_ShowCFB)();
}

int HleForwardTask(void* user_defined)
{
    return -1;
}


/* DLL-exported functions */
EXPORT void CALL CloseDLL(void)
{
}

EXPORT unsigned int CALL DoRspCycles(unsigned int Cycles)
{
    hle_execute(&g_hle);
    return Cycles;
}

EXPORT void CALL InitiateRSP(RSP_INFO Rsp_Info, unsigned int* UNUSED(CycleCount))
{
    hle_init(&g_hle,
             Rsp_Info.RDRAM,
             Rsp_Info.DMEM,
             Rsp_Info.IMEM,
             Rsp_Info.MI_INTR_REG,
             Rsp_Info.SP_MEM_ADDR_REG,
             Rsp_Info.SP_DRAM_ADDR_REG,
             Rsp_Info.SP_RD_LEN_REG,
             Rsp_Info.SP_WR_LEN_REG,
             Rsp_Info.SP_STATUS_REG,
             Rsp_Info.SP_DMA_FULL_REG,
             Rsp_Info.SP_DMA_BUSY_REG,
             Rsp_Info.SP_PC_REG,
             Rsp_Info.SP_SEMAPHORE_REG,
             Rsp_Info.DPC_START_REG,
             Rsp_Info.DPC_END_REG,
             Rsp_Info.DPC_CURRENT_REG,
             Rsp_Info.DPC_STATUS_REG,
             Rsp_Info.DPC_CLOCK_REG,
             Rsp_Info.DPC_BUFBUSY_REG,
             Rsp_Info.DPC_PIPEBUSY_REG,
             Rsp_Info.DPC_TMEM_REG,
             NULL);

    l_CheckInterrupts = Rsp_Info.CheckInterrupts;
    l_ProcessDlistList = Rsp_Info.ProcessDlistList;
    l_ProcessAlistList = Rsp_Info.ProcessAlistList;
    l_ProcessRdpList = Rsp_Info.ProcessRdpList;
    l_ShowCFB = Rsp_Info.ShowCFB;
	
    g_hle.hle_gfx = 1;
    g_hle.hle_aud = 0;
}

EXPORT void CALL RomClosed(void)
{
    g_hle.cached_ucodes.count = 0;
}
