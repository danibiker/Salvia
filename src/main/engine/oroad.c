/***************************************************************************
    Road Rendering & Control

    This is a complete port of the 68000 SUB CPU Program ROM.
    
    The original code consists of a shared Sega library and some routines
    which are OutRun specific.
    
    Some of the original code is not used and is therefore not ported.
    
    This is the most complex area of the game code, and an area of the code
    in need of refactoring.

    Useful background reading on road rendering:
    http://www.extentofthejam.com/pseudo/

    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#include <stdint.h>
#include "globals.h"
#include "roms.h"
#include "trackloader.h"

#include "engine/oaddresses.h"
#include "engine/outils.h"
#include "engine/oinitengine.h"

#include "engine/oroad.h"
#include "engine/ostats.h"

static void ORoad_set_default_hscroll(ORoad* self);
static void ORoad_clear_road_ram(ORoad* self);
static void ORoad_init_stage1(ORoad* self);
static void ORoad_do_road(ORoad* self);
static void ORoad_rotate_values(ORoad* self);
static void ORoad_check_load_road(ORoad* self);
static void ORoad_setup_road_x(ORoad* self);
static void ORoad_setup_x_data(ORoad* self, uint32_t);
static void ORoad_set_tilemap_x(ORoad* self, uint32_t);
static void ORoad_create_curve(ORoad* self, int16_t*, int16_t*,
                      const int32_t, const int32_t, const int16_t, const int16_t);
static void ORoad_setup_hscroll(ORoad* self);
static void ORoad_do_road_offset(ORoad* self, int16_t*, int16_t, bool);
static void ORoad_setup_road_y(ORoad* self);
static void ORoad_init_height_seg(ORoad* self);
static void ORoad_init_elevation(ORoad* self, uint32_t*);
static void ORoad_do_elevation(ORoad* self);
static void ORoad_init_elevation_delay(ORoad* self, uint32_t*);
static void ORoad_do_elevation_delay(ORoad* self);
static void ORoad_init_elevation_mixed(ORoad* self, uint32_t*);
static void ORoad_do_elevation_mixed(ORoad* self);
static void ORoad_init_horizon_adjust(ORoad* self, uint32_t*);
static void ORoad_do_horizon_adjust(ORoad* self);
static void ORoad_set_road_y(ORoad* self);
static void ORoad_set_y_interpolate(ORoad* self);
static void ORoad_set_y_horizon(ORoad* self);
static void ORoad_set_y_2044(ORoad* self);
static void ORoad_read_next_height(ORoad* self);
static void ORoad_set_elevation(ORoad* self);
static void ORoad_set_horizon_y(ORoad* self);
static void ORoad_do_road_data(ORoad* self);
static void ORoad_blit_roads(ORoad* self);
static void ORoad_blit_road(ORoad* self, uint32_t);
static void ORoad_output_hscroll(ORoad* self, int16_t*, uint32_t);
static void ORoad_copy_bg_color(ORoad* self);

ORoad oroad;



void ORoad_tick(ORoad* self)
{
    /* Enhancement: Adjust View */
    if (self->horizon_target != self->horizon_offset)
    {
        if (self->horizon_offset > self->horizon_target)
        {
            self->horizon_offset -= 16;
            if (self->horizon_offset < self->horizon_target)
                self->horizon_offset = self->horizon_target;
        }
        else if (self->horizon_offset < self->horizon_target)
        {
            self->horizon_offset += 16;
            if (self->horizon_offset > self->horizon_target)
                self->horizon_offset = self->horizon_target;
        }
    }

    ORoad_do_road(self);
}

/* Helper function */
int16_t ORoad_get_road_y(ORoad* self, uint16_t index)
{
    return -(self->road_y[self->road_p0 + index] >> 4) + 223;
}

/* Initialize Road Values */
/*  */
/* Source Address: 0x10B8 */
/* Input:          None */
/* Output:         None */

void ORoad_init(ORoad* self)
{
    /* Extra initalization code here */
    ORoad_set_view_mode(self, VIEW_ORIGINAL, true);
    self->road_pos         = 0;
    self->tilemap_h_target = 0;
    self->stage_lookup_off = 0;
    self->road_width_bak   = 0;
    self->car_x_bak        = 0;
    self->height_lookup    = 0;
    self->road_pos_change  = 0;
    self->road_load_end    = 0;
    self->road_ctrl        = ROAD_OFF;
    self->road_load_split  = 0;
    self->road_width       = 0;  
    self->horizon_y2       = 0;
    self->horizon_y_bak    = 0;
    self->pos_fine         = 0;
    self->horizon_base     = 0;

    { int i; for (i = 0; i < ARRAY_LENGTH; i++)
    {
        self->road_x[i] = 0;
        self->road0_h[i] = 0;
        self->road1_h[i] = 0;
        self->road_unk[i] = 0;
    } }

     { int i; for (i = 0; i < 0x1000; i++)
         self->road_y[i] = 0; }

    self->road_pos_old = 0;
    self->road_data_offset = 0;
    self->height_start = 0;
    self->height_ctrl = 0;
    self->pos_fine_old = 0;
    self->pos_fine_diff = 0;
    self->counter = 0;
    self->height_index = 0;
    self->height_final = 0;
    self->height_inc = 0;
    self->height_step = 0;
    self->height_ctrl2 = 0;
    self->height_addr = 0;
    self->elevation = 0;
    self->height_lookup_wrk = 0;
    self->height_delay = 0;
    self->step_adjust = 0;
    self->do_height_inc = 0;
    self->height_end = 0;
    self->up_mult = 0;
    self->down_mult = 0;
    self->horizon_mod = 0;
    self->length_offset = 0;
    self->a1_lookup = 0;
    self->change_per_entry = 0;
    self->d5_o = 0;
    self->a3_o = 0;
    self->y_addr = 0;
    self->scanline = 0;
    self->total_height = 0;
    /* End of extra initialization code  */

    self->stage_loaded = -1;
    self->horizon_set  = 0;

    self->road_p0 = 0;
    self->road_p1 = self->road_p0 + 0x400;
    self->road_p2 = self->road_p1 + 0x400;
    self->road_p3 = self->road_p2 + 0x400;

    ORoad_set_default_hscroll(self);
    ORoad_clear_road_ram(self);
    ORoad_init_stage1(self);
}

/* ---------------------------------------------------------------------------- */
/* View Mode Enhancements */
/* */
/* Original: Same as Arcade */
/* Elevated: Camera positioned further above car */
/* In Car  : Lower viewpoint */
/* ---------------------------------------------------------------------------- */

uint8_t ORoad_get_view_mode(ORoad* self)
{
    return self->view_mode;
}

void ORoad_set_view_mode(ORoad* self, uint8_t mode, bool snap)
{
    self->view_mode = mode;

    if (mode == VIEW_ORIGINAL)
    {
        self->horizon_target = 0;
    }
    else if (mode == VIEW_ELEVATED)
    {
        self->horizon_target = 0x280;
    }
    else if (mode == VIEW_INCAR)
    {
        self->horizon_target = -0x70;
    }

    if (snap)
        self->horizon_offset = self->horizon_target;
}

