/**
 * glN64_GX - RSP.cpp
 * Copyright (C) 2003 Orkin
 * Copyright (C) 2008, 2009 sepp256 (Port to Wii/Gamecube/PS3)
 *
 * glN64 homepage: http://gln64.emulation64.com
 * Wii64 homepage: http://www.emulatemii.com
 * email address: sepp256@gmail.com
 *
**/

#ifdef __GX__
#include <gccore.h>
#include <stdio.h>
#include "../gui/DEBUG.h"
#endif // __GX__

#ifndef __LINUX__
# include <windows.h>
#else
# include "../main/winlnxdefs.h"
#endif

#include <math.h>
#include <string.h>
#include <ctype.h>
#include "../xenon64_diag.h"   /* XBOX64_TRAZAS */
#include "glN64.h"
#include "OpenGL.h"
#include "Debug.h"
#include "RSP.h"
#include "RDP.h"
#include "N64.h"
#include "F3D.h"
#include "3DMath.h"
#include "VI.h"
#include "../main/timers.h"
#include "Combiner.h"
//#include "textures.h"
#include "Config.h"
#include "FrameBuffer.h"
#include "DepthBuffer.h"
#include "DepthCopy.h"
#include "GBI.h"
#include "Turbo3D.h"

RSPInfo		RSP;

void RSP_LoadMatrix( f32 mtx[4][4], u32 address )
{
	/* [Xbox360] upstream la comento al pasar el camino GEKKO a psq_l, pero la
	 * rama generica de abajo (la que compilamos nosotros: __LINUX__ sin X86_ASM
	 * ni GEKKO) la sigue usando.  Descomentada. */
	f32 recip = 1.5258789e-05f;
#ifndef __LINUX__
	__asm {
		mov		esi, dword ptr [RDRAM];
		add		esi, dword ptr [address];
		mov		edi, dword ptr [mtx];

		mov		ecx, 4
LoadLoop:
		fild	word ptr [esi+02h]
		movzx	eax, word ptr [esi+22h]
		mov		dword ptr [edi], eax
		fild	dword ptr [edi]
		fmul	dword ptr [recip]
		fadd
		fstp	dword ptr [edi]

		fild	word ptr [esi+00h]
		movzx	eax, word ptr [esi+20h]
		mov		dword ptr [edi+04h], eax
		fild	dword ptr [edi+04h]
		fmul	dword ptr [recip]
		fadd
		fstp	dword ptr [edi+04h]

		fild	word ptr [esi+06h]
		movzx	eax, word ptr [esi+26h]
		mov		dword ptr [edi+08h], eax
		fild	dword ptr [edi+08h]
		fmul	dword ptr [recip]
		fadd
		fstp	dword ptr [edi+08h]

		fild	word ptr [esi+04h]
		movzx	eax, word ptr [esi+24h]
		mov		dword ptr [edi+0Ch], eax
		fild	dword ptr [edi+0Ch]
		fmul	dword ptr [recip]
		fadd
		fstp	dword ptr [edi+0Ch]

		add		esi, 08h
		add		edi, 10h
		loop	LoadLoop
	}
#else // !__LINUX__
# ifdef X86_ASM
	__asm__ __volatile__(
	".intel_syntax noprefix"					"\n\t"
	"LoadLoop:"									"\n\t"
	"	fild	word ptr [esi+0x02]"			"\n\t"
	"	movzx	eax, word ptr [esi+0x22]"		"\n\t"
	"	mov		dword ptr [edi], eax"			"\n\t"
	"	fild	dword ptr [edi]"				"\n\t"
	"	fmul	%0"								"\n\t"
	"	fadd"									"\n\t"
	"	fstp	dword ptr [edi]"				"\n\t"

	"	fild	word ptr [esi+0x00]"			"\n\t"
	"	movzx	eax, word ptr [esi+0x20]"		"\n\t"
	"	mov		dword ptr [edi+0x04], eax"		"\n\t"
	"	fild	dword ptr [edi+0x04]"			"\n\t"
	"	fmul	%0"								"\n\t"
	"	fadd"									"\n\t"
	"	fstp	dword ptr [edi+0x04]"			"\n\t"

	"	fild	word ptr [esi+0x06]"			"\n\t"
	"	movzx	eax, word ptr [esi+0x26]"		"\n\t"
	"	mov		dword ptr [edi+0x08], eax"		"\n\t"
	"	fild	dword ptr [edi+0x08]"			"\n\t"
	"	fmul	%0"								"\n\t"
	"	fadd"									"\n\t"
	"	fstp	dword ptr [edi+0x08]"			"\n\t"

	"	fild	word ptr [esi+0x04]"			"\n\t"
	"	movzx	eax, word ptr [esi+0x24]"		"\n\t"
	"	mov		dword ptr [edi+0x0C], eax"		"\n\t"
	"	fild	dword ptr [edi+0x0C]"			"\n\t"
	"	fmul	%0"								"\n\t"
	"	fadd"									"\n\t"
	"	fstp	dword ptr [edi+0x0C]"			"\n\t"

	"	add		esi, 0x08"						"\n\t"
	"	add		edi, 0x10"						"\n\t"
	"	loop	LoadLoop"						"\n\t"
	".att_syntax prefix"						"\n\t"
	: /* no output */
	: "f"(recip), "S"((int)RDRAM+address), "D"(mtx), "c"(4)
	: "memory" );
#elif defined(GEKKO) // X86_ASM
	
	struct _N64Matrix
	{
		SHORT integer[4][4];
		WORD fraction[4][4];
	} *n64Mat = (struct _N64Matrix *)&RDRAM[address];
	
	for(int i=0; i<4; ++i){
		/*for(int j=0; j<4; ++j)
			mtx[i][j] = (float)n64Mat->integer[i][j] + (float)n64Mat->fraction[i][j] * recip;*/
		
		__asm__ volatile(
			"psq_l    3,   (%3*8)(%0), 0, 3 \n"
			"psq_l    4,   (%3*8)(%1), 0, 7 \n"
			"psq_l    5, (%3*8+4)(%0), 0, 3 \n"
			"psq_l    6, (%3*8+4)(%1), 0, 7 \n"
			
			"ps_add  4, 3, 4     \n"
			"ps_add  6, 5, 6     \n"
			
			"psq_st   4,   (%3*16)(%2), 0, 0 \n"
			"psq_st   6, (%3*16+8)(%2), 0, 0 \n"
			:: "r" (n64Mat->fraction), "r" (n64Mat->integer),
			   "r" (mtx), "n" (i)
			 : "fr2", "fr3", "fr4", "fr5", "fr6",
			   "r0", "memory");
	}
	
# else // GEKKO
	struct _N64Matrix
	{
		SHORT integer[4][4];
		WORD fraction[4][4];
	} *n64Mat = (struct _N64Matrix *)&RDRAM[address];
	int i, j;

	for (i = 0; i < 4; i++)
		for (j = 0; j < 4; j++)
#  ifndef _BIG_ENDIAN
			mtx[i][j] = (GLfloat)(n64Mat->integer[i][j^1]) + (GLfloat)(n64Mat->fraction[i][j^1]) * recip;
#  else // !_BIG_ENDIAN -> This should fix a Big Endian issue.
			mtx[i][j] = (GLfloat)(n64Mat->integer[i][j^0]) + (GLfloat)(n64Mat->fraction[i][j^0]) * recip;
#  endif // _BIG_ENDIAN
# endif // !( X86_ASM || GEKKO )
#endif // __LINUX__
}

