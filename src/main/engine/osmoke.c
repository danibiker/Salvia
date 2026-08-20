/***************************************************************************
    Smoke & Spray Control.
    
    Animate the smoke and spray below the Ferrari's wheels.

    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#include "engine/ocrash.h"
#include "engine/oferrari.h"
#include "engine/olevelobjs.h"
#include "engine/osmoke.h"

static void OSmoke_tick_smoke_anim(OSmoke* self, oentry* sprite, int8_t anim_ctrl, uint32_t addr);

OSmoke osmoke;

void OSmoke_init(OSmoke* self)
{
    self->load_smoke_data = 0;
}

/* Called twice, once for each plume of smoke from the car */
/* */
/* ------------------------------------- */
/* Animation Data Format For Smoke/Spray */
/* ------------------------------------- */
/* */
/* Format: */
/* */
/* [+0] Long: Sprite Data Address */
/* [+4] Byte: Sprite Z value of smoke (bigger value means in front of car, and zoomed further) */
/* [+5] Byte: Sprite Palette */
/* [+6] Byte: Sprite X (Bottom 4 bits)  */
/*            Sprite Y (Top 4 bits) */
/* [+7] Byte: Bit 0: H-Flip sprite */
/*            Bit 1: Zoom shift value */
/*            Bits 4-7: Priority Change Per Frame */


/* Source: 0xA816 */
void OSmoke_draw_ferrari_smoke(OSmoke* self, oentry *sprite)
{
    OSmoke_setup_smoke_sprite(self, false);

    if (outrun.game_state != GS_ATTRACT)
    {
        if (outrun.game_state < GS_START1 || outrun.game_state >= GS_INIT_GAMEOVER) return; 
    }

    if (ocrash.crash_counter && !ocrash.crash_z) return;

    /* ------------------------------------------------------------------------ */
    /* Spray from water. More violent than the offroad wheel stuff */
    /* ------------------------------------------------------------------------ */

    if (olevelobjs.spray_counter)
    {
        OSmoke_tick_smoke_anim(self, sprite, 1, RomLoader_read32(roms.rom0p, outrun.adr.spray_data + olevelobjs.spray_type));
        return;
    }

    /* Enhancement: When not displaying car, don't draw smoke effects */
    if (ORoad_get_view_mode(&oroad) == VIEW_INCAR && !OCrash_is_flip(&ocrash))
        return;
    
    /* ------------------------------------------------------------------------ */
    /* Car Slipping/Skidding */
    /* ------------------------------------------------------------------------ */

    if (oferrari.is_slipping && oferrari.wheel_state == WHEELS_ON)
    {
        OSmoke_tick_smoke_anim(self, sprite, 0, RomLoader_read32(roms.rom0p, outrun.adr.smoke_data + self->smoke_type_slip));
        return;
    }

    /* ------------------------------------------------------------------------ */
    /* Wheels Offroad */
    /* ------------------------------------------------------------------------ */

    if (oferrari.wheel_state != WHEELS_ON)
    {
        uint32_t smoke_adr = RomLoader_read32(roms.rom0p, outrun.adr.smoke_data + self->smoke_type_offroad);

        /* Left Wheel Only */
        if (sprite == &osprites.jump_table[SPRITE_SMOKE2] && oferrari.wheel_state == WHEELS_LEFT_OFF)
            OSmoke_tick_smoke_anim(self, sprite, 1, smoke_adr);

        /* Right Wheel Only */
        else if (sprite == &osprites.jump_table[SPRITE_SMOKE1] && oferrari.wheel_state == WHEELS_RIGHT_OFF)
            OSmoke_tick_smoke_anim(self, sprite, 1, smoke_adr);
        
        /* Both Wheels */
        else if (oferrari.wheel_state == WHEELS_OFF)
            OSmoke_tick_smoke_anim(self, sprite, 1, smoke_adr);

        return;
    }

    /* test_crash_intro: */
    
    /* Normal */
    if (oferrari.car_state == CAR_NORMAL)
    {
        sprite->type = sprite->xw1; /* Copy frame number to type */
    }
    /* Smoke from wheels */
    else if (oferrari.car_state == CAR_SMOKE)
    {
        OSmoke_tick_smoke_anim(self, sprite, 1, RomLoader_read32(roms.rom0p, outrun.adr.smoke_data + self->smoke_type_onroad));
    }
    /* Animation Sequence */
    else
    {
        if (oferrari.wheel_state != WHEELS_ON)
            sprite->type = sprite->xw1; /* Copy frame number to type */
        else
        {
            OSmoke_tick_smoke_anim(self, sprite, 1, RomLoader_read32(roms.rom0p, outrun.adr.smoke_data + self->smoke_type_onroad));
        }
    }
}