/* Main Loop */
/*  */
/* Source Address: 0x1044 */
/* Input:          None */
/* Output:         None */static 
void ORoad_do_road(ORoad* self)
{
    ORoad_rotate_values(self);
    ORoad_setup_road_x(self);
    ORoad_setup_road_y(self);
    ORoad_set_road_y(self);
    ORoad_set_horizon_y(self);
    ORoad_do_road_data(self);
    ORoad_blit_roads(self);
    ORoad_output_hscroll(self, &self->road0_h[0], HW_HSCROLL_TABLE0);
    ORoad_output_hscroll(self, &self->road1_h[0], HW_HSCROLL_TABLE1);
    ORoad_copy_bg_color(self);

    HWRoad_read_road_control(&hwroad); /* swap halves of road ram */
}

/* Set Default Horizontal Scroll Values */
/*  */
/* Source Address: 0x1106 */
/* Input:          None */
/* Output:         None */static 

void ORoad_set_default_hscroll(ORoad* self)
{
    uint32_t adr = HW_HSCROLL_TABLE0;

    { uint16_t i; for (i = 0; i <= 0x1FF; i++)
        HWRoad_write32(&hwroad, &adr, 0x1000100); }

    HWRoad_read_road_control(&hwroad);
}

/* Clear Road RAM */
/*  */
/* Source Address: 0x115A */
/* Input:          None */
/* Output:         None */

/* Initialises 0x80000 - 0x8001C0 as follows. */
/* */
/* 0x80000 : 00 00 [scanline 0] */
/* 0x80002 : 00 01 [scanline 1] */
/* 0x80004 : 00 02 [scanline 2] */static 

void ORoad_clear_road_ram(ORoad* self)
{
    uint32_t adr = 0x80000; /* start of road ram */

    { uint8_t scanline; for (scanline = 0; scanline < S16_HEIGHT; scanline++)
        HWRoad_write16_a(&hwroad, &adr, scanline); }

    HWRoad_read_road_control(&hwroad);
}

/* Initalize Stage 1 */
/*  */
/* Source Address: 0x10DE */
/* Input:          None */
/* Output:         None */static 

void ORoad_init_stage1(ORoad* self)
{
    TrackLoader_init_path(&trackloader, 0); 
    self->road_pos = 0;
    self->road_ctrl = ROAD_BOTH_P0;
    /* ignore init counter stuff (move.b  #$A,$49(a5)) */
}

/* Rotate Values & Load Next Level/Split/Bonus Road */
/*  */
/* Source Address: 0x119E */
/* Input:          None */
/* Output:         None */static 

void ORoad_rotate_values(ORoad* self)
{
    uint16_t p0_copy = self->road_p0;
    self->road_p0 = self->road_p1;
    self->road_p1 = self->road_p2;
    self->road_p2 = self->road_p3;
    self->road_p3 = p0_copy;

    self->road_pos_change = (self->road_pos >> 16) - self->road_pos_old;
    self->road_pos_old = self->road_pos >> 16;

    ORoad_check_load_road(self);
}

/* Check whether we need to load the next stage / road split / bonus data */
/*  */
/* Source Address: 0x11D0 */
/* Input:          None */
/* Output:         None */static 

void ORoad_check_load_road(ORoad* self)
{
    /* If stage is not loaded */
    if (ostats.cur_stage != self->stage_loaded)
    {
        self->stage_loaded = ostats.cur_stage; /* Denote loaded */
        TrackLoader_init_path(&trackloader, self->stage_lookup_off);
        return;
    }

    /* Check Split */
    if (self->road_load_split)
    {
        TrackLoader_init_path_split(&trackloader);
        self->road_load_split = 0;
    }

    /* Check End Section */
    else if (self->road_load_end)
    {
        TrackLoader_init_path_end(&trackloader);
        self->road_load_end = 0;
    }
}

/* 1/ Setup Road X Data from ROM */
/* 2/ Apply H-Scroll To Data */
/*  */
/* Source Address: 0x1590 */
/* Input:          None */
/* Output:         None */static 

void ORoad_setup_road_x(ORoad* self)
{
    /* If moved to next chunk of road data, setup x positions */
    if (self->road_pos_change != 0)
    {
        uint32_t addr;
        self->road_data_offset = (self->road_pos >> 16) << 2;
        /*uint32_t addr = stage_addr + road_data_offset; */
        addr = self->road_data_offset;
        ORoad_set_tilemap_x(self, addr);
        ORoad_setup_x_data(self, addr);
    }
    ORoad_setup_hscroll(self);
}

/* Setup Road Path */
/* */
/* Road Path is stored as a series of words representing x,y change */
/* */
/* Source Address: 0x15B0 */static 

void ORoad_setup_x_data(ORoad* self, uint32_t addr)
{
    const int16_t x = TrackLoader_readPath(&trackloader, addr)     + TrackLoader_readPath(&trackloader, addr + 4); /* Length 1 */
    const int16_t y = TrackLoader_readPath(&trackloader, addr + 2) + TrackLoader_readPath(&trackloader, addr + 6); /* Length 2 */

    /* Use Pythagorus' theorem to find the distance/length between x & y */
    const uint16_t distance = outils_isqrt((x * x) + (y * y));

    { const int16_t curve_x_dist = (x << 14) / distance; /* Scale up is for fixed point division in create_curve */
        int16_t scanline;
        int16_t curve_inc_old;
        int16_t curve_inc;
        int16_t curve_end;
        int16_t curve_start;
        int32_t curve_y_total;
        int32_t curve_x_total;
    const int16_t curve_y_dist = (y << 14) / distance;

    /* Setup Default Straight Positions Of Road at 60800 - 608FF */
    { uint8_t i; for (i = 0; i < 0x80;)
    {
        /* Set marker to ignore current road position, and use last good value */
        self->road_x[i++] = 0x3210;
        self->road_x[i++] = 0x3210;
    } }

    /* Now We Setup The Real Road Data at 60800 */
    curve_x_total = 0;
    curve_y_total = 0;
    curve_start = 0;
    curve_end = 0;
    curve_inc = 0;
    curve_inc_old = 0;

    scanline = 0x37E / 2;

    /* Note this works its way from closest to the camera, further into the horizon */
    /* So the amount to offset x is going to increase over time */
    /* We sample 20 Road Positions to generate the road. */
    { uint8_t i; for (i = 0; i <= 0x20; i++)
    {
        const int32_t x_next = TrackLoader_readPath(&trackloader, addr)     + TrackLoader_readPath(&trackloader, addr + 4); /* Length 1 */
        const int32_t y_next = TrackLoader_readPath(&trackloader, addr + 2) + TrackLoader_readPath(&trackloader, addr + 6); /* Length 2 */
        addr += 8;

        curve_x_total += x_next;
        curve_y_total += y_next;

        /* Calculate curve increment and end position */
        ORoad_create_curve(self, &curve_inc, &curve_end, curve_x_total, curve_y_total, curve_x_dist, curve_y_dist);
        { int16_t curve_steps = curve_end - curve_start;
            int32_t xinc;
        if (curve_steps < 0) return;
        if (curve_steps == 0) continue; /* skip_pos */

        /* X Amount to increment curve by each iteration */
        xinc = (curve_inc - curve_inc_old) / curve_steps;
        { int16_t x = curve_inc_old;

        { int16_t pos; for (pos = curve_start; pos <= curve_end; pos++)
        {
            x += xinc;              
            if (x < -0x3200 || x > 0x3200) return;
            self->road_x[scanline] = x;           
            if (--scanline < 0) return;
        } }

        curve_inc_old = curve_inc;
        curve_start = curve_end;
     } }} }
 }}

