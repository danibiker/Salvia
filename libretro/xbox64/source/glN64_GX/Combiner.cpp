/**
 * glN64_GX - Combiner.cpp
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
#include "../gui/DEBUG.h"
#include <stdio.h>
#endif // __GX__

#ifndef __LINUX__
# include <windows.h>
#else
# include "../main/winlnxdefs.h"
# include <stdlib.h>
#endif // __LINUX__
#include "OpenGL.h"
#include "Combiner.h"
#ifndef __GX__
# include "NV_register_combiners.h"
# include "texture_env_combine.h"
# include "texture_env.h"
#else // !__GX__
# include "TEV_combiner.h"
#endif // __GX__
#include "Debug.h"
#include "gDP.h"

CombinerInfo combiner;

void Combiner_Init()
{
#ifndef __GX__
	if (OGL.NV_register_combiners)
		combiner.compiler = NV_REGISTER_COMBINERS;
	else if (OGL.EXT_texture_env_combine || OGL.ARB_texture_env_combine)
		combiner.compiler = TEXTURE_ENV_COMBINE;
	else
		combiner.compiler = TEXTURE_ENV;
#else // !__GX__
//	combiner.compiler = TEXTURE_ENV;	//Use simple combining. (removed from Makefile)
	combiner.compiler = TEV_COMBINE;	//Use GX combiner designed by sepp256.
#endif // __GX__

	switch (combiner.compiler)
	{
#ifndef __GX__
		case TEXTURE_ENV_COMBINE:
			Init_texture_env_combine();
			break;

		case NV_REGISTER_COMBINERS:
			Init_NV_register_combiners();
			break;
		case TEXTURE_ENV:
			Init_texture_env();
			break;
#else // !__GX__
		case TEV_COMBINE:
#ifdef SHOW_DEBUG
			DEBUG_print((char*)"Combiner: TEV_Combiner",DBG_CCINFO);
#endif
			Init_TEV_combine();
			break;
#endif // __GX__
	}
	combiner.root = NULL;
}

void Combiner_UpdateCombineColors()
{
	switch (combiner.compiler)
	{
#ifndef __GX__
		case TEXTURE_ENV_COMBINE:
			Update_texture_env_combine_Colors( (TexEnvCombiner*)combiner.current->compiled );
			break;

		case NV_REGISTER_COMBINERS:
			Update_NV_register_combiners_Colors( (RegisterCombiners*)combiner.current->compiled );
			break;
#else // !__GX__
		case TEV_COMBINE:
			Update_TEV_combine_Colors( (TEVCombiner*)combiner.current->compiled );
			break;
#endif // __GX__
	}

	gDP.changed &= ~CHANGED_COMBINE_COLORS;
}

void Combiner_SimplifyCycle( CombineCycle *cc, CombinerStage *stage )
{
	// Load the first operand
	stage->op[0].op = LOAD;
	stage->op[0].param1 = cc->sa;
	stage->numOps = 1;

	// If we're just subtracting zero, skip it
	if (cc->sb != ZERO)
	{
		// Subtracting a number from itself is zero
		if (cc->sb == stage->op[0].param1)
			stage->op[0].param1 = ZERO;
		else
		{
			stage->op[1].op = SUB;
			stage->op[1].param1 = cc->sb;
			stage->numOps++;
		}
	}

	// If we either subtracted, or didn't load a zero
	if ((stage->numOps > 1) || (stage->op[0].param1 != ZERO))
	{
		// Multiplying by zero is zero
		if (cc->m == ZERO)
		{
			stage->numOps = 1;
			stage->op[0].op = LOAD;
			stage->op[0].param1 = ZERO;
		}
		else
		{
			// Multiplying by one, so just do a load
			if ((stage->numOps == 1) && (stage->op[0].param1 == ONE))
				stage->op[0].param1 = cc->m;
			else
			{
				stage->op[stage->numOps].op = MUL;
				stage->op[stage->numOps].param1 = cc->m;
				stage->numOps++;
			}
		}
	}

	// Don't bother adding zero
	if (cc->a != ZERO)
	{
		// If all we have so far is zero, then load this instead
		if ((stage->numOps == 1) && (stage->op[0].param1 == ZERO))
			stage->op[0].param1 = cc->a;
		else
		{
			stage->op[stage->numOps].op = ADD;
			stage->op[stage->numOps].param1 = cc->a;
			stage->numOps++;
		}
	}

	// Handle interpolation
	if ((stage->numOps == 4) && (stage->op[1].param1 == stage->op[3].param1))
	{
		stage->numOps = 1;
		stage->op[0].op = INTER;
		stage->op[0].param2 = stage->op[1].param1;
		stage->op[0].param3 = stage->op[2].param1;
	}
}

void Combiner_MergeStages( Combiner *c )
{
	// If all we have is a load in the first stage we can just replace
	// each occurance of COMBINED in the second stage with it
	if ((c->stage[0].numOps == 1) && (c->stage[0].op[0].op == LOAD))
	{
		int combined = c->stage[0].op[0].param1;

		for (int i = 0; i < c->stage[1].numOps; i++)
		{
			c->stage[0].op[i].op = c->stage[1].op[i].op;
			c->stage[0].op[i].param1 = (c->stage[1].op[i].param1 == COMBINED) ? combined : c->stage[1].op[i].param1;
			c->stage[0].op[i].param2 = (c->stage[1].op[i].param2 == COMBINED) ? combined : c->stage[1].op[i].param2;
			c->stage[0].op[i].param3 = (c->stage[1].op[i].param3 == COMBINED) ? combined : c->stage[1].op[i].param3;
		}

		c->stage[0].numOps = c->stage[1].numOps;
		c->numStages = 1;
	}
	// We can't do any merging on an interpolation
	else if (c->stage[1].op[0].op != INTER)
	{
		int numCombined = 0;

		// See how many times the first stage is used in the second one
		for (int i = 0; i < c->stage[1].numOps; i++)
			if (c->stage[1].op[i].param1 == COMBINED)
				numCombined++;

		// If it's not used, just replace the first stage with the second
		if (numCombined == 0)
		{
			for (int i = 0; i < c->stage[1].numOps; i++)
			{
				c->stage[0].op[i].op = c->stage[1].op[i].op;
				c->stage[0].op[i].param1 = c->stage[1].op[i].param1;
				c->stage[0].op[i].param2 = c->stage[1].op[i].param2;
				c->stage[0].op[i].param3 = c->stage[1].op[i].param3;
			}
			c->stage[0].numOps = c->stage[1].numOps;

			c->numStages = 1;
		}
		// If it's only used once
		else if (numCombined == 1)
		{
			// It's only used in the load, so tack on the ops from stage 2 on stage 1
			if (c->stage[1].op[0].param1 == COMBINED)
			{
				for (int i = 1; i < c->stage[1].numOps; i++)
				{
					c->stage[0].op[c->stage[0].numOps].op = c->stage[1].op[i].op;
					c->stage[0].op[c->stage[0].numOps].param1 = c->stage[1].op[i].param1;
					c->stage[0].numOps++;
				}

				c->numStages = 1;
			}
			// Otherwise, if it's used in the second op, and that op isn't SUB
			// we can switch the parameters so it works out to tack the ops onto stage 1
			else if ((c->stage[1].op[1].param1 == COMBINED) && (c->stage[1].op[1].op != SUB))
			{
				c->stage[0].op[c->stage[0].numOps].op = c->stage[1].op[1].op;
				c->stage[0].op[c->stage[0].numOps].param1 = c->stage[1].op[0].param1;
				c->stage[0].numOps++;

				// If there's another op, tack it onto stage 1 too
				if (c->stage[1].numOps > 2)
				{
					c->stage[0].op[c->stage[0].numOps].op = c->stage[1].op[2].op;
					c->stage[0].op[c->stage[0].numOps].param1 = c->stage[1].op[2].param1;
					c->stage[0].numOps++;
				}

				c->numStages = 1;
			}
		}
	}
}

CachedCombiner *Combiner_Compile( u64 mux )
{
	gDPCombine combine;

	combine.mux = mux;

	int numCycles;

	Combiner color, alpha;

	if (gDP.otherMode.cycleType == G_CYC_2CYCLE)
	{
		numCycles = 2;
		color.numStages = 2;
		alpha.numStages = 2;
	}
	else
	{
		numCycles = 1;
		color.numStages = 1;
		alpha.numStages = 1;
	}

	CombineCycle cc[2];
	CombineCycle ac[2];

	// Decode and expand the combine mode into a more general form
	cc[0].sa = saRGBExpanded[combine.saRGB0];
	cc[0].sb = sbRGBExpanded[combine.sbRGB0];
	cc[0].m  = mRGBExpanded[combine.mRGB0];
	cc[0].a  = aRGBExpanded[combine.aRGB0];
	ac[0].sa = saAExpanded[combine.saA0];
	ac[0].sb = sbAExpanded[combine.sbA0];
	ac[0].m  = mAExpanded[combine.mA0];
	ac[0].a  = aAExpanded[combine.aA0];

	cc[1].sa = saRGBExpanded[combine.saRGB1];
	cc[1].sb = sbRGBExpanded[combine.sbRGB1];
	cc[1].m  = mRGBExpanded[combine.mRGB1];
	cc[1].a  = aRGBExpanded[combine.aRGB1];
	ac[1].sa = saAExpanded[combine.saA1];
	ac[1].sb = sbAExpanded[combine.sbA1];
	ac[1].m  = mAExpanded[combine.mA1];
	ac[1].a  = aAExpanded[combine.aA1];

	/* [Xbox360] ★★ [Mario Tennis, las estrellitas del golpe salian como lineas
	 * finas] EN UN CICLO SE USAN LAS ECUACIONES DEL SEGUNDO CICLO, como el RDP y
	 * GLideN64 (Combiner.cpp:230-231, "1 cycle mode uses combiner equations from
	 * 2nd cycle").  glN64 usaba las del PRIMERO.  Casi todos los juegos ponen
	 * los dos iguales y no se nota; la estrella de Mario Tennis (0030FE61:
	 * FFFFFFFB, tex CI8 32x32 en espejo) tiene color PRIM*TEXEL0 en los dos y
	 * ALFA 0 en el primero y PRIM_A en el segundo.  Se pinta en el depth image
	 * 00000400, usado de mascara, y el rect de pantalla que la saca (00157EC0:
	 * F1FFF278, omL 0F0A4345) lleva alpha compare por umbral contra ese alfa:
	 * con el 0 del primer ciclo la estrella se descartaba casi entera y solo
	 * quedaban restos en lineas.  MEDIDO (logs (1)-(5) del 2026-10-24, sondas
	 * ya retiradas): geometria, st, shift x2 y espejo de los tres pasos bien.
	 * Va antes del cruce y de la correccion TEXEL1 -> TEXEL0 de abajo, que
	 * se aplican sobre la etapa 0 = la ecuacion que de verdad se usa. */
	if (gDP.otherMode.cycleType != G_CYC_2CYCLE)
	{
		cc[0] = cc[1];
		ac[0] = ac[1];
	}

	/* [Xbox360] ★★ [Mario Tennis, estela de la bola] EN EL SEGUNDO CICLO LAS
	 * TEXTURAS VAN CRUZADAS, como en la consola y como GLideN64
	 * (glsl_CombinerProgramBuilder.cpp:118-140, `correctSecondStageParam`, que
	 * aplica a la etapa 2 de todo combinador de dos ciclos): TEXEL0 <-> TEXEL1 y
	 * TEXEL0_ALPHA <-> TEXEL1_ALPHA.  glN64 no lo hacia.
	 *
	 * Medido con [FBACT] (log 13): la estela es 00157EC0:F1FFF278, dos ciclos,
	 * con la mascara en la textura 0 (depth image 00000400) y el buffer actual
	 * en la 1 (003AC000).  El segundo ciclo es (1 - TEXEL0)*COMBINED + TEXEL0,
	 * que en la consola es la TEXTURA 1 -- la escena --; sin el cruce glN64 leia
	 * la mascara (negra) y la textura 1 ni se activaba: el cuadrado negro.
	 *
	 * Se hace sobre la forma expandida y ANTES de simplificar, que es lo mismo
	 * que la referencia hace sobre la etapa 2: la fusion de etapas viene despues
	 * y ya ve las entradas buenas.
	 *
	 * ★★ [Banjo-Kazooie, 1080 Snowboarding: todo negro] LOS DOS CICLOS IGUALES
	 * NO SE CRUZAN, como GLideN64 (Combiner.cpp:280-287: `equalStages` ->
	 * numStages = 1, y el cruce solo se aplica con numStages == 2).  Sin esto un juego que pone el MISMO combinador en los
	 * dos ciclos (TEXEL0*SHADE y TEXEL0*SHADE) se quedaba, tras la fusion de
	 * etapas, con TEXEL1*SHADE: una textura que no ha cargado -> negro.  Con el
	 * marcado `textura` (que lee la unidad 0 sin combinador) si se veia.
	 *
	 * Aqui solo se SALTA el cruce y se deja la fusion de glN64 como estaba: con
	 * ciclos iguales el resultado es el de antes de Mario Tennis, al bit. */
	const BOOL ciclosIguales =
	    (cc[0].sa == cc[1].sa && cc[0].sb == cc[1].sb &&
	     cc[0].m  == cc[1].m  && cc[0].a  == cc[1].a  &&
	     ac[0].sa == ac[1].sa && ac[0].sb == ac[1].sb &&
	     ac[0].m  == ac[1].m  && ac[0].a  == ac[1].a) ? TRUE : FALSE;

	if (numCycles == 2 && !ciclosIguales)
	{
		int *ent[8];
		int k;
		ent[0] = &cc[1].sa;  ent[1] = &cc[1].sb;  ent[2] = &cc[1].m;  ent[3] = &cc[1].a;
		ent[4] = &ac[1].sa;  ent[5] = &ac[1].sb;  ent[6] = &ac[1].m;  ent[7] = &ac[1].a;
		for (k = 0; k < 8; k++)
		{
			switch (*ent[k])
			{
				case TEXEL0:       *ent[k] = TEXEL1;       break;
				case TEXEL1:       *ent[k] = TEXEL0;       break;
				case TEXEL0_ALPHA: *ent[k] = TEXEL1_ALPHA; break;
				case TEXEL1_ALPHA: *ent[k] = TEXEL0_ALPHA; break;
			}
		}
	}

	/* [Xbox360] ★ [Paper Mario, el logo: fondo negro y cuadrados blancos] EN UN
	 * CICLO NO HAY TEXEL1: es TEXEL0, como GLideN64 (glsl_CombinerProgramBuilder
	 * .cpp, compileCombiner: con cycleType != 2CYCLE, _correctFirstStageParams
	 * sobre la etapa 0 del color y del alfa: TEXEL1 -> TEXEL0 y TEXEL1_ALPHA ->
	 * TEXEL0_ALPHA).  glN64 leia la tesela siguiente.  El logo es 00FFA7FF:
	 * FF4CFE7F en un ciclo: color = TEXEL0 (tiras de 200x2 en RGBA32), alfa =
	 * TEXEL1_A * PRIM_A.  La tesela siguiente es una mascara de papel de 16x16
	 * CI4 en espejo y con shift 3 (un texel cada 8x8 pixeles), con restos de la
	 * TMEM (53 versiones distintas en el volcado del log 18): salia el fondo
	 * negro de las tiras y los bloques blancos de la mascara.  Con TEXEL0_A el
	 * alfa es el contorno del papel. */
	if (numCycles == 1)
	{
		int *ent[8];
		int k;
		ent[0] = &cc[0].sa;  ent[1] = &cc[0].sb;  ent[2] = &cc[0].m;  ent[3] = &cc[0].a;
		ent[4] = &ac[0].sa;  ent[5] = &ac[0].sb;  ent[6] = &ac[0].m;  ent[7] = &ac[0].a;
		for (k = 0; k < 8; k++)
		{
			if (*ent[k] == TEXEL1)
				*ent[k] = TEXEL0;
			else if (*ent[k] == TEXEL1_ALPHA)
				*ent[k] = TEXEL0_ALPHA;
		}
	}

	for (int i = 0; i < numCycles; i++)
	{
		// Simplify each RDP combiner cycle into a combiner stage
		Combiner_SimplifyCycle( &cc[i], &color.stage[i] );
		Combiner_SimplifyCycle( &ac[i], &alpha.stage[i] );
	}

	/* [Xbox360] ★ [Paper Mario, llamas] EL "SIGN-EXTEND" DE GLideN64, por
	 * canal: las MISMAS condiciones que combinedColorC/ABD y combinedAlphaC/ABD
	 * (glsl_CombinerProgramBuilder.cpp:214-248), sobre el mux tal cual.  Y solo
	 * si el primer ciclo, ya simplificado, puede salirse de [0,1] (tiene SUB o
	 * ADD): si no, envolver no cambia nada y el canal se queda como estaba, al
	 * bit.  Con ciclos iguales tampoco (no hay cruce ni segundo ciclo propio).
	 * Ese canal NO se fusiona: la fusion borra la frontera entre ciclos, que es
	 * justo donde va el envuelto.  El porque, en Compile_TEV_combine. */
	u8 signoColor = 0, signoAlpha = 0;
