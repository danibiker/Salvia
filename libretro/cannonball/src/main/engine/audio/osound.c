/***************************************************************************
    Ported Z80 Sound Playing Code.
    Controls Sega PCM and YM2151 Chips.
    
    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

/*
TODO:

- Double check passing car tones, especially when going through checkpoint areas
- Engine tones seem to jump channels. How do we stop this? Is it a bug in the original?

*/

#include <string.h> /* For memset on GCC */
#include "engine/audio/osound.h"

static uint8_t OSound_pcm_r(OSound* self, uint16_t adr);
static void OSound_pcm_w(OSound* self, uint16_t adr, uint8_t v);
static uint16_t OSound_r16(OSound* self, uint8_t* adr);
static void OSound_w16(OSound* self, uint8_t* adr, uint16_t v);
static void OSound_process_command(OSound* self);
static void OSound_pcm_backup(OSound* self);
static void OSound_check_fm_mapping(OSound* self);
static void OSound_process_channels(OSound* self);
static void OSound_process_channel(OSound* self, uint16_t chan_id);
static void OSound_process_section(OSound* self, uint8_t* chan);
static void OSound_calc_end_marker(OSound* self, uint8_t* chan);
static void OSound_next_mml_cmd(OSound* self, uint8_t* chan, uint8_t cmd);
static void OSound_new_command(OSound* self);
static void OSound_play_pcm_index(OSound* self, uint8_t* chan, uint8_t cmd);
static void OSound_setvol(OSound* self, uint8_t* chan);
static void OSound_pcm_setpitch(OSound* self, uint8_t* chan);
static void OSound_set_loop_adr(OSound* self);
static void OSound_do_loop(OSound* self, uint8_t* chan);
static void OSound_pcm_finalize(OSound* self, uint8_t* chan);
static void OSound_init_sound(OSound* self, uint8_t cmd, uint16_t src, uint16_t dst);
static void OSound_process_pcm(OSound* self, uint8_t* chan);
static void OSound_pcm_send_cmds(OSound* self, uint8_t* chan, uint16_t pcm_adr, uint8_t channel_pair);
static void OSound_fm_dotimera(OSound* self);
static void OSound_fm_reset(OSound* self);
static void OSound_fm_write_reg_c(OSound* self, uint8_t ix0, uint8_t reg, uint8_t value);
static void OSound_fm_write_reg(OSound* self, uint8_t reg, uint8_t value);
static void OSound_fm_write_block(OSound* self, uint8_t ix0, uint16_t adr, uint8_t chan);
static void OSound_ym_set_levels(OSound* self);
static void OSound_ym_load_patch(OSound* self, uint8_t* chan);
static uint16_t OSound_ym_lookup_data(OSound* self, uint8_t cmd, uint8_t offset, uint8_t block);
static void OSound_ym_set_connect(OSound* self, uint8_t* chan, uint8_t pan);
static void OSound_ym_end_track(OSound* self, uint8_t* chan);
static void OSound_read_mod_table(OSound* self, uint8_t* chan);
static void OSound_call_adr(OSound* self, uint8_t* chan);
static void OSound_engine_process(OSound* self);
static void OSound_engine_process_chan(OSound* self, uint8_t* chan, uint8_t* pcm);
static void OSound_vol_thicken(OSound* self, uint16_t* pos, uint8_t* chan, uint8_t* pcm);
static uint8_t OSound_get_adjusted_vol(OSound* self, uint16_t* pos, uint8_t* chan);
static void OSound_engine_set_pitch(OSound* self, uint16_t* pos, uint8_t* pcm);
static void OSound_engine_mute_channel(OSound* self, uint8_t* chan, uint8_t* pcm, bool do_check);
static void OSound_unk78c7(OSound* self, uint8_t* chan, uint8_t* pcm);
static void OSound_ferrari_vol_pan(OSound* self, uint8_t* chan, uint8_t* pcm);
static uint16_t OSound_engine_get_table_adr(OSound* self, uint8_t* chan, uint8_t* pcm);
static void OSound_engine_adjust_volume(OSound* self, uint8_t* chan);
static uint16_t OSound_engine_set_adr(OSound* self, uint16_t* pos, uint8_t* chan, uint8_t* pcm);
static void OSound_engine_set_adr_end(OSound* self, uint16_t* pos, uint16_t loop_adr, uint8_t* chan, uint8_t* pcm);
static void OSound_engine_set_pan(OSound* self, uint16_t* pos, uint8_t* chan, uint8_t* pcm);
static void OSound_engine_read_data(OSound* self, uint8_t* chan, uint8_t* pcm);
static void OSound_traffic_process(OSound* self);
static void OSound_traffic_process_chan(OSound* self, uint8_t* pcm);
static void OSound_traffic_process_entry(OSound* self, uint8_t* pcm);
static void OSound_traffic_disable(OSound* self, uint8_t* pcm);
static void OSound_traffic_set_vol(OSound* self, uint8_t* pcm);
static void OSound_traffic_set_pan(OSound* self, uint8_t* pcm);
static uint8_t OSound_traffic_get_vol(OSound* self, uint16_t pos, uint8_t* pcm);
static void OSound_traffic_note_changes(OSound* self, uint8_t new_vol, uint8_t* pcm);
static void OSound_traffic_read_data(OSound* self, uint8_t* pcm);

void OSound_init(OSound* self, YM2151* ym, uint8_t* pcm_ram)
{
    self->ym      = ym;
    self->pcm_ram = pcm_ram;

    self->command_input = 0;
    self->sound_props   = 0;
    self->pos           = 0;
    self->counter1      = 0;
    self->counter2      = 0;
    self->counter3      = 0;
    self->counter4      = 0;

    self->engine_counter= 0;

    /* Clear AM RAM 0xF800 - 0xFFFF */
    { uint16_t i; for (i = 0; i < CHAN_RAM_SIZE; i++)
        self->chan_ram[i] = 0; }

    /* Enable all PCM channels by default */
    { int8_t i; for (i = 0; i < 16; i++)
        pcm_ram[0x86 + (i * 8)] = 1; } /* Channel Active */

    OSound_init_fm_chip(self);
}

/* Initialize FM Chip. Initalize and start Timer A. */
/* Source: 0x79 */
void OSound_init_fm_chip(OSound* self)
{
   self->command_input = SOUND_RESET;

   /* Initialize the FM Chip with the set of default commands */
   OSound_fm_write_block(self, 0, YM_INIT_CMDS, 0);

   /* Start Timer A & enable its IRQ, and do an IRQ reset (%00110101) */
   OSound_fm_write_reg(self, 0x14, 0x35);
}

void OSound_tick(OSound* self)
{
    OSound_fm_dotimera(self);          /* FM: Process Timer A. Stop Timer B */
    OSound_process_command(self);      /* Process Command sent by main program code (originally the main 68k processor) */
    OSound_process_channels(self);     /* Run logic on individual sound channel (both YM & PCM channels) */
    OSound_engine_process(self);       /* Ferrari Engine Tone & Traffic Noise */
    OSound_traffic_process(self);      /* Traffic Volume/Panning & Pitch */
}

/* PCM RAM Read/Write Helper Functions */static 
uint8_t OSound_pcm_r(OSound* self, uint16_t adr)
{
    return self->pcm_ram[adr & 0xFF];
}static 

void OSound_pcm_w(OSound* self, uint16_t adr, uint8_t v)
{
    self->pcm_ram[adr & 0xFF] = v;
}

/* RAM Read/Write Helper Functions */static 
uint16_t OSound_r16(OSound* self, uint8_t* adr)
{
    return ((*(adr+1) << 8) | *adr);
}static 

void OSound_w16(OSound* self, uint8_t* adr, uint16_t v)
{
    *adr     = v & 0xFF;
    *(adr+1) = v >> 8;
}

