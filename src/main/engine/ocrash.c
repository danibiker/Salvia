/***************************************************************************
    Collision & Crash Code. 
    
    There are two types of collision: Scenery & Traffic.
    
    1/ Traffic: The Ferrari will spin after a collision.
    2/ Scenery: There are three types of scenery collision:
       - Low speed bump. Car rises slightly in the air and stalls.
       - Mid speed spin. Car spins and slides after collision.
       - High speed flip. If slightly slower, car rolls into screen.
         Otherwise, grows towards screen and vanishes
         
    Known Issues With Original Code:
    - Passenger sprites flicker if they land moving in the water on Stage 1
    
    The Ferrari sprite is used differently by the crash code.
    As there's only one of them, I've rolled the additional variables into
    this class. 
    
    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#include "engine/oferrari.h"
#include "engine/oinputs.h"
#include "engine/olevelobjs.h"
#include "engine/outils.h"
#include "engine/ocrash.h"

static void OCrash_do_crash(OCrash* self);
static void OCrash_spin_switch(OCrash* self, const uint16_t);
static void OCrash_crash_switch(OCrash* self);
static void OCrash_init_collision(OCrash* self);
static void OCrash_do_collision(OCrash* self);
static void OCrash_end_collision(OCrash* self);
static void OCrash_do_bump(OCrash* self);
static void OCrash_do_car_flip(OCrash* self);
static void OCrash_init_finger(OCrash* self, uint32_t);
static void OCrash_trigger_smoke(OCrash* self);
static void OCrash_post_flip_anim(OCrash* self);
static void OCrash_pan_camera(OCrash* self);
static void OCrash_init_spin1(OCrash* self);
static void OCrash_init_spin2(OCrash* self);
static void OCrash_collide_slow(OCrash* self);
static void OCrash_collide_med(OCrash* self);
static void OCrash_collide_fast(OCrash* self);
static void OCrash_done(OCrash* self, oentry*);
static void OCrash_do_shadow(OCrash* self, oentry*, oentry*);
static void OCrash_do_crash_passengers(OCrash* self, oentry*);
static void OCrash_flip_start(OCrash* self, oentry*);
static void OCrash_crash_pass1(OCrash* self, oentry*);
static void OCrash_crash_pass2(OCrash* self, oentry*);
static void OCrash_crash_pass_flip(OCrash* self, oentry*);
static void OCrash_pass_flip(OCrash* self, oentry*);
static void OCrash_pass_situp(OCrash* self, oentry*);
static void OCrash_pass_turnhead(OCrash* self, oentry*);

OCrash ocrash;




void OCrash_init(OCrash* self, oentry* f, oentry* s, oentry* p1, oentry* p1s, oentry* p2, oentry* p2s)
{
    self->spr_ferrari = f;
    self->spr_shadow  = s;
    self->spr_pass1   = p1;
    self->spr_pass1s  = p1s;
    self->spr_pass2   = p2;
    self->spr_pass2s  = p2s;

    /* Setup function pointers for passenger sprites */
    self->function_pass1 = OCrash_do_crash_passengers;
    self->function_pass2 = OCrash_do_crash_passengers;
}

bool OCrash_is_flip(OCrash* self)
{
    return self->crash_counter && self->crash_type == CRASH_FLIP;
}

void OCrash_enable(OCrash* self)
{
    /* This is called multiple times, so need this check in place */
    if (self->spr_ferrari->control & ENABLE) 
        return;

    self->spr_ferrari->control |= ENABLE;
    
    /* Reset all corresponding variables */
    self->spinflipcount1 = 0;
    self->spinflipcount2 = 0;
    self->slide = 0;
    self->frame = 0;
    self->addr = 0;
    self->camera_x_target = 0;
    self->camera_xinc = 0;
    self->lookup_index = 0;
    self->frame_restore = 0;
    self->shift = 0;
    self->crash_speed = 0;
    self->crash_zinc = 0;
    self->crash_side = 0;

    self->spr_ferrari->counter = 0;

    outrun.ttrial.crashes++;
}

/* Source: 0x1128 */
void OCrash_clear_crash_state(OCrash* self)
{
    self->spin_control1 = 0;
    self->coll_count1 = 0;
    self->coll_count2 = 0;
    olevelobjs.collision_sprite = 0;
    self->crash_counter = 0;
    self->crash_state = 0;
    self->crash_z = 0;
    self->spin_pass_frame = 0;
    self->crash_spin_count = 0;
    self->crash_delay = 0;
    self->crash_type  = 0;
    self->skid_counter = 0;
}

void OCrash_tick(OCrash* self)
{
    if (!outrun.tick_frame &&
        ORoad_get_view_mode(&oroad) == VIEW_INCAR &&
        self->crash_type != CRASH_FLIP)
    {
        return;
    }

    /* Do Ferrari */
    if (self->spr_ferrari->control & ENABLE)
    {
        if (outrun.tick_frame)
            OCrash_do_crash(self);
        else
            OSprites_do_spr_order_shadows(&osprites, self->spr_ferrari);
    }

    /* Do Car Shadow */
    if (self->spr_shadow->control & ENABLE)
    {
        if (outrun.tick_frame)
            OCrash_do_shadow(self, self->spr_ferrari, self->spr_shadow);
        else
            OSprites_do_spr_order_shadows(&osprites, self->spr_shadow);
    }

    /* Do Passenger 1 */
    if (self->spr_pass1->control & ENABLE)
    {
        if (outrun.tick_frame)
            (self->function_pass1)(&ocrash, self->spr_pass1);
        else
            OSprites_do_spr_order_shadows(&osprites, self->spr_pass1);
    }

    /* Do Passenger 1 Shadow */
    if (self->spr_pass1s->control & ENABLE)
    {
        if (outrun.tick_frame)
            OCrash_do_shadow(self, self->spr_pass1, self->spr_pass1s);
        else
            OSprites_do_spr_order_shadows(&osprites, self->spr_pass1s);
    }

    /* Do Passenger 2 */
    if (self->spr_pass2->control & ENABLE)
    {
        if (outrun.tick_frame)
            (self->function_pass2)(&ocrash, self->spr_pass2);
        else
            OSprites_do_spr_order_shadows(&osprites, self->spr_pass2);
    }

    /* Do Passenger 2 Shadow */
    if (self->spr_pass2s->control & ENABLE)
    {
        if (outrun.tick_frame)
            OCrash_do_shadow(self, self->spr_pass2, self->spr_pass2s);
        else
            OSprites_do_spr_order_shadows(&osprites, self->spr_pass2s);
    }
}

