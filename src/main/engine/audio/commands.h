/***************************************************************************
    Shared Sound Commands.
    Used by both the ported 68K and Z80 program code.
    
    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#pragma once
#ifdef __cplusplus
extern "C" {
#endif


/* ---------------------------------------------------------------------------- */
/* Commands to send from main program code */
/* ---------------------------------------------------------------------------- */

enum
{
    SOUND_FM_RESET = 0,            /* Reset FM Chip (Stop Music etc.) */
    SOUND_RESET  = 0x80,           /* Reset sound code */
    SOUND_MUSIC_BREEZE = 0x81,     /* Music: Passing Breeze */
    SOUND_MUSIC_SPLASH = 0x82,     /* Music: Splash Wave */
    SOUND_MUSIC_CUSTOM = 0x83,     /* Enhancement: Play Custom Imported Data */
    SOUND_COIN_IN = 0x84,          /* Coin IN Effect */
    SOUND_MUSIC_MAGICAL = 0x85,    /* Music: Magical Sound Shower */
    SOUND_YM_CHECKPOINT = 0x86,    /* YM: Checkpoint Ding */
    SOUND_INIT_SLIP = 0x8A,        /* Slip (Looped) */
    SOUND_STOP_SLIP = 0x8B,
    SOUND_INIT_CHEERS = 0x8D,
    SOUND_STOP_CHEERS = 0x8E,
    SOUND_CRASH1 = 0x8F,
    SOUND_REBOUND = 0x90,
    SOUND_CRASH2 = 0x92,
    SOUND_NEW_COMMAND = 0x93,
    SOUND_SIGNAL1 = 0x94,
    SOUND_SIGNAL2 = 0x95,
    SOUND_INIT_WEIRD = 0x96,
    SOUND_STOP_WEIRD = 0x97,
    SOUND_REVS = 0x98,             /* New: Added to support revs during WAV playback */
    SOUND_BEEP1 = 0x99,            /* YM Beep */
    SOUND_UFO = 0x9A,              /* Unused sound. Note that the z80 code to play this is not implemented in this conversion. */
    SOUND_BEEP2 = 0x9B,            /* YM Double Beep */
    SOUND_INIT_CHEERS2     = 0x9C, /* Cheers (Looped)    */
    SOUND_VOICE_CHECKPOINT = 0x9D, /* Voice: Checkpoint */
    SOUND_VOICE_CONGRATS   = 0x9E, /* Voice: Congratulations */
    SOUND_VOICE_GETREADY   = 0x9F, /* Voice: Get Ready */
    SOUND_INIT_SAFETYZONE  = 0xA0,
    SOUND_STOP_SAFETYZONE  = 0xA1,
    SOUND_YM_SET_LEVELS    = 0xA2,
    /* 0xA3 Unused - Should be voice 4, but isn't hooked up */
    SOUND_PCM_WAVE         = 0xA4, /* Wave Sample */
    SOUND_MUSIC_LASTWAVE   = 0xA5 /* Music: Last Wave */
};

/* ---------------------------------------------------------------------------- */
/* Engine Commands to send from main program code */
/* ---------------------------------------------------------------------------- */

enum
{
    SOUND_UNUSED,
    SOUND_ENGINE_PITCH_H,
    SOUND_ENGINE_PITCH_L,
    SOUND_ENGINE_VOL,
    SOUND_TRAFFIC1,
    SOUND_TRAFFIC2,
    SOUND_TRAFFIC3,
    SOUND_TRAFFIC4
};

#ifdef __cplusplus
}
#endif