/* Process Command Sent By 68K CPU */
/* Source: 0x74F */static 
void OSound_process_command(OSound* self)
{
    if (self->command_input == SOUND_RESET)
        return;
    /* Clear Z80 Command */
    else if (self->command_input < 0x80 || self->command_input >= 0xFF)
    {
        /* Reset FM Chip */
        if (self->command_input == SOUND_FM_RESET || self->command_input == 0xFF)
            OSound_fm_reset(self);

        self->command_input = SOUND_RESET;
        OSound_new_command(self);
    }
    else
    {
        uint8_t cmd   = self->command_input;
        self->command_input = SOUND_RESET;

        switch (cmd)
        {
            case SOUND_RESET:
                break;

            case SOUND_MUSIC_BREEZE:
                /*sound_props |= BIT_0; // Trigger rev effect (moved into 68k code) */
                cmd = SOUND_MUSIC_BREEZE;
                OSound_fm_reset(self);
                OSound_init_sound(self, cmd, DATA_BREEZE, CHANNEL_YM1);
                break;

            case SOUND_MUSIC_SPLASH:
                /*sound_props |= BIT_0; // Trigger rev effect (moved into 68k code) */
                cmd = SOUND_MUSIC_SPLASH;
                OSound_fm_reset(self);
                OSound_init_sound(self, cmd, DATA_SPLASH, CHANNEL_YM1);
                break;

            case SOUND_COIN_IN:
                OSound_init_sound(self, cmd, DATA_COININ, CHANNEL_YM_FX1);
                break;

            case SOUND_MUSIC_MAGICAL:
                /*sound_props |= BIT_0; // Trigger rev effect (moved into 68k code) */
                cmd = SOUND_MUSIC_MAGICAL;
                OSound_fm_reset(self);
                OSound_init_sound(self, cmd, DATA_MAGICAL, CHANNEL_YM1);
                break;

            case SOUND_YM_CHECKPOINT:
                OSound_init_sound(self, cmd, DATA_CHECKPOINT, CHANNEL_YM_FX1);
                break;

            case SOUND_INIT_SLIP:
                OSound_init_sound(self, cmd, DATA_SLIP, CHANNEL_PCM_FX3);
                break;

            case SOUND_INIT_CHEERS:
                OSound_init_sound(self, cmd, DATA_CHEERS, CHANNEL_PCM_FX1);
                break;

            case SOUND_STOP_CHEERS:
                self->chan_ram[CHANNEL_PCM_FX1] = 0;
                self->chan_ram[CHANNEL_PCM_FX2] = 0;
                OSound_pcm_w(self, 0xF08E, 1); /* Set inactive flag on channels */
                OSound_pcm_w(self, 0xF09E, 1);
                break;

            case SOUND_CRASH1:
                OSound_init_sound(self, cmd, DATA_CRASH1, CHANNEL_PCM_FX5);
                break;

            case SOUND_REBOUND:
                OSound_init_sound(self, cmd, DATA_REBOUND, CHANNEL_PCM_FX5);
                break;

            case SOUND_CRASH2:
                OSound_init_sound(self, cmd, DATA_CRASH2, CHANNEL_PCM_FX5);
                break;

            case SOUND_NEW_COMMAND:
                OSound_new_command(self);
                break;

            case SOUND_SIGNAL1:
                OSound_init_sound(self, cmd, DATA_SIGNAL1, CHANNEL_YM_FX1);
                break;

            case SOUND_SIGNAL2:
                self->sound_props &= ~BIT_0; /* Clear rev effect */
                OSound_init_sound(self, cmd, DATA_SIGNAL2, CHANNEL_YM_FX1);
                break;

            case SOUND_INIT_WEIRD:
                OSound_init_sound(self, cmd, DATA_WEIRD, CHANNEL_PCM_FX5);
                break;

            case SOUND_STOP_WEIRD:
                self->chan_ram[CHANNEL_PCM_FX5] = 0;
                self->chan_ram[CHANNEL_PCM_FX6] = 0;
                OSound_pcm_w(self, 0xF0CE, 1); /* Set inactive flag on channels */
                OSound_pcm_w(self, 0xF0DE, 1);
                break;

            case SOUND_REVS:
                OSound_fm_reset(self);
                self->sound_props |= BIT_0; /* Trigger rev effect */
                break;

            case SOUND_BEEP1:
                OSound_init_sound(self, cmd, DATA_BEEP1, CHANNEL_YM_FX1);
                break;

            case SOUND_UFO:
                OSound_fm_reset(self);
                OSound_init_sound(self, cmd, DATA_UFO, CHANNEL_YM_FX1);
                break;

            case SOUND_BEEP2:
                OSound_fm_reset(self);
                OSound_init_sound(self, cmd, DATA_BEEP2, CHANNEL_YM1);
                break;

            case SOUND_INIT_CHEERS2:
                OSound_init_sound(self, cmd, DATA_CHEERS2, CHANNEL_PCM_FX1);
                break;

            case SOUND_VOICE_CHECKPOINT:
                OSound_pcm_backup(self);
                OSound_init_sound(self, cmd, DATA_VOICE1, CHANNEL_PCM_FX7);
                break;

            case SOUND_VOICE_CONGRATS:
                OSound_pcm_backup(self);
                OSound_init_sound(self, cmd, DATA_VOICE2, CHANNEL_PCM_FX7);
                break;

            case SOUND_VOICE_GETREADY:
                OSound_pcm_backup(self);
                OSound_init_sound(self, cmd, DATA_VOICE3, CHANNEL_PCM_FX7);
                break;

            case SOUND_INIT_SAFETYZONE:
                OSound_init_sound(self, cmd, DATA_SAFETY, CHANNEL_PCM_FX3);
                break;

            case SOUND_STOP_SLIP:
            case SOUND_STOP_SAFETYZONE:
                self->chan_ram[CHANNEL_PCM_FX3] = 0;
                self->chan_ram[CHANNEL_PCM_FX4] = 0;
                OSound_pcm_w(self, 0xF0AE, 1); /* Set inactive flag on channels */
                OSound_pcm_w(self, 0xF0BE, 1);
                break;

            case SOUND_YM_SET_LEVELS:
                OSound_ym_set_levels(self);
                break;

            case SOUND_PCM_WAVE:
                OSound_init_sound(self, cmd, DATA_WAVE, CHANNEL_PCM_FX1);
                break;

            case SOUND_MUSIC_LASTWAVE:
                OSound_init_sound(self, cmd, DATA_LASTWAVE, CHANNEL_YM1);
                break;

            /* Enhancement: Load New Binary Data */
            case SOUND_MUSIC_CUSTOM:
                /*sound_props |= BIT_0; // Trigger rev effect */
                cmd = SOUND_MUSIC_CUSTOM;
                OSound_fm_reset(self);
                OSound_init_sound(self, cmd, DATA_CUSTOM, CHANNEL_YM1);
                break;
        }
    }
}

/* Called before initalizing a new command (PCM standalone sample, FM sample, New Music) */
/* Source: 0x833 */static 
void OSound_new_command(OSound* self)
{
    uint16_t pcm_enable;
    /* ------------------------------------------------------------------------ */
    /* FM Sound Effects Only (Increase Volume) */
    /* ------------------------------------------------------------------------ */
    if (self->chan_ram[CHANNEL_YM_FX1] & BIT_7)
    {
        self->chan_ram[CHANNEL_YM_FX1] = 0;

        { uint16_t adr = YM_LEVEL_CMDS2;

        /* Send four level commands */
        { uint8_t i; for (i = 0; i < 4; i++)
        {
            uint8_t reg = RomLoader_read8_16a(&(roms.z80), &adr);
            uint8_t val = RomLoader_read8_16a(&(roms.z80), &adr);
            OSound_fm_write_reg(self, reg, val);
        } }
     }}

    /* Clear channel memory area used for internal format of PCM sound data */
    { int16_t i; for (i = CHANNEL_PCM_FX1; i < CHANNEL_YM_FX1; i++)
        self->chan_ram[i] = 0; }

    /* PCM Channel Enable */
    pcm_enable = 0xF088 + 6;

    /* Disable 6 PCM Channels */
    { int16_t i; for (i = 0; i < 6; i++)
    {
        OSound_pcm_w(self, pcm_enable, OSound_pcm_r(self, pcm_enable) | BIT_0); 
        pcm_enable += 0x10; /* Advance to next channel */
    } }
}

/* Copy PCM Channel RAM contents to Channel RAM */
/* Source: 0x961 */static 
void OSound_pcm_backup(OSound* self)
{
    /* Return if PCM Channel contents already backed up */
    if (self->sound_props & BIT_1)
        return;

    memcpy(self->chan_ram + CH09_CMDS1, self->pcm_ram + 0x40, 8); /* Channel 9 Blocks */
    memcpy(self->chan_ram + CH09_CMDS2, self->pcm_ram + 0xC0, 8);
    memcpy(self->chan_ram + CH11_CMDS1, self->pcm_ram + 0x50, 8); /* Channel 11 Blocks */
    memcpy(self->chan_ram + CH11_CMDS1, self->pcm_ram + 0xD0, 8);

    self->sound_props |= BIT_1; /* Denote contents backed up */
}

/* Check whether FM Channel is in use */
/* Map back to corresponding music channel */
/* */
/* Source: 0x95 */static 
void OSound_check_fm_mapping(OSound* self)
{
    uint16_t chan_id = CHANNEL_MAP1;

    /* 8 Channels */
    { uint8_t c; for (c = 0; c < 8; c++)
    {
        /* Map back to corresponding music channel */
        if (self->chan_ram[chan_id] & BIT_7)    
            self->chan_ram[chan_id - 0x2C0] |= BIT_2;
        chan_id += CHAN_SIZE;
    } }
}

/* Process and update all sound channels */
/* */
/* Source: 0xB3 */static 
void OSound_process_channels(OSound* self)
{
    uint16_t chan_id;
    /* Allows FM Music & FM Effect to be played simultaneously */
    OSound_check_fm_mapping(self);

    /* Channel to process */
    chan_id = CHANNEL_YM1;

    { uint8_t c; for (c = 0; c < 30; c++)
    {
        /* If channel is enabled, process the channel */
        if (self->chan_ram[chan_id] & BIT_7)
            OSound_process_channel(self, chan_id);

        chan_id += CHAN_SIZE; /* Advance to next channel in memory */
    } }
}