/* Interpolates road data into a smooth curve. */
/* */
/* Source Address: 0x16B6 */static 

void ORoad_create_curve(ORoad* self, 
       int16_t*curve_inc, int16_t*curve_end,
       int32_t curve_x_total, int32_t curve_y_total, int16_t curve_x_dist, int16_t curve_y_dist)
{    
    /* Note multiplication result should be 32bit, inputs 16 bit */
    int32_t d0 = ((curve_x_total >> 5) * curve_y_dist) - ((curve_y_total >> 5) * curve_x_dist);
    int32_t d2 = ((curve_x_total >> 5) * curve_x_dist) + ((curve_y_total >> 5) * curve_y_dist);

    d2 >>= 7;        
    { int32_t d1 = (d2 >> 7) + 0x410;     

    (*curve_inc) = d0 / d1;        /* Total amount to increment x by */
    (*curve_end) = (d2 / d1) * 4;
 }}

/* Set The Correct Tilemap X *target* Position From The Road Data */
/*  */
/* Source Address: 0x16F6 */
/* */
/* Use Euclidean distance between a series of points. */
/* This is essentially the standard distance that you'd measure with a ruler. */static 

void ORoad_set_tilemap_x(ORoad* self, uint32_t addr)
{    
    /* d0 = Word 0 + Word 2 + Word 4 + Word 6 [Next 4 x positions] */
    /* d1 = Word 1 + Word 3 + Word 5 + Word 7 [Next 4 y positions] */
    
    int16_t x = TrackLoader_readPath_a(&trackloader, &addr);
    int16_t y = TrackLoader_readPath_a(&trackloader, &addr);
    x += TrackLoader_readPath_a(&trackloader, &addr);
    y += TrackLoader_readPath_a(&trackloader, &addr);
    x += TrackLoader_readPath_a(&trackloader, &addr);
    y += TrackLoader_readPath_a(&trackloader, &addr);
    x += TrackLoader_readPath_a(&trackloader, &addr);
    y += TrackLoader_readPath_a(&trackloader, &addr);

    { int16_t x_abs = x;
    int16_t y_abs = y;
    if (x_abs < 0) x_abs = -x_abs;
    if (y_abs < 0) y_abs = -y_abs;

    { int16_t scroll_x;

    /* scroll right */
    if (y_abs > x_abs)
    {
        scroll_x = (0x100 * x) / y;
    }
    /* scroll left */
    else
    {
        scroll_x = (-0x100 * y) / x;
    }
    
    /* turn right */
    if (x > 0)       scroll_x += 0x200;
    else if (x < 0)  scroll_x += 0x600;
    else if (y >= 0) scroll_x += 0x200;
    else             scroll_x += 0x600;

    if (y_abs > x_abs)
    {
        int32_t d0 = x * y;

        if (d0 >= 0)
            scroll_x -= 0x200;
        else
            scroll_x += 0x200;
    }

    self->tilemap_h_target = scroll_x;
 } }}

/* Creates the bend in the next section of road. */
/* */
/* Source Address: 0x180C */static 

void ORoad_setup_hscroll(ORoad* self)
{
    switch (self->road_ctrl)
    {
        case ROAD_OFF:
            return;

        case ROAD_R0:
        case ROAD_R0_SPLIT:
            ORoad_do_road_offset(self, self->road0_h, -self->road_width_bak, false);
            break;

        case ROAD_R1:
            ORoad_do_road_offset(self, self->road1_h, +self->road_width_bak, false);
            break;

        case ROAD_BOTH_P0:
        case ROAD_BOTH_P1:
            ORoad_do_road_offset(self, self->road0_h, -self->road_width_bak, false);
            ORoad_do_road_offset(self, self->road1_h, +self->road_width_bak, false);
            break;

        case ROAD_BOTH_P0_INV:
        case ROAD_BOTH_P1_INV:
            ORoad_do_road_offset(self, self->road0_h, -self->road_width_bak, false);
            ORoad_do_road_offset(self, self->road1_h, +self->road_width_bak, true);
            break;

        case ROAD_R1_SPLIT:
            ORoad_do_road_offset(self, self->road1_h, +self->road_width_bak, true);
            break;  
    }
}

/* Adjust the offset for Road 0 or Road 1 */
/* */
/* Source Address: 0x1864 */
/* */
/* Setup the H-Scroll Values in RAM (not Road RAM) for Road 0. */
/* Take into account the position of the player's car. */
/* */
/* The road data is adjusted per scanline. */
/* It is adjusted a different amount depending on how far into the horizon it is.  */
/* So lines closer to the player's car are adjusted more. */static 

void ORoad_do_road_offset(ORoad* self, int16_t* dst_x, int16_t width, bool invert)
{
    int32_t car_offset = self->car_x_bak + width + oinitengine.camera_x_off; /* note extra debug of camera x */
    int32_t scanline_inc = 0; /* Total amount to increment scanline (increased every line) */
    int16_t* src_x = self->road_x; /* a0 */

    /* --------------------------------------------------------------- */
    /* Process H-Scroll: Car not central on road 0 */
    /* The following loop ensures that the road offset  */
    /* becomes before significant the closer it is to the player's car */
    /* --------------------------------------------------------------- */
    if (car_offset != 0)
    {
        car_offset <<= 7;
        { uint16_t i; for (i = 0; i <= 0x1FF; i++)
        {
            int16_t x_off;
            int16_t h_scroll = scanline_inc >> 16;

            /* If ignore car position */
            if (*src_x == 0x3210)
                h_scroll = 0;

            /* Get next line of road x data */
            x_off = (*src_x++) >> 6;

            if (!invert)
                h_scroll += x_off;
            else
                h_scroll -= x_off;
            
            /* Write final h-scroll value  */
            (*dst_x++) = h_scroll;

            scanline_inc += car_offset;
        } }
        return;
    }
    /* ------------------------------ */
    /* Ignore Car Position / H-Scroll */
    /* ------------------------------ */

    if (!invert)
    {
        { uint16_t i; for (i = 0; i <= 0x3F; i++)
        {
            (*dst_x++) = (*src_x++) >> 6;
            (*dst_x++) = (*src_x++) >> 6;
            (*dst_x++) = (*src_x++) >> 6;
            (*dst_x++) = (*src_x++) >> 6;

            (*dst_x++) = (*src_x++) >> 6;
            (*dst_x++) = (*src_x++) >> 6;
            (*dst_x++) = (*src_x++) >> 6;
            (*dst_x++) = (*src_x++) >> 6;
        } }
    }
    else
    {
        { uint16_t i; for (i = 0; i <= 0x3F; i++)
        {
            (*dst_x++) = -(*src_x++) >> 6;
            (*dst_x++) = -(*src_x++) >> 6;
            (*dst_x++) = -(*src_x++) >> 6;
            (*dst_x++) = -(*src_x++) >> 6;

            (*dst_x++) = -(*src_x++) >> 6;
            (*dst_x++) = -(*src_x++) >> 6;
            (*dst_x++) = -(*src_x++) >> 6;
            (*dst_x++) = -(*src_x++) >> 6;
        } }
    }
}

/* Main Loop */
/*  */
/* Source Address: 0x1B12 */
/* Input:          None */
/* Output:         None */

