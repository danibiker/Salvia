#pragma once
#ifdef __cplusplus
extern "C" {
#endif


#include <boolean.h>

#include <stdint.h>

enum { ROAD_RAM_SIZE = 0x1000 };

static const uint16_t rom_size = 0x8000;

typedef struct HWRoad
{
    uint8_t road_control;
    uint16_t color_offset1;
    uint16_t color_offset2;
    uint16_t color_offset3;
    int32_t x_offset;
    uint8_t roads[0x40200];
    uint16_t ram[ROAD_RAM_SIZE / 2];
    uint16_t ramBuff[ROAD_RAM_SIZE / 2];
    void (*render_background)(struct HWRoad* self, uint16_t*);
    void (*render_foreground)(struct HWRoad* self, uint16_t*);
} HWRoad;

extern HWRoad hwroad;

void HWRoad_init(struct HWRoad* self, const uint8_t*, const bool hires);

void HWRoad_write16(struct HWRoad* self, uint32_t adr, const uint16_t data);

void HWRoad_write16_a(struct HWRoad* self, uint32_t* adr, const uint16_t data);

void HWRoad_write32(struct HWRoad* self, uint32_t* adr, const uint32_t data);

uint16_t HWRoad_read_road_control(struct HWRoad* self);

void HWRoad_write_road_control(struct HWRoad* self, const uint8_t);

#ifdef __cplusplus
}
#endif