/* Source: 0x1162 */static 
void OCrash_do_crash(OCrash* self)
{
    int16_t spin1_copy;
    int16_t spin2_copy;
    int16_t steering_adjust;
    switch (outrun.game_state)
    {
        case GS_INIT_MUSIC:
        case GS_MUSIC:
            OCrash_end_collision(self);
            return;

        /* Fall through to continue code below */
        case GS_ATTRACT:
        case GS_INGAME:
        case GS_BONUS:
            break;

        default:
            /* In other modes render crashing ferrari if crash counter is set */
            if (self->crash_counter)
            {
                if (ORoad_get_view_mode(&oroad) != VIEW_INCAR || self->crash_type == CRASH_FLIP)
                    OSprites_do_spr_order_shadows(&osprites, self->spr_ferrari);
            }
            /* Set Distance Into Screen from crash counter */
            self->spr_ferrari->road_priority = self->spr_ferrari->counter;
            return;
    }

    /* ------------------------------------------------------------------------ */
    /* cont1: Adjust steering */
    /* ------------------------------------------------------------------------ */
    steering_adjust = oinputs.steering_adjust;
    oinputs.steering_adjust = 0;

    if (!oferrari.car_ctrl_active)
    {
        if (self->spin_control1 == 0)
        {
            uint16_t inc = ((oinitengine.car_increment >> 16) * 31) >> 5;
            oinitengine.car_increment = (oinitengine.car_increment & 0xFFFF) | (inc << 16);
            oferrari.car_inc_old = inc;
        }
        else
        {
            oinputs.steering_adjust = steering_adjust >> 1;
        }
    }

    /* ------------------------------------------------------------------------ */
    /* Determine whether to init spin or crash code */
    /* ------------------------------------------------------------------------ */
    spin2_copy = self->spin_control2;

    /* dec_spin2: */
    if (spin2_copy != 0)
    {
        spin2_copy -= 2;
        if (spin2_copy < 0)
            self->spin_control2 = 0;
        else
            OCrash_spin_switch(self, spin2_copy);
    }

    /* dec_spin1: */
    spin1_copy = self->spin_control1;

    if (spin1_copy != 0)
    {
        spin1_copy -= 2;
        if (spin1_copy < 0)
            self->spin_control1 = 0;
        else
            OCrash_spin_switch(self, spin1_copy);
    }
    /* Not spinning, init crash code */
    else
        OCrash_crash_switch(self);
}

/* Source: 0x1224 */static 
void OCrash_spin_switch(OCrash* self, const uint16_t ctrl)
{
    self->crash_counter++;
    self->crash_z = 0;
    
    switch (ctrl & 3)
    {
        /* No Spin - Need to init crash/spin routines */
        case 0:
            OCrash_init_collision(self);
            break;
        /* Init Spin */
        case 1:
            OCrash_do_collision(self);
            break;
        /* Spin In Progress - End Collision Routine */
        case 2:
        case 3:
            OCrash_end_collision(self);
            break;
    }
}

/* Init Crash. */
/* */
/* Source: 0x1252 */static 
void OCrash_crash_switch(OCrash* self)
{
    self->crash_counter++;
    self->crash_z = 0;

    switch (self->crash_state & 7)
    {
        /* No Crash. Need to setup crash routines. */
        case 0:
            OCrash_init_collision(self);
            break;
        /* Initial Collision */
        case 1:
            if ((self->crash_type & 3) == 0) OCrash_do_bump(self);
            else OCrash_do_collision(self);
            break;
        /* Flip Car */
        case 2:
            OCrash_do_car_flip(self);
            break;
        /* Horizontally Flip Car, Trigger Smoke Cloud */
        case 3:
        case 4:
            OCrash_trigger_smoke(self);
            break;
        /* Do Girl Pointing Finger Animation/Delay Before Pan */
        case 5:
            OCrash_post_flip_anim(self);
            break;
        /* Pan Camera To Track Centre */
        case 6:
            OCrash_pan_camera(self);
            break;
        /* Camera Repositioned. Prepare For Restart. */
        case 7:
            OCrash_end_collision(self);
            break;
    }
}

/* Init Collision. Used For Spin & Flip */
/* */
/* Source: 0x1962 */static 
void OCrash_init_collision(OCrash* self)
{
    oferrari.car_state = CAR_ANIM_SEQ; /* Denote car animation sequence */

    /* Enable crash sprites */
    self->spr_shadow->control |= ENABLE;
    self->spr_pass1->control  |= ENABLE;
    self->spr_pass2->control  |= ENABLE;

    /* Disable normal sprites */
    oferrari.spr_ferrari->control &= ~ENABLE;
    oferrari.spr_shadow->control  &= ~ENABLE;
    oferrari.spr_pass1->control   &= ~ENABLE;
    oferrari.spr_pass2->control   &= ~ENABLE;

    self->spr_ferrari->x = oferrari.spr_ferrari->x;
    self->spr_ferrari->y = 221;
    self->spr_ferrari->counter = 0x1FC;
    self->spr_ferrari->draw_props = ANCHOR_BOTTOM;

    /* Collided with another vechicle */
    if (self->spin_control2)
        OCrash_init_spin2(self);
    else if (self->spin_control1)
        OCrash_init_spin1(self);
    /* Crash into scenery */
    else
    {
        self->skid_counter = 0;
        { uint16_t car_inc = oinitengine.car_increment >> 16;
        if (car_inc < 0x64)
            OCrash_collide_slow(self);
        else if (car_inc < 0xC8)
            OCrash_collide_med(self);
        else
            OCrash_collide_fast(self);
     }}
}