/* Road height information is stored in blocks in this ROM. */
/* Each block is interchangeable and can be used with any segment of road. */
/* Each block can vary in length. */
/* */
/* The format of each height block is as follows: */
/* */
/* +0  [Byte] Jump Table Control */
/*            0 = Elevated section */
/*            4 = Flat section */
/* +1  [Byte] Length of each section */
/* +2  [Byte] Length of ascents (multiplied with +1) */
/* +3  [Byte] Length of descents (multiplied with +1) */
/* +4  [Word] 1st Signed Height Value For Section Of Road */
/* +6  [Word] 2st Signed Height Value For Section Of Road */
/* */
/* etc. */
/* */
/* +xx [Word] 0xFFFF / -1 Signifies end of height data */static 

void ORoad_setup_road_y(ORoad* self)
{
    self->pos_fine_diff = self->pos_fine - self->pos_fine_old;
    self->pos_fine_old = self->pos_fine;

    /* Setup horizon base value if necessary */
    if (!self->horizon_set)
    {
        self->horizon_base = 0x240;
        self->horizon_set = 1;
    }

    /* Code omitted that doesn't seem to be used */

    switch (self->height_ctrl)
    {
        /* Clear Road Height Segment & Init Segment 0 */
        case 0:
            self->height_lookup = 0;
            ORoad_init_height_seg(self);
            break;

        /* Init Next Road Height Segment */
        case 1:
            ORoad_init_height_seg(self);
            break;

        /* Segment Initialized: Use Elevation Flag */
        case 2:
            ORoad_do_elevation(self);
            break;

        /* Segment Initalized: Delayed Elevation Section */
        case 3:
            ORoad_do_elevation_delay(self);
            break;

        /* Needed for stage 0x1b */
        case 4:
            ORoad_do_elevation_mixed(self);
            break;

        /* Segment Initalized: Ignore Elevation Flag */
        case 5:
            ORoad_do_horizon_adjust(self);
            break;
    }
}

/* Source Address: 0x1BCE */static 
void ORoad_init_height_seg(ORoad* self)
{
    self->height_index = 0;         /* Set to first entry in height segment */
    self->height_inc   = 0;         /* Do not increment to next segment */
    self->elevation    = ROAD_NO_CHANGE; /* No Change to begin with */
    self->height_step  = 1;    /* Set to start of height segment */

    /* Get Address of actual road height data */
    self->height_lookup_wrk = self->height_lookup;
    { uint32_t h_addr = TrackLoader_read_heightmap_table(&trackloader, self->height_lookup_wrk);

    self->height_ctrl2 = TrackLoader_read8_a(trackloader.heightmap_data, &h_addr);
    self->step_adjust  = TrackLoader_read8_a(trackloader.heightmap_data, &h_addr); /* Speed at which to move through height segment */

    switch (self->height_ctrl2)
    {
        case 0:
            ORoad_init_elevation(self, &h_addr);
            break;
        
        case 1:
        case 2:
            ORoad_init_elevation_delay(self, &h_addr);
            break;
        
        case 3:
            ORoad_init_elevation_mixed(self, &h_addr);
            break;
        
        case 4:
            ORoad_init_horizon_adjust(self, &h_addr);
            break;
    }
 }}



/* Standard Elevation Height Section. */
/* A series of sequential values are provided to adjust the horizon position. */
/* */
/* The duration of each value can be adjusted using the 'step' value and the multipliers.  */
/* */
/* To map a road position to a height segment length: */
/* */
/*  1 Position */
/*  = 10 Pos Fine Difference (* 10) */
/*  = 120 Height Step        (* 12) */
/*  = 120 / 3 Step Adjust */
/*  = 40 */
/*  = 255 / 40 = 6.3 positions per height segment */
/* */
/* Source Address: 0x1C2C */static 
void ORoad_init_elevation(ORoad* self, uint32_t* addr)
{
    self->down_mult   = TrackLoader_read8_a(trackloader.heightmap_data, &(*addr));
    self->up_mult     = TrackLoader_read8_a(trackloader.heightmap_data, &(*addr));
    self->height_addr = (*addr);
    self->height_ctrl = 2; /* Use do_elevation() */
    ORoad_do_elevation(self);
}

static 

void ORoad_do_elevation(ORoad* self)
{
    uint16_t d3;
    /* By Default: One Height Entry Per Two Road Positions  */
    /* Potential advance stage of height map we're on  */
    self->height_step += self->pos_fine_diff * 12;

    /* Adjust the speed at which we transverse elevation sections of the height map */
    d3 = self->step_adjust;
    if (self->elevation == ROAD_UP)
        d3 *= self->up_mult;
    else if (self->elevation == ROAD_DOWN)
        d3 *= self->down_mult;

    { uint16_t d1 = (self->height_step / d3);
    if (d1 > 0xFF) d1 = 0xFF;
    d1 += 0x100;

    self->height_start = d1;
    self->height_end   = d1;

    /* Advance to next height entry if appropriate.  */
    self->height_index += self->height_inc; 
    self->height_inc = 0;

    /* Increment to next height entry in table. */
    if (self->height_start == 0x1FF)
    {
        self->height_start = 0x1FF;
        self->height_end   = 0x1FF;
        self->height_step  = 1; /* Start Of Height Segment */
        self->height_inc   = 1;
        self->elevation    = ROAD_NO_CHANGE;
        return;
    }
    if (self->height_lookup == 0 || self->height_lookup_wrk != 0)
        return;
    /* If working road height index is 0, then init next section */
    self->height_ctrl = 1;
 }}

/* Hold Elevation For Specified Delay */
/* */
/* Two values are provided by the data referenced by Height Index and split over three parts. */
/* */
/* Part 1: [Height Index = 0] Height Start Increments from 256 to 511. Change horizon to relevant height over this period */
/* Part 2: [Height Index = 1] Decrement Delay from provided value to 0. Hold horizon at this height */
/* Part 3: [Height Index = 1] Height Start Decrements from 511 to 256. Revert horizon to original height over this period */
/* */
/*          <-delay-> */
/*          _________ */
/*    [1]  /   [2]   \ [3] */
/* _______/           \______ */
/* */
/* Height End remains constant during this period at 512 */
/* */
/* Note: There is an intriguing bug in this routine, where the actual length of part 2 differs dependent on the speed you */
/*       are driving at. It's actually possible to elongate the length of hills by driving slower! */
/* */
/* Example Data: 0x29AA */
/* Source      : 0x1CE4 (first used at the chicane at stage 1) */static 

void ORoad_init_elevation_delay(ORoad* self, uint32_t* addr)
{
    self->height_delay  = TrackLoader_read16_a(trackloader.heightmap_data, &(*addr));
    self->height_addr   = (*addr);
    self->do_height_inc = 1;     /* Set to Part 1 */
    self->height_inc    = 0;
    self->height_end    = 0x100; /* Remains constant */
    self->height_ctrl   = 3;     /* Use do_elevation_delay() */
    ORoad_do_elevation_delay(self);
}