/* Update Individual Channel. */
/* */
/* Inputs: */
/* chan_id = Channel ID to process */
/* */
/* Source: 0xCD */static 
void OSound_process_channel(OSound* self, uint16_t chan_id)
{
    uint8_t reg;
    uint16_t seq_end;
    uint8_t* chan;
    self->chanid_prev = chan_id;

    /* Get correct offset in RAM */
    chan = &self->chan_ram[chan_id];

    /* Increment sequence position */
    self->pos = OSound_r16(self, &chan[CH_SEQ_POS]) + 1;
    OSound_w16(self, &chan[CH_SEQ_POS], self->pos);

    /* Sequence end marker */
    seq_end = OSound_r16(self, &chan[CH_SEQ_END]);

    if (self->pos == seq_end)
    {
        self->pos = OSound_r16(self, &chan[CH_SEQ_CMD]);
        OSound_process_section(self, chan);
        
        /* Hack to return when last command was a PCM/YM finalize */
        /* This is here to facilitate program flow.  */
        /* The Z80 code does loads of funky (or nasty depending on your point of view) stuff with the stack.  */
        if (self->cmd_prev == 0x84 || self->cmd_prev == 0x99) 
            return;
    }

    /* Return if not FM channel */
    if (chan[CH_FM_FLAGS] & BIT_6) return;

    /* ------------------------------------------------------------------------ */
    /* FM CHANNELS */
    /* ------------------------------------------------------------------------ */

    /* 0xF9:   */
    { uint8_t chan_index = chan[CH_FM_FLAGS] & 7;

    /* Use Phase and Amplitude Modulation Sensitivity Table? */
    if (chan[CH_FM_PHASETBL])
    {
        OSound_read_mod_table(self, chan);
    }

    /* If note set to 0xFF, turn off the channel output */
    if (chan[CH_FM_NOTE] == 0xFF)
    {
        OSound_fm_write_reg_c(self, chan[CH_FLAGS], 8, chan_index);
        return;
    }

    /* FM Noise Channel */
    if (chan[CH_FLAGS] & BIT_1)
    {
        reg = 0xF; /* Register = Noise Enable & Frequency */
    }
    /* Set Volume or Mute if PHASETBL not setup */
    else
    {
        /* 0x119 */
        /* Register = Phase and Amplitude Modulation Sensitivity ($38-$3F) */
        OSound_fm_write_reg_c(self, chan[CH_FLAGS], 0x30 + chan_index, chan[CH_FM_PHASETBL] ? chan[CH_FM_PHASE_AMP] : 0);

        /* Register = Create Note ($28-$2F) */
        reg = 0x28 + chan_index;
    }

    /* set_octave_note:  */
    OSound_fm_write_reg_c(self, chan[CH_FLAGS], reg, chan[CH_FM_NOTE]);

    /* Check position in sequence. If expired, set channels on/off */
    if (OSound_r16(self, &chan[CH_SEQ_POS])) return;

    /* Turn channels off */
    OSound_fm_write_reg_c(self, chan[CH_FLAGS], 8, chan_index);

    /* Turn modulator and carry channels off */
    OSound_fm_write_reg_c(self, chan[CH_FLAGS], 8, chan_index | 0x78);
 }}

/* Process Channel Section */
/* */
/* Source: 0x2E1 */static 
void OSound_process_section(OSound* self, uint8_t* chan)
{
    uint8_t cmd = RomLoader_read8_16a(&(roms.z80), &self->pos);
    self->cmd_prev = cmd;
    if (cmd >= 0x80)
    {
        OSound_next_mml_cmd(self, chan, cmd);
        return;
    }

    /* ------------------------------------------------------------------------ */
    /* FM Only Code From Here Onwards */
    /* ------------------------------------------------------------------------ */

    /* 0x30d set_note_octave */
    /* Command is an offset into the Note Offset table in ROM. */
    if (cmd)
    {
        uint16_t adr = YM_NOTE_OCTAVE + (cmd - 1 + (int8_t) chan[CH_NOTE_OFFSET]);
        chan[CH_FM_NOTE] = RomLoader_read8(&(roms.z80), adr);
    }
    /* If Channel is mute, clear note information. */
    else if (!(chan[CH_FM_FLAGS] & BIT_6))
    {
        chan[CH_FM_NOTE] = 0xFF;
    }

    OSound_calc_end_marker(self, chan);
}

/* Source: 0x31C */static 
void OSound_calc_end_marker(OSound* self, uint8_t* chan)
{
    uint16_t end_marker = RomLoader_read8(&(roms.z80), self->pos);
    
    /* 32d */
    if (chan[CH_FM_MARKER] & BIT_1)
    {
        /* Mask on high bit */
        if (chan[CH_FM_MARKER] & BIT_0)
        {
            chan[CH_FM_MARKER] &= ~BIT_0;
            end_marker += (RomLoader_read8(&(roms.z80), ++self->pos) << 8); 
        }
    }
    /* 325 */
    else
    {
        end_marker = chan[CH_END_MARKER] * end_marker;
    }
  
    OSound_w16(self, &chan[CH_SEQ_END], end_marker); /* Set End Marker     */
    OSound_w16(self, &chan[CH_SEQ_CMD], ++self->pos);      /* Set Next Sequence Command */
    OSound_w16(self, &chan[CH_UNKNOWN], 0);
    OSound_w16(self, &chan[CH_SEQ_POS], 0);
}


/* Trigger New MML Command From Sound Data. */
/* */
/* Source: 0x3A8 */static 
void OSound_next_mml_cmd(OSound* self, uint8_t* chan, uint8_t cmd)
{
    /* Play New PCM Sample */
    if (cmd >= 0xBF)
    {
        OSound_play_pcm_index(self, chan, cmd);
        return;
    }

    /* Trigger New Command On Channel */
    switch (cmd & 0x3F)
    {
        /* YM & PCM: Set Volume */
        case MML_SAMPLE_LEVEL:
            OSound_setvol(self, chan);
            break;

        case MML_END_FM_TRACK:
            OSound_ym_end_track(self, chan);
            return;

        /* YM: Enable/Disable Modulation table */
        case MML_KEY_FRACTIONS:
            chan[CH_FM_PHASETBL] = RomLoader_read8(&(roms.z80), self->pos);
            break;

        /* Write Sequence Address (Used by PCM Drum Samples) */
        case MML_CALL:
            OSound_call_adr(self, chan);
            break;

        /* Set Next Sequence Address [pos] (Used by PCM Drum Samples) */
        case MML_RET:
            self->pos = OSound_r16(self, &chan[chan[CH_MEM_OFFSET]]);
            chan[CH_MEM_OFFSET] += 2;
            break;

        case MML_LOOP_FOREVER:
            OSound_set_loop_adr(self);
            break;

        /* YM: Set Note/Octave Offset */
        case MML_TRANSPOSE:
            chan[CH_NOTE_OFFSET] += RomLoader_read8(&(roms.z80), self->pos);
            break;

        /* LOOP	loopNumber, loopCount, loopAddress */
        case MML_LOOP:
            OSound_do_loop(self, chan);
            break;

        /* Only used by Step On Beat (Switch) */
        case MML_PITCH_BEND_END:
            chan[CH_FLAGS] &= ~BIT_5;
            self->pos--;
            break;

        case MML_LOAD_PATCH:
            OSound_ym_load_patch(self, chan);
            break;

        case MML_VOICE_PITCH:
            OSound_pcm_setpitch(self, chan);
            break;

        /* FM: Note duration is read directly from track rather than being computed */
        case MML_FIXED_TEMPO:
            chan[CH_FM_MARKER] |= BIT_1;
            self->pos--;
            break;

        /* FM: Used for 'long' notes, restsand percussion samples */
        case MML_LONG:
            chan[CH_FM_MARKER] |= BIT_0;
            self->pos--;
            break;

        /* FM: Connect Channel to Right Speaker */
        case MML_RIGHT_CH_ONLY:
            OSound_ym_set_connect(self, chan, PAN_RIGHT);
            break;

        /* FM: Connect Channel to Left Speaker */
        case MML_LEFT_CH_ONLY:
            OSound_ym_set_connect(self, chan, PAN_LEFT);
            break;

        /* FM: Connect Channel to Both Speakers */
        case MML_BOTH_CH:
            OSound_ym_set_connect(self, chan, PAN_CENTRE);
            break;

        case 0x19:
            OSound_pcm_finalize(self, chan);
            return;
    }

    self->pos++;
    OSound_process_section(self, chan);
}

/* Set Volume (Left & Right Channels) */
/* Source: 0x3D7 */static 
void OSound_setvol(OSound* self, uint8_t* chan)
{  
    uint8_t vol_l = RomLoader_read8(&(roms.z80), self->pos);

    /* PCM Percussion Sample */
    if (chan[CH_FM_FLAGS] & BIT_6)
    {
        uint8_t vol_r;
        const uint8_t VOL_MAX = 0x40;

        /* Set left volume */
        chan[CH_VOL_L] = vol_l > VOL_MAX ? 0 : vol_l;

        /* Set right volume */
        vol_r = RomLoader_read8(&(roms.z80), ++self->pos);
        chan[CH_VOL_R] = vol_r > VOL_MAX ? 0 : vol_r;
    }
    /* YM */
    else
    {
        chan[CH_FM_MARKER] = vol_l;
    }
}

/* PCM Command: Set Pitch */
/* Source: 0x3C4 */static 
void OSound_pcm_setpitch(OSound* self, uint8_t* chan)
{
    /* PCM Percussion Sample (Pitch can't be applied to voices) */
    if (chan[CH_FM_FLAGS] & BIT_6)
        chan[CH_PCM_PITCH] = RomLoader_read8(&(roms.z80), self->pos);
}

/* Sets Loop Address For FM & PCM Commands. */
/* For example, used by Checkpoint FM sample and Crowd PCM Sample */
/* Source: 0x446 */static 
void OSound_set_loop_adr(OSound* self)
{
    self->pos = RomLoader_read16_16(&(roms.z80), self->pos) - 1;
}

/* Set Loop Counter For FM & PCM Data */
/* LOOP	loopNumber, loopCount, loopAddress */
/* Source: 0x454 */static 
void OSound_do_loop(OSound* self, uint8_t* chan)
{
    uint8_t offset = RomLoader_read8(&(roms.z80), self->pos++) + 0x18;
    uint8_t a = chan[offset];

    /* Reload counter */
    if (a == 0)
    {
        chan[offset] = RomLoader_read8(&(roms.z80), self->pos);
    }

    self->pos++;
    if (--chan[offset] != 0)
        OSound_set_loop_adr(self);
    else
        self->pos++;
}

/* Write Commands to PCM Channel (For Individual Sound Effects) */
/* Source: 0x483 */static 
void OSound_pcm_finalize(OSound* self, uint8_t* chan)
{
    self->sound_props &= ~BIT_1; /* Clear PCM sound effect triggered */

    memcpy(self->pcm_ram + 0x40, self->chan_ram + CH09_CMDS1, 8); /* Channel 9 Blocks */
    memcpy(self->pcm_ram + 0xC0, self->chan_ram + CH09_CMDS2, 8);
    memcpy(self->pcm_ram + 0x50, self->chan_ram + CH11_CMDS1, 8); /* Channel 11 Blocks */
    memcpy(self->pcm_ram + 0xD0, self->chan_ram + CH11_CMDS1, 8);
    
    OSound_ym_end_track(self, chan);
}

