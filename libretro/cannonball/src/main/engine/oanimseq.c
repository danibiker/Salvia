/***************************************************************************
    Animation Sequences.
    
    Used in three areas of the game:
    - The sequence at the start with the Ferrari driving in from the side
    - Flag Waving Man
    - 5 x End Sequences
    
    See "oanimsprite.hpp" for the specific format used by animated sprites.
    It is essentially a deviation from the normal sprites in the game.
    
    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#include "../frontend/config.h"

#include "obonus.h"
#include "oferrari.h"
#include "oinputs.h"
#include "oanimseq.h"

static void OAnimSeq_init_end_sprites(OAnimSeq* self);
static void OAnimSeq_tick_ferrari(OAnimSeq* self);
static void OAnimSeq_anim_seq_outro(OAnimSeq* self, oanimsprite*, int pal_override);
static void OAnimSeq_anim_seq_shadow(OAnimSeq* self, oanimsprite*, oanimsprite*);
static void OAnimSeq_anim_seq_outro_ferrari(OAnimSeq* self);
static bool OAnimSeq_read_anim_data(OAnimSeq* self, oanimsprite*);

/* ---------------------------------------------------------------------------- */
/* Animation Data Format. */
/* Animation blocks are stored in groups of 8 bytes, and formatted as follows: */
/* */
/* +00 [Byte] Sprite Colour Palette */
/* +01 [Byte] Bit 7: Make X Position Negative */
/*            Bits 4-6: Sprite To Sprite Priority */
/*            Bits 0-3: Top Bits Of Sprite Data Address */
/* +02 [Word] Sprite Data Address */
/* +04 [Byte] Sprite X Position */
/* +05 [Byte] Sprite Y Position */
/* +06 [Byte] Sprite To Road Priority */
/* +07 [Byte] Bit 7: Set To Load Next Block Of Sprite Animation Data To 0x1E */
/*            Bit 6: Set For H-Flip */
/*            Bit 4: */
/*            Bits 0-3: Animation Frame Delay  */
/*                     (Before Incrementing To Next Block Of 8 Bytes) */
/* ---------------------------------------------------------------------------- */

OAnimSeq oanimseq;




void OAnimSeq_init(OAnimSeq* self, oentry* jump_table)
{
    oentry* sprite_pass2;
    oentry* sprite_pass1;
    oentry* sprite_ferrari;
    /* -------------------------------------------------------------------------------------------- */
    /* Flag Animation Setup */
    /* -------------------------------------------------------------------------------------------- */
    oentry* sprite_flag = &jump_table[SPRITE_FLAG];
    self->anim_flag.sprite = sprite_flag;
    self->anim_flag.anim_state = 0;

    /* Jump table initalisations */
    sprite_flag->shadow = 7;
    sprite_flag->draw_props = ANCHOR_BOTTOM;

    /* Routine initalisations */
    sprite_flag->control |= ENABLE;
    sprite_flag->z = 400 << 16;

    /* -------------------------------------------------------------------------------------------- */
    /* Ferrari & Passenger Animation Setup For Intro */
    /* -------------------------------------------------------------------------------------------- */
    sprite_ferrari = &jump_table[SPRITE_FERRARI];
    oanimsprite_init(&self->anim_ferrari, sprite_ferrari);
    self->anim_ferrari.anim_addr_curr = outrun.adr.anim_ferrari_curr;
    self->anim_ferrari.anim_addr_next = outrun.adr.anim_ferrari_next;
    sprite_ferrari->control |= ENABLE;
    sprite_ferrari->draw_props = ANCHOR_BOTTOM;

    sprite_pass1 = &jump_table[SPRITE_PASS1];
    oanimsprite_init(&self->anim_pass1, sprite_pass1);
    self->anim_pass1.anim_addr_curr = outrun.adr.anim_pass1_curr;
    self->anim_pass1.anim_addr_next = outrun.adr.anim_pass1_next;
    sprite_pass1->draw_props = ANCHOR_BOTTOM;

    sprite_pass2 = &jump_table[SPRITE_PASS2];
    oanimsprite_init(&self->anim_pass2, sprite_pass2);
    self->anim_pass2.anim_addr_curr = outrun.adr.anim_pass2_curr;
    self->anim_pass2.anim_addr_next = outrun.adr.anim_pass2_next;
    sprite_pass2->draw_props = ANCHOR_BOTTOM;

    /* -------------------------------------------------------------------------------------------- */
    /* End Sequence Animation */
    /* -------------------------------------------------------------------------------------------- */
    self->end_seq_state = 0; /* init */
    self->seq_pos = 0;
    self->ferrari_stopped = false;

    oanimsprite_init(&self->anim_obj1, &jump_table[SPRITE_CRASH]);
    oanimsprite_init(&self->anim_obj2, &jump_table[SPRITE_CRASH_SHADOW]);
    oanimsprite_init(&self->anim_obj3, &jump_table[SPRITE_SHADOW]);
    oanimsprite_init(&self->anim_obj4, &jump_table[SPRITE_CRASH_PASS1]);
    oanimsprite_init(&self->anim_obj5, &jump_table[SPRITE_CRASH_PASS1_S]);
    oanimsprite_init(&self->anim_obj6, &jump_table[SPRITE_CRASH_PASS2]);
    oanimsprite_init(&self->anim_obj7, &jump_table[SPRITE_CRASH_PASS2_S]);
    oanimsprite_init(&self->anim_obj8, &jump_table[SPRITE_FLAG]);
}