#ifdef RSPTHREAD
DWORD WINAPI RSP_ThreadProc( LPVOID lpParameter )
{
	RSP_Init();

	SetEvent( RSP.threadFinished );
#ifndef _DEBUG
	__try
	{
#endif
		while (TRUE)
		{
			switch (WaitForMultipleObjects( 6, RSP.threadMsg, FALSE, INFINITE ))
			{
				case (WAIT_OBJECT_0 + RSPMSG_PROCESSDLIST):
					RSP_ProcessDList();
					break;
				case (WAIT_OBJECT_0 + RSPMSG_UPDATESCREEN):
					VI_UpdateScreen();
					break;
				case (WAIT_OBJECT_0 + RSPMSG_CLOSE):
					OGL_Stop();
					SetEvent( RSP.threadFinished );
					return 1;
				case (WAIT_OBJECT_0 + RSPMSG_DESTROYTEXTURES):
					Combiner_Destroy();
					FrameBuffer_Destroy();
					TextureCache_Destroy();
					break;
				case (WAIT_OBJECT_0 + RSPMSG_INITTEXTURES):
					FrameBuffer_Init();
					TextureCache_Init();
					Combiner_Init();
					gSP.changed = gDP.changed = 0xFFFFFFFF;
					break;
				case (WAIT_OBJECT_0 + RSPMSG_CAPTURESCREEN):
					OGL_SaveScreenshot();
					break;
			}
			SetEvent( RSP.threadFinished );
		}
#ifndef _DEBUG
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		char exception[256];
		sprintf( exception, "Win32 exception 0x%08X occured in glN64", GetExceptionCode() );
		MessageBox( NULL, exception, pluginName, MB_OK | MB_ICONERROR );

		GBI_Destroy();
		DepthBuffer_Destroy();
		OGL_Stop();
	}
#endif
	RSP.thread = NULL;
	return 0;
}
#endif // RSPTHREAD

#define SP_STATUS_HALT		0x0001
#define SP_STATUS_BROKE		0x0002
#define SP_STATUS_TASKDONE	0x0200


static
void _ProcessDListFactor5()
{
	while (!RSP.halt)
	{
		if ((RSP.PC[RSP.PCi] + 8) > RDRAMSize)
			break;

		u32 w0 = *(u32*)&RDRAM[RSP.PC[RSP.PCi]];
		u32 w1 = *(u32*)&RDRAM[RSP.PC[RSP.PCi] + 4];
		RSP.cmd = _SHIFTR( w0, 24, 8 );

		// Upstream reads this unguarded; the peek is one command past the one
		// being executed, so it can run off the end of RDRAM on a malformed
		// list.
		if ((RSP.PC[RSP.PCi] + 16) <= RDRAMSize)
			RSP.nextCmd = _SHIFTR( *(u32*)&RDRAM[RSP.PC[RSP.PCi] + 8], 24, 8 );
		else
			RSP.nextCmd = 0;

		GBI.cmd[RSP.cmd]( w0, w1 );

		RSP.PC[RSP.PCi] += 8;

		if (RSP.count != 0xFFFFFFFF)
		{
			--RSP.count;
			if (RSP.count == 0)
			{
				RSP.count = (u32)-1;
				--RSP.PCi;
			}
		}
	}
}

/* [Xbox360] ¿esta lista salio por una via mala?  Ver la nota junto a `salok`
 * al final de RSP_ProcessDList. */
static int s_gln64_salida_mala = 0;

/* [Xbox360] El reinicio del otherMode que glN64 hacia al empezar CADA lista,
 * movido al swap: una vez por frame, como GLideN64 (DisplayWindow::swapBuffers;
 * 51b185e3).  El contenido es el de siempre; con los mismos hacks de los
 * Quake (hack_doNotResetOtherModeL/H).  Lo llama VI_UpdateScreen tras cada
 * GX_CopyDisp.  Ver la nota en RSP_ProcessDList. */