/* Source: 1D04 */static 
void ORoad_do_elevation_delay(ORoad* self)
{
    int16_t d1 = self->pos_fine_diff * 12;
    self->height_index += self->height_inc; /* Next height entry */
    self->height_inc = 0;

    /* Part 1             || Part 3 */
    if (self->height_index == 0 || self->do_height_inc == 0)
    {
        /* 1D50 inc_pos_in_seg: */
        self->height_step += d1;
        /*d1 = 0; */
        d1 = self->height_step / self->step_adjust;
        if (d1 > 0xFF) d1 = 0xFF;

        self->height_start = d1;

        /* Advance to Part 2. */
        if (self->height_start > 0xFE)
        {
            self->height_start = 0xFF;
            self->height_step  = 1;
            self->height_inc   = 1;
            self->elevation    = ROAD_NO_CHANGE; /* Keep horizon at level for delayed section */
        }

        /* Part 3: Invert counter */
        if (self->height_index != 0)
        {
            d1 = 0xFF - self->height_start;
        }

        self->height_start = d1 + 0x100;
    }
    /* Part 2 */
    /* Source: 0x1D2E  */
    else
    {
        self->height_delay -= (d1 / self->step_adjust);    
        self->height_start = 0x1FF;

        if (self->height_delay < 0) /* Set flag so we inc next time */
            self->do_height_inc = 0;
    }
}

/* Mixed Elevation Section */
/* */
/* This is a combination of looking ahead at the height map entries, but then holding on entry 6 when reached. */
/* It's like a combination of the previous two, although the way in which it's used seems to offer */
/* no real advantage over the previous hold section code. */
/* */
/* Part 1: [Height Index = 0 - 5] Change horizon to relevant height over this period by looking at upcoming height entries. */
/*                                Height Start & End Increments from 256 to 511.  */
/* Part 2: [Height Index = 6]     Decrement Delay from provided value to 0. Hold horizon at this height. */
/*                                Height Start constant at 511 and end at 256 */
/* Part 3: [Height Index = 1]     Height Start Decrements from 511 to 256. Revert horizon to original height over this period. */
/* */
/* Source      : 0x1DAC */
/* Example Data: 0x2A8E */static 
void ORoad_init_elevation_mixed(ORoad* self, uint32_t* addr)
{
    self->height_delay  = TrackLoader_read16_a(trackloader.heightmap_data, &(*addr));
    self->height_addr   = (*addr);
    self->do_height_inc = 1;
    self->height_inc    = 0;
    self->height_ctrl   = 4;    /* Use do_elevation_mixed() function */
    ORoad_do_elevation_mixed(self);
}

/* Source: 0x1DC6 */static 
void ORoad_do_elevation_mixed(ORoad* self)
{
    uint16_t d1 = self->pos_fine_diff * 12;
    self->height_index += self->height_inc;
    self->height_inc = 0;
    
    /* Parts 2 & 3 - Delayed Section. Source: 0x1E1C */
    if (self->height_index >= 6)
    {
        uint16_t d3 = self->step_adjust;

        /* Part 2: Create a delay on the sixth entry */
        if (self->do_height_inc != 0)
        {
            self->height_delay -= (d1 / d3);
            self->height_start = 0x1FF;
            self->height_end = 0x100;
            /* Delay has expired. Advance to last entry. Source: 0x1E46 */
            if (self->height_delay < 0)
            {
                self->height_addr += 12; /* Advance 6 words. */
                self->do_height_inc = 0;
            }
        }
        /* Part 3. Source: 0x1E58 */
        else
        {
            self->height_step += d1;
            d1 = self->height_step / d3;
            if (d1 > 0xFF) d1 = 0xFF;
            self->height_start = 0x1FF - d1;
            if (self->height_start != 0x100) return;
            
            self->height_step = 1; /* Set position on road segment to start */
            self->height_inc  = 1;
            self->elevation   = ROAD_NO_CHANGE;
        }
    }
    /* Part 1. Source: 0x1DEA */
    /* Look ahead at next 6 entries in data as normal.  */
    else
    {
        self->height_step += d1;
        d1 = self->height_step / 4;
        if (d1 > 0xFF) d1 = 0xFF;
        d1 += 0x100;
        self->height_start = d1;
        self->height_end   = d1;

        if (self->height_start < 0x1FF) return;
        
        /* set_end2: */
        self->height_start = 0x1FF;
        self->height_end   = 0x1FF;
        self->height_step  = 1; /* Set position on road segment to start */
        self->height_inc   = 1;
        self->elevation    = ROAD_NO_CHANGE;
    }
}

/* Adjust Horizon To New Position */
/* */
/* Part 1: Height Start Increments from 256 to 512. Change horizon to relevant height over this period. */
/*         Alter the speed at which this takes place with the step value. */
/* Part 2: Horizon is set and remains at this position until changed with another horizon data block.  */
/* */
/* Example Data  : 0x3672 */
/* Source        : 0x1EB6 */static 
void ORoad_init_horizon_adjust(ORoad* self, uint32_t* addr)
{
    self->height_addr = (*addr);

    /* Set Horizon Modifier */
    self->horizon_mod = TrackLoader_read16(trackloader.heightmap_data, (*addr)) - self->horizon_base;
    
    /* Use do_horizon_adjust() function */
    self->height_ctrl = 5;
    ORoad_do_horizon_adjust(self);
}

static 

void ORoad_do_horizon_adjust(ORoad* self)
{
    self->height_step += self->pos_fine_diff * 12;
    { uint16_t d1 = (self->height_step / self->step_adjust);

    /* Scale height between 100 and 1FF */
    if (d1 > 0xFF) d1 = 0xFF;
    d1 += 0x100;

    self->height_start = d1;
    self->height_end   = d1;
 }}

/* ---------------------------------------------------------------------------- */
/* END OF SETUP SECTION */
/* ---------------------------------------------------------------------------- */

/* Source Address: 0x1F00 */static 
void ORoad_set_road_y(ORoad* self)
{
    switch (self->height_ctrl2)
    {
        /* Set Normal Y Coordinate */
        case 0:
        case 1:
        case 2:
        case 3:
            ORoad_set_y_interpolate(self);
            break;
        /* Set Y with Horizon Adjustment (Stage 2 - Gateway) */
        case 4:
            ORoad_set_y_horizon(self);
            break;
    }
}

/* 1/ Takes distance into track section */
/* 2/ Interpolates this value into a series of counters, stored between 60700 - 6070D */
/* 3/ Each of these counters define how many times to write and adjust the height value */
/* */
/* section_lengths stores the distance from the player to the horizon. */
/* This essentially represents 7 track segments. */

/* Source Address: 0x1F22 */static 
void ORoad_set_y_interpolate(ORoad* self)
{
    /* ------------------------------------------------------------------------ */
    /* Convert desired elevation of track into a series of section lengths */
    /* Each stored in 7 counters which will be used to output the track */
    /* ------------------------------------------------------------------------ */

    uint16_t d2 = self->height_end;
    uint16_t d1 = 0x1FF - d2;

    self->section_lengths[0] = d1;
    d2 >>= 1;
    self->section_lengths[1] = d2;    
    
    { uint16_t d3 = 0x200 - d1 - d2;
        int32_t horizon_copy;
        int16_t next_height_value;
    self->section_lengths[3] = d3;
    d3 >>= 1;
    self->section_lengths[2] = d3;
    self->section_lengths[3] -= d3;
    
    d3 = self->section_lengths[3];
    self->section_lengths[4] = d3;
    d3 >>= 1;
    self->section_lengths[3] = d3;
    self->section_lengths[4] -= d3;
    
    d3 = self->section_lengths[4];
    self->section_lengths[5] = d3;
    d3 >>= 1;
    self->section_lengths[4] = d3;
    self->section_lengths[5] -= d3;

    d3 = self->section_lengths[5];
    self->section_lengths[6] = d3;
    d3 >>= 1;
    self->section_lengths[5] = d3;
    self->section_lengths[6] -= d3;

    self->length_offset = 0;

    /* Address of next entry in road height table data [rom so no need to scale] */
    self->a1_lookup = (self->height_index * 2) + self->height_addr;

    /* Road Y Positions (references to this decrement) [Destination]     */
    self->y_addr = 0x200 + self->road_p1;

    self->a3_o = 0;
    self->road_unk[self->a3_o++] = 0x1FF;
    self->road_unk[self->a3_o++] = 0;
    self->counter = 0; /* Reset interpolated self->counter index to 0 */

    next_height_value = TrackLoader_read16(trackloader.heightmap_data, self->a1_lookup);
    self->height_final = (next_height_value * (self->height_start - 0x100)) >> 4;

    /* 1faa  */
    horizon_copy = (self->horizon_base + self->horizon_offset) << 4;
    if (self->height_ctrl2 == 2)  /* hold state */
        horizon_copy += self->height_final;

    /*set_y_2044(0x200, horizon_copy, 0, y_addr, 2); */

    self->change_per_entry = horizon_copy;
    self->scanline = 0x200; /* 200 (512 pixels) */
    self->total_height = 0;
    ORoad_set_y_2044(self);  
 }}