/* ------------------------------------------------------------------------------------------------ */
/* START ANIMATION SEQUENCES (FLAG, FERRARI DRIVING IN) */
/* ------------------------------------------------------------------------------------------------ */

void OAnimSeq_flag_seq(OAnimSeq* self)
{
    if (!(self->anim_flag.sprite->control & ENABLE))
        return;

    if (outrun.tick_frame)
    {
        if (outrun.game_state < GS_START1 || outrun.game_state > GS_GAMEOVER)
        {
            self->anim_flag.sprite->control &= ~ENABLE;
            return;
        }

        /* Init Flag Sequence */
        if (outrun.game_state < GS_INGAME && self->anim_flag.anim_state != outrun.game_state)
        {
            uint32_t index;
            self->anim_flag.anim_state = outrun.game_state;

            /* Index of animation sequences */
            index = outrun.adr.anim_seq_flag + ((outrun.game_state - 9) << 3);

            self->anim_flag.anim_addr_curr = RomLoader_read32_a(roms.rom0p, &index);
            self->anim_flag.anim_addr_next = RomLoader_read32_a(roms.rom0p, &index);

            self->anim_flag.frame_delay = RomLoader_read8(roms.rom0p, 7 + self->anim_flag.anim_addr_curr) & 0x3F;
            self->anim_flag.anim_frame  = 0;
        }

        /* Wave Flag  */
        if (outrun.game_state <= GS_INGAME)
        {
            uint32_t index = self->anim_flag.anim_addr_curr + (self->anim_flag.anim_frame << 3);

            self->anim_flag.sprite->addr    = RomLoader_read32(roms.rom0p, index) & 0xFFFFF;
            self->anim_flag.sprite->pal_src = RomLoader_read8(roms.rom0p, index);

	        { uint32_t addr = SPRITE_ZOOM_LOOKUP + (((self->anim_flag.sprite->z >> 16) << 2) | osprites.sprite_scroll_speed);
	        uint32_t value = RomLoader_read32(roms.rom0p, addr);
	        self->anim_flag.sprite->z += value;
            { uint16_t z16 = self->anim_flag.sprite->z >> 16;
                int16_t sprite_x;
	    
            if (z16 >= 0x200)
	        {
                self->anim_flag.sprite->control &= ~ENABLE;
		        return;
	        }
	        self->anim_flag.sprite->priority = z16;
	        self->anim_flag.sprite->zoom     = z16 >> 2;

            /* Set X Position */
            sprite_x = (int8_t) RomLoader_read8(roms.rom0p, 4 + index);
            sprite_x -= oroad.road0_h[z16];
            { int32_t final_x = (sprite_x * z16) >> 9;
                int16_t sprite_y;

            if (RomLoader_read8(roms.rom0p, 1 + index) & BIT_7)
                final_x = -final_x;

            self->anim_flag.sprite->x = final_x;

            /* Set Y Position */
            sprite_y = (int8_t) RomLoader_read8(roms.rom0p, 5 + index);
            { int16_t final_y       = (sprite_y * z16) >> 9;
            self->anim_flag.sprite->y   = ORoad_get_road_y(&oroad, z16) - final_y;

            /* Set H-Flip */
            if (RomLoader_read8(roms.rom0p, 7 + index) & BIT_6)
                self->anim_flag.sprite->control |= HFLIP;
            else
                self->anim_flag.sprite->control &= ~HFLIP;

            /* Ready for next frame */
            if (--self->anim_flag.frame_delay == 0)
            {
                /* Load Next Block Of Animation Data */
                if (RomLoader_read8(roms.rom0p, 7 + index) & BIT_7)
                {
                    self->anim_flag.anim_addr_curr = self->anim_flag.anim_addr_next;
                    self->anim_flag.frame_delay    = RomLoader_read8(roms.rom0p, 7 + self->anim_flag.anim_addr_curr) & 0x3F;
                    self->anim_flag.anim_frame     = 0;
                }
                /* Last Block */
                else
                {
                    self->anim_flag.frame_delay = RomLoader_read8(roms.rom0p, 0x0F + index) & 0x3F;
                    self->anim_flag.anim_frame++;
                }
            }
         } } } }}
    }

    /* Order sprites */
    OSprites_map_palette(&osprites, self->anim_flag.sprite);
    OSprites_do_spr_order_shadows(&osprites, self->anim_flag.sprite);
}

