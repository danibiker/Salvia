/***************************************************************************
    Attract Mode: Animated OutRun Logo Graphic
    
    The logo is built from multiple sprite components.
    
    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#pragma once
#ifdef __cplusplus
extern "C" {
#endif


extern const uint8_t bg_pal[];

typedef struct OLogo
{
    uint32_t palm_frames[8];
    uint8_t entry_start;
    int16_t y_off;
} OLogo;

extern OLogo ologo;

void OLogo_enable(OLogo* self, int16_t y);

void OLogo_disable(OLogo* self);

void OLogo_tick(OLogo* self);

void OLogo_blit(OLogo* self);

#ifdef __cplusplus
}
#endif