/* Source: 0x2044 */static 
void ORoad_set_y_2044(ORoad* self)
{
    int16_t section_length;
    if (self->change_per_entry > 0x10000)
        self->change_per_entry = 0x10000;

    self->d5_o = self->change_per_entry;

    /* ------------------------------------------------------------------------ */
    /* Get length of this road section in scanlines */
    /* ------------------------------------------------------------------------ */
    section_length = self->section_lengths[self->length_offset++] - 1;
    self->scanline -= section_length;

    /* ------------------------------------------------------------------------ */
    /* Write this section of road number of times dictated by length */
    /* Source: 0x2080: write_y */
    /* ------------------------------------------------------------------------ */
    if (section_length >= 0)
    {
        { uint16_t i; for (i = 0; i <= section_length; i++)
        {
            self->total_height += self->change_per_entry; 
            self->road_y[--self->y_addr] = (self->total_height << 4) >> 16;
        } }        
    }

    /* ------------------------------------------------------------------------ */
    /* Whilst there are still sections of track to write, read the height for */
    /* the upcoming section */
    /* Source: 0x216C */
    /* ------------------------------------------------------------------------ */
    if (++self->counter != 7)
    {
        ORoad_read_next_height(self);
        return;
    }

    /* Writing last part of interpolated data */
    self->road_unk[self->a3_o] = 0;

    { int16_t y = TrackLoader_read16(trackloader.heightmap_data, self->a1_lookup);

    /* Return if not end at end of height section data */
    if (y != -1) return;

    /* At end of height section data, initalize next segmnet  */
    if (self->height_lookup == self->height_lookup_wrk)
        self->height_lookup = 0;

    self->height_ctrl = 1; /* init next road segment */
 }}

/* Source Address: 0x1FC6 */static 
void ORoad_read_next_height(ORoad* self)
{
    switch (self->height_ctrl2)
    {
        case 0:
            /* set_elevation_flag */
            break;

        case 1:
            self->change_per_entry += self->height_final;
        case 2:
            ORoad_set_elevation(self);
            return; /* Note we return */
        case 3:
            if (self->height_index >= 6)
            {
                self->change_per_entry += self->height_final;
                ORoad_set_elevation(self);
                return;
            }
            /* otherwise set_elevation_flag */
            break;
    }

    /* 1ff2: set_elevation_flag */
    /* Note the way this bug was fixed */
    /* Needed to read the signed value into an int16_t before assigning to a 32 bit value */
    self->change_per_entry = TrackLoader_read16_a(trackloader.heightmap_data, &self->a1_lookup) << 4;
    self->change_per_entry += self->d5_o;
    if (self->counter != 1)
    {
        ORoad_set_elevation(self);
        return;
    }

    /* Writing first part of interpolated data */
    self->change_per_entry -= self->height_final;

    { int32_t horizon_shift = (self->horizon_base + self->horizon_offset) << 4;

    if (horizon_shift == self->change_per_entry)
    {
        ORoad_set_elevation(self);
    }
    else if (self->change_per_entry > horizon_shift)
    {
        self->elevation = ROAD_UP;
        ORoad_set_elevation(self);
    }
    else
    {
        self->elevation = ROAD_DOWN;
        ORoad_set_elevation(self);
    }
 }}

/* Source Address: 0x2028 */static 
void ORoad_set_elevation(ORoad* self)
{
    /* d2: h */
    /* d3: total_height */
    /* d4: scanline */
    /* d5: h_copy (global) */
    self->d5_o -= self->change_per_entry;

    if (self->d5_o == 0)
    {
        ORoad_set_y_2044(self);
        return;
    }

    /* should be 2400, 2400, 2400, ffffe764, ffffdc00 */
    if (self->d5_o < 0)
        self->d5_o = -self->d5_o;

    self->road_unk[self->a3_o++] = self->scanline;
    { int32_t total_height_wrk = self->total_height;
    if (total_height_wrk < 0)
        total_height_wrk = -total_height_wrk;
    total_height_wrk <<= 4;

    self->road_unk[self->a3_o++] = total_height_wrk >> 16;

    ORoad_set_y_2044(self);
 }}

/* Set Horizon Base Position. Used on Gateway. */
/* */
/* Source Address: 0x219E */static 
void ORoad_set_y_horizon(ORoad* self)
{
    uint32_t a0 = 0x200 + self->road_p1; /* a0 = Road Height Positions + Relevant Offset */
    
    int32_t d1 = (self->horizon_mod * (self->height_start - 0x100)) >> 4;
    int32_t d2 = ((self->horizon_base + self->horizon_offset) << 4) + d1;
    
    uint32_t total_height = 0;

    /* write_next_y: */
    { uint16_t i; for (i = 0; i <= 0x1FF; i++) /* (1FF height positions) */
    {
        total_height += d2;
        { uint32_t d0 = (total_height << 4) >> 16;
        self->road_y[--a0] = d0;
     }}

    self->road_unk[0] = 0; }
    
    /* If not at end of section return. Otherwise, setup new horizon y-value */
    if (self->height_start != 0x1FF) return;

    /* Read Up/Down Multiplier Word From Lookup Table and set new horizon base value */
    self->horizon_base = (int32_t) (TrackLoader_read16(trackloader.heightmap_data, self->height_addr));

    if (self->height_lookup == self->height_lookup_wrk)
        self->height_lookup = 0;

    /* Set master switch control to init next road segment */
    self->height_ctrl = 1;
}