/* Setup Ferrari Animation Sequence */
/*  */
/* Source: 0x6036 */
void OAnimSeq_ferrari_seq(OAnimSeq* self)
{
    if (!(self->anim_ferrari.sprite->control & ENABLE))
        return;

    if (outrun.game_state == GS_MUSIC) return;

    self->anim_pass1.sprite->control |= ENABLE;
    self->anim_pass2.sprite->control |= ENABLE;

    if (outrun.game_state <= GS_LOGO)
    {
        OFerrari_init_ingame(&oferrari);
        return;
    }

    self->anim_ferrari.frame_delay = RomLoader_read8(roms.rom0p, 7 + self->anim_ferrari.anim_addr_curr) & 0x3F;
    self->anim_pass1.frame_delay   = RomLoader_read8(roms.rom0p, 7 + self->anim_pass1.anim_addr_curr) & 0x3F;
    self->anim_pass2.frame_delay   = RomLoader_read8(roms.rom0p, 7 + self->anim_pass2.anim_addr_curr) & 0x3F;

    oferrari.car_state = CAR_NORMAL;
    oferrari.state     = FERRARI_SEQ2;

    OAnimSeq_anim_seq_intro(self, &self->anim_ferrari);
}

/* Process Animations for Ferrari and Passengers on intro */
/* */
/* Source: 60AE */
void OAnimSeq_anim_seq_intro(OAnimSeq* self, oanimsprite* anim)
{
    if (outrun.game_state <= GS_LOGO)
    {
        OFerrari_init_ingame(&oferrari);
        return;
    }

    if (outrun.tick_frame)
    {
        if (anim->anim_frame >= 1)
            oferrari.car_state = CAR_ANIM_SEQ;

        { uint32_t index              = anim->anim_addr_curr + (anim->anim_frame << 3);
            int16_t sprite_x;

        anim->sprite->addr          = RomLoader_read32(roms.rom0p, index) & 0xFFFFF;
        anim->sprite->pal_src       = anim == &self->anim_ferrari ? oferrari.ferrari_pal : RomLoader_read8(roms.rom0p, index);
        anim->sprite->zoom          = 0x7F;
        anim->sprite->road_priority = 0x1FE;
        anim->sprite->priority      = 0x1FE - ((RomLoader_read16(roms.rom0p, index) & 0x70) >> 4);

        /* Set X */
        sprite_x = (int8_t) RomLoader_read8(roms.rom0p, 4 + index);
        { int32_t final_x = (sprite_x * anim->sprite->priority) >> 9;
        if (RomLoader_read8(roms.rom0p, 1 + index) & BIT_7)
            final_x = -final_x;
        anim->sprite->x = final_x;

        /* Set Y */
        anim->sprite->y = 221 - ((int8_t) RomLoader_read8(roms.rom0p, 5 + index));

        /* Set H-Flip */
        if (RomLoader_read8(roms.rom0p, 7 + index) & BIT_6)
            anim->sprite->control |= HFLIP;
        else
            anim->sprite->control &= ~HFLIP;

        /* Ready for next frame */
        if (--anim->frame_delay == 0)
        {
            /* Load Next Block Of Animation Data */
            if (RomLoader_read8(roms.rom0p, 7 + index) & BIT_7)
            {
                /* Yeah the usual OutRun code hacks to do really odd stuff! */
                /* In this case, to exit the routine and setup the Ferrari on the last entry for passenger 2 */
                if (anim == &self->anim_pass2)
                {
                    if (ORoad_get_view_mode(&oroad) != VIEW_INCAR)
                    {
                        OSprites_map_palette(&osprites, anim->sprite);
                        OSprites_do_spr_order_shadows(&osprites, anim->sprite);
                    }
                    OFerrari_init_ingame(&oferrari);
                    return;
                }

                anim->anim_addr_curr = anim->anim_addr_next;
                anim->frame_delay    = RomLoader_read8(roms.rom0p, 7 + anim->anim_addr_curr) & 0x3F;
                anim->anim_frame     = 0;
            }
            /* Last Block */
            else
            {
                anim->frame_delay = RomLoader_read8(roms.rom0p, 0x0F + index) & 0x3F;
                anim->anim_frame++;
            }
        }
     } }}

    /* Order sprites */
    if (ORoad_get_view_mode(&oroad) != VIEW_INCAR)
    {
        OSprites_map_palette(&osprites, anim->sprite);
        OSprites_do_spr_order_shadows(&osprites, anim->sprite);
    }
}