#ifdef __GX__
	if (numCycles == 2 && !ciclosIguales)
	{
		if (combine.mRGB1 == G_CCMUX_COMBINED)
			signoColor = GXD3D_TEV_WRAP_C;
		else if ((combine.aRGB1 == G_CCMUX_COMBINED) ||
		         ((combine.saRGB1 == G_CCMUX_COMBINED || combine.sbRGB1 == G_CCMUX_COMBINED) &&
		          combine.mRGB1 != G_CCMUX_0))
			signoColor = GXD3D_TEV_WRAP_ABD;

		if (combine.mA1 == G_ACMUX_COMBINED)
			signoAlpha = GXD3D_TEV_WRAP_C;
		else if ((combine.aA1 == G_ACMUX_COMBINED) ||
		         ((combine.saA1 == G_ACMUX_COMBINED || combine.sbA1 == G_ACMUX_COMBINED) &&
		          combine.mA1 != G_ACMUX_0))
			signoAlpha = GXD3D_TEV_WRAP_ABD;

		BOOL salC = FALSE, salA = FALSE;
		for (int i = 0; i < color.stage[0].numOps; i++)
			if (color.stage[0].op[i].op == SUB || color.stage[0].op[i].op == ADD)
				salC = TRUE;
		for (int i = 0; i < alpha.stage[0].numOps; i++)
			if (alpha.stage[0].op[i].op == SUB || alpha.stage[0].op[i].op == ADD)
				salA = TRUE;
		if (!salC)
			signoColor = 0;
		if (!salA)
			signoAlpha = 0;
	}