/* Aspect correct road inclines. */
/* Set Horizon Tilemap Y Position */
/* */
/* Source Address: 0x13B8 */static 
void ORoad_set_horizon_y(ORoad* self)
{
    int16_t y_pos;
    /* ------------------------------------------------------------------------ */
    /* Aspect correct road inclines */
    /* ------------------------------------------------------------------------ */

    uint32_t road_int = 0; /* a0 */
    uint32_t road_y_addr = (0 + self->road_p1); /* a1 */

    /* read_next */
    while (true)
    {
        int16_t d0 = self->road_unk[road_int];
        if (d0 == 0) break;
        { int16_t d7 = d0 >> 3;
        int16_t d6 = (d7 + d0) - 0x1FF;

        if (d6 > 0)
        {
            d7 += d7;
            d7 -= d6;
            d7 >>= 1;
            d0 = 0x1FF - d7;
        }
        /* 13e6 */
        d6 = d7 >> 1; 
        if (d6 <= 2) break;
        { int16_t d1 = d0 - d6;
            uint16_t a2;
            int16_t d5;
        int16_t d2 = d0;
        int16_t d3 = d0 + d6;
        int16_t d4 = d0 + d7;

        /* 1402: Chris - made some edits below here, seems to go rather wrong from here onwards */
        d0 -= d7;
        d5 = d0;
        a2 = road_y_addr + d5;
        
        d0 = self->road_y[road_y_addr + d0];
        d1 = self->road_y[road_y_addr + d1];
        d2 = self->road_y[road_y_addr + d2];
        d3 = self->road_y[road_y_addr + d3];
        d4 = self->road_y[road_y_addr + d4];

        /* 1426: */
        d5 = d0;
        
        { int32_t d2l = (d2 + d0 + d4) * 0x5555; /* turn d2 into a long */
            int32_t d3l;
            int32_t d1l;
        d2l >>= 16;
        d1l = (d1 + d0 + (int16_t) d2l) * 0x5555;
        d1l >>= 16;
        d3l = (d3 + d4 + (int16_t) d2l) * 0x5555;
        d3l >>= 16;

        d0 -=  (int16_t) d1l;
        d1l -= (int16_t) d2l;
        d2l -= (int16_t) d3l;
        d3l -= d4;

        d0 = (d0 << 2)  / d6;
        d1 = (d1l << 2) / d6;
        d2 = (d2l << 2) / d6;
        d3 = (d3l << 2) / d6;

        d5 <<= 2;
        d6--;

        /* loop 1: */
        { int16_t i; for (i = 0; i <= d6; i++)
        {
            self->road_y[a2++] = d5 >> 2;
            d5 -= d0;
        } }
        /* loop 2: */
        { int16_t i; for (i = 0; i <= d6; i++)
        {
            self->road_y[a2++] = d5 >> 2;
            d5 -= d1;
        } }
        /* loop 3: */
        { int16_t i; for (i = 0; i <= d6; i++)
        {
            self->road_y[a2++] = d5 >> 2;
            d5 -= d2;
        } }
        /* loop 4: */
        { int16_t i; for (i = 0; i <= d6; i++)
        {
            self->road_y[a2++] = d5 >> 2;
            d5 -= d3;
        } }

        road_int += 2; 
     } } }} /* end loop */

    /* ------------------------------------------------------------------------ */
    /* Set Horizon Y Position */
    /* ------------------------------------------------------------------------ */

    y_pos = self->road_y[self->road_p3] >> 4;
    self->horizon_y2 = -y_pos + 224;
}

/* Parse Road Scanline Data. */
/* */
/* Output hardware ready format. */
/* Also output priority information for undulations in road for sprite hardware. */
/* This is so that sprites can be hidden behind crests of hills. */
/* */
/* Destination Output Format: */
/* */
/* ----s--- --------  Solid fill (1) or ROM fill */
/* -------- -ccccccc  Solid color (if solid fill) */
/* -------i iiiiiiii  Index for other tables */
/* */
/* Source: 0x1318 */static 

void ORoad_do_road_data(ORoad* self)
{
    int16_t scanline;
    /* Road data in RAM #1 [Destination] (Solid fill/index fill etc) */
    /* This is the final block of data to be output to road hardware */
    uint32_t addr_dst = 0x400 + self->road_p1;        /* [a0] */

    /* Road data in RAM [Source] */
    uint32_t addr_src = 0x200 + self->road_p1;        /* [a1] */

    /* Road Priority Elevation Data [Destination] */
    uint32_t addr_priority = 0x280 + self->road_p1;

    int16_t rom_line_select = 0x1FF;            /* This is the triangular road shape from ROM. Lines 0 - 512.  */
    int16_t src_this = self->road_y[--addr_src] >> 4; /* source entry #1 / 16 [d0] */
    int16_t src_next = 0;                       /* [d4] */
    int16_t write_priority = 0;                 /* [d5] */

    self->road_y[--addr_dst] = rom_line_select; /* Write source entry counter to a0 / destination */
    scanline = 254;

    /* ------------------------------------------------------------------------ */
    /* Draw normal road */
    /* ------------------------------------------------------------------------ */
    while (--rom_line_select > 0) /* Read next line of road source data */
    {
        src_next = self->road_y[--addr_src] >> 4;
        
        /* Plot next position of road using rom_line_select source */
        /* The speed at which we advance through the source data is controlled by the values in road_y,  */
        /* which are a sequential list of numbers when the road is flat */
        if (src_this < src_next)
        {
            /* Only draw if within screen area */
            if (--scanline <= 0)
            {
                self->road_y[addr_priority] = 0; self->road_y[addr_priority + 1] = 0; /* Denote end of priority list */
                return;
            }
            src_this = src_next; /* d0 = source entry #1 / 16 */
         
            self->road_y[--addr_dst] = rom_line_select; /* Set next line of road appearance info, from ROM */
            write_priority = -1;                  /* Denote that we need to write priority data */
        }
        else if (src_this == src_next) 
            continue;
        /* Write priority data for elevation. Only called when there is a hill.  */
        /* This is denoted by the next number in the sequence being smaller than the current */
        /* Note: This isn't needed to directly render the road. */
        else if (write_priority == -1)
        {
            self->road_y[addr_priority++] = rom_line_select; /* Write rom line select value */
            self->road_y[addr_priority++] = src_this; /* Write source entry value */
            write_priority = 0; /* Denote that values have been written */
        }
    } /* end loop */

    /* ------------------------------------------------------------------------ */
    /* Fill Horizon above road at top of screen */
    /* */
    /* Note that this is the area above the road, and is detected because */
    /* the rom line select is negative, and therefore there is no more to  */
    /* read. */
    /* ------------------------------------------------------------------------ */
    if (rom_line_select <= 0)
    {
        const uint16_t SOLID_FILL = 0x800;
        
        /* d3 = (0x3F | 0x800) = 0x83F [1000 00111111] Sets Solid Fill, Transparent Colour */
        const uint16_t TRANSPARENT = 0x3F | SOLID_FILL; 
        int16_t d7 = 255 - src_next - scanline;

        if (d7 < 0)
            d7 = SOLID_FILL; /* use default solid fill */

        /* loc 1388 */
        else if (d7 > 0x3F)
        {
            while (scanline-- > 0)
                self->road_y[--addr_dst] = TRANSPARENT; /* Copy transparent line to ram */
            
            /* end */
            self->road_y[addr_priority] = 0; self->road_y[addr_priority + 1] = 0;
            return;
        }
        else
            d7 |= SOLID_FILL; /* Solid Fill Bits | Colour Info */

        /* 1392 */
        do /* Output solid colours while looping (compare with default solid fill d3) */
        {
            self->road_y[--addr_dst] = d7; /* Output solid colour */
            if (--scanline < 0) /* Check whether scanloop counter expired */
            {
                self->road_y[addr_priority] = 0; self->road_y[addr_priority + 1] = 0;
                return;
            }
            self->road_y[--addr_dst] = d7; /* Output solid colour */
            if (--scanline < 0) /* Check whether scanloop counter expired */
            {
                self->road_y[addr_priority] = 0; self->road_y[addr_priority + 1] = 0;
                return;
            }
            d7++; /* Increment solid colour */
        }
        while (d7 <= TRANSPARENT);
        
        while (scanline-- > 0)
            self->road_y[--addr_dst] = TRANSPARENT; /* Copy transparent line to ram */
    /* 1346 */
    }

    /* end: */
    self->road_y[addr_priority] = 0; self->road_y[addr_priority + 1] = 0; /* Denote end of priority list */
}