/* ------------------------------------------------------------------------------------------------ */
/* END ANIMATION SEQUENCES */
/* ------------------------------------------------------------------------------------------------ */

/* Initialize end sequence animations on game complete */
/* Source: 0x9978 */
void OAnimSeq_init_end_seq(OAnimSeq* self)
{
    /* Process animation sprites instead of normal routine */
    oferrari.state = FERRARI_END_SEQ;

    /* Setup Ferrari Sprite */
    self->anim_ferrari.sprite->control |= ENABLE; 
    self->anim_ferrari.sprite->id = 0;
    self->anim_ferrari.sprite->draw_props = ANCHOR_BOTTOM;
    self->anim_ferrari.anim_frame = 0;
    self->anim_ferrari.frame_delay = 0;

    self->seq_pos = 0;

    /* Disable Passenger Sprites. These are replaced with new versions by the animation sequence. */
    oferrari.spr_pass1->control &= ~ENABLE;
    oferrari.spr_pass2->control &= ~ENABLE;

    obonus.bonus_control += 4;
}

void OAnimSeq_tick_end_seq(OAnimSeq* self)
{
    switch (self->end_seq_state)
    {
        case 0: /* init */
            if (outrun.tick_frame) OAnimSeq_init_end_sprites(self);
            else return;
            
        case 1: /* tick & blit */
            OAnimSeq_anim_seq_outro_ferrari(self);                           /* Ferrari Sprite */
            OAnimSeq_anim_seq_outro(self, &self->anim_obj1, oferrari.ferrari_pal);   /* Car Door Opening Animation */
            OAnimSeq_anim_seq_outro(self, &self->anim_obj2, -1);                         /* Interior of Ferrari */
            OAnimSeq_anim_seq_shadow(self, &self->anim_ferrari, &self->anim_obj3);         /* Car Shadow */
                                                                /* Man Sprite */
            /* Fix Wrong Palette Bug: Only occurs on 3 of the 5 possible end sequences (0 and 3 are ok) */
            OAnimSeq_anim_seq_outro(self, &self->anim_pass1, config.engine.fix_bugs ? 10 : -1);
            OAnimSeq_anim_seq_shadow(self, &self->anim_pass1, &self->anim_obj4);           /* Man Shadow */
            OAnimSeq_anim_seq_outro(self, &self->anim_pass2, -1);                        /* Female Sprite */
            OAnimSeq_anim_seq_shadow(self, &self->anim_pass2, &self->anim_obj5);           /* Female Shadow */
            OAnimSeq_anim_seq_outro(self, &self->anim_obj6, -1);                         /* Man Presenting Trophy */
            if (self->end_seq == 4)
                OAnimSeq_anim_seq_outro(self, &self->anim_obj7, -1);                     /* Varies */
            else
                OAnimSeq_anim_seq_shadow(self, &self->anim_obj6, &self->anim_obj7);
            OAnimSeq_anim_seq_outro(self, &self->anim_obj8, -1);                         /* Effects */
            break;
    }
}