/* This code also triggers a flip, if the crash_type is set correctly. */
/* Source: 0x138C */static 
void OCrash_do_collision(OCrash* self)
{
    uint32_t property_table;
    if (olevelobjs.collision_sprite)
    {
        olevelobjs.collision_sprite = 0;
        if (self->spin_control1 || self->spin_control2)
        {
            self->spin_control2 = 0;
            self->spin_control1 = 0;
            OCrash_init_collision(self); /* Init collision with another sprite */
            return;
        }

        /* Road generator 1 */
        if (oinitengine.car_x_pos - (oroad.road_width >> 16) >= 0)
        {
            if (self->slide < 0)
            {
                self->slide = -self->slide;
                oinitengine.car_x_pos -= self->slide;
                OSoundInt_queue_sound(&osoundint, SOUND_CRASH2);
            }
        }
        /* Road generator 2 */
        else
        {
            if (self->slide >= 0)
            {
                self->slide = -self->slide;
                oinitengine.car_x_pos -= self->slide;
                OSoundInt_queue_sound(&osoundint, SOUND_CRASH2);
            }
        }
    }
    /* 0x13F8 */
    property_table = self->addr + (self->frame << 3);
    self->crash_z = self->spr_ferrari->counter;
    self->spr_ferrari->zoom = 0x80;
    self->spr_ferrari->priority = 0x1FD;
    oinitengine.car_x_pos -= self->slide;
    self->spr_ferrari->addr = RomLoader_read32(roms.rom0p, property_table);

    if (RomLoader_read8(roms.rom0p, 4 + property_table))
        self->spr_ferrari->control |= HFLIP;
    else
        self->spr_ferrari->control &= ~HFLIP;

    /*spr_ferrari->pal_src = roms.rom0p->read8(5 + property_table); */
    self->spr_ferrari->pal_src = oferrari.ferrari_pal;
    self->spin_pass_frame = (int8_t) RomLoader_read8(roms.rom0p, 6 + property_table);

    if (--self->spinflipcount2 > 0)
    {
        OCrash_done(self, self->spr_ferrari);
        return;
    }

    self->spinflipcount2 = self->crash_spin_count; /* Expired: spinflipcount */

    if (self->spinflipcount1)
    {
        self->frame++;

        /* 0x1470 */
        /* Initialize Car Flip */
        if (!self->spin_control1 && !self->spin_control2 && self->frame == 2 && self->crash_type != CRASH_SPIN)
        {
            self->crash_state = 2; /* flip */
            self->addr = outrun.adr.sprite_crash_flip;
            self->spinflipcount1 = 3; /* 3 flips remaining */
            self->spinflipcount2 = self->crash_spin_count;
            self->frame = 0;

            /* Enable passenger shadows */
            self->spr_pass1s->control |= ENABLE;
            self->spr_pass2s->control |= ENABLE;
            OCrash_done(self, self->spr_ferrari);
            return;
        }
        /* Do Spin */
        else
        {
            if (self->slide > 0)
                self->slide -= 2;
            else if (self->slide < -2)
                self->slide += 2;

            /* End of frame sequence */
            if (!RomLoader_read8(roms.rom0p, 7 + property_table))
            {
                OCrash_done(self, self->spr_ferrari);
                return;
            }
        }
    }
    /* 0x14F4 */
    self->frame = 0;
    
    /* Last spin */
    if (--self->spinflipcount1 <= 0)
    {
        OSoundInt_queue_sound(&osoundint, SOUND_STOP_SLIP);
        if (self->spin_control2)
        {
            self->spin_control2++;
        }
        else if (self->spin_control1)
        {
            self->spin_control1++;
        }
        /* Init smoke */
        else
        {
            self->crash_state = 4; /* trigger smoke */
            self->crash_spin_count = 1;
            self->spr_ferrari->x += self->slide; /* inc ferrari x based on slide value */
        }
    }
    else
        self->crash_spin_count++;

    OCrash_done(self, self->spr_ferrari);
}

/* Source: 0x1D0C */static 
void OCrash_end_collision(OCrash* self)
{
    /* Enable 'normal' Ferrari object */
    oferrari.spr_ferrari->control |= ENABLE;
    oferrari.spr_shadow->control  |= ENABLE;
    oferrari.spr_pass1->control   |= ENABLE;
    oferrari.spr_pass2->control   |= ENABLE;

    self->coll_count2 = self->coll_count1;
    if (!self->coll_count2)
        self->coll_count2 = self->coll_count1 = 1;

    self->crash_counter = 0;
    self->crash_state = 0;
    olevelobjs.collision_sprite = 0;
    
    oferrari.spr_ferrari->x = 0;
    oferrari.spr_ferrari->y = 221;
    oferrari.car_ctrl_active = true;
    oferrari.car_state = CAR_NORMAL;
    olevelobjs.spray_counter = 0;
    self->crash_z = 0;

    if (self->spin_control1)
        oferrari.car_inc_old = oinitengine.car_increment >> 16;
    else
        OFerrari_reset_car(&oferrari);

    self->spin_control2 = 0;
    self->spin_control1 = 0;

    self->spr_ferrari->control &= ~ENABLE;
    self->spr_shadow->control  &= ~ENABLE;
    self->spr_pass1->control   &= ~ENABLE;
    self->spr_pass1s->control  &= ~ENABLE;
    self->spr_pass2->control   &= ~ENABLE;
    self->spr_pass2s->control  &= ~ENABLE;

    self->function_pass1 = OCrash_do_crash_passengers;
    self->function_pass2 = OCrash_do_crash_passengers;
    oinputs.crash_input = 0x10; /* Set delay in processing steering */
}

/* Low Speed Bump - Car Rises in air and sinks */
/* Source: 0x12BE */static 
void OCrash_do_bump(OCrash* self)
{
    oferrari.car_ctrl_active = false;   /* Disable user control of car */
    self->spr_ferrari->zoom = 0x80;           /* Set Entry Number For Zoom Lookup Table */
    self->spr_ferrari->priority = 0x1FD;
    
    { int16_t new_position = (int8_t) RomLoader_read8(&(roms.rom0), DATA_MOVEMENT + (self->lookup_index << 3));

    if (new_position)
        self->crash_z = self->spr_ferrari->counter;

    self->spr_ferrari->y = 221 - (new_position >> self->shift);

    { uint32_t frames = self->addr + (self->frame << 3);
    self->spr_ferrari->addr = RomLoader_read32(roms.rom0p, frames);
    
    if (RomLoader_read8(roms.rom0p, frames + 4))
        self->spr_ferrari->control |= HFLIP;
    else
        self->spr_ferrari->control &= ~HFLIP;
    
    /*spr_ferrari->pal_src = roms.rom0p->read8(frames + 5); */
    self->spr_ferrari->pal_src = oferrari.ferrari_pal;
    self->spin_pass_frame = (int8_t) RomLoader_read8(roms.rom0p, frames + 6);

    if (++self->lookup_index >= 0x10)
    {
        self->addr += (self->frame_restore << 3);
        self->spr_ferrari->addr = RomLoader_read32(roms.rom0p, self->addr);
        self->spin_pass_frame = (int8_t) RomLoader_read8(roms.rom0p, self->addr + 6);
        self->crash_state = 4;      /* Trigger smoke cloud */
        self->crash_spin_count = 1; /* Denote Crash */
    }

    OCrash_done(self, self->spr_ferrari);
 } }}