#endif // __GX__

	/* [Xbox360] ★ [Ridge Racer 64, faros: un rectangulo del color PRIM] EL
	 * COLOR DEL SEGUNDO CICLO PUEDE LEER EL ALFA DEL PRIMERO (COMBINED_ALPHA).
	 * Los faros son 0040B467:F0FFFE3E en dos ciclos:
	 *     ciclo 0: color = SHADE*TEXEL0,  alfa = PRIM_A*TEXEL1_A
	 *     ciclo 1: color = (PRIM - COMBINED)*COMBINED_ALPHA + COMBINED,  alfa = 1
	 * con PRIM_A = 0: sale la textura.  La fusion de glN64 veia que el alfa del
	 * ciclo 1 (ONE) no usa COMBINED y TIRABA el del ciclo 0; el color, una
	 * interpolacion, se quedaba en dos etapas y su COMBINED_ALPHA (APREV en el
	 * TEV) valia 1: el rectangulo entero del color PRIM, rojo B30000 detras y
	 * blanco FEFEFE delante.  Medido con el volcado de texturas (log 17): las
	 * texturas de los faros estan bien.  Con COMBINED_ALPHA en el color del
	 * ciclo 1 no se fusiona ninguno de los dos (la fusion del color tampoco
	 * sabe de COMBINED_ALPHA), y Compile_TEV_combine alinea los ciclos.  Sin
	 * COMBINED_ALPHA ahi, todo sigue al bit como estaba. */
	BOOL leeAlfaCiclo0 = FALSE;
	if (numCycles == 2)
	{
		for (int j = 0; j < color.stage[1].numOps; j++)
			if (color.stage[1].op[j].param1 == COMBINED_ALPHA ||
			    (color.stage[1].op[j].op == INTER &&
			     (color.stage[1].op[j].param2 == COMBINED_ALPHA ||
			      color.stage[1].op[j].param3 == COMBINED_ALPHA)))
				leeAlfaCiclo0 = TRUE;
	}

	if (numCycles == 2)
	{
		// Attempt to merge the two stages into one
		if (!signoColor && !leeAlfaCiclo0)
			Combiner_MergeStages( &color );
		if (!signoAlpha && !leeAlfaCiclo0)
			Combiner_MergeStages( &alpha );
	}

	CachedCombiner *cached = (CachedCombiner*)malloc( sizeof( CachedCombiner ) );

	cached->combine.mux = combine.mux;
	cached->left = NULL;
	cached->right = NULL;

