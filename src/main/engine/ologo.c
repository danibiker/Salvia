/***************************************************************************
    Attract Mode: Animated OutRun Logo Graphic
    
    The logo is built from multiple sprite components.
    
    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#include <stdint.h>

#include "osprites.h"
#include "outils.h"
#include "ologo.h"

static void OLogo_setup_sprite1(OLogo* self);
static void OLogo_setup_sprite2(OLogo* self);
static void OLogo_setup_sprite3(OLogo* self);
static void OLogo_setup_sprite4(OLogo* self);
static void OLogo_setup_sprite5(OLogo* self);
static void OLogo_setup_sprite6(OLogo* self);
static void OLogo_setup_sprite7(OLogo* self);
static void OLogo_sprite_logo_bg(OLogo* self);
static void OLogo_sprite_logo_car(OLogo* self);
static void OLogo_sprite_logo_bird1(OLogo* self);
static void OLogo_sprite_logo_bird2(OLogo* self);
static void OLogo_sprite_logo_road(OLogo* self);
static void OLogo_sprite_logo_palm(OLogo* self);
static void OLogo_sprite_logo_text(OLogo* self);

OLogo ologo;

const uint8_t bg_pal[] = { 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9A, 0x9B, 0x9C };



void OLogo_enable(OLogo* self, int16_t y)
{
    self->y_off = -y;

    self->entry_start = SPRITE_ENTRIES - 0x10;

    /* Enable block of sprites */
    { int i; for (i = self->entry_start; i < self->entry_start + 7; i++)
    {
        oentry_init(&osprites.jump_table[i], i);
    } }

    self->palm_frames[0] = outrun.adr.sprite_logo_palm1;
    self->palm_frames[1] = outrun.adr.sprite_logo_palm2;
    self->palm_frames[2] = outrun.adr.sprite_logo_palm3;
    self->palm_frames[3] = outrun.adr.sprite_logo_palm2;
    self->palm_frames[4] = outrun.adr.sprite_logo_palm1;
    self->palm_frames[5] = outrun.adr.sprite_logo_palm2;
    self->palm_frames[6] = outrun.adr.sprite_logo_palm3;
    self->palm_frames[7] = outrun.adr.sprite_logo_palm2;

    OLogo_setup_sprite1(self); 
    OLogo_setup_sprite2(self);
    OLogo_setup_sprite3(self);
    OLogo_setup_sprite4(self);
    OLogo_setup_sprite5(self);
    OLogo_setup_sprite6(self);
    OLogo_setup_sprite7(self);
}

void OLogo_disable(OLogo* self)
{
    /* Enable block of sprites */
    { int i; for (i = self->entry_start; i < self->entry_start + 7; i++)
    {
        osprites.jump_table[i].control &= ~ENABLE;
    } }
}

void OLogo_tick(OLogo* self)
{
    OLogo_sprite_logo_bg(self);
    OLogo_sprite_logo_car(self);
    OLogo_sprite_logo_bird1(self);
    OLogo_sprite_logo_bird2(self);
    OLogo_sprite_logo_road(self);
    OLogo_sprite_logo_palm(self);
    OLogo_sprite_logo_text(self);
}

/* Animated Background Oval Sprite. */static 
void OLogo_setup_sprite1(OLogo* self)
{
    oentry *e = &osprites.jump_table[self->entry_start + 0];
    e->x = 0;
    e->y = 0x70 - self->y_off;
    e->road_priority = 0xFF;
    e->priority = 0x1FA;
    e->zoom = 0x7F;
    e->pal_src = 0x99;
    e->addr = outrun.adr.sprite_logo_bg;
    OSprites_map_palette(&osprites, e);
}

/* Sprite Car Graphic */static 
void OLogo_setup_sprite2(OLogo* self)
{
    oentry *e = &osprites.jump_table[self->entry_start + 1];
    e->x = -3;
    e->y = 0x88 - self->y_off;
    e->road_priority = 0x100;
    e->priority = 0x1FB;
    e->zoom = 0x7F;
    e->pal_src = 0x6E;
    e->addr = outrun.adr.sprite_logo_car;
    OSprites_map_palette(&osprites, e);
}

/* Sprite Flying Bird #1 */static 
void OLogo_setup_sprite3(OLogo* self)
{
    oentry *e = &osprites.jump_table[self->entry_start + 2];
    e->x = 0x8;
    e->y = 0x4E - self->y_off;
    e->road_priority = 0x102;
    e->priority = 0x1FD;
    e->zoom = 0x7F;
    e->counter = 0;
    e->pal_src = 0x8B;
    e->addr = outrun.adr.sprite_logo_bird1;
    OSprites_map_palette(&osprites, e);
}

/* Sprite Flying Bird #2 */static 
void OLogo_setup_sprite4(OLogo* self)
{
    oentry *e = &osprites.jump_table[self->entry_start + 3];
    e->x = 0x8;
    e->y = 0x4E - self->y_off;
    e->road_priority = 0x102;
    e->priority = 0x1FD;
    e->zoom = 0x7F;
    e->counter = 0x20;
    e->pal_src = 0x8C;
    e->addr = outrun.adr.sprite_logo_bird2;
    OSprites_map_palette(&osprites, e);
}

/* Sprite Road Base Section */static 
void OLogo_setup_sprite5(OLogo* self)
{
    oentry *e = &osprites.jump_table[self->entry_start + 4];
    e->x = -0x20;
    e->y = 0x8F - self->y_off;
    e->road_priority = 0x101;
    e->priority = 0x1FC;
    e->zoom = 0x7F;
    e->pal_src = 0x6E;
    e->addr = outrun.adr.sprite_logo_base;
    OSprites_map_palette(&osprites, e);
}