/* Source: 0x1562 */static 
void OCrash_do_car_flip(OCrash* self)
{
    uint32_t frames;
    /* Do this if during the flip, the car has recollided with a new sprite + slow crash (similar to spin_collide) */
    if (olevelobjs.collision_sprite && self->crash_speed == 1)
    {
        uint16_t car_inc16 = oinitengine.car_increment >> 16;

        /* Road generator 1 */
        if (oinitengine.car_x_pos - (oroad.road_width >> 16) >= 0)
        {
            /* swap_slide_dir2 */
            if (self->slide < 0)
            {
                self->slide = -self->slide;
                oinitengine.car_increment = ((car_inc16 >> 1) << 16) | (oinitengine.car_increment & 0xFFFF);
                OSoundInt_queue_sound(&osoundint, SOUND_CRASH2);
                if (oinitengine.car_increment >> 16 > 0x14)
                {
                    int16_t z = self->spr_ferrari->counter > 0x1FD ? 0x1FD : self->spr_ferrari->counter; /* d3 */
                    int16_t x_adjust = (0x50 * z) >> 9; /* d1 */
                    if (self->slide < 0) x_adjust = -x_adjust;
                    oinitengine.car_x_pos -= x_adjust;
                }
            }
            /* 0x15F6 */
            else
                self->slide += (self->slide >> 3);
        }
        /* Road generator 2 */
        else
        {
            /* swap_slide_dir2: */
            if (self->slide >= 0)
            {
                self->slide = -self->slide;
                oinitengine.car_increment = ((car_inc16 >> 1) << 16) | (oinitengine.car_increment & 0xFFFF);
                OSoundInt_queue_sound(&osoundint, SOUND_CRASH2);
                if (oinitengine.car_increment >> 16 > 0x14)
                {
                    int16_t z = self->spr_ferrari->counter > 0x1FD ? 0x1FD : self->spr_ferrari->counter; /* d3 */
                    int16_t x_adjust = (0x50 * z) >> 9; /* d1 */
                    if (self->slide < 0) x_adjust = -x_adjust;
                    oinitengine.car_x_pos -= x_adjust;
                }
            }
            /* 0x15F6 */
            else
                self->slide += (self->slide >> 3);
        }
    }
    
    /* flip_cont */
    olevelobjs.collision_sprite = 0; /* Moved this for clarity */
    frames = self->addr + (self->frame << 3);
    self->spr_ferrari->addr = RomLoader_read32(roms.rom0p, frames);

    /* ------------------------------------------------------------------------ */
    /* Fast Crash: Car Heads towards camera in sky, before vanishing (0x161E) */
    /* ------------------------------------------------------------------------ */
    if (self->crash_speed == 0)
    {
        self->spr_shadow->control &= ~ENABLE; /* Disable Shadow */
        self->spr_ferrari->counter += self->crash_zinc;       /* Increment Crash Z */
        if (self->spr_ferrari->counter > 0x3FF)
        {
            self->spr_ferrari->zoom = 0;
            self->spr_ferrari->counter = 0;
            OCrash_init_finger(self, frames);
            OCrash_done(self, self->spr_ferrari);
            return;
        }
        else
            self->crash_zinc++;
    }
    /* ------------------------------------------------------------------------ */
    /* Slow Crash (0x1648 flip_slower) */
    /* ------------------------------------------------------------------------ */
    else
    {
        self->spr_ferrari->counter -= self->crash_zinc;       /* Decrement Crash Z */
        if (self->crash_zinc > 2)
            self->crash_zinc--;
    }

    /* set_crash_z_inc */
    /* Note that we've set crash_zinc previously now for clarity */

    /* use ferrari_crash_z to set priority */
    self->spr_ferrari->priority = self->spr_ferrari->counter > 0x1FD ? 0x1FD : self->spr_ferrari->counter;

    { int16_t x_diff = (self->slide * self->spr_ferrari->priority) >> 9;
    oinitengine.car_x_pos -= x_diff;

    { int16_t passenger_frame = (int8_t) RomLoader_read8(roms.rom0p, 6 + frames);
        int16_t y;

    /* Start of sequence */
    if (passenger_frame == 0)
    {
        self->slide >>= 1;
        OSoundInt_queue_sound(&osoundint, SOUND_CRASH2);
    }

    /* Set Z during lower frames */
    if (passenger_frame <= 0x10 && self->spr_ferrari->counter <= 0x1FE)
        self->crash_z = self->spr_ferrari->counter;

    /* 0x16CC */
    passenger_frame = (passenger_frame * self->spr_ferrari->priority) >> 9;

    /* Set Ferrari Y */
    y = -(oroad.road_y[oroad.road_p0 + self->spr_ferrari->priority] >> 4) + 223;
    y -= passenger_frame;
    self->spr_ferrari->y = y;

    /* Set Ferrari Zoom from Z */
    self->spr_ferrari->zoom = (self->spr_ferrari->counter >> 2);
    if (self->spr_ferrari->zoom < 0x40) self->spr_ferrari->zoom = 0x40;

    /* Set Ferrari H-Flip */
    if (self->crash_side) 
        self->spr_ferrari->control |= HFLIP;
    else 
        self->spr_ferrari->control &= ~HFLIP;

    /* Palette Hack for recoloured cars. Original version was simply: spr_ferrari->pal_src = roms.rom0p->read8(4 + frames); */
    if (self->frame >= 7)
        self->spr_ferrari->pal_src = oferrari.ferrari_pal;
    else
        self->spr_ferrari->pal_src = oferrari.ferrari_pal == PAL_RED ? RomLoader_read8(roms.rom0p, 4 + frames) : oferrari.ferrari_pal + 4;

    if (--self->spinflipcount2 > 0)
    {
        OCrash_done(self, self->spr_ferrari);
        return;
    }

    self->spinflipcount2 = self->crash_spin_count;

    /* 0x1736 */
    /* Advance to next frame in sequence */
    if (self->spinflipcount1)
    {
        self->frame++;
        /* End of frame sequence */
        if ((RomLoader_read8(roms.rom0p, 7 + frames) & BIT_7) == 0)
        {
            OCrash_done(self, self->spr_ferrari);
            return;
        }
    }

    self->frame = 0;
    
    if (--self->spinflipcount1 > 0)
    {
        self->crash_spin_count++;
    }
    else
    {
        OCrash_init_finger(self, frames);
    }
    OCrash_done(self, self->spr_ferrari);
 } }}