/* Percussion sample properties */
/* Sample Start Address, Sample End Address (High Byte Only), Pitch, Sample Flags */
const static uint16_t PCM_PERCUSSION [] =
{
    0x17C0, 0x42, 0x84, 0xD6,
    0x302F, 0x3A, 0x84, 0xC6,
    0x0090, 0x0B, 0x84, 0xC2,
    0x00F0, 0x08, 0x84, 0xD2,
    0x49DE, 0x4C, 0x84, 0xD2,
    0x437C, 0x48, 0x84, 0xD2,
    0x29AB, 0x2E, 0x84, 0xC2,
    0x1C03, 0x28, 0x84, 0xC2,
    0x0951, 0x16, 0x84, 0xD2,
    0x3BE6, 0x7F, 0x90, 0xC2,
    0x5830, 0x5F, 0x84, 0xD2,
    0x4DA0, 0x57, 0x84, 0xD2,
    0x0C1D, 0x1B, 0x78, 0xC2,
    0x6002, 0x7F, 0x84, 0xD2,
    0x0C1D, 0x1B, 0x40, 0xC2,
};

/* Play PCM Sample. */
/* This routine is used both for standalone samples, and samples used by the music. */
/* */
/* +0 [Word]: Start Address In Bank */
/* +1 [Byte]: End Address (<< 8 + 1) */
/* +2 [Byte]: Sample Flags */
/* */
/* Bank 0: opr10193.66 */
/* Bank 1: opr10192.67 */
/* Bank 2: opr10191.68 */
/* */
/* Source: 0x57A */static 
void OSound_play_pcm_index(OSound* self, uint8_t* chan, uint8_t cmd)
{
    if (cmd == 0)
    {
        OSound_calc_end_marker(self, chan);
        return;
    }

    /* ------------------------------------------------------------------------ */
    /* Initalize PCM Effects */
    /* ------------------------------------------------------------------------ */
    if (cmd >= 0xD0)
    {
        /* First sample is at index 0xD0, so reset to 0 */
        uint16_t pcm_index = PCM_INFO + ((cmd - 0xD0) << 2);

        chan[CH_PCM_ADR1L] = RomLoader_read8_16a(&(roms.z80), &pcm_index);           /* Wave Start Address */
        chan[CH_PCM_ADR1H] = RomLoader_read8_16a(&(roms.z80), &pcm_index);
        chan[CH_PCM_ADR2]  = RomLoader_read8_16a(&(roms.z80), &pcm_index);           /* Wave End Address */
        chan[CH_CTRL]      = RomLoader_read8_16a(&(roms.z80), &pcm_index);           /* Sample Flags */
    }
    /* ------------------------------------------------------------------------ */
    /* Initalize PCM Percussion Samples */
    /* ------------------------------------------------------------------------ */
    else
    {
        uint16_t pcm_index = (cmd - 0xC0) << 2;
        OSound_w16(self, &chan[CH_PCM_ADR1L], PCM_PERCUSSION[pcm_index]);        /* Wave Start Address */
        chan[CH_PCM_ADR2]  = (uint8_t) PCM_PERCUSSION[pcm_index+1]; /* Wave End Address */
        chan[CH_PCM_PITCH] = (uint8_t) PCM_PERCUSSION[pcm_index+2]; /* Wave Pitch */
        chan[CH_CTRL]      = (uint8_t) PCM_PERCUSSION[pcm_index+3]; /* Sample Flags */
    }

    OSound_process_pcm(self, chan);
}

/* Initalize Sound from ROM to RAM. Used for both PCM and YM sounds. */
/* */
/* Inputs:  */
/* a = Command, de = dst, hl = src */
/* */
/* Source: 0x9E8 */static 
void OSound_init_sound(OSound* self, uint8_t cmd, uint16_t src, uint16_t dst)
{
    uint8_t channels;
    uint16_t dst_backup = dst;

    self->command_index = cmd - 0x81;
    
    /* Get offset to channel setup */
    src = RomLoader_read16_16(&(roms.z80), src);

    /* Get number of channels */
    channels = RomLoader_read8_16a(&(roms.z80), &src);

    /* next_channel */
    { uint8_t ch; for (ch = 0; ch < channels; ch++)
    {
        /* Address of default channel setup data */
        uint16_t adr = RomLoader_read16_16a(&(roms.z80), &src);

        /* Copy default setup code for block of sound (14 bytes) */
        { int i; for (i = 0; i < 0xE; i++)
            self->chan_ram[dst++] = RomLoader_read8_16a(&(roms.z80), &adr); }

        /* Write command byte (at position 0xE) */
        self->chan_ram[dst++] = self->command_index;

        /* Write zero 17 times (essentially pad out the 0x20 byte entry) */
        /* This empty space is updated later */
        { int i; for (i = 0xF; i < CHAN_SIZE; i++)
            self->chan_ram[dst++] = 0; }
    } }
}

/* Source: 0xCC3 */static 
void OSound_process_pcm(OSound* self, uint8_t* chan)
{
    /* ------------------------------------------------------------------------ */
    /* PCM Music Sample (Drums) */
    /* ------------------------------------------------------------------------ */
    if (chan[CH_CTRL] & BIT_7)
    {
        const uint16_t BASE_ADR = 0xF088; /* Channel 2 Base Address */
        const uint16_t CHAN_SIZE = 0x10;  /* Size of each channel entry (2 Channel Increment) */

        uint16_t adr = BASE_ADR; 

        /* Check Wave End Address */
        if (chan[CH_CTRL] & BIT_2)
        {
            /* get_chan_adr2: */
            { int i; for (i = 0; i < 6; i++)
            {
                uint8_t channel_pair = OSound_pcm_r(self, adr + 6);

                /* If channel active, play sample */
                if ((channel_pair & 0x84) == 0x84 && (channel_pair & BIT_0) == 0)
                {
                    OSound_pcm_send_cmds(self, chan, adr, channel_pair);
                    return;
                }
                /* d79 */
                adr += CHAN_SIZE; /* Advance to next channel */
            } }
            adr = BASE_ADR;
        }
        /* get_chan_adr3: */
        { int i; for (i = 0; i < 6; i++)
        {
            uint8_t channel_pair = OSound_pcm_r(self, adr + 6);

            if (channel_pair & BIT_0)
            {
                OSound_pcm_send_cmds(self, chan, adr, channel_pair);
                return;
            }

            adr += CHAN_SIZE; /* Advance to next channel */
        } }

        adr = BASE_ADR;
        /* get_chan_adr4: */
        { int i; for (i = 0; i < 6; i++)
        {
            uint8_t channel_pair = OSound_pcm_r(self, adr + 6);

            if (channel_pair & BIT_7)
            {
                OSound_pcm_send_cmds(self, chan, adr, channel_pair);
                return;
            }
            adr += CHAN_SIZE; /* Advance to next channel */
        } }

        /* No need to restore positioning info from stack, as stored in 'pos' variable */
        OSound_calc_end_marker(self, chan);
    }

    /* ------------------------------------------------------------------------ */
    /* Standard PCM Samples */
    /* ------------------------------------------------------------------------ */
    else
    {
        uint16_t pcm_adr;
        /* Mask on channel pair select */
        uint8_t channel_pair = chan[CH_CTRL] & 0xC;
        /* Channel selected [b] */
        uint8_t selected = 0;

        /* select_ch_8or10: */
        if (channel_pair < 4)
        {
            /* Denote PCM sound effect triggered. */
            self->sound_props |= BIT_1;
            if (++self->counter1 & BIT_0)
            {
                selected = 10;      
                OSound_pcm_w(self, 0xF0D6, channel_pair = 1); /* Set flags for channel 10 (active, loop disabled) */
            }
            else
            {
                selected = 8;
                OSound_pcm_w(self, 0xF0C6, channel_pair = 1); /* Set flags for channel 10 (active, loop disabled) */
            }
        }
        /* select_ch_1or3 */
        else if (channel_pair == 4)
        {
            if (++self->counter2 & BIT_0)
                selected = 3;      
            else
                selected = 1;
        }
        /* select_ch_9or11 */
        else if (channel_pair == 8)
        {
            if (++self->counter3 & BIT_0)
                selected = 11;      
            else
                selected = 9;
        }
        /* select_ch_5or7 */
        else
        {
            if (++self->counter4 & BIT_0)
                selected = 7;      
            else
                selected = 5;
        }

        /* Channel Address = Channel 1 Base Address  */
        pcm_adr = 0xF080 + (selected * 8);

        OSound_pcm_send_cmds(self, chan, pcm_adr, channel_pair);
    }
}

/* Source: 0xDA8 */static 
void OSound_pcm_send_cmds(OSound* self, uint8_t* chan, uint16_t pcm_adr, uint8_t channel_pair)
{
    OSound_pcm_w(self, pcm_adr + 0x80, channel_pair);        /* Write channel pair selected value */
    OSound_pcm_w(self, pcm_adr + 0x82, chan[CH_VOL_L]);     /* Volume left */
    OSound_pcm_w(self, pcm_adr + 0x83, chan[CH_VOL_R]);     /* Volume Right */
    OSound_pcm_w(self, pcm_adr + 0x84, chan[CH_PCM_ADR1L]); /* PCM Start Address Low */
    OSound_pcm_w(self, pcm_adr + 0x4,  chan[CH_PCM_ADR1L]); /* PCM Loop Address Low */
    OSound_pcm_w(self, pcm_adr + 0x85, chan[CH_PCM_ADR1H]); /* PCM Start Address High */
    OSound_pcm_w(self, pcm_adr + 0x5,  chan[CH_PCM_ADR1H]); /* PCM Loop Address High */
    OSound_pcm_w(self, pcm_adr + 0x86, chan[CH_PCM_ADR2]);  /* PCM End Address High */
    OSound_pcm_w(self, pcm_adr + 0x87, chan[CH_PCM_PITCH]); /* PCM Pitch */
    OSound_pcm_w(self, pcm_adr + 0x6,  chan[CH_CTRL]);      /* PCM Flags */

    /* No need to restore positioning info from stack, as stored in 'pos' variable */
    OSound_calc_end_marker(self, chan);
}static 