/* Initalize Sprites For End Sequence. */
/* Source: 0x588A */static 
void OAnimSeq_init_end_sprites(OAnimSeq* self)
{
    /* Ferrari Object [0x5B12 entry point] */
    uint32_t addr = outrun.adr.anim_endseq_obj1 + (self->end_seq << 3);
    self->anim_ferrari.anim_addr_curr = RomLoader_read32_a(roms.rom0p, &addr);
    self->anim_ferrari.anim_addr_next = RomLoader_read32_a(roms.rom0p, &addr);
    self->ferrari_stopped = false;
    
    /* 0x58A4: Car Door Opening Animation [seq_sprite_entry] */
    self->anim_obj1.sprite->control |= ENABLE;
    self->anim_obj1.sprite->id = 1;
    self->anim_obj1.sprite->shadow = 3;
    self->anim_obj1.sprite->draw_props = ANCHOR_BOTTOM;
    self->anim_obj1.anim_frame = 0;
    self->anim_obj1.frame_delay = 0;
    self->anim_obj1.anim_props = 0;
    addr = outrun.adr.anim_endseq_obj2 + (self->end_seq << 3);
    self->anim_obj1.anim_addr_curr = RomLoader_read32_a(roms.rom0p, &addr);
    self->anim_obj1.anim_addr_next = RomLoader_read32_a(roms.rom0p, &addr);
    
    /* 0x58EC: Interior of Ferrari (Note this wobbles a little when passengers exit) [seq_sprite_entry] */
    self->anim_obj2.sprite->control |= ENABLE;
    self->anim_obj2.sprite->id = 2;
    self->anim_obj2.sprite->draw_props = ANCHOR_BOTTOM;
    self->anim_obj2.anim_frame = 0;
    self->anim_obj2.frame_delay = 0;
    self->anim_obj2.anim_props = 0;
    addr = outrun.adr.anim_endseq_obj3 + (self->end_seq << 3);
    self->anim_obj2.anim_addr_curr = RomLoader_read32_a(roms.rom0p, &addr);
    self->anim_obj2.anim_addr_next = RomLoader_read32_a(roms.rom0p, &addr);

    /* 0x592A: Car Shadow [SeqSpriteShadow] */
    self->anim_obj3.sprite->control |= ENABLE;
    self->anim_obj3.sprite->id = 3;
    self->anim_obj3.sprite->draw_props = ANCHOR_BOTTOM;
    self->anim_obj3.anim_frame = 0;
    self->anim_obj3.frame_delay = 0;
    self->anim_obj3.anim_props = 0;
    self->anim_obj3.sprite->addr = outrun.adr.shadow_data;

    /* 0x5960: Man Sprite [seq_sprite_entry] */
    self->anim_pass1.sprite->control |= ENABLE;
    self->anim_pass1.sprite->id = 4;
    self->anim_pass1.sprite->draw_props = ANCHOR_BOTTOM;
    self->anim_pass1.anim_frame = 0;
    self->anim_pass1.frame_delay = 0;
    self->anim_pass1.anim_props = 0;
    addr = outrun.adr.anim_endseq_obj4 + (self->end_seq << 3);
    self->anim_pass1.anim_addr_curr = RomLoader_read32_a(roms.rom0p, &addr);
    self->anim_pass1.anim_addr_next = RomLoader_read32_a(roms.rom0p, &addr);

    /* 0x5998: Man Shadow [SeqSpriteShadow] */
    self->anim_obj4.sprite->control = ENABLE;
    self->anim_obj4.sprite->id = 5;
    self->anim_obj4.sprite->shadow = 7;
    self->anim_obj4.sprite->draw_props = ANCHOR_BOTTOM;
    self->anim_obj4.anim_frame = 0;
    self->anim_obj4.frame_delay = 0;
    self->anim_obj4.anim_props = 0;
    self->anim_obj4.sprite->addr = outrun.adr.shadow_data;

    /* 0x59BE: Female Sprite [seq_sprite_entry] */
    self->anim_pass2.sprite->control |= ENABLE;
    self->anim_pass2.sprite->id = 6;
    self->anim_pass2.sprite->draw_props = ANCHOR_BOTTOM;
    self->anim_pass2.anim_frame = 0;
    self->anim_pass2.frame_delay = 0;
    self->anim_pass2.anim_props = 0;
    addr = outrun.adr.anim_endseq_obj5 + (self->end_seq << 3);
    self->anim_pass2.anim_addr_curr = RomLoader_read32_a(roms.rom0p, &addr);
    self->anim_pass2.anim_addr_next = RomLoader_read32_a(roms.rom0p, &addr);

    /* 0x59F6: Female Shadow [SeqSpriteShadow] */
    self->anim_obj5.sprite->control = ENABLE;
    self->anim_obj5.sprite->id = 7;
    self->anim_obj5.sprite->shadow = 7;
    self->anim_obj5.sprite->draw_props = ANCHOR_BOTTOM;
    self->anim_obj5.anim_frame = 0;
    self->anim_obj5.frame_delay = 0;
    self->anim_obj5.anim_props = 0;
    self->anim_obj5.sprite->addr = outrun.adr.shadow_data;

    /* 0x5A2C: Person Presenting Trophy [seq_sprite_entry] */
    self->anim_obj6.sprite->control |= ENABLE;
    self->anim_obj6.sprite->id = 8;
    self->anim_obj6.sprite->draw_props = ANCHOR_BOTTOM;
    self->anim_obj6.anim_frame = 0;
    self->anim_obj6.frame_delay = 0;
    self->anim_obj6.anim_props = 0;
    addr = outrun.adr.anim_endseq_obj6 + (self->end_seq << 3);
    self->anim_obj6.anim_addr_curr = RomLoader_read32_a(roms.rom0p, &addr);
    self->anim_obj6.anim_addr_next = RomLoader_read32_a(roms.rom0p, &addr);

    /* Alternate Use Based On End Sequence */
    self->anim_obj7.sprite->control |= ENABLE;
    self->anim_obj7.sprite->id = 9;
    self->anim_obj7.sprite->draw_props = ANCHOR_BOTTOM;
    self->anim_obj7.anim_frame = 0;
    self->anim_obj7.frame_delay = 0;
    self->anim_obj7.anim_props = 0;

    /* [seq_sprite_entry] */
    if (self->end_seq == 4)
    {
        addr = outrun.adr.anim_endseq_objB + (self->end_seq << 3);
        self->anim_obj7.anim_addr_curr = RomLoader_read32_a(roms.rom0p, &addr);
        self->anim_obj7.anim_addr_next = RomLoader_read32_a(roms.rom0p, &addr);
    }
    /* Trophy Shadow */
    else
    {
        self->anim_obj7.sprite->shadow = 7;
        self->anim_obj7.sprite->addr = outrun.adr.shadow_data;
    }

    /* 0x5AD0: Enable After Effects (e.g. cloud of smoke for genie) [seq_sprite_entry] */
    self->anim_obj8.sprite->control |= ENABLE;
    self->anim_obj8.sprite->id = 10;
    self->anim_obj8.sprite->draw_props = ANCHOR_BOTTOM;
    self->anim_obj8.anim_frame = 0;
    self->anim_obj8.frame_delay = 0;
    self->anim_obj8.anim_props = 0xFF00;
    addr = outrun.adr.anim_endseq_obj7 + (self->end_seq << 3);
    self->anim_obj8.anim_addr_curr = RomLoader_read32_a(roms.rom0p, &addr);
    self->anim_obj8.anim_addr_next = RomLoader_read32_a(roms.rom0p, &addr);
    
    self->end_seq_state = 1;
}