/* Init Delay/Girl Pointing Finger */
/* Source: 0x175C */static 
void OCrash_init_finger(OCrash* self, uint32_t frames)
{
    self->crash_spin_count = 1;           /* Denote Crash has taken place */
    
    /* Do Delay whilst girl points finger */
    if (self->crash_type == CRASH_FLIP)
    {
        oferrari.wheel_state = WHEELS_ON;
        oferrari.car_state   = CAR_NORMAL;
        self->slide = 0;
        self->addr += frames;
        self->crash_delay = 30;
        self->crash_state = 5; 
    }
    /* Slide Car and Trigger Smoke Cloud */
    else
    {
        self->crash_state = 3;
        self->frame = 0;
        self->addr = outrun.adr.sprite_crash_man1;
        self->crash_spin_count = 4;   /* Denote third flip */
        self->spinflipcount2   = 4;
    }
}

/* Post Crash: Slide Car Slightly, then trigger smoke */
/* Source: 0x17D2 */static 
void OCrash_trigger_smoke(OCrash* self)
{
    self->crash_z = self->spr_ferrari->counter;
    { int16_t slide_copy = self->slide;

    if (self->slide < 0)
        self->slide++;
    else if (self->slide > 0)
        self->slide--;

    /* Slide Car */
    oinitengine.car_x_pos -= slide_copy;

    self->spr_ferrari->addr = RomLoader_read32(roms.rom0p, self->addr);

    /* Set Ferrari H-Flip */
    if (RomLoader_read8(roms.rom0p, 4 + self->addr))
        self->spr_ferrari->control |= HFLIP;
    else 
        self->spr_ferrari->control &= ~HFLIP;

    /*spr_ferrari->pal_src =     roms.rom0p->read8(5 + addr); */
    self->spr_ferrari->pal_src = oferrari.ferrari_pal;
    self->spin_pass_frame = (int8_t) RomLoader_read8(roms.rom0p, 6 + self->addr);

    /* Slow Car */
    oinitengine.car_increment = 
        (oinitengine.car_increment - ((oinitengine.car_increment >> 2) & 0xFFFF0000)) 
        | (oinitengine.car_increment & 0xFFFF);

    /* Car stationary */
    if (oinitengine.car_increment >> 16 <= 0)
    {
        oinitengine.car_increment = 0;
        oferrari.wheel_state = WHEELS_ON;
        oferrari.car_state   = CAR_NORMAL;
        self->slide = 0;
        self->crash_delay = 30;
        self->crash_state = 5; /* post crash animation delay state */
    }

    OCrash_done(self, self->spr_ferrari);
 }}

/* Source: 0x1870 */static 
void OCrash_post_flip_anim(OCrash* self)
{
    int16_t road_width;
    oferrari.car_ctrl_active = false;  /* Car and road updates disabled */
    if (--self->crash_delay > 0)
    {
        OCrash_done(self, self->spr_ferrari);
        return;
    }

    oferrari.car_ctrl_active = true;
    self->crash_state = 6; /* Denote pan camera to track centre */
    
    road_width = oroad.road_width >> 16;
    { int16_t car_x_pos  = oinitengine.car_x_pos;
    self->camera_xinc = 8;
    
    /* Double Road */
    if (road_width >= 0xD7)
    {
        if (car_x_pos < 0)
            road_width = -road_width;
    }
    else
    {
        road_width = 0;
    }

    self->camera_x_target = road_width;

    /* 1/ Car on road generator 1 (1 road enabled) */
    /* 2/ Car on road generator 1 (2 roads enabled) */
    if (road_width < car_x_pos)
    {
        self->camera_xinc += (car_x_pos - road_width) >> 6;
        self->camera_xinc = -self->camera_xinc;
    }
    else
    {
        self->camera_xinc += (road_width - car_x_pos) >> 6;
    }

    OCrash_done(self, self->spr_ferrari);
 }}

/* Pan Camera Back To Centre After Flip */
/* Source: 0x18EC */static 
void OCrash_pan_camera(OCrash* self)
{
    oferrari.car_ctrl_active = true;

    oinitengine.car_x_pos += self->camera_xinc;

    { int16_t x_diff = (oferrari.car_x_diff * self->spr_ferrari->counter) >> 9;
    self->spr_ferrari->x += x_diff;

    /* Pan Right */
    if (self->camera_xinc >= 0)
    {
        if (self->camera_x_target <= oinitengine.car_x_pos)
            self->crash_state = 7; /* Denote camera position and ready for restart */
    }
    /* Pan Left */
    else
    {
        if (self->camera_x_target >= oinitengine.car_x_pos)
            self->crash_state = 7;
    }

    OCrash_done(self, self->spr_ferrari);
 }}

/* Source: 0x1C7E */static 
void OCrash_init_spin1(OCrash* self)
{
    OSoundInt_queue_sound(&osoundint, SOUND_INIT_SLIP);
    { uint16_t car_inc = oinitengine.car_increment >> 16;
    uint16_t spins = 1;
    if (car_inc > 0xB4)
        spins += outils_random() & 1;

    self->spinflipcount1 = spins;
    self->crash_spin_count = 2;
    self->spinflipcount2 = 2;

    self->slide = ((spins + 1) << 2) + ((car_inc > 0xFF ? 0xFF : car_inc) >> 3);

    if (self->skid_counter_bak < 0)
        self->addr = outrun.adr.sprite_crash_spin1;
    else
    {
        self->addr = outrun.adr.sprite_crash_spin1;
        self->slide = -self->slide;
    }
    
    self->spin_control1++;
    self->frame = 0;
    self->skid_counter = 0;
    self->spr_ferrari->road_priority = self->spr_ferrari->counter;
 }}

/* Source: 0x1C10 */static 
void OCrash_init_spin2(OCrash* self)
{
    OSoundInt_queue_sound(&osoundint, SOUND_INIT_SLIP);
    { uint16_t car_inc = oinitengine.car_increment >> 16;
    self->spinflipcount1 = 1;
    self->crash_spin_count = 2;
    self->spinflipcount2 = 8;

    self->slide = (car_inc > 0xFF) ? 0xFF >> 3 : car_inc >> 3;

    if (oinitengine.road_type != ROAD_RIGHT)
    {
        self->addr = outrun.adr.sprite_crash_spin1;  
    }
    else
    {
        self->addr = outrun.adr.sprite_crash_spin1;
        self->slide = -self->slide;
    }

    self->spin_control2++;
    self->frame = 0;
    self->skid_counter = 0;
    self->spr_ferrari->road_priority = self->spr_ferrari->counter;
 }}