#ifdef DBGCOMBINE
#ifdef GLN64_SDLOG
	u32 mux1 = ((mux >> 32) & 0xFFFFFFFF);
	u32 mux2 = (mux & 0xFFFFFFFF);
//	if (mux2 == 0xfffff9fc) 
//	{
		sprintf(txtbuffer,"Combiner Compile: mux1 = %x, mux2 = %x\n", mux1, mux2);
//		DEBUG_print(txtbuffer,9);
		DEBUG_print(txtbuffer,DBG_SDGECKOPRINT);
		sprintf(txtbuffer," combine.muxs0 = %x, combine.muxs1 = %x\n", combine.muxs0, combine.muxs1);
		DEBUG_print(txtbuffer,DBG_SDGECKOPRINT);
		sprintf(txtbuffer," cc[0]: sa %d, sb %d, m %d, a %d; cc[1]: sa %d, sb %d, m %d, a %d\n", cc[0].sa, cc[0].sb, cc[0].m, cc[0].a, cc[1].sa, cc[1].sb, cc[1].m, cc[1].a);
		DEBUG_print(txtbuffer,DBG_SDGECKOPRINT);
		sprintf(txtbuffer," ac[0]: sa %d, sb %d, m %d, a %d; ac[1]: sa %d, sb %d, m %d, a %d\n", ac[0].sa, ac[0].sb, ac[0].m, ac[0].a, ac[1].sa, ac[1].sb, ac[1].m, ac[1].a);
		DEBUG_print(txtbuffer,DBG_SDGECKOPRINT);
		sprintf(txtbuffer," Cyc = %d, colStag = %d (1) %d (2) %d, alphaStag = %d (1) %d (2) %d\n", numCycles, color.numStages, color.stage[0].numOps, color.stage[1].numOps, alpha.numStages, alpha.stage[0].numOps, alpha.stage[1].numOps);