void OSound_fm_dotimera(OSound* self)
{
    /* Return if YM2151 is busy */
    if (!(YM2151_read_status(self->ym) & BIT_0))
        return;
    /* Set Timer A, Enable its IRQ and also reset its IRQ */
    YM2151_write_reg(self->ym, 0x14, 0x15); /* %10101 */
}

/* Reset Yamaha YM2151 Chip. */
/* Called before inititalizing a new music track, or when z80 has just initalized and has no specific command. */
/* Source: 0x561 */static 
void OSound_fm_reset(OSound* self)
{
    /* Clear YM & Drum Channels in RAM (0xF820 - 0xF9DF) */
    { uint16_t i; for (i = CHANNEL_YM1; i < CHANNEL_PCM_FX1; i++)
        self->chan_ram[i] = 0; }

    OSound_fm_write_block(self, 0, YM_INIT_CMDS, 0);
}

/* Write to FM Register With Check */
/* Source: 0xA70 */static 
void OSound_fm_write_reg_c(OSound* self, uint8_t ix0, uint8_t reg, uint8_t value)
{
    /* Is corresponding music channel enabled? */
    if (ix0 & BIT_2)
        return;

    OSound_fm_write_reg(self, reg, value);
}

/* Write to FM Register */
/* Source: 0xA75 */static 
void OSound_fm_write_reg(OSound* self, uint8_t reg, uint8_t value)
{
    /* Return if YM2151 is busy */
    if (YM2151_read_status(self->ym) & BIT_7)
        return;
    YM2151_write_reg(self->ym, reg, value);
}

/* Write Block of FM Data From ROM */
/* */
/* Inputs: */
/* */
/* adr  = Address of commands and data to write to FM */
/* chan = Register offset */
/* */
/* Format: */
/* 2 = End of data block */
/* 3 = Next word specifies next address in memory */
/* */
/* Source: 0xA84 */static 
void OSound_fm_write_block(OSound* self, uint8_t ix0, uint16_t adr, uint8_t chan)
{
    uint8_t cmd = RomLoader_read8_16a(&(roms.z80), &adr);

    /* Return if end of data block */
    if (cmd == 2) return;

    /* Next word specifies next address in memory */
    if (cmd == 3)
    {
        adr = RomLoader_read16_16(&(roms.z80), adr);
    }
    else
    {
        uint8_t reg = cmd + chan;
        uint8_t val = RomLoader_read8_16a(&(roms.z80), &adr);
        OSound_fm_write_reg_c(self, 0, reg, val);
    }

    OSound_fm_write_block(self, ix0, adr, chan);
}

/* Write level info to YM Channels */
/* Source: 0x91A */static 
void OSound_ym_set_levels(OSound* self)
{
    uint8_t entries;
    /* Clear YM & Drum Channels in RAM (0xF820 - 0xF9DF) */
    { uint16_t i; for (i = CHANNEL_YM1; i < CHANNEL_PCM_FX1; i++)
        self->chan_ram[i] = 0; }

    /* FM Sound Effects: Write fewer levels */
    entries = (self->chan_ram[CHANNEL_YM_FX1] & BIT_7) ? 28 : 32;
    { uint16_t adr = YM_LEVEL_CMDS1;

    /* Write Level Info */
    { uint8_t i; for (i = 0; i < entries; i++)
    {
        uint8_t reg = RomLoader_read8_16a(&(roms.z80), &adr);
        uint8_t val = RomLoader_read8_16a(&(roms.z80), &adr);
        OSound_fm_write_reg(self, reg, val);
    } }
 }}

const static uint16_t FM_DATA_TABLE[] =
{
    DATA_BREEZE,
    DATA_SPLASH,
    DATA_CUSTOM,
    DATA_COININ,
    DATA_MAGICAL,
    DATA_CHECKPOINT,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    DATA_SIGNAL1,
    DATA_SIGNAL2,
    0,
    0,
    0,
    DATA_BEEP1,
    DATA_UFO,
    DATA_BEEP2,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    DATA_LASTWAVE,
};

/* This is called first, when setting up YM Samples. */
/* The global 'pos' variable stores the location of the block table. */
/* Source: 0x515 */static 
void OSound_ym_load_patch(OSound* self, uint8_t* chan)
{
    /* Set block address */
    chan[CH_FM_BLOCK] = RomLoader_read8(&(roms.z80), self->pos);
    
    if (!chan[CH_FM_BLOCK])
        return;

    { uint16_t adr = OSound_ym_lookup_data(self, chan[CH_COMMAND], 3, chan[CH_FM_BLOCK]); /* Use Routine 3. */
    OSound_fm_write_block(self, chan[CH_FLAGS], adr, chan[CH_FM_FLAGS] & 7);
 }}

/* Source: 0xAAA */static 
uint16_t OSound_ym_lookup_data(OSound* self, uint8_t cmd, uint8_t offset, uint8_t block)
{
    uint16_t adr;
    block = (block - 1) << 1;
    
    /* Address of data for FM routine */
    adr = RomLoader_read16_16(&(roms.z80), (uint16_t) (FM_DATA_TABLE[cmd] + (offset << 1)));
    return RomLoader_read16_16(&(roms.z80), (uint16_t) (adr + block));
}

/* "Connect" Channels To Play Out of Left/Right Speakers. */
/* Source: 0x534 */static 
void OSound_ym_set_connect(OSound* self, uint8_t* chan, uint8_t pan)
{
    uint8_t reg_value;
    uint8_t chan_ctrl_reg;
    uint8_t block = chan[CH_FM_BLOCK];                          /* FM Routine To Choose from Data Block */
    uint16_t adr = OSound_ym_lookup_data(self, chan[CH_COMMAND], 3, block);  /* Send Block of FM Commands */
    adr += 0x33;

    /* c = Channel Control Register (0x20 - 0x27) */
    chan_ctrl_reg = (chan[CH_FM_FLAGS] & 7) + 0x20;

    /* Register Value */
    reg_value = (RomLoader_read8(&(roms.z80), adr) & 0x3F) | pan;

    self->pos--;

    /* Write Register */
    OSound_fm_write_reg_c(self, chan[CH_FLAGS], chan_ctrl_reg, reg_value);
}

/* iy = chan_ram */
/* */
/* Source: 0x4BF */static 
void OSound_ym_end_track(OSound* self, uint8_t* chan)
{
    uint8_t block;
    /* Get channel number */
    uint8_t chan_index = chan[CH_FM_FLAGS] & 7;

    /* Write block of release commands */
    OSound_fm_write_block(self, chan[CH_FLAGS], YM_RELEASE_RATE, chan_index);
    
    /* Register: KEY ON Turns on and off output from each operator of each channel. (Disable in this case) */
    OSound_fm_write_reg(self, 0x8, chan_index);
    /* Register: noise mode enable, noise period (Disable in this case) */
    OSound_fm_write_reg(self, 0xf, 0);

    chan[CH_FLAGS] = 0;

    /* Check whether YM channel is also playing music */
    if (self->chanid_prev < CHANNEL_MAP1)
    {
        /* pop and return */
        return;
    }
    
    chan -= 0x2C0; /* = corresponding music channel */

    /* Return if no sound playing on corresponding channel */
    if (!(chan[CH_FLAGS] & BIT_7))
        return;

    /* ------------------------------------------------------------------------ */
    /* FM Sound effect is playing & Music is playing simultaneously */
    /* ------------------------------------------------------------------------ */

    chan[CH_FLAGS] &= ~BIT_2;

    /* Write remaining FM Data block, if specified */
    block = chan[CH_FM_BLOCK];

    if (!block)
        return;

    { uint16_t adr = OSound_ym_lookup_data(self, chan[CH_COMMAND], 3, block);
    OSound_fm_write_block(self, chan[CH_FLAGS], adr, chan[CH_FM_FLAGS] & 7);
 }}

/* Use Phase and Amplitude Modulation Sensitivity Table */
/* Enabled with FM_PHASETBL flag. */
/* Source: 0x1D6 */static 
void OSound_read_mod_table(OSound* self, uint8_t* chan)
{
    uint16_t adr = OSound_ym_lookup_data(self, chan[CH_COMMAND], 2, chan[CH_FM_PHASETBL]); /* Use Routine 2. */

    while (true)
    {
        uint16_t offset = chan[CH_FM_PHASEOFF];
        uint8_t table_entry = RomLoader_read8(&(roms.z80), (uint16_t) (adr + offset));

        /* Reset table position */
        if (table_entry == 0xFD)
            chan[CH_FM_PHASEOFF] = 0;
        /* Decrement table position */
        else if (table_entry == 0xFE)
            chan[CH_FM_PHASEOFF]--;
        /* Unused special case */
        else if (table_entry == 0xFC)
        {
        }
        /* Increment table position */
        else
        {
            chan[CH_FM_PHASEOFF]++;
            { uint8_t carry = (table_entry < 0xFC) ? 2 : 0;
            /* rotate table_entry left through 9-bits twice */
            chan[CH_FM_PHASE_AMP] = ((table_entry << 2) + carry) + ((table_entry & 0x80) >> 7);
            return;
         }}
    }
}