/* - Set wheel spray sprite data dependent on upcoming stage */
/* - Use Main entry point when we know for a fact road isn't splitting */
/* - Use SetSmokeSprite1 entry point when road could potentially be splitting */
/*   Source: 0xA94C */
void OSmoke_setup_smoke_sprite(OSmoke* self, bool force_load)
{
    const static uint8_t OFFROAD_SMOKE[] =
    { 
        0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* Stage 1 */
        0x09, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* Stage 2 */
        0x02, 0x09, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, /* Stage 3 */
        0x08, 0x05, 0x05, 0x06, 0x00, 0x00, 0x00, 0x00, /* Stage 4 */
        0x08, 0x02, 0x08, 0x06, 0x09, 0x00, 0x00, 0x00  /* Stage 5 */
    };
    const static uint8_t ONROAD_SMOKE[] =
    { 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* Stage 1 */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* Stage 2 */
        0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, /* Stage 3 */
        0x00, 0x00, 0x0A, 0x07, 0x00, 0x00, 0x00, 0x00, /* Stage 4 */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  /* Stage 5 */
    };
    uint16_t stage_lookup = outrun.cannonball_mode != MODE_ORIGINAL ? oroad.stage_lookup_off : 0;

    /* Check whether we should load new sprite data when transitioning between stages */
    if (!force_load)
    {
        bool set = self->load_smoke_data & BIT_0;
        self->load_smoke_data &= ~BIT_0;
        if (!set) return; /* Don't load new smoke data */
        stage_lookup = oroad.stage_lookup_off + 8;
    }

    /* Set Smoke Colour When On Road */

    self->smoke_type_onroad = ONROAD_SMOKE[stage_lookup] << 2;
    self->smoke_type_slip = self->smoke_type_onroad;

    /* Set Smoke Colour When Off Road */

    self->smoke_type_offroad = OFFROAD_SMOKE[stage_lookup] << 2;
}