void RSP_ReiniciaOtherMode()
{
	if ((config.generalEmulation.hacks & hack_doNotResetOtherModeL) == 0)
	{
		gDPSetAlphaCompare( G_AC_NONE );
		gDPSetDepthSource( G_ZS_PIXEL );
		gDPSetRenderMode( 0, 0 );
	}

	if ((config.generalEmulation.hacks & hack_doNotResetOtherModeH) == 0)
	{
		gDPSetAlphaDither( G_AD_DISABLE );
		gDPSetColorDither( G_CD_DISABLE );
		gDPSetCombineKey( G_CK_NONE );
		gDPSetTextureConvert( G_TC_FILT );
		gDPSetTextureFilter( G_TF_POINT );
		gDPSetTextureLUT( G_TT_NONE );
		gDPSetTextureLOD( G_TL_TILE );
		gDPSetTextureDetail( G_TD_CLAMP );
		gDPSetTexturePersp( G_TP_PERSP );
		gDPSetCycleType( G_CYC_1CYCLE );
		gDPPipelineMode( G_PM_NPRIMITIVE );
	}
}

void RSP_ProcessDList()
{
	GLN64_PROF_SCOPE(dlist);   /* [Xbox360] sonda temporal, xenon/gln64_prof.h */
	s_gln64_salida_mala = 0;
	if (RSP.infloop)
	{
		// Resuming a display list that ended by branching to itself (e.g. Gauntlet Legends)
		RSP.infloop = FALSE;
		RSP.halt = FALSE;
	}
	else
	{
		VI_UpdateSize();
		OGL_UpdateScale();

		RSP.PC[0] = *(u32*)&DMEM[0x0FF0];
		RSP.PCi = 0;
		RSP.count = -1;

		RSP.halt = FALSE;
		RSP.busy = TRUE;
		GLN64_WD_RESET();   /* [Xbox360] sonda temporal, xenon/gln64_prof.h */

		gSP.matrix.stackSize = MIN( 32, *(u32*)&DMEM[0x0FE4] >> 6 );
		gSP.matrix.modelViewi = 0;
		gSP.changed |= CHANGED_MATRIX;
		/* [Xbox360] [Conker] Como GLideN64 (RSP.cpp:151): la luz avanzada de
		 * F3DCBFD dura lo que la display list que la encendio. */
		gSP.cbfd.advancedLighting = FALSE;

		/* [Xbox360] ★ [Virtual Pool 64, bolas y fondo "con zoom" segun la camara]
		 * La perspectiva de textura SI se reinicia en cada lista, como GLideN64
		 * hoy (RSP.cpp:153): dcbc303c "Return gDPSetTexturePersp(G_TP_PERSP)
		 * back to begin of RSP_ProcessDList(). Fixed balls in Virtual Pool",
		 * tres dias despues de 51b185e3 (ver la nota del otherMode mas abajo),
		 * que este arbol porto sin ella.  Sin esto, un G_TP_NONE de la lista
		 * anterior se quedaba y las UV salian a la mitad (texPerspHalf,
		 * OpenGL.cpp).  ★RESUELTO (log 42): Virtual Pool bien, Quake II de
		 * control bien; los dos heredaban G_TP_NONE (sonda [TEXPERSP] retirada). */
		gDPSetTexturePersp( G_TP_PERSP );

		/* [Xbox360] Se probo a NO reiniciar modelView[0] (como GLideN64) para la
		 * VS de Mario Tennis y se retiro: no hacia falta (la escena del rival
		 * carga ella misma su modelview, [MTXL] del log 31) y coincidio con
		 * Banjo-Kazooie y 1080 en negro (log 35).  Se queda el reinicio de
		 * Wii64. */
		for (int i = 0; i < 4; i++)
			for (int j = 0; j < 4; j++)
				gSP.matrix.modelView[0][i][j] = 0.0f;

		gSP.matrix.modelView[0][0][0] = 1.0f;
		gSP.matrix.modelView[0][1][1] = 1.0f;
		gSP.matrix.modelView[0][2][2] = 1.0f;
		gSP.matrix.modelView[0][3][3] = 1.0f;

		u32 uc_start = *(u32*)&DMEM[0x0FD0];
		u32 uc_dstart = *(u32*)&DMEM[0x0FD8];
		u32 uc_dsize = *(u32*)&DMEM[0x0FDC];

		if ((uc_start != RSP.uc_start) || (uc_dstart != RSP.uc_dstart))
			gSPLoadUcodeEx( uc_start, uc_dstart, uc_dsize );

		/* [Xbox360] ★ [Quake II: objetos a traves del suelo y las paredes, un
		 * cuerpo por delante del arma] AQUI YA NO SE REINICIA EL otherMode: se
		 * hace una vez por frame, en el swap (RSP_ReiniciaOtherMode, desde
		 * VI_UpdateScreen), como GLideN64 (51b185e3 "Remove reset to initial
		 * state from RSP_ProcessDList(). It breakes logic of multiple display
		 * lists for one frame"; hoy DisplayWindow::swapBuffers).  MEDIDO (sonda
		 * [Q2Z]): Quake II manda DOS listas por frame (94 listas/s, 47 img/s) y
		 * ~4000 de cada 32768 triangulos salian con omL 00000000 -- el modo
		 * reiniciado al empezar la segunda --, sin test de Z. */

#ifdef __GX__
		OGL_GXinitDlist();
		DepthCopy_BeginDList();
#endif //__GX__
		if (GBI.current && (GBI.current->type == ZSortBOSS) && REG.SP_STATUS)
		{
			RSP.PC[1] = *(u32*)&DMEM[0x0FF8];
			*REG.SP_STATUS &= ~0x300; // clear sig1 | sig2
			*REG.SP_STATUS |= 0x800;  // set sig4
		}
		else if (GBI.current && (GBI.current->type == F5Rogue))
		{
			RSP.F5DL[0] = _SHIFTR( *(u32*)&RDRAM[RSP.PC[0]], 0, 24 );
			RSP.PC[0] += 8;

			static const u32 vAddrToClear[7] = { 0x11C >> 2, 0x120 >> 2, 0x124 >> 2,
				0x37C >> 2, 0x58C >> 2, 0x5B0 >> 2, 0x5B4 >> 2 };
			u32 * pDmem32 = (u32*)DMEM;
			for (u32 i = 0; i < 7; ++i)
				pDmem32[vAddrToClear[i]] = 0U;
		}
	}

	if (GBI.current && (GBI.current->type == F5Rogue || GBI.current->type == F5Indi_Naboo))
		_ProcessDListFactor5();
	else if (GBI.current && (GBI.current->type == Turbo3D))
		RunTurbo3D();
	else

	while (!RSP.halt)
	{
		if ((RSP.PC[RSP.PCi] + 8) > RDRAMSize)
		{
			GLN64_PROF_CNT(saldir);   /* [Xbox360] paso 0 del hilo grafico */
			s_gln64_salida_mala = 1;
#ifdef DEBUG
			switch (Debug.level)
			{
				case DEBUG_LOW:
                    DebugMsg( DEBUG_LOW | DEBUG_ERROR, "ATTEMPTING TO EXECUTE RSP COMMAND AT INVALID RDRAM LOCATION\n" );
					break;
				case DEBUG_MEDIUM:
                    DebugMsg( DEBUG_MEDIUM | DEBUG_ERROR, "Attempting to execute RSP command at invalid RDRAM location\n" );
					break;
				case DEBUG_HIGH:
                    DebugMsg( DEBUG_HIGH | DEBUG_ERROR, "// Attempting to execute RSP command at invalid RDRAM location\n" );
					break;
			}
#endif
			break;
		}

//		printf( "!!!!!! RDRAM = 0x%8.8x\n", RDRAM );//RSP.PC[RSP.PCi] );
/*		{
			static u8 *lastRDRAM = 0;
			if (lastRDRAM == 0)
				lastRDRAM = RDRAM;
			if (RDRAM != lastRDRAM)
			{
				__asm__( "int $3" );
			}
		}*/
		u32 w0 = *(u32*)&RDRAM[RSP.PC[RSP.PCi]];
		u32 w1 = *(u32*)&RDRAM[RSP.PC[RSP.PCi] + 4];
		RSP.cmd = _SHIFTR( w0, 24, 8 );

#ifdef DEBUG
		DebugRSPState( RSP.PCi, RSP.PC[RSP.PCi], _SHIFTR( w0, 24, 8 ), w0, w1 );
		DebugMsg( DEBUG_LOW | DEBUG_HANDLED, "0x%08lX: CMD=0x%02lX W0=0x%08lX W1=0x%08lX\n", RSP.PC[RSP.PCi], _SHIFTR( w0, 24, 8 ), w0, w1 );
#endif

		/* [Xbox360] sonda temporal, xenon/gln64_prof.h */
		if (GLN64_WD_TICK( RSP.PC[RSP.PCi], RSP.cmd ))
		{
			GLN64_PROF_CNT(salwd);   /* [Xbox360] paso 0 del hilo grafico */
			s_gln64_salida_mala = 1;
			break;
		}

		RSP.PC[RSP.PCi] += 8;
		{
			u32 pci = RSP.PCi;
			if (RSP.count == 1)
				--pci;
			RSP.nextCmd = _SHIFTR( *(u32*)&RDRAM[RSP.PC[pci]], 24, 8 );
		}

		GBI.cmd[RSP.cmd]( w0, w1 );

		if (RSP.count != 0xFFFFFFFF)
		{
			--RSP.count;
			if (RSP.count == 0)
			{
				RSP.count = (u32)-1;
				--RSP.PCi;
			}
		}
	}

/*	if (OGL.frameBufferTextures && gDP.colorImage.changed)
	{
		FrameBuffer_SaveBuffer( gDP.colorImage.address, gDP.colorImage.size, gDP.colorImage.width, gDP.colorImage.height );
		gDP.colorImage.changed = FALSE;
	}*/

	if (RSP.infloop && REG.SP_STATUS)
	{
		/* [Xbox360] ★★ LA SALIDA QUE PROHIBE EL HILO GRAFICO, y no solo por
		 * el DP: aqui se BORRAN los bits de tarea hecha para que el nucleo
		 * REENCOLE la lista (memory.c, do_SP_task).  Con la lista en otro
		 * hilo el emulador ya paso por ese punto y el reencolado se
		 * perderia.  Ver la nota del paso 0 en xenon/gln64_prof.h. */
		GLN64_PROF_CNT(salloop);
		*REG.SP_STATUS &= ~(SP_STATUS_TASKDONE | SP_STATUS_HALT | SP_STATUS_BROKE);
		// Didn't finish the display list, mark it as such for our D/L's counter
		dlist_incomplete();
		return;
	}

	/* [Xbox360] ★ `ok` SOLO cuando el bucle acabo por las buenas.  La primera
	 * version lo contaba siempre, y como el `break` del perro guardian y el
	 * de direccion invalida caen aqui tambien, el log 5 saco `ok=3 wd=3`
	 * siendo las MISMAS tres listas.  Un contador ambiguo hace perder una
	 * iteracion entera. */
	if (!s_gln64_salida_mala)
		GLN64_PROF_CNT(salok);

	DepthCopy_EndDList();

	RSP.busy = FALSE;
	RSP.DList++;
	gSP.changed |= CHANGED_COLORBUFFER;
}