/* 1/ bc = Read next value in sequence (from address in de)  */
/* 2/ Store value on stack */
/* 3/ Decrement internal pointer  */
/* 4/ hl =  Address in block + bc */
/* 5/ Increment and store next 'de' value in sequence within 0x20 block */
/* Source: 0x418 */static 
void OSound_call_adr(OSound* self, uint8_t* chan)
{
    uint16_t value = RomLoader_read16_16(&(roms.z80), self->pos);
    self->pos++;

    chan[CH_MEM_OFFSET]--;
    { uint8_t offset = chan[CH_MEM_OFFSET];
    chan[CH_MEM_OFFSET]--;

    chan[offset]   = self->pos >> 8;
    chan[offset-1] = self->pos & 0xFF;

    self->pos = value - 1;
 }}

/* ---------------------------------------------------------------------------- */
/*                                ENGINE TONE CODE  */
/* ---------------------------------------------------------------------------- */

/* Process Ferrari Engine Tone & Traffic Sound Effects */
/* */
/* Original addresses used:  */
/* 0xFC00: Engine Channel - Player's Car */
/* 0xFC20: Engine Channel - Traffic 1 */
/* 0xFC40: Engine Channel - Traffic 2 */
/* 0xFC60: Engine Channel - Traffic 3 */
/* 0xFC80: Engine Channel - Traffic 4 */
/* */
/* counter = Increments every tick (0 - 0xFF) */
/* */
/* Source: 0x7501 */static 
void OSound_engine_process(OSound* self)
{
    uint16_t iy;
    uint16_t ix;
    /* DEBUG */
    /* bpset 7501,1,{b@0xf801 = 0xe; b@0xf802 = 0xf1; b@f803 = 0x3f;} */
    /* engine_data[SOUND_ENGINE_PITCH_H] = 0xE; */
    /* engine_data[SOUND_ENGINE_PITCH_L] = 0xF1; */
    /* engine_data[SOUND_ENGINE_VOL]     = 0x3F; */
    /* END DEBUG */

    /* Return 1 in 2 times when this routine is called */
    if ((++self->engine_counter & 1) == 0)
        return;

    ix = 0;
    iy = CHANNEL_ENGINE_CH1;

    for (self->engine_channel = 6; self->engine_channel > 0; self->engine_channel--)
    {
        OSound_engine_process_chan(self, &self->chan_ram[iy], &self->pcm_ram[ix]);
        ix += 0x10;
        iy += CHAN_SIZE;
    }
}

/* Source: 0x7531 */static 
void OSound_engine_process_chan(OSound* self, uint8_t* chan, uint8_t* pcm)
{
    uint16_t engine_pos;
    uint16_t old_revs;
    uint16_t revs;
    /* Return if PCM Sample Being Played On Channel */
    if (self->engine_channel < 3)
    {
        if (self->sound_props & BIT_1)
            return;
    }

    /* Read Engine Data that has been sent by 68K CPU */
    OSound_engine_read_data(self, chan, pcm);

    /* ------------------------------------------------------------------------ */
    /* Car is stationary, do rev effect at start line. */
    /* This bit is set whenever the music start is triggered */
    /* ------------------------------------------------------------------------ */
    if (self->sound_props & BIT_0)
    {
        /* 0x7663 */
        uint16_t revs = OSound_r16(self, pcm); /* Read Revs/Pitch which has just been stored by engine_read_data */

        /* No revs, mute engine channel and get out of here */
        if (revs == 0)
        {
            OSound_engine_mute_channel(self, chan, pcm, true);
            return;
        }
        /* 0x766E */
        /* Do High Pitched Rev Sound When Car Is At Starting Line */
        /* Set Volume & Pitch, Then Return */
        if (revs >= 0xFA)
        {
            /* 0x7682 */
            /* Return if channel already active */
            if (!(pcm[0x86] & BIT_0))
                return;

            if (self->engine_channel < 3)
            {
                pcm[2] = 0x20; /* l */
                pcm[3] = 0;    /* r */
                pcm[7] = 0x41; /* pitch */
            }
            else if (self->engine_channel < 5)
            {
                pcm[2] = 0x10; /* l */
                pcm[3] = 0x10; /* r */
                pcm[7] = 0x42; /* pitch */
            }
            else
            {
                pcm[2] = 0x20; /* l */
                pcm[3] = 0;    /* r */
                pcm[7] = 0x40; /* pitch */
            }

            pcm[4] = pcm[0x84] = 0;    /* start/loop low */
            pcm[5] = pcm[0x85] = 0x36; /* start/loop high */
            pcm[6] = 0x55;             /* end address high */
            pcm[0x86] = 2;             /* Flags: Enable loop, set active */

            return;
        }
        /* Some code relating to 0xFD08 that I don't think is used */
    }
    /* 0x754A */
    if (self->engine_channel & BIT_0)
    {
        OSound_unk78c7(self, chan, pcm);
    }

    /* Check engine volume and mute channel if disabled */
    if (!chan[CH_ENGINES_VOL0])
    {
        OSound_engine_mute_channel(self, chan, pcm, true);
        return;
    }

    /* Set Engine Volume */
    if (chan[CH_ENGINES_VOL0] == chan[CH_ENGINES_VOL1])
    {
        /* Denote volume already set */
        chan[CH_ENGINES_FLAGS] |= BIT_1; 
    }
    else
    {
        chan[CH_ENGINES_FLAGS] &= ~BIT_1; 
        chan[CH_ENGINES_VOL1] = chan[CH_ENGINES_VOL0];
    }

    /* 0x755C */
    /* Check we have some revs */
    revs = OSound_r16(self, pcm);
    if (revs == 0)
    {
        OSound_engine_mute_channel(self, chan, pcm, true);
        return;
    }

    /* Rev Change Setup */
    /* 0x774E routine rolled in here */
    old_revs = OSound_r16(self, pcm + 0x80);

    /* Revs Unchanged */
    if (revs == old_revs)
    {
        chan[CH_ENGINES_FLAGS] |= BIT_0; /* denotes start address / end address has been set */
    }
    /* Revs Changed */
    else 
    {
        if (revs - old_revs < 0)
            chan[CH_ENGINES_FLAGS] &= ~BIT_2; /* loop enabled */
        else
            chan[CH_ENGINES_FLAGS] |= BIT_2;  /* loop disabled */

        chan[CH_ENGINES_FLAGS] &= ~BIT_0;     /* Start end address not set */
        OSound_w16(self, pcm + 0x80, revs);                 /* Write new revs value */
    }

    /* 0x756A */
    chan[CH_ENGINES_ACTIVE] &= ~BIT_0; /* Mute */

    /* Return if addresses have already been set */
    if ((chan[CH_ENGINES_FLAGS] & BIT_0) && chan[CH_ENGINES_FLAGS] & BIT_1)
        return;

    /* 0x757A */
    /* PLAYER'S CAR */
    if (self->engine_channel >= 5)
    {
        int16_t off = OSound_r16(self, pcm) - 0x30;

        if (off >= 0)
        {
            if (chan[CH_ENGINES_FLAGS] & BIT_4)
            {
                chan[CH_ENGINES_FLAGS] &= ~BIT_3; /* Loop Address Not Set */
                chan[CH_ENGINES_FLAGS] &= ~BIT_4;
            }

            OSound_ferrari_vol_pan(self, chan, pcm);
            return;
        }
        else
        {
            if (!(chan[CH_ENGINES_FLAGS] & BIT_4))
            {
                chan[CH_ENGINES_FLAGS] &= ~BIT_3; /* Loop Address Not Set */
                chan[CH_ENGINES_FLAGS] |=  BIT_4;
            }
        }
    }
    /* 0x75B2 */
    engine_pos = OSound_engine_get_table_adr(self, chan, pcm);
    
    /* Mute Engine Channel */
    if (chan[CH_ENGINES_FLAGS] & BIT_5)
    {
        OSound_engine_mute_channel(self, chan, pcm, false);
        return;
    }

    /* 0x75bc Has Start Address Been Set Already? */
    /* Used on start line */
    if (chan[CH_ENGINES_FLAGS] & BIT_0)
    {
        engine_pos += 2;
    }
    else
    {
        uint16_t start_adr = OSound_engine_set_adr(self, &engine_pos, chan, pcm); /* Set Start Address */
        OSound_engine_set_adr_end(self, &engine_pos, start_adr, chan, pcm);       /* Set End Address */
    }

    OSound_vol_thicken(self, &engine_pos, chan, pcm); /* Thicken engine effect by panning left/right dependent on channel. */
    OSound_engine_set_pitch(self, &engine_pos, pcm);  /* Set Engine Pitch from lookup table specified by hl */
    pcm[0x86] = 0;                      /* Set Active & Loop Enabled */
}

/* Only called for odd number channels */
/* I've not really worked out what this does yet */
/* Source: 0x78C7 */static 
void OSound_unk78c7(OSound* self, uint8_t* chan, uint8_t* pcm)
{
    uint16_t adr_offset;
    uint16_t adr; /* Channel address in RAM */

    if (self->engine_channel == 1)
    {
        adr = 0xFD10;
    }
    else if (self->engine_channel == 3)
    {
        adr = 0xFD30;
    }
    else
    {
        adr = 0xFD50;
    }

    /* STORE: Calculate offset into Channel Block To Store Data At */
    adr_offset = adr + (chan[CH_ENGINES_OFFSET] * 3);
    adr_offset &= 0x7FF;
    self->chan_ram[adr_offset++] = pcm[0x0];               /* Copy Engine Pitch Low */
    self->chan_ram[adr_offset++] = pcm[0x1];               /* Copy Engine Pitch High */
    self->chan_ram[adr_offset++] = chan[CH_ENGINES_VOL0]; /* Copy Engine Volume */

    /* Wrap around block of three entries */
    if (++chan[CH_ENGINES_OFFSET] >= 8)
    {
        chan[CH_ENGINES_OFFSET] = 0;
        adr_offset = adr & 0x7FF;
    }

    /* RESTORE: 7915 */
    pcm[0x0] = self->chan_ram[adr_offset++];
    pcm[0x1] = self->chan_ram[adr_offset++];
    chan[CH_ENGINES_VOL0] = self->chan_ram[adr_offset++];
}

