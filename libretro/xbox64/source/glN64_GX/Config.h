/**
 * glN64_GX - Config.h
 * Copyright (C) 2003 Orkin
 *
 * glN64 homepage: http://gln64.emulation64.com
 * Wii64 homepage: http://www.emulatemii.com
 *
**/

#ifndef CONFIG_H
#define CONFIG_H

#include "Types.h"

struct Config
{
	struct
	{
		u32 hacks;
	} generalEmulation;
};

extern Config config;

#define hack_subscreen				(1<<6)  //Fix subscreen delay in Zelda OOT and Doubutsu no Mori
#define hack_clearAloneDepthBuffer	(1<<3)  //Force clear depth buffer if there is no frame buffer for it. Multiplayer in GE and PD.
#define hack_paperMarioSubscreen	(1<<5)  //[Xbox360] GLideN64 1f4d04f4 hack_paper_mario_subscreen: in the color copy to RDRAM, blue += 4 if 0 < b <= 0xFB. Paper Mario subscreen delay
#define hack_rectDepthBufferCopyPD	(1<<8)  //Perfect Dark's depth buffer reads, which drive its light coronas
#define hack_texrectPaletteMod		(1<<9)  //[Xbox360] GLideN64 texturedRectPaletteMod: Paper Mario "2D lighting" palette written to RDRAM by a 16x1 texrect, and its darkness texture at 0x400
#define hack_viDosBuffers			(1<<12) //[Xbox360] VI origin inside a colour buffer with another one right after it: draw the end of the first and the start of the next (GLideN64 renderBuffer pNextBuffer). Beetle Adventure Racing menu transition
#define hack_zeldaMMLente			(1<<11) //[Xbox360] GLideN64 hack_ZeldaMM, only its depth copy by S2DEX BG rects (5347689b): Lens of Truth depth in Zelda MM
#define hack_RE2					(1<<13) //[Xbox360] GLideN64 hack_RE2 (5e51b386, 022b3756): 512-wide BG images and buffers at VI width, ModifyVertex w = 1, depth textures, N64 depth words. Resident Evil 2
#define hack_texrectShadowMap		(1<<10) //[Xbox360] GLideN64 texturedRectShadowMap: a texrect that reads the depth image is drawn as fog colour with alpha = TLUT[N64 depth]. Beetle Adventure Racing fog
#define hack_fbTextureOffset		(1<<23) //Offset Conker's shadow in CBFD and the Bob-ombs in Mario Tennis
#define hack_fbCopyToRDRAM			(1<<24) //Write the EFB back to RDRAM for real. Majora's Mask pictograph
#define hack_copyDepthToRDRAM		(1<<25) //Copy the EFB's depth into the N64 depth image a frame late. OoT/MM glows/sun, DK64 camera/sun
#define hack_texrectCopyToItself	(1<<26) //[Xbox360] GLideN64 texturedRectCopyToItself: skip texrects that copy an 8-bit colour image onto itself. Conker's shadow
#define hack_doNotResetOtherModeH	(1<<14) //Don't reset othermode.h at frame swap (RSP_ReiniciaOtherMode). Quake and Quake 2
#define hack_doNotResetOtherModeL	(1<<15) //Don't reset othermode.l at frame swap (RSP_ReiniciaOtherMode). Quake
#define hack_copyFromRDRAM			(1<<28) //[Xbox360] GLideN64.custom.ini frameBufferEmulation\copyFromRDRAM=1: at VI, draw what the CPU wrote into the RDRAM color buffer over the frame (with alpha), then zero it. Dr. Mario 64 pills
#define hack_copyToRDRAM			(1<<29) //[Xbox360] GLideN64.custom.ini frameBufferEmulation\copyToRDRAM: write the main color buffer to RDRAM at full sync, 1-3 frames late (no GPU wait). Mario Kart 64 TV, Banjo-Kazooie puzzle, Castlevania menu lens
#define hack_copyToRDRAMSync		(1<<30) //[Xbox360] GLideN64 copyToRDRAM=1 (ctSync), with hack_copyToRDRAM: the copy waits for the GPU and lands in THIS frame's buffer before the DP. Banjo-Kazooie puzzle assembling after a load (e8a5e829)
#define hack_n64DepthCompareDec		(1<<27) //[Xbox360] GLideN64.custom.ini frameBufferEmulation\N64DepthCompare=1, only its "decal" mode (ZMODE_DEC) in the shader against a depth copy. ISS64 player shadows
#define hack_ziComoColor			(1<<17) //[Xbox360] The DEPTH IMAGE used as color image is drawn in its own render target (second EDRAM color surface), so the single EFB no longer overwrites it; GLideN64 keeps a surface per buffer. Mario Tennis ball trail mask (0400), which otherwise picks up the scoreboard corner
#define hack_texrectShadeAlpha		(1<<19) //[Xbox360] GLideN64 hack_texrect_shade_alpha (d506d613, 74c0980d; removed upstream in 9a69fe8e with its N64 blending): texrect vertex alpha = 1 when the alpha combiner is SHADE only (a 16-bit N64 framebuffer stores coverage, not alpha). Bottom of the 9th intro and pitcher reticle in auxiliary buffers
#define hack_StarCraftBackgrounds	(1<<21) //[Xbox360] GLideN64 hack_StarCraftBackgrounds (729bf94c, 751fe2b3): a load with the RGBA16 TLUT is never a frame buffer texture. StarCraft 64 keeps its 8-bit map picture inside its own 16-bit frame buffer (0x00176540)
#define hack_fbAuxFilasDeMas		(1<<20) //[Xbox360] NOT in GLideN64 (which shows the same garbage; angrylion is right): a load that starts on a row of an AUXILIARY buffer may run up to 8 rows past its drawn height and is still served from it. Bottom of the 9th player face (48x48 buffer read as rows 9-50)
#define hack_alfaAleatoria			(1<<18) //[Xbox360] NOT in GLideN64 (copies angrylion's LOOK, not the commands we get): a texrect with the threshold alpha compare at blend alpha 0 and no FORCE_BL uses a RANDOM threshold per N64 pixel (passes with probability = alpha). Top Gear Rally wheel dust, which angrylion shows as scattered grains
#define hack_fbImagenPropia			(1<<16) //[Xbox360] NOT in GLideN64: a load that reads a frame buffer AS ITS OWN IMAGE (same start, width and size) is served from it even if the RDRAM under it no longer matches. Mario Tennis replay panel (the CPU writes text into that RDRAM)

void Config_LoadConfig();
void Config_DoConfig();
#endif