//		DEBUG_print(txtbuffer,10);
		DEBUG_print(txtbuffer,DBG_SDGECKOPRINT);
		for (int i = 0; i < 6; i++)
		{
			sprintf(txtbuffer," op %d col (1) op %d par1 %d (2) op %d par1 %d; alpha (1) op %d par1 %d (2) op %d par1 %d\n", i+1, color.stage[0].op[i].op, color.stage[0].op[i].param1, color.stage[1].op[i].op, color.stage[1].op[i].param1, alpha.stage[0].op[i].op, alpha.stage[0].op[i].param1, alpha.stage[1].op[i].op, alpha.stage[1].op[i].param1);
//			DEBUG_print(txtbuffer,11+i);
			DEBUG_print(txtbuffer,DBG_SDGECKOPRINT);
		}
//	}
#endif // GLN64_SDLOG
#endif // DBGCOMBINE

	// Send the simplified combiner to the hardware-specific compiler
	switch (combiner.compiler)
	{
#ifndef __GX__
		case TEXTURE_ENV_COMBINE:
			cached->compiled = (void*)Compile_texture_env_combine( &color, &alpha );
			break;

		case NV_REGISTER_COMBINERS:
			cached->compiled = (void*)Compile_NV_register_combiners( &color, &alpha );
			break;

		case TEXTURE_ENV:
			cached->compiled = (void*)Compile_texture_env( &color, &alpha );
			break;
#else // !__GX__
		case TEV_COMBINE:
			cached->compiled = (void*)Compile_TEV_combine( &color, &alpha, signoColor, signoAlpha );
			break;
#endif // __GX__
	}