/* Source: 0x75DA */static 
void OSound_ferrari_vol_pan(OSound* self, uint8_t* chan, uint8_t* pcm)
{
    uint16_t pitch;
    uint16_t pos;
    int16_t pitch_table_index;
    /* Adjust Engine Volume and write to new memory area (0x6) */
    OSound_engine_adjust_volume(self, chan);

    /* Set Pitch Table Details */
    pitch_table_index = OSound_r16(self, pcm + 0x80) - 0x30;

    if (pitch_table_index < 0)
    {
        OSound_engine_mute_channel(self, chan, pcm, false);
        return;
    }

    OSound_w16(self, chan + CH_ENGINES_PITCH_L, pitch_table_index);

    /* Set PCM Sample Addresses */
    pos = ENGINE_ADR_TABLE;
    OSound_engine_set_adr(self, &pos, chan, pcm);

    /* Set PCM Sample End Address */
    pcm[0x6] = RomLoader_read8(&(roms.z80), ++pos);

    /* Set Volume Pan */
    OSound_engine_set_pan(self, &pos, chan, pcm);

    /* Set Pitch */
    pos = ENGINE_ADR_TABLE + 4; /* Set position to pitch offset */
    pitch = RomLoader_read8(&(roms.z80), pos);
    pitch += OSound_r16(self, chan + CH_ENGINES_PITCH_L) >> 1;
    if (pitch > 0xFF) pitch = 0xFF;

    /* Tweak pitch slightly based on channel id */
    if (self->engine_channel & BIT_0)
        pitch -= 2;

    pcm[0x7] = (uint8_t) pitch;
    pcm[0x86] = 0x10; /* Set channel active and enabled */
}

/* Set Table Index For Engine Sample Start / End Addresses */
/* Table starts at ENGINE_ADR_TABLE offset in ROM. */
/* Source: 0x7819 */static 
uint16_t OSound_engine_get_table_adr(OSound* self, uint8_t* chan, uint8_t* pcm)
{
    const static uint8_t ENTRY = 5; /* bytes per entry */
    int16_t off = OSound_r16(self, pcm + 0x80) - 0x52;
    int16_t table_offset;

    if (off < 0)
    {
        chan[CH_ENGINES_FLAGS] &= ~BIT_5; /* Unmute Engine Sounds */
        table_offset = OSound_r16(self, pcm + 0x80);
        OSound_w16(self, pcm + 0x82, 0);
    }
    else
    {
        chan[CH_ENGINES_FLAGS] |= BIT_5; /* Mute Engine Sounds */
        table_offset = 1;
        OSound_w16(self, pcm + 0x82, off);
    }
    /* get_adr: */
    table_offset--;

    return (ENGINE_ADR_TABLE + ENTRY) + (table_offset * ENTRY); /* table has 54 entries */
}

/* Setup engine addresses from table (START, LOOP) */
/* Source: 0x77AD */

/*bpset 77b0,1,{printf "start adr:%02x pos:=%02x",bc, hl; g} */static 
uint16_t OSound_engine_set_adr(OSound* self, uint16_t* pos, uint8_t* chan, uint8_t* pcm)
{
    uint16_t start_adr = RomLoader_read16_16(&(roms.z80), (*pos)++);
    OSound_w16(self, pcm + 0x4, start_adr); /* Set Wave Start Address */

    /* TRAFFIC */
    if (self->engine_channel < 5)
    {
        if (chan[CH_ENGINES_FLAGS] & BIT_5) /* Mute */
        {
            if (chan[CH_ENGINES_FLAGS] & BIT_6)
            {
                chan[CH_ENGINES_FLAGS] |= BIT_6;               
                chan[CH_ENGINES_FLAGS] |= BIT_3; /* Denote loop address set                 */
                OSound_w16(self, pcm + 0x84, start_adr);       /* Set default loop address to start address */
                return start_adr;
            }
        }
        else
        {
            chan[CH_ENGINES_FLAGS] &= ~BIT_6;
        }
    }

    /* Return if loop address already set */
    if (chan[CH_ENGINES_FLAGS] & BIT_3)
        return start_adr;

    chan[CH_ENGINES_FLAGS] |= BIT_3; /* Denote loop address set   */
    OSound_w16(self, pcm + 0x84, start_adr);       /* Set default loop address to start address */
    return start_adr;
}

/* Source: 0x7853 */static 
void OSound_engine_set_adr_end(OSound* self, uint16_t* pos, uint16_t loop_adr, uint8_t* chan, uint8_t* pcm)
{
    /* Set wave end address from table */
    pcm[0x6] = RomLoader_read8(&(roms.z80), ++(*pos));

    /* Loop Disabled */
    if (chan[CH_ENGINES_FLAGS] & BIT_2)
        return;

    /* Loop Address >= End Address */
    if (pcm[0x6] >= pcm[0x85])
        return;

    /* Set loop address */
    OSound_w16(self, pcm + 0x84, loop_adr);
}

/* Thicken engine effect by panning left/right dependent on channel. */
/* Source: 0x77EA */static 
void OSound_vol_thicken(OSound* self, uint16_t* pos, uint8_t* chan, uint8_t* pcm)
{
    (*pos)++; /* Address of volume multiplier */

    /* Odd Channels: Pan Left */
    if (self->engine_channel & BIT_0)
    {
        pcm[0x2] = pcm[0x82] & BIT_5 ? 0 : OSound_get_adjusted_vol(self, pos, chan); /* left (if enabled) */
        pcm[0x3] = 0; /* right */
    }
    /* Even Channels: Pan Right */
    else
    {
        pcm[0x2] = 0; /* left */
        pcm[0x3] = pcm[0x83] & BIT_5 ? 0 : OSound_get_adjusted_vol(self, pos, chan); /* right (if enabled) */
    }
}

/* Get Adjusted Volume */
/* Source: 0x78A7 */static 
uint8_t OSound_get_adjusted_vol(OSound* self, uint16_t* pos, uint8_t* chan)
{
    uint8_t multiply =  RomLoader_read8(&(roms.z80), (*pos));
    uint16_t vol = (chan[CH_ENGINES_VOL1] * multiply) >> 6;

    if (vol > 0x3F)
        vol = 0x3F;

    return (uint8_t) vol;
}

/* bpset 7877,1,{printf "bc=%02x hl=%02x",bc, hl; g} */
/* Set Engine Pitch From Table */
/* Source: 0x7870 */static 
void OSound_engine_set_pitch(OSound* self, uint16_t* pos, uint8_t* pcm)
{
    uint16_t bc;
    (*pos)++; /* Increment to pitch entry in table */

    bc = OSound_r16(self, pcm + 0x82);
    bc >>= 2;

    if (bc & 0xFF00)
        bc = (bc & 0xFF00) | 0xFF;

    { uint16_t pitch = RomLoader_read8(&(roms.z80), (*pos));

    /*std::cout << std::hex << (*pos) << std::endl; */

    /* Read pitch from table */
    if (bc)
    {
        pitch += (bc & 0xFF);
        if (pitch > 0xFF)
            pitch = 0xFC;
    }

    /* Adjust the pitch slightly dependent on the channel selected */
    if (self->engine_channel & BIT_0)
        pcm[0x7] = (uint8_t) pitch;
    else
        pcm[0x7] = (uint8_t) pitch + 3;
 }}

/* Mute an engine channel */
/* Source: 0x7639 */static 
void OSound_engine_mute_channel(OSound* self, uint8_t* chan, uint8_t* pcm, bool do_check)
{
    /* Return if already muted */
    if (do_check && (chan[CH_ENGINES_ACTIVE] & BIT_0))
        return;

    /* Denote channel muted */
    chan[CH_ENGINES_ACTIVE] |= BIT_0;

    pcm[0x02] = 0;      /* Clear volume left */
    pcm[0x03] = 0;      /* Clear volume right */
    pcm[0x07] = 0;      /* Clear pitch */
    pcm[0x86] |= BIT_0; /* Denote not active */

    /* Clear some stuff */
    chan[CH_ENGINES_VOL0]    = 0;
    chan[CH_ENGINES_VOL1]    = 0;
    chan[CH_ENGINES_FLAGS]   = 0;
    chan[CH_ENGINES_PITCH_L] = 0;
    chan[CH_ENGINES_PITCH_H] = 0;
    chan[CH_ENGINES_VOL6]    = 0;
}

/* Adjust engine volume and write to new memory area */
/* Source: 0x76D7 */static 
void OSound_engine_adjust_volume(OSound* self, uint8_t* chan)
{
    uint16_t vol = (chan[CH_ENGINES_VOL1] * 0x18) >> 6;

    if (vol > 0x3F)
        vol = 0x3F;

    chan[CH_ENGINES_VOL6] = (uint8_t) vol;
}   

/* Set engine pan.  */
/* Adjust Volume and write to new memory area */
/* Also write to ix (PCM Channel RAM) */
/* Source: 0x76FD */static 
void OSound_engine_set_pan(OSound* self, uint16_t* pos, uint8_t* chan, uint8_t* pcm)
{
    uint16_t pitch = OSound_r16(self, chan + CH_ENGINES_PITCH_L) >> 1;
    pitch += RomLoader_read8(&(roms.z80), ++(*pos));

    { uint16_t vol = (chan[CH_ENGINES_VOL1] * pitch) >> 6;

    if (vol > 0x3F)
        vol = 0x3F;

    if (vol >= chan[CH_ENGINES_VOL6])
        vol = chan[CH_ENGINES_VOL6];

    /* Pan Left */
    if (self->engine_channel & BIT_0)
    {
        pcm[0x2] = (uint8_t) vol;      /* left */
        pcm[0x3] = (uint8_t) vol >> 1; /* right */
    }
    /* Pan Right */
    else
    {
        pcm[0x2] = (uint8_t) vol >> 1; /* left */
        pcm[0x3] = (uint8_t) vol;      /* right; */
    }
 }}