/* ------------------------------------------------------------------------------------------------ */
/* ROUTINES FOLLOW TO BLIT RAM TO ROAD HARDWARE */
/* ------------------------------------------------------------------------------------------------ */

/* Source Address: 0x14C8 */static 
void ORoad_blit_roads(ORoad* self)
{
    const uint32_t road0_adr = 0x801C0;
    const uint32_t road1_adr = 0x803C0;

    switch (self->road_ctrl)
    {
        case ROAD_OFF:         /* Both Roads Off */
            return;

        case ROAD_R0:          /* Road 0 */
        case ROAD_R0_SPLIT:    /* Road 0 (Road Split.) */
            ORoad_blit_road(self, road0_adr);
            HWRoad_write_road_control(&hwroad, 0);
            break;

        case ROAD_R1:          /* Road 1 */
        case ROAD_R1_SPLIT:    /* Road 1 (Road Split. Invert Road 1) */
            ORoad_blit_road(self, road1_adr);
            HWRoad_write_road_control(&hwroad, 3);
            break;

        case ROAD_BOTH_P0:     /* Both Roads (Road 0 Priority) [DEFAULT] */
        case ROAD_BOTH_P0_INV: /* Both Roads (Road 0 Priority) (Road Split. Invert Road 1) */
            ORoad_blit_road(self, road0_adr);
            ORoad_blit_road(self, road1_adr);
            HWRoad_write_road_control(&hwroad, 1);
            break;

        case ROAD_BOTH_P1:     /* Both Roads (Road 1 Priority)  */
        case ROAD_BOTH_P1_INV: /* Both Roads (Road 1 Priority) (Road Split. Invert Road 1) */
            ORoad_blit_road(self, road0_adr);
            ORoad_blit_road(self, road1_adr);
            HWRoad_write_road_control(&hwroad, 2);
            break;
    }
}

static 

void ORoad_blit_road(ORoad* self, uint32_t a0)
{
    uint32_t a2 = 0x400 + self->road_p2;

    /* Write 0x1A0 bytes total Src: (0x260 - 0x400) Dst: 0x1C0 */
    { int16_t i; for (i = 0; i <= 13; i++)
    {
        a0 -= 2; HWRoad_write16(&hwroad, a0, self->road_y[--a2]);
        a0 -= 2; HWRoad_write16(&hwroad, a0, self->road_y[--a2]);
        a0 -= 2; HWRoad_write16(&hwroad, a0, self->road_y[--a2]);
        a0 -= 2; HWRoad_write16(&hwroad, a0, self->road_y[--a2]);
        a0 -= 2; HWRoad_write16(&hwroad, a0, self->road_y[--a2]);
        a0 -= 2; HWRoad_write16(&hwroad, a0, self->road_y[--a2]);
        a0 -= 2; HWRoad_write16(&hwroad, a0, self->road_y[--a2]);
        a0 -= 2; HWRoad_write16(&hwroad, a0, self->road_y[--a2]);

        a0 -= 2; HWRoad_write16(&hwroad, a0, self->road_y[--a2]);
        a0 -= 2; HWRoad_write16(&hwroad, a0, self->road_y[--a2]);
        a0 -= 2; HWRoad_write16(&hwroad, a0, self->road_y[--a2]);
        a0 -= 2; HWRoad_write16(&hwroad, a0, self->road_y[--a2]);
        a0 -= 2; HWRoad_write16(&hwroad, a0, self->road_y[--a2]);
        a0 -= 2; HWRoad_write16(&hwroad, a0, self->road_y[--a2]);
        a0 -= 2; HWRoad_write16(&hwroad, a0, self->road_y[--a2]);
        a0 -= 2; HWRoad_write16(&hwroad, a0, self->road_y[--a2]);
    } }
}

static 

void ORoad_output_hscroll(ORoad* self, int16_t* src, uint32_t dst)
{
    const int32_t d6 = 0x654;
    uint32_t src_addr = 0;

    /* Copy 16 bytes */
    { int32_t i; for (i = 0; i <= 0x3F; i++)
    {
        int16_t d0h = -src[src_addr++] + d6;
        int16_t d0l = -src[src_addr++] + d6;
        int16_t d1h = -src[src_addr++] + d6;
        int16_t d1l = -src[src_addr++] + d6;
        int16_t d2h = -src[src_addr++] + d6;
        int16_t d2l = -src[src_addr++] + d6;
        int16_t d3h = -src[src_addr++] + d6;
        int16_t d3l = -src[src_addr++] + d6;
        HWRoad_write16_a(&hwroad, &dst, d0h);
        HWRoad_write16_a(&hwroad, &dst, d0l);
        HWRoad_write16_a(&hwroad, &dst, d1h);
        HWRoad_write16_a(&hwroad, &dst, d1l);
        HWRoad_write16_a(&hwroad, &dst, d2h);
        HWRoad_write16_a(&hwroad, &dst, d2l);
        HWRoad_write16_a(&hwroad, &dst, d3h);
        HWRoad_write16_a(&hwroad, &dst, d3l);
    } }
}

/* Copy Background Colour To Road. */
/* + Scroll stripe data over road based on fine position */
/* + pos_fine is used as an offset into a table in rom. */
/* */
/* Source Address: 0x1ABC */
/* */
/* Notes: */
/* */
/* C00-FFF  ----bbbb --------  Background color index */
/*          -------- s-------  Road 1: stripe color index */
/*          -------- -a------  Road 1: pixel value 2 color index */
/*          -------- --b-----  Road 1: pixel value 1 color index */
/*          -------- ---c----  Road 1: pixel value 0 color index */
/*          -------- ----s---  Road 0: stripe color index */
/*          -------- -----a--  Road 0: pixel value 2 color index */
/*          -------- ------b-  Road 0: pixel value 1 color index */
/*          -------- -------c  Road 0: pixel value 0 color index */static 
 
void ORoad_copy_bg_color(ORoad* self)
{
    /* Scroll stripe data over road based on fine position */
    uint32_t pos_fine_copy = (self->pos_fine & 0x1F) << 10;
    uint32_t src = ROAD_BGCOLOR + pos_fine_copy;
    uint32_t dst = HW_BGCOLOR;

    /* Copy 1K */
    { int i; for (i = 0; i < 32; i++)
    {
        HWRoad_write32(&hwroad, &dst, RomLoader_read32_a(roms.rom1p, &src));
        HWRoad_write32(&hwroad, &dst, RomLoader_read32_a(roms.rom1p, &src));
        HWRoad_write32(&hwroad, &dst, RomLoader_read32_a(roms.rom1p, &src));
        HWRoad_write32(&hwroad, &dst, RomLoader_read32_a(roms.rom1p, &src));
        HWRoad_write32(&hwroad, &dst, RomLoader_read32_a(roms.rom1p, &src));
        HWRoad_write32(&hwroad, &dst, RomLoader_read32_a(roms.rom1p, &src));
        HWRoad_write32(&hwroad, &dst, RomLoader_read32_a(roms.rom1p, &src));
        HWRoad_write32(&hwroad, &dst, RomLoader_read32_a(roms.rom1p, &src));
    } }
}
