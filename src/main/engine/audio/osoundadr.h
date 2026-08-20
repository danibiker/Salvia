/***************************************************************************
    Z80 Program Code Addresses. 
    Addresses to data within the Z80 Program ROM.
    
    These are typically large blocks of data that we don't want to include
    in the codebase. 
    
    For example the music tracks are stored and referenced here.
    
    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#pragma once
#ifdef __cplusplus
extern "C" {
#endif


/* FM Note & Octave Lookup Table */
#define YM_NOTE_OCTAVE (0xAC9)

/* Command Lists to send to FM Chip. Format is register, value pairs */
#define YM_INIT_CMDS (0xB29)
/* Address is ($60 + (8 * Operator) + Channel). Total Level affects the total output volume of the sound. */
#define YM_LEVEL_CMDS1 (0xB49)
#define YM_LEVEL_CMDS2 (0xB81)
#define YM_RELEASE_RATE (0xB8A)

/* PCM Sample Properties */
#define PCM_INFO (0xDDD)

/* YM: Passing Breeze */
#define DATA_BREEZE (0xE26)

/* YM: Splash Wave */
#define DATA_SPLASH (0x20C8)

/* YM: Magical Sound Shower */
#define DATA_MAGICAL (0x3D5F)

/* YM: Last Wave */
#define DATA_LASTWAVE (0x5F2D)

/* PCM: Safety Zone */
#define DATA_SAFETY (0x69A9)

/* PCM: Slip Sound */
#define DATA_SLIP (0x69E6)

/* YM: Coin IN Sound */
#define DATA_COININ (0x6A24)

/* YM: Checkpoint beeps */
#define DATA_CHECKPOINT (0x6A60)

/* YM: Signal 1 Sound */
#define DATA_SIGNAL1 (0x6A87)

/* YM: Signal 2 Sound */
#define DATA_SIGNAL2 (0x6AA7)

/* YM: Beep 1 Sound */
#define DATA_BEEP1 (0x6AC7)

/* PCM: Crash Sound */
#define DATA_CRASH1 (0x6C15)

/* PCM: Rebound Sound */
#define DATA_REBOUND (0x6CFF)

/* PCM: Crash Sound */
#define DATA_CRASH2 (0x6C8A)

/* PCM: Weird Sound */
#define DATA_WEIRD (0x6D61)

/* PCM: Crowd Cheers */
#define DATA_CHEERS (0x6F16)

/* PCM: Crowd Cheers 2 */
#define DATA_CHEERS2 (0x6F53)

/* PCM: Voice 1, Checkpoint (Sound Data) */
#define DATA_VOICE1 (0x6F91)

/* PCM: Voice 2, Congratulations */
#define DATA_VOICE2 (0x6FCA)

/* PCM: Voice 3, Get Ready */
#define DATA_VOICE3 (0x7003)

/* YM: UFO (Unused) */
#define DATA_UFO (0x703D)

/* YM: Beep 2 Sound */
#define DATA_BEEP2 (0x71F9)

/* PCM: Wave Sample */
#define DATA_WAVE (0x748B)

/* Enhancement: Add Custom Music Data To End Of Z80 ROM */
#define DATA_CUSTOM (0x84B9)

/* Engine Tone Table. 5 Bytes per entry. */
/* Engine Note: Start Address Low, Start Address High, Volume Multiplier, Unknown, Pitch */
#define ENGINE_ADR_TABLE (0x7951)

/* Traffic Volume Multiply Table. How much to increase traffic volume by dependent on distance. */
#define TRAFFIC_VOL_MULTIPLY (0x7CEF)

#ifdef __cplusplus
}
#endif
