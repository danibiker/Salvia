/***************************************************************************
    Smoke & Spray Control.
    
    Animate the smoke and spray below the Ferrari's wheels.

    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#pragma once
#ifdef __cplusplus
extern "C" {
#endif


#include "outrun.h"

typedef struct OSmoke
{
    /* Load smoke sprites for next level? */
    int8_t load_smoke_data;

    /* Ferrari wheel smoke type on road */
    uint16_t smoke_type_onroad;

    /* Ferrari wheel smoke type off road */
    uint16_t smoke_type_offroad;

    /* Ferrari wheel smoke type after car collision */
    uint16_t smoke_type_slip;
} OSmoke;

extern OSmoke osmoke;

void OSmoke_init(OSmoke* self);
void OSmoke_setup_smoke_sprite(OSmoke* self, bool force_load);
void OSmoke_draw_ferrari_smoke(OSmoke* self, oentry* sprite);
void OSmoke_draw(OSmoke* self, oentry* sprite);

#ifdef __cplusplus
}
#endif