/* Ferrari Outro Animation Sequence */
/* Source: 0x5B12 */static 
void OAnimSeq_anim_seq_outro_ferrari(OAnimSeq* self)
{
    if (!self->ferrari_stopped)
    {
        /* Car is moving. Turn Brake On. */
        if (oinitengine.car_increment >> 16)
        {
            oferrari.auto_brake  = true;
            oinputs.brake_adjust = 0xFF;
        }
        else
        {
            OSoundInt_queue_sound(&osoundint, SOUND_VOICE_CONGRATS);
            self->ferrari_stopped = true;
        }
    }
    OAnimSeq_anim_seq_outro(self, &self->anim_ferrari, oferrari.ferrari_pal);
}

/* End Sequence: Setup Animated Sprites  */
/* Source: 0x5B42 */static 
void OAnimSeq_anim_seq_outro(OAnimSeq* self, oanimsprite* anim, int pal_override)
{
    int16_t sprite_y;
    uint32_t index;
    oinputs.steering_adjust = 0;

    /* Return if no animation data to process */
    if (!OAnimSeq_read_anim_data(self, anim)) 
        return;

    /* Process Animation Data */
    index = anim->anim_addr_curr + (anim->anim_frame << 3);

    anim->sprite->addr          = RomLoader_read32(roms.rom0p, index) & 0xFFFFF;
    /* Override palette to overcome bugs / recolour Ferrari */
    anim->sprite->pal_src       = pal_override != -1 ? pal_override : RomLoader_read8(roms.rom0p, index);
    anim->sprite->zoom          = RomLoader_read8(roms.rom0p, 6 + index) >> 1;
    anim->sprite->road_priority = RomLoader_read8(roms.rom0p, 6 + index) << 1;
    anim->sprite->priority      = anim->sprite->road_priority - ((RomLoader_read16(roms.rom0p, index) & 0x70) >> 4); /* (bits 4-6) */
    anim->sprite->x             = (RomLoader_read8(roms.rom0p, 4 + index) * anim->sprite->priority) >> 9;
    
    if (RomLoader_read8(roms.rom0p, 1 + index) & BIT_7)
        anim->sprite->x = -anim->sprite->x;

    /* set_sprite_xy: (similar to flag code again) */

    /* Set Y Position */
    sprite_y = (int8_t) RomLoader_read8(roms.rom0p, 5 + index);
    { int16_t final_y  = (sprite_y * anim->sprite->priority) >> 9;
    anim->sprite->y  = ORoad_get_road_y(&oroad, anim->sprite->priority) - final_y;

    /* Set H-Flip */
    if (RomLoader_read8(roms.rom0p, 7 + index) & BIT_6)
        anim->sprite->control |= HFLIP;
    else
        anim->sprite->control &= ~HFLIP;

    /* Ready for next frame */
    if (outrun.tick_frame && --anim->frame_delay == 0)
    {
        /* Load Next Block Of Animation Data */
        if (RomLoader_read8(roms.rom0p, 7 + index) & BIT_7)
        {
            anim->anim_props    |= 0xFF;
            anim->anim_addr_curr = anim->anim_addr_next;
            anim->frame_delay    = RomLoader_read8(roms.rom0p, 7 + anim->anim_addr_curr) & 0x3F;
            anim->anim_frame     = 0;
        }
        /* Last Block */
        else
        {
            anim->frame_delay = RomLoader_read8(roms.rom0p, 0x0F + index) & 0x3F;
            anim->anim_frame++;
        }
    }
    OSprites_map_palette(&osprites, anim->sprite);
    /* Order sprites */
    OSprites_do_spr_order_shadows(&osprites, anim->sprite);
 }}