/* Collision: Slow  */
/* Rebound and bounce car in air */
/* Source: 0x19EE */static 
void OCrash_collide_slow(OCrash* self)
{
    int16_t y;
    uint16_t car_inc;
    OSoundInt_queue_sound(&osoundint, SOUND_REBOUND);
    
    /* Setup shift value for car bump, based on current speed, which ultimately determines how much car rises in air */
    car_inc = oinitengine.car_increment >> 16;

    if (car_inc <= 0x28)
        self->shift = 6;
    else if (car_inc <= 0x46)
        self->shift = 5;
    else
        self->shift = 4;

    self->lookup_index = 0;

    /* Calculate change in road y, so we can determine incline frame for ferrari */
    y = oroad.road_y[oroad.road_p0 + (0x3E0 / 2)] - oroad.road_y[oroad.road_p0 + (0x3F0 / 2)];
    self->frame_restore = 0;
    if (y >= 0x12) self->frame_restore++;
    if (y >= 0x13) self->frame_restore++;
    
    /* Right Hand Side: Increment Frame Entry By 3 */
    if (oinitengine.car_x_pos < 0)
        self->addr = outrun.adr.sprite_bump_data2;
    else
        self->addr = outrun.adr.sprite_bump_data1;

    self->crash_type = CRASH_BUMP; /* low speed bump */
    oinitengine.car_increment &= 0xFFFF;

    /* set_collision: */
    self->frame = 0;
    self->crash_state = 1; /* collision with object */
    self->spr_ferrari->road_priority = self->spr_ferrari->counter;
}

/* Collision: Medium */
/* Spin car */
/* Source: 0x1A98 */static 
void OCrash_collide_med(OCrash* self)
{
    uint16_t car_inc;
    OSoundInt_queue_sound(&osoundint, SOUND_INIT_SLIP);
    
    /* Set number of spins based on car speed */
    car_inc = oinitengine.car_increment >> 16;
    self->spinflipcount1 = car_inc <= 0x96 ? 1 : 2;
    self->spinflipcount2 = self->crash_spin_count = 2;
    self->slide = ((self->spinflipcount1 + 1) << 2) + ((car_inc > 0xFF ? 0xFF : car_inc) >> 3);

    /* Right Hand Side: Increment Frame Entry By 3 */
    if (oinitengine.car_x_pos < 0)
    {
        self->addr = outrun.adr.sprite_crash_spin1;
        self->slide = -self->slide;
    }
    else
        self->addr = outrun.adr.sprite_crash_spin1;

    self->crash_type = CRASH_SPIN;

    /* set_collision: */
    self->frame = 0;
    self->crash_state = 1; /* collision with object */
    self->spr_ferrari->road_priority = self->spr_ferrari->counter;
}

/* Collision: Fast */
/* Spin, Then Flip Car */
/* */
/* Source: 0x1B12 */static 
void OCrash_collide_fast(OCrash* self)
{
    OSoundInt_queue_sound(&osoundint, SOUND_CRASH1);

    { uint16_t car_inc = oinitengine.car_increment >> 16;
    if (car_inc > 0xFA)
    {
        self->crash_zinc = 1;
        self->crash_speed = 0;
    }
    else
    {
        self->crash_zinc = 0x10;
        self->crash_speed = 1;
    }

    self->spinflipcount1 = 1;
    self->spinflipcount2 = self->crash_spin_count = 2;
    
    self->slide = (car_inc > 0xFF ? 0xFF : car_inc) >> 2;
    self->slide += (self->slide >> 1);

    if (oinitengine.road_type != ROAD_STRAIGHT)
    {
        int16_t d2 = (0x78 - (oinitengine.road_curve <= 0x78 ? oinitengine.road_curve : 0x78)) >> 1;

        /* collide_fast_curve: */
        self->slide += d2;
        if (oinitengine.road_type == ROAD_RIGHT)
            self->slide = -self->slide;
    }
    else
    {
        if (oinitengine.car_x_pos < 0) self->slide = -self->slide; /* rhs */
    }
    
    /* set_fast_slide: */
    if (self->slide > 0x78) 
        self->slide = 0x78;

    if (oinitengine.car_x_pos < 0)
    {
        self->addr = outrun.adr.sprite_crash_spin2;
        self->crash_side = 0; /* RHS */
    }
    else
    {
        self->addr = outrun.adr.sprite_crash_spin1;
        self->crash_side = 1; /* LHS */
    }

    self->crash_type = CRASH_FLIP; /* Flip */

    /* set_collision: */
    self->frame = 0;
    self->crash_state = 1; /* collision with object */
    self->spr_ferrari->road_priority = self->spr_ferrari->counter;
 }}

/* Source: 0x1556 */static 
void OCrash_done(OCrash* self, oentry* sprite)
{
    OSprites_map_palette(&osprites, sprite);

    if (ORoad_get_view_mode(&oroad) != VIEW_INCAR || self->crash_type == CRASH_FLIP)
        OSprites_do_spr_order_shadows(&osprites, sprite);

    sprite->road_priority = sprite->counter;
}

/* ------------------------------------------------------------------------------------------------ */
/*                                      SHADOW CRASH ROUTINES */
/* ------------------------------------------------------------------------------------------------ */

/* Render Shadow */
/* */
/* Disabled during fast car flip, when car rapidly heading towards screen */
/* */
/* Source: 0x1DF2 */static 
void OCrash_do_shadow(OCrash* self, oentry* src_sprite, oentry* dst_sprite)
{
    uint16_t offset;
    uint16_t counter;
    uint8_t shadow_shift;

    /* Ferrari Shadow */
    if (src_sprite == self->spr_ferrari)
    {
        dst_sprite->draw_props = ANCHOR_BOTTOM;
        shadow_shift = 1;
    }
    else
    {
        shadow_shift = 3;
    }

    dst_sprite->x = src_sprite->x;
    dst_sprite->road_priority = src_sprite->road_priority;

    /* Get Z from source sprite (stored in counter) */
    counter = (src_sprite->counter) >> shadow_shift;
    counter = counter - (counter >> 2);
    dst_sprite->zoom = (uint8_t) counter;

    /* Set shadow y */
    offset = src_sprite->counter > 0x1FF ? 0x1FF : src_sprite->counter;
    dst_sprite->y = -(oroad.road_y[oroad.road_p0 + offset] >> 4) + 223;

    if (ORoad_get_view_mode(&oroad) != VIEW_INCAR || self->crash_type == CRASH_FLIP)
        OSprites_do_spr_order_shadows(&osprites, dst_sprite);
}

/* ------------------------------------------------------------------------------------------------ */
/*                                     PASSENGER CRASH ROUTINES */
/* ------------------------------------------------------------------------------------------------ */