/* Read Engine Data & Store the engine pitch and volume to PCM Channel RAM */
/* Source: 0x778D */static 
void OSound_engine_read_data(OSound* self, uint8_t* chan, uint8_t* pcm)
{
    uint16_t pitch = (self->engine_data[SOUND_ENGINE_PITCH_H] << 8) + self->engine_data[SOUND_ENGINE_PITCH_L];

    pitch = (pitch >> 5) & 0x1FF;
    
    /* Store pitch in scratch space of channel (due to mirroring this wraps round to 0x00 in the channel) */
    pcm[0x0] = pitch & 0xFF;
    pcm[0x1] = (pitch >> 8) & 0xFF;
    chan[CH_ENGINES_VOL0] = self->engine_data[SOUND_ENGINE_VOL];
}

/* ---------------------------------------------------------------------------- */
/*                               PASSING TRAFFIC FX  */
/* ---------------------------------------------------------------------------- */

/* should be 0x9b 0x9b 0xe3 0xe3 for starting traffic */

/* Process Passing Traffic Channels */
/* Source: 0x7AFB */static 
void OSound_traffic_process(OSound* self)
{
    if ((self->engine_counter & 1) == 0)
        return;

    { uint16_t pcm_adr = 0x60; /* Channel 13: PCM Channel RAM Address */

    /* Iterate PCM Channels 13 to 16 */
    for (self->engine_channel = 4; self->engine_channel > 0; self->engine_channel--)
    {
        OSound_traffic_process_chan(self, &self->pcm_ram[pcm_adr]);
        pcm_adr += 0x8; /* Advance to next channel */
    }

 }}

/* Process Single Channel Of Traffic Sounds */
/* Source: 0x7B1F */static 
void OSound_traffic_process_chan(OSound* self, uint8_t* pcm)
{
    /* No slide/pitch reduction applied yet */
    if (!(pcm[0x82] & BIT_4))
    {
        uint8_t vol;
        OSound_traffic_read_data(self, pcm); /* Read Traffic Data that has been sent by 68K CPU */
        
        vol = pcm[0x00];
        
        /* vol on */
        if (vol)
        {
            uint8_t flags;
            OSound_traffic_note_changes(self, vol, pcm); /* Record changes to traffic volume and panning */
            flags = pcm[0x82];

            /* Change in Volume or Panning: Set volume, panning & pitch based on distance of traffic. */
            if (!(flags & BIT_0) || !(flags & BIT_1))
            {
                OSound_traffic_process_entry(self, pcm);
                return;
            }
            /* Return if start/end position of wave is already setup */
            else if (flags & BIT_2)
                return;

            /* Set volume, panning & pitch based on distance of traffic. */
            OSound_traffic_process_entry(self, pcm); 
            return;
        }
        /* vol off: decrease pitch */
        else
        {
            /* Check whether to instantly disable channel */
            if (!(pcm[0x82] & BIT_3))
            {
                OSound_traffic_disable(self, pcm);
                return;
            }

            pcm[0x82] |= BIT_4; /* Denote pitch reduction */

            /* Adjust pitch */
            if (pcm[0x07] < 0x81)
                pcm[0x07] -= 4;
            else
                pcm[0x07] -= 6;
        }
    }

    /* Reduce Volume Right Channel */
    if (pcm[0x03])
        pcm[0x03]--;

    /* Reduce Volume Left Channel */
    if (pcm[0x02])
        pcm[0x02]--;

    /* Once both channels have been reduced to zero, disable the sample completely */
    if (!pcm[0x02] && !pcm[0x03])
        OSound_traffic_disable(self, pcm);
}

const uint8_t TRAFFIC_PITCH_H[] = { 0, 2, 4, 4, 0, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8 };

/* Process traffic entry. Set volume, panning & pitch based on distance of traffic. */
/* Source: 0x7B82 */static 
void OSound_traffic_process_entry(OSound* self, uint8_t* pcm)
{
    uint8_t pitch;
    int8_t vol_boost;
    /* Wave Start/End Address has not been setup yet */
    if (!(pcm[0x82] & BIT_2))
    {
        pcm[0x82] |= BIT_2;           /* Denote set        */
        pcm[0x04] = pcm[0x84] = 0x82; /* Set Wave Start & Loop Addresses (Word) */
        pcm[0x05] = pcm[0x85] = 0x00;
        pcm[0x06] = 0x6;              /* Set Wave End     */
    }
    /* do_pan_vol */
    OSound_traffic_set_vol(self, pcm); /* Set Traffic Volume Multiplier */
    OSound_traffic_set_pan(self, pcm); /* Set Traffic Volume / Panning on each channel */

    vol_boost = pcm[0x80] - 0x16;
    pitch = 0;

    if (vol_boost >= 0)
        pitch = TRAFFIC_PITCH_H[vol_boost];

    pitch += (self->engine_channel & 1) ? 0x60 : 0x80;
    
    /* set_pitch2: */
    pcm[0x07] = pitch; /* Set Pitch */
    pcm[0x86] = 0x10;  /* Set Active & Enabled */

    /*std::cout << std::hex << (uint16_t) pitch << std::endl; */
}

/* Disable Traffic PCM Channel */
/* Source: 0x7BDC */static 
void OSound_traffic_disable(OSound* self, uint8_t* pcm)
{
    pcm[0x86] |= BIT_0; /* Disable sound */
    pcm[0x82] = 0;      /* Clear Flags */
    pcm[0x02] = 0;      /* Clear Volume Left */
    pcm[0x03] = 0;      /* Clear Volume Right */
    pcm[0x07] = 0;      /* Clear Delta (Pitch) */
    pcm[0x00] = 0;      /* Clear New Vol Index */
    pcm[0x80] = 0;      /* Clear Old Vol Index */
    pcm[0x01] = 0;      /* Clear New Pan Index */
    pcm[0x81] = 0;      /* Clear Old Pan Index */
}

/* Set Traffic Volume Multiplier */
/* Source: 0x7C28 */static 
void OSound_traffic_set_vol(OSound* self, uint8_t* pcm)
{
    /* Return if volume index is not set */
    uint8_t vol_entry = pcm[0x80];

    if (!vol_entry)
        return;

    { uint16_t multiply = TRAFFIC_VOL_MULTIPLY + vol_entry - 1;

    /* Set traffic volume multiplier */
    pcm[0x83] = RomLoader_read8(&(roms.z80), multiply);

    if (pcm[0x83] < 0x10)
        pcm[0x82] &= ~BIT_3; /* Disable Traffic Sound */
    else
        pcm[0x82] |= BIT_3;  /* Enable Traffic Sound */
 }}

/* Traffic Panning Indexes are as follows: */
/* 0 = Both */
/* 1 = Pan Left */
/* 2 = Pan Left More */
/* 3 = Hard Left Pan */
/* */
/* 5 = Both */
/* 6 = Hard Pan Right */
/* 7 = Pan Right More */
/* 8 = Pan Right */

const uint8_t TRAFFIC_PANNING[] = 
{ 
    0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x08, 0x0D, /* Right Channel */
    0x10, 0x0D, 0x08, 0x00, 0x10, 0x10, 0x10, 0x10  /* Left Channel */
};

/* Set Traffic Panning On Channel From Table */
/* Source: 0x7BFA */static 
void OSound_traffic_set_pan(OSound* self, uint8_t* pcm)
{
    pcm[0x03] = OSound_traffic_get_vol(self, pcm[0x81] + 0, pcm); /* Set Volume Right */
    pcm[0x02] = OSound_traffic_get_vol(self, pcm[0x81] + 8, pcm); /* Set Volume Left */
}

/* Read Traffic Volume Value from Table And Multiply Appropriately */
/* Source: 0x7C16 */static 
uint8_t OSound_traffic_get_vol(OSound* self, uint16_t pos, uint8_t* pcm)
{
    /* return volume from table * multiplier */
    return (TRAFFIC_PANNING[pos] * pcm[0x83]) >> 4;
}

/* Has Traffic Volume or Pitch changed? */
/* Set relevant flags when it has to denote the fact. */
/* Source: 0x7C48 */static 
void OSound_traffic_note_changes(OSound* self, uint8_t new_vol, uint8_t* pcm)
{
    /* Denote no volume entry change */
    if (new_vol == pcm[0x80])
        pcm[0x82] |= BIT_0;
    /* Record entry change */
    else
    {
        pcm[0x82] &= ~BIT_0;
        pcm[0x80] = new_vol;
    }

    /* Denote no pan entry change */
    if (pcm[0x01] == pcm[0x81])
        pcm[0x82] |= BIT_1;
    else
    {
        pcm[0x82] &= ~BIT_1;
        pcm[0x81] = pcm[0x01];
    }
}

/* Read Traffic Data that has been sent by 68K CPU */static 
void OSound_traffic_read_data(OSound* self, uint8_t* pcm)
{
    /* Get volume of traffic for channel */
    uint8_t vol = self->engine_data[SOUND_ENGINE_VOL + self->engine_channel];
    /*std::cout << std::hex << "ch: " << (int16_t) engine_channel << " vol: " << (int16_t) vol << std::endl; */
    pcm[0x01] = vol & 7;  /* Put bottom 3 bits in 01    (pan entry) */
    pcm[0x00] = vol >> 3; /* And remaining 5 bits in 00 (used as vol entry) */
}