/* Render Sprite Shadow For End Sequence */
/* Use parent sprite as basis to set this up */
/* Source: 0x5C48 */static 
void OAnimSeq_anim_seq_shadow(OAnimSeq* self, oanimsprite* parent, oanimsprite* anim)
{
    /* Return if no animation data to process */
    if (!OAnimSeq_read_anim_data(self, anim)) 
        return;

    if (outrun.tick_frame)
    {  
        uint16_t priority;
        uint8_t zoom_shift = 3;

        /* Car Shadow */
        if (anim->sprite->id == 3)
        {
            zoom_shift = 1;
            if ((parent->anim_props & 0xFF) == 0 && oferrari.sprite_ai_x <= 5)
                zoom_shift++;
        }
        /* 5C88 set_sprite_xy: */
        anim->sprite->x    = parent->sprite->x;
        priority = parent->sprite->road_priority >> zoom_shift;
        anim->sprite->zoom = priority - (priority >> 2);
        anim->sprite->y    = ORoad_get_road_y(&oroad, parent->sprite->road_priority);
    
        /* Chris - The following extra line seems necessary due to the way I set the sprites up. */
        /* Actually, I think it's a bug in the original game, relying on this being setup by  */
        /* the crash code previously. But anyway! */
        anim->sprite->road_priority = parent->sprite->road_priority;
    }

    OSprites_do_spr_order_shadows(&osprites, anim->sprite);
}