/* Palm Tree */static 
void OLogo_setup_sprite6(OLogo* self)
{
    oentry *e = &osprites.jump_table[self->entry_start + 5];
    e->x = -0x40;
    e->y = 0x6D - self->y_off;
    e->road_priority = 0x102;
    e->priority = 0x1FD;
    e->zoom = 0x7F;
    e->pal_src = 0x65;
    e->addr = outrun.adr.sprite_logo_palm1;
    OSprites_map_palette(&osprites, e);
}

/* OutRun logo text */static 
void OLogo_setup_sprite7(OLogo* self)
{
    oentry *e = &osprites.jump_table[self->entry_start + 6];
    e->x = 0x11;
    e->y = 0x65 - self->y_off;
    e->road_priority = 0x103;
    e->priority = 0x1FE;
    e->zoom = 0x7F;
    e->counter = 0;
    e->pal_src = 0x65;
    e->addr = outrun.adr.sprite_logo_text;
    OSprites_map_palette(&osprites, e);
}

/* Draw Background of OutRun logo in attract mode */static 
void OLogo_sprite_logo_bg(OLogo* self)
{
    oentry *e = &osprites.jump_table[self->entry_start + 0];
    e->reload++;
    { uint16_t d0 = e->reload;
    uint16_t d1 = d0 - 1;
    d1 ^= d0;

    /* Map new palette */
    if (d1 & BIT_3)
    {
        e->pal_src = bg_pal[outils_random() & 7];
        OSprites_map_palette(&osprites, e);
    }

    OSprites_do_spr_order_shadows(&osprites, e);
 }}static 

void OLogo_sprite_logo_car(OLogo* self)
{
    oentry *e = &osprites.jump_table[self->entry_start + 1];
    /* Flicker background palette of car */
    e->reload++;
    e->pal_src = ((e->reload & 2) >> 1) ? 0x8A : 0x6E;
    OSprites_map_palette(&osprites, e);
    OSprites_do_spr_order_shadows(&osprites, e);
}static 

void OLogo_sprite_logo_bird1(OLogo* self)
{
    uint16_t index;
    oentry *e = &osprites.jump_table[self->entry_start + 2];
    e->counter++;

    /* Set Bird X Value */
    index = (e->counter << 1) & 0xFF;
    { int8_t bird_x = RomLoader_read8(&(roms.rom0), DATA_MOVEMENT + index); /* Note we sign the value here */
    int8_t zoom = bird_x >> 3;
    e->x = (bird_x >> 3) + 8;

    /* Set Zoom Lookup */
    e->zoom = zoom + 0x70;

    /* Set Bird Y Value */
    index = (index << 1) & 0xFF;
    { int8_t bird_y = RomLoader_read8(&(roms.rom0), DATA_MOVEMENT + index); /* Note we sign the value here */
        uint16_t frame;
    e->y = (bird_y >> 5) + 0x4E - self->y_off;

    /* Set Frame */
    e->reload++;
    frame = (e->reload & 4) >> 2;
    e->addr = frame ? outrun.adr.sprite_logo_bird2 : outrun.adr.sprite_logo_bird1;
    OSprites_do_spr_order_shadows(&osprites, e);
 } }}static 

void OLogo_sprite_logo_bird2(OLogo* self)
{
    uint16_t index;
    oentry *e = &osprites.jump_table[self->entry_start + 3];
    e->counter++;

    /* Set Bird X Value */
    index = (e->counter << 1) & 0xFF;
    { int8_t bird_x = RomLoader_read8(&(roms.rom0), DATA_MOVEMENT + index); /* Note we sign the value here */
    int8_t zoom = bird_x >> 3;
    e->x = (bird_x >> 3) - 2; /* Different from sprite_logo_bird1 */

    /* Set Zoom Lookup */
    e->zoom = zoom + 0x70;

    /* Set Bird Y Value */
    index = (index << 1) & 0xFF;
    { int8_t bird_y = RomLoader_read8(&(roms.rom0), DATA_MOVEMENT + index); /* Note we sign the value here */
        uint16_t frame;
    e->y = (bird_y >> 5) + 0x52 - self->y_off; /* Different from sprite_logo_bird1 */

    /* Set Frame */
    e->reload++;
    frame = (e->reload & 4) >> 2;
    e->addr = frame ? outrun.adr.sprite_logo_bird2 : outrun.adr.sprite_logo_bird1;
    OSprites_do_spr_order_shadows(&osprites, e);
 } }}static 

void OLogo_sprite_logo_road(OLogo* self)
{
    OSprites_do_spr_order_shadows(&osprites, &osprites.jump_table[self->entry_start + 4]); 
}static 

void OLogo_sprite_logo_palm(OLogo* self)
{
    oentry *e = &osprites.jump_table[self->entry_start + 5]; 
    e->reload++; /* Increment frame number */
    e->addr = self->palm_frames[(e->reload & 0xE) >> 1];
    OSprites_do_spr_order_shadows(&osprites, e);
}static 

void OLogo_sprite_logo_text(OLogo* self)
{
    OSprites_do_spr_order_shadows(&osprites, &osprites.jump_table[self->entry_start + 6]); 
}

/* Blit Only: Used when frame skipping */
void OLogo_blit(OLogo* self)
{
    { int i; for (i = 0; i < 7; i++)
        OSprites_do_spr_order_shadows(&osprites, &osprites.jump_table[self->entry_start + i]); }
}
