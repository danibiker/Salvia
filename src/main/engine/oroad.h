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

#pragma once
#ifdef __cplusplus
extern "C" {
#endif


enum {
		ROAD_OFF = 0,         /* Both Roads Off */
		ROAD_R0 = 1,          /* Road 0 */
		ROAD_R1 = 2,          /* Road 1 */
		ROAD_BOTH_P0 = 3,     /* Both Roads (Road 0 Priority) [DEFAULT] */
		ROAD_BOTH_P1 = 4,     /* Both Roads (Road 1 Priority)  */
		ROAD_BOTH_P0_INV = 5, /* Both Roads (Road 0 Priority) (Road Split. Invert Road 1) */
		ROAD_BOTH_P1_INV = 6, /* Both Roads (Road 1 Priority) (Road Split. Invert Road 1) */
		ROAD_R0_SPLIT = 7,    /* Road 0 (Road Split.) */
		ROAD_R1_SPLIT = 8    /* Road 1 (Road Split. Invert Road 1) */
	};

enum { ARRAY_LENGTH = 0x200 };

enum {ROAD_DOWN = -1, ROAD_NO_CHANGE = 0, ROAD_UP = 1};

#define HORIZON_OFF (-0x3FF)

#define VIEW_ORIGINAL 0

#define VIEW_ELEVATED 1

#define VIEW_INCAR 2

#define HW_HSCROLL_TABLE0 (0x80400)

#define HW_HSCROLL_TABLE1 (0x80800)

#define HW_BGCOLOR (0x80C00)

typedef struct ORoad
{
    uint32_t road_pos;
    int16_t tilemap_h_target;
    int16_t stage_lookup_off;
    uint16_t road_p0;
    uint16_t road_p1;
    uint16_t road_p2;
    uint16_t road_p3;
    int16_t road_width_bak;
    int16_t car_x_bak;
    uint16_t height_lookup;
    uint16_t height_lookup_wrk;
    int32_t road_pos_change;
    uint8_t road_load_end;
    uint8_t road_ctrl;
    int8_t road_load_split;
    int32_t road_width;
    uint16_t road_data_offset;
    int16_t horizon_y2;
    int16_t horizon_y_bak;
    uint16_t pos_fine;
    int32_t horizon_base;
    uint8_t horizon_set;
    int16_t road_x[ARRAY_LENGTH];
    int16_t road0_h[ARRAY_LENGTH];
    int16_t road1_h[ARRAY_LENGTH];
    int16_t road_unk[ARRAY_LENGTH];
    int16_t road_y[0x1000];
    uint8_t view_mode;
    int16_t horizon_target;
    int16_t horizon_offset;
    uint16_t stage_loaded;
    uint32_t road_pos_old;
    uint16_t height_start;
    uint16_t height_ctrl;
    uint16_t pos_fine_old;
    int16_t pos_fine_diff;
    int8_t counter;
    int16_t height_index;
    int32_t height_final;
    uint16_t height_inc;
    uint16_t height_step;
    uint16_t height_ctrl2;
    uint32_t height_addr;
    int16_t elevation;
    int16_t height_delay;
    uint16_t step_adjust;
    uint16_t do_height_inc;
    uint16_t height_end;
    int8_t up_mult;
    int8_t down_mult;
    uint32_t horizon_mod;
    uint16_t section_lengths[7];
    int8_t length_offset;
    uint32_t a1_lookup;
    int32_t change_per_entry;
    int32_t d5_o;
    uint32_t a3_o;
    uint32_t y_addr;
    int16_t scanline;
    int32_t total_height;
} ORoad;

extern ORoad oroad;

void ORoad_init(ORoad* self);

void ORoad_tick(ORoad* self);

uint8_t ORoad_get_view_mode(ORoad* self);

int16_t ORoad_get_road_y(ORoad* self, uint16_t);

void ORoad_set_view_mode(ORoad* self, uint8_t, bool snap);

#ifdef __cplusplus
}
#endif
