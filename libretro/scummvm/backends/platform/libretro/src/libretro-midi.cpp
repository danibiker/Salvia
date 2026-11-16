/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/*
 * Libretro MIDI output driver
 * Based on the ScummVM CoreMIDI and STMIDI drivers
 * Implements RETRO_ENVIRONMENT_GET_MIDI_INTERFACE for MIDI Out support
 */

// Disable symbol overrides so that we can use system headers.
#define FORBIDDEN_SYMBOL_ALLOW_ALL

#include <stdio.h>
#include <string.h>
#include <libretro.h>
#include "common/scummsys.h"
#include "audio/mpu401.h"
#include "common/error.h"
#include "common/util.h"
#include "common/system.h"
#include "common/config-manager.h"
#include "audio/musicplugin.h"
#include "backends/platform/libretro/include/libretro-defs.h"
#include "backends/platform/libretro/include/libretro-core.h"

// External libretro MIDI interface
extern struct retro_midi_interface *retro_midi_interface;

class MidiDriver_Libretro : public MidiDriver_MPU401 {
public:
	MidiDriver_Libretro() : _isOpen(false), _lastWriteUs(0), _gain(100), _lastGainCheck(0) {
		memset(_chanVol, 0, sizeof(_chanVol));
		memset(_chanVolSet, 0, sizeof(_chanVolSet));
	}
	int open();
	bool isOpen() const {
		return _isOpen;
	}
	void close();
	void send(uint32 b) override;
	void sysEx(const byte *msg, uint16 length) override;

private:
	bool _isOpen;
	uint64 _lastWriteUs;
	uint32 calcDeltaUs();

	/* "MIDI gain" (midi_gain, 0..1000, 100 = unchanged). ScummVM only applies
	 * it to its own software synths; here the frontend synthesizes, so it is
	 * applied to the channel volume (CC 7) messages instead. It cannot go past
	 * the synth's maximum channel volume (127). */
	uint16 _gain;
	uint32 _lastGainCheck;
	byte _chanVol[16];      /* last CC 7 value received for each channel, unscaled */
	bool _chanVolSet[16];
	byte scaleVolume(byte v) const {
		uint32 x = ((uint32)v * _gain + 50) / 100;
		return (byte)(x > 127 ? 127 : x);
	}
	void pushShort(byte status, byte d1, byte d2) {
		retro_midi_queue_push(status, calcDeltaUs());
		retro_midi_queue_push(d1, calcDeltaUs());
		retro_midi_queue_push(d2, calcDeltaUs());
	}
	static uint16 readGain() {
		int g = ConfMan.getInt("midi_gain");
		return (uint16)(g < 0 ? 0 : (g > 1000 ? 1000 : g));
	}
	void checkGain();
	inline bool outputAvailable() const {
		return retro_midi_interface && retro_midi_interface->output_enabled();
	}

};

/* Diagnostics: the first messages sent to the frontend are written to the
 * log in hex, so it can be checked what actually goes out (see also the
 * periodic byte counters in libretro-core.cpp). */
static int midi_log_budget_sysex = 16;
static int midi_log_budget_short = 96;
static uint32 midi_note_on[16];
static uint32 midi_note_last_log = 0;

/* Define LIBRETRO_MIDI_DEBUG to get these diagnostics in the log. */
static void logNoteSummary(bool force) {
#ifndef LIBRETRO_MIDI_DEBUG
	return;
#endif
	if (!retro_log_cb)
		return;
	uint32 now = g_system->getMillis();
	if (!force && now - midi_note_last_log < 5000)
		return;
	midi_note_last_log = now;
	char buf[16 * 12 + 1];
	char *p = buf;
	for (int i = 0; i < 16; i++)
		p += sprintf(p, "%d:%u ", i + 1, (unsigned)midi_note_on[i]);
	retro_log_cb(RETRO_LOG_INFO, "[scummvm] MIDI note-ons per channel (1-16): %s\n", buf);
}