/* Flips & Spins Only */
/* */
/* Process passengers during crash scenario. */
/* */
/* Source: 0x1E66 */static 
void OCrash_do_crash_passengers(OCrash* self, oentry* sprite)
{
    /* -------------------------------------------------------------------------------------------- */
    /* Flip car */
    /* -------------------------------------------------------------------------------------------- */
    if (self->crash_state == 2)
    {
        /* Update pointer to functions */
        if (sprite == self->spr_pass1) 
            self->function_pass1 = OCrash_flip_start;
        else if (sprite == self->spr_pass2) 
            self->function_pass2 = OCrash_flip_start;

        /* Crash Passenger Flip */
        OCrash_crash_pass_flip(self, sprite);
        return;
    }

    /* -------------------------------------------------------------------------------------------- */
    /* Non-Flip */
    /* -------------------------------------------------------------------------------------------- */
    if (self->crash_state < 5)
        OCrash_crash_pass1(self, sprite);
    else
        OCrash_crash_pass2(self, sprite);

    if (ORoad_get_view_mode(&oroad) != VIEW_INCAR || self->crash_type == CRASH_FLIP)
    {
        OSprites_map_palette(&osprites, sprite);
        OSprites_do_spr_order_shadows(&osprites, sprite);
    }
}

/* Position Passenger Sprites During Crash (But Not Flip) */
/* */
/* - Process passenger sprites in crash scenario */
/* - Called separately for man and girl */
/* */
/* Source: 0x1EA6 */static 
void OCrash_crash_pass1(OCrash* self, oentry* sprite)
{
    uint32_t frames = (sprite == self->spr_pass1 ? outrun.adr.sprite_crash_man1 : outrun.adr.sprite_crash_girl1) + (self->spin_pass_frame << 3);
    
    sprite->addr    = RomLoader_read32(roms.rom0p, frames);
    { uint8_t props   = RomLoader_read8(roms.rom0p, 4 + frames);
    sprite->pal_src = RomLoader_read8(roms.rom0p, 5 + frames);
    sprite->x       = self->spr_ferrari->x + (int8_t) RomLoader_read8(roms.rom0p, 6 + frames);
    sprite->y       = self->spr_ferrari->y + (int8_t) RomLoader_read8(roms.rom0p, 7 + frames);

    /* Check H-Flip */
    if (props & BIT_7)
        sprite->control |= HFLIP;
    else
        sprite->control &= ~HFLIP;

    /* Test whether we should set priority higher (unused on passenger sprites I think) */
    if (props & BIT_0)
        sprite->priority = sprite->road_priority = 0x1FE;
    else
        sprite->priority = sprite->road_priority = 0x1FD;

    sprite->zoom = 0x7E;
 }}

/* Passenger animations following the crash sequence i.e. when car is stationary */
/* */
/* 3 = hands battering, */
/* 2 = man scratches head, girl taps car */
/* 1 = man scratch head & girl points */
/* 0 = man subdued & girl points */
/* */
/* - Process passenger sprites in crash scenario */
/* - Called separately for man and girl */
/* - Selects which passenger animation to play */
/* */
/* Source: 0x1F26 */static 
void OCrash_crash_pass2(OCrash* self, oentry* sprite)
{
    uint32_t frames = (sprite == self->spr_pass1 ? outrun.adr.sprite_crash_man2 : outrun.adr.sprite_crash_girl2);

    /* Use coll_count2 to select one of the three animations that can be played */
    /* Use crash_delay to toggle between two distinct frames */
    frames += ((self->coll_count2 & 3) << 4) + (self->crash_delay & 8);
    
    sprite->addr    = RomLoader_read32(roms.rom0p, frames);
    { uint8_t props   = RomLoader_read8(roms.rom0p, 4 + frames);
    sprite->pal_src = RomLoader_read8(roms.rom0p, 5 + frames);
    sprite->x       = self->spr_ferrari->x + (int8_t) RomLoader_read8(roms.rom0p, 6 + frames);
    sprite->y       = self->spr_ferrari->y + (int8_t) RomLoader_read8(roms.rom0p, 7 + frames);

    /* Check H-Flip */
    if (props & BIT_7)
        sprite->control |= HFLIP;
    else
        sprite->control &= ~HFLIP;

    /* Test whether we should set priority higher (unused on passenger sprites I think) */
    if (props & BIT_0)
        sprite->priority = sprite->road_priority = 0x1FF;
    else
        sprite->priority = sprite->road_priority = 0x1FE;

    sprite->zoom = 0x7E;

    /* Man */
    if (sprite == self->spr_pass1)
    {
        const int8_t XY_OFF[] =
        {
            -0xC, -0x1E, 
            0x2,  -0x1B, 
            0x4,  -0x1A, 
            0x5,  -0x1E,
            0x11, -0x1B,
            0x0,  -0x1A,
            -0x1, -0x1B,
            -0xC, -0x1C,
            -0xE, -0x1B,
            -0xE, -0x1C,
            -0xE, -0x1D,
            -0xC, -0x1B,
            -0xC, -0x1C,
            -0xC, -0x1D,
        };

        sprite->x += XY_OFF[self->spin_pass_frame << 1];
        sprite->y += XY_OFF[(self->spin_pass_frame << 1) + 1];
    }
    /* Woman */
    else
    {
        const int8_t XY_OFF[] =
        {
            0xA,   -0x1A,
            0x0,   -0x1B,
            -0xF,  -0x1B,
            -0x15, -0x1B,
            -0x2,  -0x1E,
            0x7,   -0x1A,
            0x13,  -0x1D,
            0x9,   -0x1B,
            0x3,   -0x1B,
            0x3,   -0x1C,
            0x3,   -0x1D,
            0x7,   -0x1B,
            0x7,   -0x1C,
            0x7,   -0x1D,
        };

        sprite->x += XY_OFF[self->spin_pass_frame << 1];
        sprite->y += XY_OFF[(self->spin_pass_frame << 1) + 1];
    }
 }}

/* Handle passenger animation sequence during car flip */
/* */
/* 3 main stages: */
/* 1/ Flip passengers out of car */
/* 2/ Passengers sit up on road after crash */
/* 3/ Passengers turn head and look at car (only if camera pan) */
/* */
/* Source: 0x1FDE */static 

void OCrash_crash_pass_flip(OCrash* self, oentry* sprite)
{
    /* Some of these variable names really need refactoring */
    sprite->reload        = 0;                      /* clear passenger flip control */
    sprite->xw1           = 0;
    sprite->x             = self->spr_ferrari->x;
    sprite->traffic_speed = self->crash_spin_count;
    sprite->counter       = 0x1FE;                  /* sprite zoom */

    /* Set address of animation sequence based on whether male/female */
    sprite->z = sprite == self->spr_pass1 ? outrun.adr.sprite_crash_flip_m1 : outrun.adr.sprite_crash_flip_g1;

    OCrash_flip_start(self, sprite);
}