#if 0 //def DBGCOMBINE
//	if (mux2 == 0xfffff9fc) 
//	{
		sprintf(txtbuffer,"Combiner: mux1 = %x, mux2 = %x", mux1, mux2);
		DEBUG_print(txtbuffer,17);
		TEVCombiner* TEVcombiner = (TEVCombiner*)cached->compiled;
		sprintf(txtbuffer,"TEV:%d stages, %d tex, %d chans", TEVcombiner->numTevStages, TEVcombiner->numTexGens, TEVcombiner->numColChans);
		DEBUG_print(txtbuffer,18);
	
		for (u8 tevstage = 0; tevstage < TEVcombiner->numTevStages; tevstage++)
		{
			sprintf(txtbuffer,"stage%d: tx %d, map %d, col %d; inCol %d, %d, %d, %d; ColorOp %d, ColorOut %d", tevstage, TEVcombiner->TEVstage[tevstage].texcoord, TEVcombiner->TEVstage[tevstage].texmap, TEVcombiner->TEVstage[tevstage].color, TEVcombiner->TEVstage[tevstage].colorA, TEVcombiner->TEVstage[tevstage].colorB, TEVcombiner->TEVstage[tevstage].colorC, TEVcombiner->TEVstage[tevstage].colorD, TEVcombiner->TEVstage[tevstage].colorTevop, TEVcombiner->TEVstage[tevstage].colorTevRegOut);
			DEBUG_print(txtbuffer,19+2*tevstage);
			sprintf(txtbuffer," Kcol %d, Kalp %d;  inAlpha %d, %d, %d, %d; AlphaOp %d, AlphaOut %d", TEVcombiner->TEVstage[tevstage].tevKColSel, TEVcombiner->TEVstage[tevstage].tevKAlphaSel, TEVcombiner->TEVstage[tevstage].alphaA, TEVcombiner->TEVstage[tevstage].alphaB, TEVcombiner->TEVstage[tevstage].alphaC, TEVcombiner->TEVstage[tevstage].alphaD, TEVcombiner->TEVstage[tevstage].alphaTevop, TEVcombiner->TEVstage[tevstage].alphaTevRegOut);
			DEBUG_print(txtbuffer,20+2*tevstage);
		}
