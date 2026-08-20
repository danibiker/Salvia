/***************************************************************************
    Hardware Sprites.
    
    This class stores sprites in the converted format expected by the
    OutRun graphics hardware.

    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/
#ifdef __cplusplus
extern "C" {
#endif


#include <stdint.h>

typedef struct osprite
{
    uint16_t data[0x7];
    uint32_t scratch;
} osprite;

void osprite_init(osprite* self);

uint16_t osprite_get_x(osprite* self);

uint16_t osprite_get_y(osprite* self);

void osprite_set_x(osprite* self, uint16_t);

void osprite_inc_x(osprite* self, uint16_t);

void osprite_set_y(osprite* self, uint16_t);

void osprite_set_pitch(osprite* self, uint8_t);

void osprite_set_vzoom(osprite* self, uint16_t);

void osprite_set_hzoom(osprite* self, uint16_t);

void osprite_set_priority(osprite* self, uint8_t);

void osprite_set_offset(osprite* self, uint16_t o);

void osprite_inc_offset(osprite* self, uint16_t o);

void osprite_set_render(osprite* self, uint8_t b);

void osprite_set_pal(osprite* self, uint8_t);

void osprite_set_height(osprite* self, uint8_t);

void osprite_sub_height(osprite* self, uint8_t);

void osprite_set_bank(osprite* self, uint8_t);

void osprite_hide(osprite* self);

#ifdef __cplusplus
}
#endif