/* Source: 0x201A */static 
void OCrash_flip_start(OCrash* self, oentry* sprite)
{
    if (outrun.game_state != GS_ATTRACT && outrun.game_state != GS_INGAME)
    {
        OSprites_do_spr_order_shadows(&osprites, sprite);
        OCrash_done(self, sprite);
        return;
    }

    switch (sprite->reload & 3) /* check passenger flip control */
    {
        /* Flip passengers out of car */
        case 0:
            OCrash_pass_flip(self, sprite);
            break;

        /* Passengers sit up on road after crash */
        case 1:
            OCrash_pass_situp(self, sprite);
            break;

        /* Passengers turn head and look at car (only if camera pan) */
        case 2:
        case 3:
            OCrash_pass_turnhead(self, sprite);
            break;
    }
}

/* Flip passengers out of car */
/* Source: 0x2066 */static 
void OCrash_pass_flip(OCrash* self, oentry* sprite)
{
    int16_t zoom;
    /* Fast crash */
    if (self->crash_speed == 0)
    {
        sprite->counter += (self->crash_zinc << 2);

        if (sprite->counter > 0x3FF)
        {
            sprite->reload = 1; /* // Passenger Control: Passengers sit up on road after crash */

            /* Disable sprite and shadow */
            sprite->control &= ~ENABLE;
            if (sprite == self->spr_pass1)
                self->spr_pass1s->control &= ~ENABLE;
            else
                self->spr_pass2s->control &= ~ENABLE;
            return;
        }
    }
    /* Slow crash */
    else
    {
        int16_t zinc = self->crash_zinc >> 2;
    
        /* Adjust the z position of the female more than the man */
        if (sprite == self->spr_pass2)
        {
            zinc += (zinc >> 1);
        }

        sprite->counter -= zinc;
    }

    /* set_z_lookup */
    zoom = sprite->counter >> 2;
    if (zoom < 0x40) zoom = 0x40;
    sprite->zoom = (uint8_t) zoom;

    { uint32_t frames = sprite->z + (sprite->xw1 << 3);
    sprite->addr = RomLoader_read32(roms.rom0p, frames);

    { uint16_t offset = sprite->counter > 0x1FF ? 0x1FF : sprite->counter;
    { int16_t y_change = (((int8_t) RomLoader_read8(roms.rom0p, 6 + frames)) * offset) >> 9; /* d1 */

    sprite->y = -(oroad.road_y[oroad.road_p0 + offset] >> 4) + 223;
    sprite->y -= y_change;

    /* 2138 */

    sprite->priority = offset;
    if (self->crash_side) 
        sprite->control |= HFLIP;
    else 
        sprite->control &= ~HFLIP;

    sprite->pal_src = RomLoader_read8(roms.rom0p, 4 + frames);
    
    /* Decrement spin count */
    /* Increment frame of passengers for first spins */
    if (--sprite->traffic_speed <= 0)
    {
        sprite->traffic_speed = self->crash_spin_count;
        sprite->xw1++; /* Increase passenger frame */

        /* End of animation sequence. Progress to next sequnce of animations. */
        if (RomLoader_read8(roms.rom0p, 7 + frames) & BIT_7)
        {
            sprite->reload = 1; /* Passenger Control: Passengers sit up on road after crash */
            sprite->xw1    = 0; /* Reset passenger frame */
            
            /* Update address of animation sequence to be used */
            sprite->z      = sprite == self->spr_pass1 ? outrun.adr.sprite_crash_flip_m2 : outrun.adr.sprite_crash_flip_g2;
            frames         = sprite->z;

            /* Set Frame Delay for this animation sequence from lower bytes */
            sprite->traffic_speed = RomLoader_read8(roms.rom0p, 7 + frames) & 0x7F;

            OCrash_done(self, sprite);
            return;
        }
    }

    /* set_passenger_x */
    sprite->x =  self->spr_ferrari->x;
    sprite->x += ((int8_t) RomLoader_read8(roms.rom0p, 5 + frames));
    OCrash_done(self, sprite);
 } } }}

/* Passengers sit up on road after crash */
/* Source: 0x205A */static 
void OCrash_pass_situp(OCrash* self, oentry* sprite)
{
    /* Update passenger x position */
    int16_t x_diff = (oferrari.car_x_diff * sprite->counter) >> 9;
    sprite->x += x_diff;

    { uint32_t frames = sprite->z + (sprite->xw1 << 3);
    sprite->addr    = RomLoader_read32(roms.rom0p, frames);
    sprite->pal_src = RomLoader_read8(roms.rom0p, 4 + frames);

    /* Decrement frame delay counter */
    if (--sprite->traffic_speed <= 0)
    {
        sprite->traffic_speed = RomLoader_read8(roms.rom0p, 0xF + frames) & 0x7F;

        /* End of animation sequence. Progress to next sequnce of animations. */
        if (RomLoader_read8(roms.rom0p, 7 + frames) & BIT_7)
        {
            sprite->reload = 2; /* Passenger Control: Passengers turn head and look at car */
        }
        else
        {
            sprite->xw1++; /* Increase passenger frame */

            /* If camera pan: Make passengers turn heads! */
            if (self->crash_state == 6)
                    sprite->reload = 2;
        }
    }
    OCrash_done(self, sprite);
 }}

/* Passengers turn head and look at car (only if camera pan) */
/* Source: 0x222C */static 
void OCrash_pass_turnhead(OCrash* self, oentry* sprite)
{
    /* Update passenger x position */
    int16_t x_diff = (oferrari.car_x_diff * sprite->counter) >> 9;
    sprite->x += x_diff;

    { uint32_t frames = sprite->z + (sprite->xw1 << 3);
    sprite->addr    = RomLoader_read32(roms.rom0p, frames);
    sprite->pal_src = RomLoader_read8(roms.rom0p, 4 + frames);

    /* End of animation sequence. */
    if (RomLoader_read8(roms.rom0p, 7 + frames) & BIT_7)
    {
        OCrash_done(self, sprite);
        return;
    }

    /* Decrement frame delay counter */
    if (--sprite->traffic_speed <= 0)
    {
        sprite->traffic_speed = RomLoader_read8(roms.rom0p, 0xF + frames) & 0x7F;
        sprite->xw1++; /* Increase passenger frame */
    }

    OCrash_done(self, sprite);
 }}