extern "C" void DebugOutputString( const char *s );

static void _RSP_SetGameHacks()
{
	RSP.romname[0] = 0;

	if (HEADER != NULL)
	{
		for (int i = 0; i < 20; i++)
			RSP.romname[i] = (char)HEADER[32 + i];
		RSP.romname[20] = 0;

		// Remove all trailing spaces.
		size_t len = strlen( RSP.romname );
		while (len > 0 && RSP.romname[len - 1] == ' ')
			RSP.romname[--len] = 0;
	}

	gDPSetDepthClearColor();
	config.generalEmulation.hacks = 0;

	/* [Xbox360] ★ EL NOMBRE EN MAYUSCULAS, PARA LAS ENTRADAS QUE VIENEN DEL
	 * GLideN64.custom.ini.  GLideN64 busca su seccion con el nombre interno
	 * pasado a mayusculas (mupenplus/Config_mupenplus.cpp:347, `::toupper`), y
	 * la cabecera no siempre lo trae asi: Banjo-Kazooie dice "Banjo-Kazooie" y
	 * su seccion es [BANJO-KAZOOIE].  Con `RSP.romname` tal cual no casaba y su
	 * copia a la RDRAM no se activaba (log: `[CCOPIA] pedidas=0`).  Las
	 * comprobaciones que vienen del CODIGO de GLideN64 (RSP.cpp) siguen con
	 * `RSP.romname`, que alli tambien distingue mayusculas. */
	char nombreIni[21];
	{
		int i;
		for (i = 0; i < 20 && RSP.romname[i] != 0; i++)
		{
			const char c = RSP.romname[i];
			nombreIni[i] = (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
		}
		nombreIni[i] = 0;
	}

	if (strstr( RSP.romname, "THE LEGEND OF ZELDA" ) != NULL ||
	    strstr( RSP.romname, "ZELDA MASTER QUEST" ) != NULL)
		config.generalEmulation.hacks |= hack_subscreen | hack_copyDepthToRDRAM;
	else if (strstr( RSP.romname, "DOUBUTSUNOMORI" ) != NULL ||
	         strstr( RSP.romname, "ANIMAL FOREST" ) != NULL)
		config.generalEmulation.hacks |= hack_subscreen;
	else if (strstr( RSP.romname, "Perfect Dark" ) != NULL ||
	         strstr( RSP.romname, "PERFECT DARK" ) != NULL)
		config.generalEmulation.hacks |= hack_rectDepthBufferCopyPD | hack_clearAloneDepthBuffer;
	else if (strstr( RSP.romname, "TUROK_DINOSAUR_HUNTE" ) != NULL)
		config.generalEmulation.hacks |= hack_rectDepthBufferCopyPD; // TODO check if this works here
	else if (strstr( RSP.romname, "GOLDENEYE" ) != NULL)
		config.generalEmulation.hacks |= hack_clearAloneDepthBuffer;
	else if (strstr( RSP.romname, "CONKER BFD" ) != NULL ||
	         strstr( RSP.romname, "MarioTennis" ) != NULL)
	{
		config.generalEmulation.hacks |= hack_fbTextureOffset;
		/* [Xbox360] [Conker, sombra cuadrada] GLideN64 pone
		 * `texturedRectCopyToItself` SOLO para "CONKER BFD"
		 * (GraphicsDrawer::_setSpecialTexrect).  Ver gDPTextureRectangle. */
		if (strstr( RSP.romname, "CONKER BFD" ) != NULL)
			config.generalEmulation.hacks |= hack_texrectCopyToItself;
	}
	/* [Xbox360] [Zelda MM, Lente de la Verdad] Y hack_zeldaMMLente: la copia del
	 * Z por los rectangulos de fondo de S2DEX de GLideN64 5347689b, que alli va
	 * con hack_ZeldaMM (su RSP.cpp lo pone con el mismo `strstr("MASK")`).
	 * Ver gSPBgRect1Cyc / gSPBgRectCopy en gSP.cpp. */
	else if (strstr( RSP.romname, "MASK" ) != NULL) // ZELDA MAJORA'S MASK
		config.generalEmulation.hacks |= hack_fbCopyToRDRAM | hack_copyDepthToRDRAM |
		                                  hack_zeldaMMLente;
	else if (strstr( RSP.romname, "DONKEY KONG 64" ) != NULL ||
	         strstr( RSP.romname, "D K DISPLAY" ) != NULL)
		config.generalEmulation.hacks |= hack_copyDepthToRDRAM;
	else if (strstr( RSP.romname, "Quake" ) != NULL)
		config.generalEmulation.hacks |= hack_doNotResetOtherModeH | hack_doNotResetOtherModeL;
	/* [Xbox360] ★ [Quake II: objetos a traves de las paredes, modelos que
	 * parpadean] TAMBIEN otherMode.l, aunque GLideN64 solo le da el H (su
	 * RSP.cpp; para "Quake" -- Quake 64 -- da los dos, 732c27a0, #1169).
	 * MEDIDO (sonda [Q2Z], ya con el reinicio en el swap): ~1800 de cada 32768
	 * triangulos con el mux de los modelos (0030FE6E:FFFEFC38) salian con omL
	 * 00000000 -- el modo REINICIADO al empezar el frame, antes de que el juego
	 * lo vuelva a poner --, sin test de Z.  El RDP real no reinicia nada entre
	 * frames.  Es la queja de GLideN64 #1589 ("the corpse and other elements
	 * has wrong depth and flickers"), que alli cambiaba al mover el swap. */
	else if (strstr( RSP.romname, "QUAKE II" ) != NULL)
		config.generalEmulation.hacks |= hack_doNotResetOtherModeH | hack_doNotResetOtherModeL;
	else if (strstr( RSP.romname, "GAUNTLET LEGENDS" ) != NULL)
		config.generalEmulation.hacks |= hack_doNotResetOtherModeH;
	/* [Xbox360] ★ [ISS64, sombras de los jugadores] Las tres entradas de
	 * GLideN64.custom.ini con frameBufferEmulation\N64DepthCompare=1 (GLideN64
	 * 2cbf2b52 "Fix ISS64 shadows"); los nombres son los de sus secciones,
	 * [I%20S%20S%2064], [PERFECT%20STRIKER] y [J%20WORLD%20SOCCER3].  Aqui
	 * solo el modo decal; ver hack_n64DepthCompareDec en OGL_UpdateStates. */
	else if (strstr( nombreIni, "I S S 64" ) != NULL ||
	         strstr( nombreIni, "PERFECT STRIKER" ) != NULL ||
	         strstr( nombreIni, "J WORLD SOCCER3" ) != NULL)
		config.generalEmulation.hacks |= hack_n64DepthCompareDec;
	/* [Xbox360] ★ [Dr. Mario 64, pildoras] GLideN64.custom.ini
	 * [DR.MARIO%2064]: copyFromRDRAM=1, copyToRDRAM=0.  La CPU pinta la
	 * pildora en el color buffer de la RDRAM encima del frame del RDP.  Ver
	 * VI_GX_copiaDesdeRDRAM (VI.cpp). */
	else if (strstr( nombreIni, "DR.MARIO 64" ) != NULL)
		config.generalEmulation.hacks |= hack_copyFromRDRAM;
	/* [Xbox360] ★ [Mario Kart 64 TV, puzzle de Banjo-Kazooie]
	 * GLideN64.custom.ini [MARIOKART64] copyToRDRAM=2 y [BANJO-KAZOOIE]
	 * copyToRDRAM=1 (e8a5e829): la CPU lee el color buffer de la RDRAM.  Ver
	 * _gDPCopiaColorARDRAM (gDP.cpp). */
	else if (strstr( nombreIni, "MARIOKART64" ) != NULL)
		config.generalEmulation.hacks |= hack_copyToRDRAM;
	/* [Xbox360] ★ [Banjo-Kazooie, piezas negras al montarse tras la carga,
	 * 2026-10-24] copyToRDRAM=1 es ctSync, no el doble buffer: la copia espera
	 * a la GPU (hack_copyToRDRAMSync, ver GXD3D_CopiaColorARDRAMSincrona).
	 * Para volver a la de 1-3 frames tarde basta con quitar ese bit. */
	else if (strstr( nombreIni, "BANJO-KAZOOIE" ) != NULL)
		config.generalEmulation.hacks |= hack_copyToRDRAM | hack_copyToRDRAMSync;
	/* [Xbox360] ★ [Mario Golf, la transicion entre golpes: el frame anterior
	 * que se retira salia NEGRO] LA COPIA DEL COLOR A LA RDRAM.  GLideN64
	 * d06befb4: "CPU copies color frame buffer for replays into depth buffer
	 * area" -- la CPU copia lo que hay en la RDRAM, y GLideN64 pone ahi el color
	 * de cada frame POR DEFECTO (Config.cpp: copyToRDRAM = ctSync/ctDoubleBuffer)
	 * y [MARIOGOLF64] no lo quita (solo pone copyDepthToRDRAM=0, que aqui ya es
	 * lo que hay).  Aqui solo existe como hack_copyToRDRAM. */
	else if (strstr( nombreIni, "MARIOGOLF64" ) != NULL)
		config.generalEmulation.hacks |= hack_copyToRDRAM;
	/* [Xbox360] ★ [Castlevania, la lente del menu: salia OPACA] LA COPIA DEL
	 * COLOR A LA RDRAM, como Mario Golf: GLideN64 la pone POR DEFECTO y
	 * GLideN64.custom.ini la fuerza en [CASTLEVANIA2] (copyToRDRAM=1).  Medido
	 * con una sonda temporal en las cargas (logs 29-30): de 65536 cargas de
	 * textura, NINGUNA cae en los buffers principales (00200000, 00225800,
	 * 0024B000), asi que el RDP no los lee como textura; los lee la CPU de la
	 * RDRAM.  Casa con "CASTLEVANIA" y "CASTLEVANIA2" (Legacy of Darkness). */
	else if (strstr( nombreIni, "CASTLEVANIA" ) != NULL)
		config.generalEmulation.hacks |= hack_copyToRDRAM;
	/* [Xbox360] [Hexen] GLideN64.custom.ini [HEXEN]: copyToRDRAM=1.  OJO: NO es
	 * lo que saca las manos/arma -- eso era el umbral de cvgXAlpha (ver
	 * OGL_UpdateStates) --; se deja por fidelidad al ini, sin medir que lo
	 * necesite. */
	else if (strstr( nombreIni, "HEXEN" ) != NULL)
		config.generalEmulation.hacks |= hack_copyToRDRAM;
	/* [Xbox360] ★ [1080 Snowboarding, 2026-10-22] GLideN64 d4dce42e/d4692426:
	 * [1080%20SNOWBOARDING] copyToRDRAM=1 y copyDepthToRDRAM=1, "essential for
	 * correct work" (hoy sin entrada: son los valores por defecto de GLideN64).
	 * Sin la de Z el sol se veia a traves de las paredes (el juego lee el Z de
	 * la RDRAM); sin la del color, una pantalla del juego salia con basura.
	 * Probado el 2026-10-22 (logs 26-27): las dos cosas bien.  VI de 320x238 en
	 * un EFB de 480: ver DepthCopy_Active.  Ya no va en "foto" (gamehacks.c):
	 * con foto nada de esto llegaria al juego. */
	else if (strstr( nombreIni, "1080 SNOWBOARDING" ) != NULL)
		config.generalEmulation.hacks |= hack_copyToRDRAM | hack_copyDepthToRDRAM;
	/* [Xbox360] [Star Wars Episode I Racer, 2026-10-22] El sol se veia a traves
	 * de las paredes, como en 1080: el juego lee el Z de la RDRAM para saber si
	 * esta tapado.  GLideN64 no tiene entrada (copyDepthToRDRAM es su valor por
	 * defecto).  Solo la de Z: la del color no hace falta para esto y cuesta
	 * ~4 ms por imagen.  Nombre interno de la cabecera: "STAR WARS EP1 RACER"
	 * (comprobar en `[GLN64] rom interna=`).  Ojo: DepthCopy_Active exige un
	 * EFB al doble en horizontal; en el modo de alta resolucion del juego (con
	 * Expansion Pak) no se activaria. */
	else if (strstr( nombreIni, "EP1 RACER" ) != NULL)
		config.generalEmulation.hacks |= hack_copyDepthToRDRAM;
	/* [Xbox360] [Excitebike 64, 2026-10-23] Las luces del circuito se veian a
	 * traves del terreno: el mismo caso que 1080 y Episode I Racer, el juego
	 * mira en el Z de la RDRAM si estan tapadas.  GLideN64 no tiene entrada
	 * porque copia el Z a la RDRAM por defecto (Config.cpp:
	 * copyDepthToRDRAM = cdSoftwareRender); aqui solo por juego.  Sin la del
	 * color.  "EXCITEBIKE" cubre el nombre interno con o sin el " 64"
	 * (comprobar en `[GLN64] rom interna=`).  Que se copia de verdad lo dice
	 * [ZCOPIA] (pedidas/entregadas); en alta resolucion DepthCopy_Active no
	 * se activaria (ver la nota de arriba). */
	else if (strstr( nombreIni, "EXCITEBIKE" ) != NULL)
		config.generalEmulation.hacks |= hack_copyDepthToRDRAM;
	/* [Xbox360] [Paper Mario / Mario Story] GLideN64.custom.ini [PAPER%20MARIO]
	 * y [MARIO%20STORY]: copyToRDRAM=1 (0ad54a88).  Y lo que GLideN64 pone en
	 * su RSP.cpp para los mismos nombres y que solo actua sobre esa copia:
	 * hack_paper_mario_subscreen (1f4d04f4, "Fix Paper Mario subscreen
	 * delay"), aqui hack_paperMarioSubscreen.  Y su texrect especial
	 * (GraphicsDrawer::_setSpecialTexrect: FOUND("PAPER MARIO") ||
	 * FOUND("MARIO STORY") -> texturedRectPaletteMod), aqui
	 * hack_texrectPaletteMod: ver _gDPTexrectPaletteMod en gDP.cpp.  EN
	 * PRUEBA. */
	else if (strstr( nombreIni, "PAPER MARIO" ) != NULL ||
	         strstr( nombreIni, "MARIO STORY" ) != NULL)
		config.generalEmulation.hacks |= hack_copyToRDRAM | hack_paperMarioSubscreen |
		                                  hack_texrectPaletteMod;
	/* [Xbox360] [Beetle Adventure Racing, la niebla] El otro texrect especial
	 * de GLideN64 (GraphicsDrawer::_setSpecialTexrect, 18437d82, hoy con
	 * 10b68176): FOUND("BEETLE") || FOUND("HSV") || FOUND("DUCK DODGERS") ||
	 * FOUND("DAFFY DUCK") -> texturedRectShadowMap, con el nombre en
	 * mayusculas como alli.  Ver _gDPTexrectNieblaZ en gDP.cpp. */
	else if (strstr( nombreIni, "BEETLE" ) != NULL ||
	         strstr( nombreIni, "HSV" ) != NULL ||
	         strstr( nombreIni, "DUCK DODGERS" ) != NULL ||
	         strstr( nombreIni, "DAFFY DUCK" ) != NULL)
	{
		config.generalEmulation.hacks |= hack_texrectShadowMap;
		/* [Xbox360] Y su transicion de menus con dos buffers: solo Beetle (el
		 * juego del caso).  Ver FrameBuffer_RenderBuffer: sin el resto del
		 * renderBuffer de GLideN64, en general hacia temblar DK64. */
		if (strstr( nombreIni, "BEETLE" ) != NULL || strstr( nombreIni, "HSV" ) != NULL)
			config.generalEmulation.hacks |= hack_viDosBuffers;
	}
	/* [Xbox360] [Resident Evil 2] GLideN64 RSP.cpp (5e51b386): "Resident Evil
	 * II" / "BioHazard II" -> hack_RE2 | hack_ModifyVertexXyInShader |
	 * hack_LoadDepthTextures; aqui todo va en hack_RE2.  Y GLideN64.custom.ini
	 * (dff219f2) [RESIDENT%20EVIL%20II] / [BioHazard%20II]: copyFromRDRAM=1
	 * ("necessary for end game credits"), copyToRDRAM=0 y copyDepthToRDRAM=0,
	 * que aqui ya son lo que hay.  Por nombre en mayusculas, que cubre las dos
	 * formas. */
	else if (strstr( nombreIni, "RESIDENT EVIL II" ) != NULL ||
	         strstr( nombreIni, "BIOHAZARD II" ) != NULL)
		config.generalEmulation.hacks |= hack_RE2 | hack_copyFromRDRAM;

	/* ★ [Xbox360] OJO AL TOCAR ESTA CADENA: casa por NOMBRE INTERNO de la
	 * cabecera de la ROM y es `else if`, asi que el PRIMER `strstr` que
	 * case se lleva el juego.  "MASK" es subcadena corta y puede capturar
	 * titulos que no son Majora's Mask.  Comprobado en su dia con una sonda
	 * temporal: Donkey Kong 64 se llama "DONKEY KONG 64".  Desde Wii64
	 * 443f68b tiene su propia entrada (hack_copyDepthToRDRAM). */

	/* [Xbox360] Una linea por carga: el nombre interno y los arreglos por
	 * juego que han quedado puestos.  Sin ella, que una entrada no case solo
	 * se ve por sus efectos (Banjo, log 2). */
#if XBOX64_TRAZAS
	{
		char l[96];
		sprintf( l, "[GLN64] rom interna=\"%s\" hacks=%08X",
		         RSP.romname, (unsigned)config.generalEmulation.hacks );
		DebugOutputString( l );
	}
#endif
}

void RSP_Init()
{
        //u8 test;
	//u32 testAddress;

	// Calculate RDRAM size by intentionally causing an access violation
#ifndef __LINUX__
	__try
	{

		testAddress = 0;
		while (TRUE)
		{
			test = RDRAM[testAddress];
			testAddress++;
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		RDRAMSize = testAddress;
	}
#else // !__LINUX__
# ifdef __GX__
#  ifdef USE_EXPANSION
	RDRAMSize = 1024 * 1024 * 8;
#  else
	RDRAMSize = 1024 * 1024 * 4;
#  endif
# else // __GX__
	RDRAMSize = 1024 * 1024 * 8;
# endif // !__GX__
#endif // __LINUX__

	RSP.DList = 0;
	RSP.uc_start = RSP.uc_dstart = 0;
	RSP.infloop = FALSE;
	
	memset( &gSP, 0, sizeof( gSPInfo ) );
	/* [Xbox360] ★ [Diddy Kong Racing: todo de color plano] LA ESCALA DE
	 * TEXTURA EMPIEZA EN 1, no en el 0 del memset.  DKR solo manda
	 * G_TEXTURE BB000000/00000000 (sc=0, on=0; sonda [DKRTXC], Logs Salvia
	 * (12)) y enciende la textura en cada G_DMA_TRI.  Con el gSPTexture de
	 * GLideN64 7914c415 (con on=0 no toca la escala) se quedaba en 0: las tres
	 * UV de cada triangulo iguales y un solo texel por poligono ([DKRUV],
	 * log 11: esc=0, uv=0.0156 en los tres).  En el glN64 de Wii64 gSPTexture
	 * copiaba la escala aunque on fuera 0 y su guarda (0 -> 1.0) la dejaba en
	 * 1.0: es el mismo valor.  Un juego que mande G_TEXTURE encendido la
	 * sobrescribe en el primer comando, como antes.  Confirmado (log 13,
	 * sondas retiradas): esc=1 y UV distintas; Goemon 2, Mystical Ninja y Jet
	 * Force Gemini de control. */
	gSP.texture.scales = 1.0f;
	gSP.texture.scalet = 1.0f;
	gDP.otherMode._u64 = 0;
	gDP.m_subscreen = false;
	gDP.m_fbCopyPending = 0;
	gDP.m_fbCopySource = 0;

#if GLN64_RESET_RDP_AL_CARGAR
	/* [Xbox360] El estado del RDP que NADIE reinicia al cambiar de ROM.
	 *
	 * Sin esto, `gDP.tiles[]`, `gDP.loadInfo[]` y la TMEM llegan al juego
	 * nuevo con lo que dejo el anterior, y son justo los tres que deciden el
	 * tamano de la textura de un texrect.  Sintoma medido: el marcador POWER
	 * de Mario 64 sale bien en arranque limpio y mal si antes se cargo otro
	 * juego.  La explicacion larga -- incluido que el hueco es de upstream y
	 * no del port-- esta en GLN64_RESET_RDP_AL_CARGAR (xenon/gln64_prof.h).
	 *
	 * Va ANTES de reponer `gDP.loadTile` y `gSP.textureTile[]`, que apuntan
	 * dentro de `gDP.tiles[]`: el memset borra el contenido, no los punteros,
	 * pero dejarlos reasignados DESPUES es lo que mantiene el invariante a la
	 * vista de quien lea la funcion. */
	memset( gDP.tiles,    0, sizeof( gDP.tiles ) );
	memset( gDP.loadInfo, 0, sizeof( gDP.loadInfo ) );
	memset( TMEM,         0, sizeof( TMEM ) );
	/* [Xbox360] ★ PEGADO AL memset DE LA TMEM, Y NO EN OTRO SITIO.
	 *
	 * `gDP.paletteCRC16/256` son una CACHE de un trozo de la TMEM (ver
	 * gDP.h).  Si se borra la TMEM y no se borran ellos, el juego nuevo
	 * empieza con el CRC de paleta del anterior y se sirven texturas
	 * equivocadas -- la clase de fallo de RECARGA DE JUEGO que este arbol ya
	 * ha pagado en atari800, px68k, Frodo, prboom y nestopia.
	 *
	 * Hoy no cambia nada porque este bloque esta apagado, y justo por eso va
	 * DENTRO del `#if`: el dia que alguien lo encienda, las dos cosas se
	 * mueven juntas y no hay forma de que se separen. */
	memset( gDP.paletteCRC16, 0, sizeof( gDP.paletteCRC16 ) );
	gDP.paletteCRC256 = 0;
#endif

	_RSP_SetGameHacks();
	DepthCopy_Reset();

	gDP.loadTile = &gDP.tiles[7];
	gSP.textureTile[0] = &gDP.tiles[0];
	gSP.textureTile[1] = &gDP.tiles[1];
	DepthBuffer_Init();
	GBI_Init();
	OGL_Start();
	/* [Xbox360] Las sondas empiezan de cero con cada ROM (gln64_prof.h). */
#if XBOX64_TRAZAS
	gln64_sondas_reinicia();
#endif

#if GLN64_RESET_RDP_AL_CARGAR
	/* ★ ESTA LINEA SI ES DE UPSTREAM, y en este build no se ejecuta nunca.
	 * `RSP.cpp:222` la hace despues de reinicializar FrameBuffer, TextureCache
	 * y Combiner -- pero dentro del bloque `RSPTHREAD`, y `RomOpen` coge la
	 * rama `#else`.  Forzar "todo cambiado" obliga a rederivar el estado
	 * completo en el primer dibujo del juego nuevo en vez de heredar lo que
	 * el anterior hubiera dejado ya marcado como al dia. */
	gSP.changed = gDP.changed = 0xFFFFFFFF;
#endif
}