/* Read Animation Data for End Sequence */
/* Source: 0x5CC4 */static 
bool OAnimSeq_read_anim_data(OAnimSeq* self, oanimsprite* anim)
{
    bool DO_NOTHING;
    uint32_t addr = outrun.adr.anim_end_table + (self->end_seq << 2) + (anim->sprite->id << 2) +  (anim->sprite->id << 4); /* a0 + d1 */

    /* Read start & end position in animation timeline for this object */
    int16_t start_pos = RomLoader_read16(roms.rom0p, addr);     /* d0 */
    int16_t end_pos =   RomLoader_read16(roms.rom0p, 2 + addr); /* d3 */

    int16_t pos = self->seq_pos; /* d1 */
    
    /* Global Sequence Position: Advance to next position */
    /* Not particularly clean embedding this here obviously! */
    if (outrun.tick_frame && anim->anim_props & 0xFF00)
        self->seq_pos++;

    /* Test Whether Animation Sequence Is Over & Initalize Course Map */
    if (obonus.bonus_control != BONUS_DISABLE)
    {
        const static uint16_t END_SEQ_LENGTHS[] = {0x244, 0x244, 0x244, 0x190, 0x258};

        if (self->seq_pos == END_SEQ_LENGTHS[self->end_seq])
        {
            obonus.bonus_control = BONUS_DISABLE;
            /* we're missing all the code here to disable the animsprites, but probably not necessary? */

            if (outrun.cannonball_mode == MODE_ORIGINAL)
                outrun.game_state = GS_INIT_MAP;
            else
                Outrun_init_best_outrunners(&outrun);
        }
    }

    /* -------------------------------------------------------------------------------------------- */
    /* Process Animation Sequence */
    /* -------------------------------------------------------------------------------------------- */

    DO_NOTHING = false;
    { const bool PROCESS    = true;

    /* check_seq_pos: */
    /* Sequence: Start Position */
    /* ret_set_frame_delay:  */
    if (pos == start_pos)
    {
        /* If current animation block is set, extract frame delay */
        if (anim->anim_addr_curr)
            anim->frame_delay = RomLoader_read8(roms.rom0p, 7 + anim->anim_addr_curr) & 0x3F;

        return PROCESS;
    }
    /* Sequence: Invalid Position */
    else if (pos < start_pos || pos > end_pos) /* d1 < d0 || d1 > d3 */
        return DO_NOTHING;

    /* Sequence: In Progress */
    else if (pos < end_pos)
        return PROCESS;
    
    /* Sequence: End Position */
    else
    {
        /* End Of Animation Data */
        if (anim->sprite->id == 8) /* Trophy person */
        {
            anim->sprite->id = 11;
            if (self->end_seq >= 2)
                anim->sprite->shadow = 7;

            anim->anim_addr_curr = RomLoader_read32(roms.rom0p, outrun.adr.anim_endseq_obj8 + (self->end_seq << 3));
            anim->anim_addr_next = RomLoader_read32(roms.rom0p, outrun.adr.anim_endseq_obj8 + (self->end_seq << 3) + 4);
            anim->anim_frame = 0;
            return DO_NOTHING;
        }
        /* 5e14 */
        else if (anim->sprite->id == 10)
        {
            anim->sprite->id = 12;
            if (self->end_seq >= 2)
                anim->sprite->shadow = 7;

            anim->anim_addr_curr = RomLoader_read32(roms.rom0p, outrun.adr.anim_endseq_objA + (self->end_seq << 3));
            anim->anim_addr_next = RomLoader_read32(roms.rom0p, outrun.adr.anim_endseq_objA + (self->end_seq << 3) + 4);
            anim->anim_frame = 0;
            return DO_NOTHING;
        }
    }
    return PROCESS;
 }}