static void logMidiMessage(const char *kind, const byte *data, uint32 len) {
#ifndef LIBRETRO_MIDI_DEBUG
	return;
#endif
	int &midi_log_budget = (kind[1] == 'y') ? midi_log_budget_sysex : midi_log_budget_short;
	if (midi_log_budget <= 0 || !retro_log_cb)
		return;
	midi_log_budget--;
	char buf[3 * 32 + 8];
	uint32 n = len > 32 ? 32 : len;
	char *p = buf;
	for (uint32 i = 0; i < n; i++)
		p += sprintf(p, "%02X ", data[i]);
	if (len > n)
		strcpy(p, "...");
	else
		*p = '\0';
	retro_log_cb(RETRO_LOG_INFO, "[scummvm] MIDI %s (%u bytes): %s\n", kind, (unsigned)len, buf);
	if (midi_log_budget == 0)
		retro_log_cb(RETRO_LOG_INFO, "[scummvm] MIDI: further %s messages not logged\n", kind);
}

uint32 MidiDriver_Libretro::calcDeltaUs() {
	uint64 nowUs = (uint64)g_system->getMillis() * 1000ULL;
	uint32 delta = 0;
	if (_lastWriteUs && nowUs >= _lastWriteUs) {
		uint64 d = nowUs - _lastWriteUs;
		delta = (d > 0xFFFFFFFFULL) ? 0xFFFFFFFFU : (uint32)d;
	}
	_lastWriteUs = nowUs;
	return delta;
}


/* Picks up changes of the MIDI gain setting (the options dialog can change it
 * while the game runs) and re-sends the channel volumes with the new gain. */
void MidiDriver_Libretro::checkGain() {
	uint32 now = g_system->getMillis();
	if (now - _lastGainCheck < 500)
		return;
	_lastGainCheck = now;
	uint16 g = readGain();
	if (g == _gain)
		return;
	_gain = g;
	for (int ch = 0; ch < 16; ch++)
		if (_chanVolSet[ch])
			pushShort((byte)(0xB0 | ch), 7, scaleVolume(_chanVol[ch]));
}

int MidiDriver_Libretro::open() {
	if (_isOpen)
		return MERR_ALREADY_OPEN;

	if (!retro_midi_interface) {
		if (retro_log_cb)
			retro_log_cb(RETRO_LOG_WARN, "[scummvm] MIDI out: frontend has no MIDI interface\n");
		return MERR_DEVICE_NOT_AVAILABLE;
	}

	if (!retro_midi_interface->output_enabled()) {
		if (retro_log_cb)
			retro_log_cb(RETRO_LOG_WARN, "[scummvm] MIDI out: frontend MIDI output is disabled\n");
		return MERR_DEVICE_NOT_AVAILABLE;
	}

	_isOpen = true;
	_lastWriteUs = 0;
	_gain = readGain();
	_lastGainCheck = g_system->getMillis();
	memset(_chanVolSet, 0, sizeof(_chanVolSet));
	midi_log_budget_sysex = 16;
	midi_log_budget_short = 96;
	memset(midi_note_on, 0, sizeof(midi_note_on));
	midi_note_last_log = g_system->getMillis();
	if (retro_log_cb)
		retro_log_cb(RETRO_LOG_INFO, "[scummvm] MIDI out: opened (frontend MIDI output)\n");
	return 0;
}

void MidiDriver_Libretro::close() {
	if (_isOpen && outputAvailable())
		stopAllNotes(true);
	MidiDriver_MPU401::close();
	_isOpen = false;
}

