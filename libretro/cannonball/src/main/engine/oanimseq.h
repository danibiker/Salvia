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

#pragma once
#ifdef __cplusplus
extern "C" {
#endif


#include <boolean.h>

#include "oanimsprite.h"

typedef struct OAnimSeq
{
    oanimsprite anim_flag;
    oanimsprite anim_ferrari;
    oanimsprite anim_pass1;
    oanimsprite anim_pass2;
    oanimsprite anim_obj1;
    oanimsprite anim_obj2;
    oanimsprite anim_obj3;
    oanimsprite anim_obj4;
    oanimsprite anim_obj5;
    oanimsprite anim_obj6;
    oanimsprite anim_obj7;
    oanimsprite anim_obj8;
    uint8_t end_seq;
    int16_t seq_pos;
    uint8_t end_seq_state;
    bool ferrari_stopped;
} OAnimSeq;

extern OAnimSeq oanimseq;

void OAnimSeq_init(OAnimSeq* self, oentry*);

void OAnimSeq_flag_seq(OAnimSeq* self);

void OAnimSeq_ferrari_seq(OAnimSeq* self);

void OAnimSeq_anim_seq_intro(OAnimSeq* self, oanimsprite*);

void OAnimSeq_init_end_seq(OAnimSeq* self);

void OAnimSeq_tick_end_seq(OAnimSeq* self);

#ifdef __cplusplus
}
#endif