/* Set the smoke x,y and z co-ordinates */
/* Also sets the speed at which the animation repeats */
/* */
/* Inputs: */
/* */
/* d0 = 0: Use car speed to determine animation speed */
/*    = 1: Use revs to determine animation speed */
/* */
/* a5 = Smoke Sprite Plume */
/* Source: 0xA9B6 */
static void OSmoke_tick_smoke_anim(OSmoke* self, oentry* sprite, int8_t anim_ctrl, uint32_t addr)
{
    uint16_t frame;
    sprite->x = oferrari.spr_ferrari->x;
    sprite->y = oferrari.spr_ferrari->y;

    /* ------------------------------------------------------------------------ */
    /* Use revs to set sprite counter reload value */
    /* ------------------------------------------------------------------------ */
    if (anim_ctrl == 1)
    {
        int16_t revs = 0;

        /* Force smoke during animation sequence */
        if (oferrari.car_state == CAR_ANIM_SEQ)
            revs = 0x80;
        else
        {
            revs = oferrari.revs >> 16;
            if (revs > 0xFF) revs = 0xFF;
        }

        sprite->reload = 3 - (revs >> 6);
        sprite->z = (revs >> 1);           /* More revs = smoke is emitted further */

        /* Crash Occuring */
        if (ocrash.crash_counter)
        {
            int16_t z_shift;
            sprite->y = -(oroad.road_y[oroad.road_p0 + ocrash.crash_z] >> 4) + 223;

            /* Trigger Smoke Cloud. */
            /* Car is slid to the side, so we need to offset the smoke accordingly */
            if (ocrash.crash_state == 4)
            {
                if (ocrash.spr_ferrari->control & HFLIP)
                {
                    if (sprite == &osprites.jump_table[SPRITE_SMOKE2])
                    {
                        sprite->y -= 10;
                    }
                    else
                    {
                        sprite->x -= 64;
                        sprite->y -= 4;
                    }
                }
                else
                {
                    if (sprite == &osprites.jump_table[SPRITE_SMOKE2])
                    {
                        sprite->x += 64;
                        sprite->y -= 4;
                    }
                    else
                    {
                        sprite->y -= 10;    
                    }
                }
            } /* End trigger smoke cloud */

            z_shift = ocrash.crash_spin_count - 1;
            if (z_shift == 0) z_shift = 1;
            sprite->z = 0xFF >> z_shift;
        } 
    }
    /* ------------------------------------------------------------------------ */
    /* Use car speed to set sprite counter reload value */
    /* ------------------------------------------------------------------------ */
    else
    {
        uint16_t car_inc = (oinitengine.car_increment >> 16);
        if (car_inc > 0xFF) car_inc = 0xFF;
        sprite->reload = 7 - (car_inc >> 5);
        sprite->z = 0;
    }

    /* Return if stationary and not in animation sequence */
    if (oferrari.car_state != CAR_ANIM_SEQ && oinitengine.car_increment >> 16 == 0)
        return; 

    if (outrun.tick_frame)
    {
        if (sprite->counter > 0)
        {
            sprite->counter--;
        }
        else
        {
            /* One Wheel Off Road */
            if (oferrari.wheel_state != WHEELS_ON && oferrari.wheel_state != WHEELS_OFF)
            {
                sprite->counter = sprite->reload;
                sprite->xw1++; /* Increment Frame */
            }
            /* Two Wheels On Road */
            else if (sprite == &osprites.jump_table[SPRITE_SMOKE1])
            {
                sprite->counter = sprite->reload;
                sprite->xw1++; /* Increment Frame */

                /* Copy to second smoke sprite (yes this is crap, but it's directly ported code) */
                osprites.jump_table[SPRITE_SMOKE2].counter = sprite->reload;
                osprites.jump_table[SPRITE_SMOKE2].xw1 = sprite->xw1;
            }
        }
    }
    /* setup_smoke: */
    frame = (sprite->xw1 & 7) << 3;
    sprite->addr     = RomLoader_read32(roms.rom0p, addr + frame);
    sprite->pal_src  = RomLoader_read8(roms.rom0p, addr + frame + 5);
    { uint16_t smoke_z = RomLoader_read8(roms.rom0p, addr + frame + 4) + sprite->z;
    if (smoke_z > 0xFF) smoke_z = 0xFF;

    /* inc_crash_z: */
    /* When setting up smoke, include crash_z value into zoom if necessary */
    if (ocrash.crash_counter && ocrash.crash_z)
    {
        smoke_z = (smoke_z * ocrash.crash_z) >> 9;
    }

    /* Set Sprite Zoom */
    if (smoke_z <= 0x40) smoke_z = 0x40;
    { uint8_t shift = (RomLoader_read8(roms.rom0p, addr + frame + 7) & 2) >> 1;
        uint8_t hflip;
    uint8_t zoom = smoke_z >> shift;
    if (zoom <= 0x40) zoom = 0x40;
    sprite->zoom = zoom;

    /* Set Sprite Y */
    sprite->y += ((RomLoader_read8(roms.rom0p, addr + frame + 6) & 0xF) * zoom) >> 8;

    /* Set Sprite Priority */
    sprite->priority = oferrari.spr_ferrari->priority + ((RomLoader_read8(roms.rom0p, addr + frame + 7) >> 4) & 0xF);
    sprite->road_priority = sprite->priority;

    /* Set Sprite X */
    hflip = (RomLoader_read8(roms.rom0p, addr + frame + 7) & 1);
    { int8_t x = ((RomLoader_read8(roms.rom0p, addr + frame + 6) >> 3) & 0x1E);

    /* Enhancement: When viewing in-car, spread the spray out */
    if (ORoad_get_view_mode(&oroad) == VIEW_INCAR)
        x += 10;

    if (sprite == &osprites.jump_table[SPRITE_SMOKE1])
    {
        sprite->draw_props = ANCHOR_BOTTOM | ANCHOR_LEFT; /* Anchor bottom left */
        hflip++;
    }
    else
    {
        sprite->draw_props = ANCHOR_BOTTOM | ANCHOR_RIGHT; /* Anchor bottom right */
        x = -x;        
    }
    sprite->x += (x * zoom) >> 8;

    /* Set H-Flip */
    if (hflip & 1) sprite->control |= HFLIP;
    else sprite->control &= ~HFLIP;

    OSprites_map_palette(&osprites, sprite);
    OSprites_do_spr_order_shadows(&osprites, sprite);
 } } }}

/* Draw only helper routine. */
/* Useful for framerate changes. */
void OSmoke_draw(OSmoke* self, oentry* sprite)
{
    if (outrun.game_state != GS_ATTRACT)
    {
        if (outrun.game_state < GS_START1 || outrun.game_state >= GS_INIT_GAMEOVER) return; 
    }

    if (ocrash.crash_counter && !ocrash.crash_z) return;

    /* Return if stationary and not in animation sequence */
    if (oferrari.car_state != CAR_ANIM_SEQ && oinitengine.car_increment >> 16 == 0)
        return; 
    
    OSprites_map_palette(&osprites, sprite);
    OSprites_do_spr_order_shadows(&osprites, sprite);
}