//	}
#endif

	return cached;
}

void Combiner_DeleteCombiner( CachedCombiner *combiner )
{
	if (combiner->left) Combiner_DeleteCombiner( combiner->left );
	if (combiner->right) Combiner_DeleteCombiner( combiner->right );

	free( combiner->compiled );
	free( combiner );
}

void Combiner_Destroy()
{
	if (combiner.root)
	{
		Combiner_DeleteCombiner( combiner.root );
		combiner.root = NULL;
	}

#ifndef __GX__
	for (int i = 0; i < OGL.maxTextureUnits; i++)
	{
		glActiveTextureARB( GL_TEXTURE0_ARB + i );
		glDisable( GL_TEXTURE_2D );
	}
#endif // !__GX__
}

void Combiner_BeginTextureUpdate()
{
#ifndef __GX__
	switch (combiner.compiler)
	{
		case TEXTURE_ENV_COMBINE:
			BeginTextureUpdate_texture_env_combine();
			break;
	}
#endif // !__GX__
}

void Combiner_EndTextureUpdate()
{
#ifndef __GX__
	switch (combiner.compiler)
	{
		case TEXTURE_ENV_COMBINE:
			//EndTextureUpdate_texture_env_combine();
			Set_texture_env_combine( (TexEnvCombiner*)combiner.current->compiled );
			break;
	}
#endif // !__GX__
}