void MidiDriver_Libretro::send(uint32 b) {
	midiDriverCommonSend(b);

	if (!_isOpen)
		return;
	if (!outputAvailable())
		return;

	byte status = (byte)(b & 0xFF);
	byte d1	 = (byte)((b >> 8) & 0xFF);
	byte d2	 = (byte)((b >> 16) & 0xFF);

	checkGain();
	if ((status & 0xF0) == 0xB0 && d1 == 7) {
		_chanVol[status & 0x0F] = d2;
		_chanVolSet[status & 0x0F] = true;
		d2 = scaleVolume(d2);
	}

	// to be handled by sysEx()
	if (status == 0xF0)
		return;

	// Realtime messages: 1 byte (F8..FF)
	if (status >= 0xF8) {
		retro_midi_queue_push(status, calcDeltaUs());
		return;
	}

	// System Common
	switch (status) {
	case 0xF1: // MTC quarter frame (2 bytes total)
		retro_midi_queue_push(status, calcDeltaUs());
		retro_midi_queue_push(d1, calcDeltaUs());
		return;
	case 0xF2: // Song Position Pointer (3 bytes total)
		retro_midi_queue_push(status, calcDeltaUs());
		retro_midi_queue_push(d1, calcDeltaUs());
		retro_midi_queue_push(d2, calcDeltaUs());
		return;
	case 0xF3: // Song Select (2 bytes total)
		retro_midi_queue_push(status, calcDeltaUs());
		retro_midi_queue_push(d1, calcDeltaUs());
		return;
	case 0xF6: // Tune request (1 byte)
	case 0xF7: // EOX
		retro_midi_queue_push(status, calcDeltaUs());
		return;
	default:
		break;
	}

	// Channel voice
	{
		byte msg[3] = { status, d1, d2 };
		byte kind = status & 0xF0;
		logMidiMessage("short", msg, (kind == 0xC0 || kind == 0xD0) ? 2 : 3);
		if (kind == 0x90 && d2 != 0)
			midi_note_on[status & 0x0F]++;
		logNoteSummary(false);
	}
	retro_midi_queue_push(status, calcDeltaUs());
	switch (status & 0xF0) {
	case 0xC0: // Program change: 1 data
	case 0xD0: // Channel pressure: 1 data
		retro_midi_queue_push(d1, calcDeltaUs());
		break;
	case 0x80:
	case 0x90:
	case 0xA0:
	case 0xB0:
	case 0xE0:
		retro_midi_queue_push(d1, calcDeltaUs());
		retro_midi_queue_push(d2, calcDeltaUs());
		break;
	default:
		break;
	}
}

void MidiDriver_Libretro::sysEx(const byte *msg, uint16 length) {
	midiDriverCommonSysEx(msg, length);

	if (!_isOpen)
		return;
	if (!outputAvailable())
		return;
	if (!msg || length == 0)
		return;

	// Per MidiDriver_BASE info: SysEx max 268 bytes
	if (length > 268)
		length = 268;

	logMidiMessage("sysex", msg, length);

	// Send SysEx start
	retro_midi_queue_push(0xF0, calcDeltaUs());

	// Send SysEx data (excluding F0 start and F7 end)
	for (uint16 i = 0; i < length; i++) {
		retro_midi_queue_push(msg[i], calcDeltaUs());
	}

	// Send SysEx end
	retro_midi_queue_push(0xF7, calcDeltaUs());
}

// Plugin interface

class LibretroMusicPlugin : public MusicPluginObject {
public:
	const char *getName() const {
		return "Libretro MIDI Out";
	}

	const char *getId() const {
		return "libretro_midi";
	}

	MusicDevices getDevices() const;
	Common::Error createInstance(MidiDriver **mididriver, MidiDriver::DeviceHandle = 0) const;
	bool checkDevice(MidiDriver::DeviceHandle hdl, int checkFlags, bool quiet) const override;
};

MusicDevices LibretroMusicPlugin::getDevices() const {
	MusicDevices devices;
	// Return GM device as default
	devices.push_back(MusicDevice(this, "", MT_GM));
	return devices;
}

Common::Error LibretroMusicPlugin::createInstance(MidiDriver **mididriver, MidiDriver::DeviceHandle) const {
	*mididriver = new MidiDriver_Libretro();

	return Common::kNoError;
}

bool LibretroMusicPlugin::checkDevice(MidiDriver::DeviceHandle hdl, int checkFlags, bool quiet) const {
	if (!retro_midi_interface || !retro_midi_interface->output_enabled()) {
		if (!quiet)
			warning("Libretro MIDI: interface not available or output disabled");
		return false;
	}

	// Allow auto selection if available
	if (checkFlags & MDCK_AUTO)
		return true;

	// For explicit selection, handle should normally be non-zero
	return hdl != 0;
}

REGISTER_PLUGIN_STATIC(LIBRETRO_MIDI, PLUGIN_TYPE_MUSIC, LibretroMusicPlugin);
