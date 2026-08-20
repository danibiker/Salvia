/***************************************************************************
    Front End Menu System.

    This file is part of Cannonball. 
    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#pragma once
#ifdef __cplusplus
extern "C" {
#endif


#define MENU_MAX_ENTRIES 24

#include <stdint.h>

struct TTrial;

enum {
        MENU_STATE_MENU,
        MENU_STATE_REDEFINE_KEYS,
        MENU_STATE_REDEFINE_JOY,
        MENU_STATE_TTRIAL
    };

#define MESSAGE_TIME (5)

typedef struct Menu
{
    uint8_t state;
    struct TTrial* ttrial;
    uint8_t redef_state;
    uint32_t frame;
    int32_t message_counter;
    char msg[64];
    int16_t cursor;
    bool is_text_menu;
    const char** menu_selected;
    uint8_t menu_selected_num;
    char menu_text[MENU_MAX_ENTRIES][40];
    const char* menu_main[MENU_MAX_ENTRIES];
    uint8_t menu_main_num;
    const char* menu_gamemodes[MENU_MAX_ENTRIES];
    uint8_t menu_gamemodes_num;
    const char* menu_cont[MENU_MAX_ENTRIES];
    uint8_t menu_cont_num;
    const char* menu_timetrial[MENU_MAX_ENTRIES];
    uint8_t menu_timetrial_num;
    const char* menu_about[MENU_MAX_ENTRIES];
    uint8_t menu_about_num;
    const char* menu_settings[MENU_MAX_ENTRIES];
    uint8_t menu_settings_num;
    const char* menu_video[MENU_MAX_ENTRIES];
    uint8_t menu_video_num;
    const char* menu_sound[MENU_MAX_ENTRIES];
    uint8_t menu_sound_num;
    const char* menu_controls[MENU_MAX_ENTRIES];
    uint8_t menu_controls_num;
    const char* menu_engine[MENU_MAX_ENTRIES];
    uint8_t menu_engine_num;
    const char* menu_enhancements[MENU_MAX_ENTRIES];
    uint8_t menu_enhancements_num;
    const char* menu_handling[MENU_MAX_ENTRIES];
    uint8_t menu_handling_num;
    const char* menu_musictest[MENU_MAX_ENTRIES];
    uint8_t menu_musictest_num;
    const char* text_redefine[MENU_MAX_ENTRIES];
    uint8_t text_redefine_num;
} Menu;

void Menu_ctor(Menu* self);

void Menu_dtor(Menu* self);

void Menu_populate(Menu* self);

void Menu_init(Menu* self);

void Menu_tick(Menu* self);

void Menu_refresh_menu(Menu* self);

#ifdef __cplusplus
}
#endif
