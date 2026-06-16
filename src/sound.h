#ifndef SOUND_H
#define SOUND_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void soundSetVolume(float unused);

/* Manages muting bitmask. The bits control the following channels: */
/* 0x001 Pulse 1 */
/* 0x002 Pulse 2 */
/* 0x004 Wave */
/* 0x008 Noise */
/* 0x100 PCM 1 */
/* 0x200 PCM 2 */
void soundPause (void);
void soundResume (void);
void soundSetSampleRate(long sampleRate);
void soundReset (void);
/* Frees the heap-allocated blip-buffer storage owned by the audio
 * pipeline (the 3 stereo_buffer entries each hold ~32 KB at 32 kHz /
 * 250 ms = ~96 KB total).  Call from retro_deinit so the storage is
 * returned to the allocator rather than relying on process exit -- the
 * latter accumulates across core load/unload cycles on dynamic-load
 * frontends (Android, RetroArch hot-swap).  Safe to call when buffers
 * are already destroyed; bufs_buffer[] is zeroed after teardown. */
void soundCleanUp (void);
void soundEvent_u8( int gb_addr, uint32_t addr, uint8_t  data );
void soundEvent_u8_parallel(int gb_addr[], uint32_t address[], uint8_t data[]);
void soundEvent_u16( uint32_t addr, uint16_t data );
void soundTimerOverflow( int which );
void process_sound_tick_fn (void);
void soundSaveGameMem(uint8_t **data);
void soundReadGameMem(const uint8_t **data, int version);

extern int SOUND_CLOCK_TICKS;   /* Number of 16.8 MHz clocks between calls to soundTick() */
extern int soundTicks;          /* Number of 16.8 MHz clocks until soundTick() will be called */

#ifdef __cplusplus
}
#endif

#endif /* SOUND_H */