DWORD64 Combiner_EncodeCombineMode( WORD saRGB0, WORD sbRGB0, WORD mRGB0, WORD aRGB0,
								 WORD saA0,   WORD sbA0,   WORD mA0,   WORD aA0,
								 WORD saRGB1, WORD sbRGB1, WORD mRGB1, WORD aRGB1,
								 WORD saA1,   WORD sbA1,   WORD mA1,   WORD aA1 )
{	// 2 args fixed by sepp256, but this function isn't used anywhere.
	return (((DWORD64)CCEncodeA[saRGB0] << 52) | ((DWORD64)CCEncodeB[sbRGB0] << 28) | ((DWORD64)CCEncodeC[mRGB0] << 47) | ((DWORD64)CCEncodeD[aRGB0] << 15) |
		    ((DWORD64)ACEncodeA[saA0] << 44) | ((DWORD64)ACEncodeB[sbA0] << 12) | ((DWORD64)ACEncodeC[mA0] << 41) | ((DWORD64)ACEncodeD[aA0] << 9) |
//			((DWORD64)CCEncodeA[saRGB1] << 37) | ((DWORD64)CCEncodeB[sbRGB1] << 24) | ((DWORD64)CCEncodeC[mRGB1]      ) | ((DWORD64)CCEncodeD[aRGB1] <<  6) |
			((DWORD64)CCEncodeA[saRGB1] << 37) | ((DWORD64)CCEncodeB[sbRGB1] << 24) | ((DWORD64)CCEncodeC[mRGB1] << 32) | ((DWORD64)CCEncodeD[aRGB1] <<  6) |
//			((DWORD64)ACEncodeA[saA1] << 18) | ((DWORD64)ACEncodeB[sbA1] <<  3) | ((DWORD64)ACEncodeC[mA1] << 18) | ((DWORD64)ACEncodeD[aA1]     ));
			((DWORD64)ACEncodeA[saA1] << 21) | ((DWORD64)ACEncodeB[sbA1] <<  3) | ((DWORD64)ACEncodeC[mA1] << 18) | ((DWORD64)ACEncodeD[aA1]     ));
}

void Combiner_SelectCombine( u64 mux )
{
	/* [Xbox360] [Paper Mario, el logo] LA CLAVE DE LA CACHE LLEVA EL MODO DE
	 * CICLO, como el CombinerKey de GLideN64 (mux + cycleType).  Combiner_Compile
	 * compila distinto en dos ciclos (cruce de texturas del ciclo 1, fusion) que
	 * en uno (TEXEL1 -> TEXEL0), y la cache solo miraba el mux: el mismo mux en
	 * los dos modos se quedaba con el primero que se compilo.  El bit 56 cae en
	 * `pad0` (los 8 bits altos de muxs0, que ningun campo usa). */
	if (gDP.otherMode.cycleType == G_CYC_2CYCLE)
		mux |= (u64)1 << 56;

	CachedCombiner *current = combiner.root;
	CachedCombiner *parent = current;

	while (current)
	{
		parent = current;

		if (mux == current->combine.mux)
			break;
		else if (mux < current->combine.mux)
			current = current->left;
		else
			current = current->right;
	}

	if (current == NULL)
	{
		current = Combiner_Compile( mux );

		if (parent == NULL)
			combiner.root = current;
		else if (parent->combine.mux > current->combine.mux)
			parent->left = current;
		else
			parent->right = current;
	}

	combiner.current = current;

	gDP.changed |= CHANGED_COMBINE_COLORS;

#ifdef DBGCOMBINE
#ifdef GLN64_SDLOG
	u32 mux1 = ((mux >> 32) & 0xFFFFFFFF);
	u32 mux2 = (mux & 0xFFFFFFFF);
	sprintf(txtbuffer,"Combiner Select Combine: mux1 = %x, mux2 = %x\n", mux1, mux2);
//	DEBUG_print(txtbuffer,17);
	DEBUG_print(txtbuffer,DBG_SDGECKOPRINT);
#endif // GLN64_SDLOG
#endif // DBGCOMBINE
}

void Combiner_SetCombineStates()
{
	GLN64_PROF_SCOPE(comb);    /* [Xbox360] sonda temporal, xenon/gln64_prof.h */
	switch (combiner.compiler)
	{
#ifndef __GX__
		case TEXTURE_ENV_COMBINE:
			Set_texture_env_combine( (TexEnvCombiner*)combiner.current->compiled );
			break;

		case NV_REGISTER_COMBINERS:
			Set_NV_register_combiners( (RegisterCombiners*)combiner.current->compiled );
			break;

		case TEXTURE_ENV:
			Set_texture_env( (TexEnv*)combiner.current->compiled );
			break;
#else // !__GX__
		case TEV_COMBINE:
			Set_TEV_combine( (TEVCombiner*)combiner.current->compiled );
			break;
#endif // __GX__
	}
}

void Combiner_SetCombine( u64 mux )
{
	Combiner_SelectCombine( mux );
	Combiner_SetCombineStates();
}
